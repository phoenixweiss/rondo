#include "model.hpp"
#include "platform.hpp"
#include <cassert>
#include <iostream>
#include <unistd.h>
using namespace rondo;
int main(int argc, char **argv) {
  assert(argc == 2);
  auto root = fs::temp_directory_path() / ("rondo-model-tests-" + std::to_string(getpid()));
  fs::create_directories(root);
  try {
    Store s(root / "state");
    assert(s.data["replace_boot_logo"] == true);
    assert(s.data["locale"] == "en");
    auto next = s.data;
    next["custom"] = "preserved";
    next["artwork"] = "title";
    s.commit(next);
    Store reloaded(root / "state");
    assert(reloaded.data["custom"] == "preserved" && reloaded.data["artwork"] == "title");
    next = reloaded.data;
    next["locale"] = "ru";
    reloaded.commit(next);
    atomicWrite(root / "state/config.json", {'b', 'a', 'd'});
    Store recovered(root / "state");
    assert(recovered.data["artwork"] == "title" && !recovered.warning.empty());
    auto validRecovery = recovered.data;
    validRecovery["locale"] = "ru";
    recovered.commit(validRecovery);
    assert(readJson(root / "state/config.previous.json")["artwork"] == "title");
    std::string future = "{\"schema\":2,\"locale\":\"ru\"}";
    atomicWrite(root / "future/config.json", {future.begin(), future.end()});
    Store newer(root / "future");
    bool refused = false;
    try {
      newer.commit(newer.data);
    } catch (...) {
      refused = true;
    }
    assert(refused && readJson(root / "future/config.json")["schema"] == 2);

    // Legacy explicit values take priority; absent values inherit without a migration.
    auto legacy = s.data;legacy.erase("integer_scaling");legacy.erase("game_background");
    legacy["game_options"]={{"on",{{"integer_scaling",true}}},{"off",{{"integer_scaling",false}}}};
    auto encoded=legacy.dump();atomicWrite(root/"legacy/config.json",{encoded.begin(),encoded.end()});
    Store migrated(root/"legacy");assert(migrated.warning.empty());
    assert(!migrated.integerScaling("new") && migrated.integerScaling("on") && !migrated.integerScaling("off"));
    auto updated=migrated.data;updated["integer_scaling"]=true;migrated.commit(updated);
    Store inherited(root/"legacy");assert(inherited.integerScaling("new") && !inherited.integerScaling("off"));
    auto invalid=updated;invalid["game_background"]="invalid";
    encoded=invalid.dump();atomicWrite(root/"legacy/config.json",{encoded.begin(),encoded.end()});
    Store fallback(root/"legacy");assert(!fallback.warning.empty() && fallback.data["game_background"]=="black");

    fs::create_directories(root / "card-a/NES/Same");
    fs::create_directories(root / "card-b/NES/Same");
    atomicWrite(root / "card-a/NES/Same/test.nes", {1, 2});
    atomicWrite(root / "card-b/NES/Same/test.nes", {1, 2});
    atomicWrite(root / "card-a/NES/Same/cover.png", {1});
    auto games =
        scan({{"a", root / "card-a"}, {"b", root / "card-b"}, {"missing", root / "absent"}});
    assert(games.size() == 2 && games[0].id != games[1].id);
    auto first = games[0].id;
    fs::rename(root / "card-a", root / "moved");
    auto moved = scan({{"a", root / "moved"}, {"b", root / "card-b"}});
    atomicWrite(root / "moved/NES/._metadata.nes", {0, 5, 22, 7});
    atomicWrite(root / "moved/.Trashes/hidden.nes", {0, 5, 22, 7});
    std::string hiddenPlaylist = "game.nes\n";
    atomicWrite(root / "moved/NES/._playlist.m3u", {hiddenPlaylist.begin(), hiddenPlaylist.end()});
    assert(scan({{"a", root / "moved"}, {"b", root / "card-b"}}).size() == moved.size());
    assert(moved[0].id == first);
    std::vector<Volume> recentVolumes{{"a",root/"moved"},{"b",root/"card-b"}};
    auto history=recentAfterLaunch(Json::array(),moved[0],recentVolumes);
    history=recentAfterLaunch(history,moved[1],recentVolumes);
    assert(history.size()==2 && history[0]["id"]==moved[1].id);
    history=recentAfterLaunch(history,moved[0],recentVolumes);
    assert(history.size()==2 && history[0]["id"]==moved[0].id);
    auto absent=recentUnavailable(history,{moved[1]});
    assert(absent.size()==1 && absent[0].id==moved[0].id && !absent[0].available && absent[0].path.empty());
    for(int i=0;i<30;++i) {
      std::string rel="NES/Game"+std::to_string(i)+"/game.nes",identity="a\n"+rel;
      Game entry{hashBytes({identity.begin(),identity.end()}),"a","NES","Game "+std::to_string(i),root/"moved"/rel};
      history=recentAfterLaunch(history,entry,recentVolumes);
    }
    assert(history.size()==20 && history[0]["title"]=="Game 29" && history[19]["title"]=="Game 10");
    auto preferences=migrated.data;preferences["recent_games"]=history;
    migrated.commit(preferences);Store remembered(root/"legacy");assert(remembered.data["recent_games"]==history);
    auto hostile=history;hostile[0]["relative_path"]="../../config.json";
    preferences["recent_games"]=hostile;encoded=preferences.dump();
    atomicWrite(root/"legacy/config.json",{encoded.begin(),encoded.end()});
    Store sanitized(root/"legacy");assert(sanitized.warning.empty() && sanitized.data["recent_games"].size()==19);
    assert(sanitized.data["locale"]==preferences["locale"]);

    fs::create_directories(root / "moved/PS/Disc");
    atomicWrite(root / "moved/PS/Disc/a.cue", {});
    atomicWrite(root / "moved/PS/Disc/b.cue", {});
    std::string playlist = "a.cue\nb.cue\n";
    atomicWrite(root / "moved/PS/Disc/game.m3u", {playlist.begin(), playlist.end()});
    assert(scan({{"a", root / "moved"}}).size() == 2);
    // All platforms share one title order, independent of case and host locale.
    const std::vector<std::pair<std::string,std::string>> library = {
        {"NES/zebra", ".nes"}, {"GBA/alpha", ".gba"}, {"GB/Beta", ".gb"},
        {"SNES/Якорь", ".sfc"}, {"NES/ёлка", ".nes"}, {"GBC/Енот", ".gbc"},
        {"GBA/АЛЬФА", ".gba"}, {"NES/alpha", ".nes"}};
    for (const auto &[name, ext] : library)
      atomicWrite(root / "sorted" / (name + ext), {1});
    auto ordered = scan({{"sorted", root / "sorted"}});
    const std::vector<std::string> expected = {"alpha", "alpha", "Beta", "zebra",
                                              "АЛЬФА", "ёлка", "Енот", "Якорь"};
    for (size_t i = 0; i < expected.size(); ++i) assert(ordered.at(i).title == expected[i]);
    assert(ordered[0].system == "GBA" && ordered[1].system == "NES");
    auto reversedVolumes = scan({{"b", root / "card-b"}, {"a", root / "moved"}});
    auto forwardVolumes = scan({{"a", root / "moved"}, {"b", root / "card-b"}});
    for (size_t i = 0; i < forwardVolumes.size(); ++i)
      assert(forwardVolumes[i].id == reversedVolumes[i].id);
    auto lastPlayed = recentAfterLaunch(Json::array(), ordered.back(), {{"sorted",root/"sorted"}});
    lastPlayed = recentAfterLaunch(lastPlayed, ordered.front(), {{"sorted",root/"sorted"}});
    assert(lastPlayed[0]["id"] == ordered.front().id && lastPlayed[1]["id"] == ordered.back().id);

    auto full = scrollGeometry(500, 280, 500, 268);
    assert(full.visible && full.position + full.thumb <= 268.001);
    assert(!scrollGeometry(100, 280, 0, 268).visible);
    assert(scrollGeometry(100000, 280, 0, 268).thumb == 24);
    Locale l(argv[1]);
    l.select(argv[1], "unknown");
    assert(l.current == "en");
    l.select(argv[1], "ru");
    assert(l.tr("screen.library") == "Библиотека");
    assert(l.tr("slot.name", {{"number", 2}}).find('2') != std::string::npos);
    assert(utf8Prefix("Ёж", 1).empty());
    auto assets = root / "assets";
    fs::create_directories(assets / "Locales");
    auto registry = readJson(fs::path(argv[1]) / "Locales/registry.json");
    registry["locales"].push_back({{"code", "zz"}, {"native_name", "Test"}, {"direction", "ltr"}});
    std::string r = registry.dump();
    atomicWrite(assets / "Locales/registry.json", {r.begin(), r.end()});
    atomicWrite(assets / "Locales/en.json", readBytes(fs::path(argv[1]) / "Locales/en.json"));
    std::string c = "{\"screen.library\":\"A longer third language "
                    "title\",\"slot.name\":\"Bad {wrong}\"}";
    atomicWrite(assets / "Locales/zz.json", {c.begin(), c.end()});
    Locale third(assets);
    third.select(assets, "zz");
    assert(third.current == "zz" && third.tr("screen.library") == "A longer third language title");
    assert(third.tr("action.save_state") == "Save state");
    assert(third.tr("slot.name", {{"number", 3}}) == "Slot 3");
    Timezones z;
    assert(z.valid("Europe/London"));
    assert(!z.valid("../../etc/passwd"));
    assert(z.offset("Europe/London", 1767225600) == "UTC+00:00");
    assert(z.offset("Europe/London", 1782864000) == "UTC+01:00");
    assert(Timezones(root / "no-tzdata").zones.empty());
    bool failed = false;
    atomicWrite(root / "blocked", {1});
    try {
      atomicWrite(root / "blocked/config.json", {2});
    } catch (...) {
      failed = true;
    }
    assert(failed);
    auto sys = root / "sys";
    auto battery = sys / "class/power_supply/axp2202-battery";
    auto rfkill = sys / "class/rfkill/rfkill0";
    auto net = sys / "class/net/wlan0";
    auto proc = root / "proc";
    auto put = [](fs::path p, std::string text) { atomicWrite(p, {text.begin(), text.end()}); };
    put(battery / "capacity", "78");
    put(battery / "status", "Charging");
    put(battery / "voltage_now", "4100000");
    put(rfkill / "type", "wlan");
    put(rfkill / "soft", "0");
    put(rfkill / "hard", "0");
    put(net / "flags", "0x1003");
    put(net / "carrier", "0");
    put(net / "operstate", "dormant");
    auto telemetry = readTelemetry(sys,proc);
    assert(telemetry.capacity == 78 && telemetry.voltageMicrovolts == 4100000 &&
           telemetry.wifiState == "connecting");
    put(net / "carrier", "1");
    put(proc / "net/wireless", "Inter-| sta-| Quality\n wlan0: 0000 54. -62. -256. 0 0\n");
    assert(readTelemetry(sys,proc).wifiState == "connected");
    assert(readTelemetry(sys,proc).wifiSignalDbm== -62);
    put(proc/"net/wireless"," wlan0: 0000 54. 0. -256. 0 0\n");
    assert(!readTelemetry(sys,proc).wifiSignalDbm);
    put(net/"flags","0x1002");assert(!readTelemetry(sys,proc).wifiEnabled);
    put(net/"flags","0x1003");
    put(net / "carrier", "0");
    put(net / "operstate", "down");
    assert(readTelemetry(sys,proc).wifiState == "disconnected");
    put(rfkill / "soft", "1");
    put(battery / "capacity", "101");
    assert(!readTelemetry(sys,proc).capacity && !readTelemetry(sys,proc).wifiEnabled);
    assert(!readTelemetry(root / "no-sys").capacity);
    assert(wifiSignalLevel(-45)==3 && wifiSignalLevel(-60)==2 && wifiSignalLevel(-75)==1 && wifiSignalLevel(-90)==0);
    assert(wifiSignalLevel(std::nullopt)== -1 && wifiSignalLevel(0)== -1);
    int16_t pcm[] = {-32768, 32767, -100, 100};
    assert(mixVolume(pcm, 2, 0) == std::vector<int16_t>({0, 0, 0, 0}));
    assert(mixVolume(pcm, 2, 50) == std::vector<int16_t>({-16384, 16383, -50, 50}));
    assert(mixVolume(pcm, 2, 100) == std::vector<int16_t>(pcm, pcm + 4));
    assert(volumeKeyAction(1, 115, 1) == "VOLUME_UP" &&
           volumeKeyAction(1, 114, 1) == "VOLUME_DOWN" && volumeKeyAction(1, 114, 2).empty() && volumeKeyAction(1, 115, 0).empty());
    VolumeInputState volume;
    assert(volume.event(1,115,1,1000)==std::vector<std::string>{"VOLUME_UP"});
    assert(volume.event(1,115,1,1100).empty());
    assert(volume.event(1,115,2,1200).empty());
    assert(volume.repeat(1349).empty());
    assert(volume.repeat(1350)==std::vector<std::string>{"VOLUME_UP"});
    assert(volume.repeat(1449).empty());
    assert(volume.repeat(1450)==std::vector<std::string>{"VOLUME_UP"});
    assert(volume.repeat(9000)==std::vector<std::string>{"VOLUME_UP"});
    assert(volume.repeat(9000).empty());
    assert(volume.event(1,115,0,9001).empty() && volume.repeat(9999).empty());
    assert(volume.event(1,114,1,10000)==std::vector<std::string>{"VOLUME_DOWN"});
    assert(volume.repeat(10350)==std::vector<std::string>{"VOLUME_DOWN"});
    assert(volume.event(1,115,1,10351)==std::vector<std::string>{"VOLUME_UP"});
    assert(volume.repeat(11000).empty()); // Both held: neither repeats.
    volume.event(1,114,0,11001);
    assert(volume.repeat(11002)==std::vector<std::string>{"VOLUME_UP"});
    volume.event(0,3,0,11003);assert(volume.held.empty() && volume.repeat(12000).empty());
    assert(volume.event(1,115,1,12001).empty()); // Ignore stale events until SYN_REPORT.
    volume.event(0,0,0,12002);
    assert(volume.event(1,116,1,13000).empty() && volume.event(3,115,1,13000).empty());
    for(int i=0;i<3;++i) {
      assert(volume.event(1,115,1,14000+i*10)==std::vector<std::string>{"VOLUME_UP"});
      volume.event(1,115,0,14001+i*10);
    }
    assert(volume.repeat(15000).empty());
    VolumeFeedback feedback;
    feedback.confirmed = 80;
    auto firstVolumeOperation = feedback.begin(1000);
    assert(feedback.phase == "applying");
    auto second = feedback.begin(1001);
    feedback.result(firstVolumeOperation, 20, true, 1002);
    assert(feedback.confirmed == 80 && feedback.phase == "applying");
    feedback.result(second, 75, true, 1003);
    assert(feedback.confirmed == 75 && feedback.visible(2502) && !feedback.visible(2503));
    auto mute = feedback.begin(3000);
    feedback.result(mute, 0, true, 3001);
    assert(feedback.phase == "muted");
    auto fail = feedback.begin(5000);
    feedback.result(fail, 100, false, 5001);
    assert(feedback.confirmed == 0 && feedback.phase == "error");
    auto unknown = feedback.begin(9000);
    feedback.result(unknown, std::nullopt, true, 9001);
    assert(feedback.phase == "unknown" && !feedback.confirmed);
    feedback.begin(12000);
    feedback.timeout(14000);
    assert(feedback.phase == "error");
    auto serialized = feedback.json();
    VolumeFeedback restored;
    restored.restore(serialized);
    assert(restored.json() == serialized);
    H700InputState pad;
    assert(pad.event(1, 304, 1, 0) == std::vector<std::string>{"A"});
    assert(pad.down.count("A"));
    assert(pad.event(1, 304, 1, 1).empty());
    assert(pad.event(1, 304, 2, 2).empty());
    pad.event(1, 304, 0, 3);
    assert(!pad.down.count("A"));
    assert(pad.event(3, 17, -1, 10) == std::vector<std::string>{"UP"});
    assert(pad.repeat(359).empty());
    assert(pad.repeat(360) == std::vector<std::string>{"UP"});
    assert(pad.event(3, 17, 1, 400) == std::vector<std::string>{"DOWN"});
    assert(!pad.down.count("UP") && pad.down.count("DOWN"));
    pad.event(3, 17, 0, 401);
    assert(pad.down.empty() && pad.repeat(1000).empty());
    assert(pad.event(1, 312, 1, 1001) == std::vector<std::string>{"MENU"});
    pad.event(1, 312, 0, 1002);
    assert(pad.event(1, 354, 1, 1003).empty());
    assert(pad.event(1, 116, 1, 1004).empty());
    for (auto &[code, action] : std::map<unsigned, std::string>{{305,"B"},{307,"X"},{306,"Y"},{310,"SELECT"},{311,"START"},{308,"L1"},{309,"R1"}}) {
      assert(pad.event(1, code, 1, 1005) == std::vector<std::string>{action});
      pad.event(1, code, 0, 1006);
    }
    pad.event(1, 103, 1, 1010);
    pad.event(0, 3, 0, 1011);
    assert(pad.down.empty() && pad.repeats.empty());
    assert(brightnessRawLevel(0)==4 && brightnessRawLevel(100)==254);
    assert(brightnessRawLevel(90)==192 && brightnessRawLevel(95)==224);
    assert(brightnessRawLevel(-100)==4 && brightnessRawLevel(1000)==254);
    for(int percent=0;percent<=100;percent+=5) {
      int raw=brightnessRawLevel(percent);
      assert(raw!=200 && raw!=255); // Vendor ioctl special cases.
      assert(brightnessPercent(raw)==percent);
      if(percent)assert(raw>brightnessRawLevel(percent-5));
    }
    fs::remove_all(root);
    std::cout << "Model checks passed: preservation, recovery, volumes/playlists, "
                 "scrolling, localization extension, DST, write failure.\n";
  } catch (...) {
    std::cerr << "Failed fixture retained: " << root << "\n";
    throw;
  }
}
