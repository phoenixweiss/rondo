// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
namespace rondo {
App::App(Options opts, bool runner, const Game &entry)
    : options(std::move(opts)), store(options.state), locale(options.assets) {
  locale.select(options.assets, store.data.value("locale", "en"));
  timezoneNames = readJson(options.assets / "Timezones/names.json");
  notice = store.warning.empty() ? "" : locale.tr("notice.config_recovered");
  if (!store.warning.empty()) std::cerr << "Configuration: " << store.warning << "\n";
  artwork = store.data.value("artwork", "cover");
  section = store.data.value("section", "All");
  previewVolume = store.data.value("volume", 80);
  ui = std::make_unique<UI>(options.assets, options.headless);
  device = deviceControl(!options.headless);
  brightness = device->brightness();
  if (brightness) {
    bool stored=false;
    try { stored=readJson(options.state/"config.json").contains("brightness"); } catch (...) {}
    try {
      if (stored) device->brightness(store.data.value("brightness",*brightness));
      brightness=device->brightness();
      if (brightness) store.data["brightness"]=*brightness;
    } catch (const std::exception &e) { notice=locale.tr("brightness.error");std::cerr<<e.what()<<"\n"; }
  }
  if (!runner && !options.headless && bootLogoSupported())
    requestBootLogo(store.data.value("replace_boot_logo", true));
  if (runner) {
    game = entry;
    core = std::make_unique<Core>(options.coreFor(game.system), game, store.gameDir(game), options.headless);
    core->volume = previewVolume;
    screen = Screen::Running;
  } else {
    games = scan(options.volumes);
    sections = {"All", "Favorites", "Recent"};
    for (auto &g : games)
      if (std::find(sections.begin(), sections.end(), g.system) == sections.end())
        sections.push_back(g.system);
    auto unavailable=recentUnavailable(store.data.at("recent_games"),games);
    games.insert(games.end(),unavailable.begin(),unavailable.end());
    if (std::find(sections.begin(), sections.end(), section) == sections.end())
      section = "All";
    auto v = visible();
    auto id = store.data.value("focus", "");
    for (size_t i = 0; i < v.size(); ++i)
      if (games[v[i]].id == id)
        selected = int(i);
  }
  lastActivity=monotonicMs();
  if(!runner)offerStartupResume();
  volumeFeedback.confirmed = store.data.value("volume", 80);
  wifiDevice=wifiAdapter(options.state,!runner && !options.headless);
  wifi.available=wifiDevice->supported();wifi.phase=wifi.available?"initializing":"unavailable";
  if(wifi.available) {
    auto adapter=wifiDevice;bool wanted=store.data.value("wifi_enabled",false);
    wifiRun("startup",[adapter,wanted]{return adapter->enable(wanted);});
  }
  btDevice=bluetoothAdapter(options.assets.parent_path()/"tools/bluetooth-radio.sh",!options.headless);
  bt.available=btDevice->supported();
  if(bt.available && store.data.value("bluetooth_enabled",false) && !runner) btResume();
  clockDevice=clockAdapter(options.assets.parent_path()/"tools/ntp-notify.sh",!runner&&!options.headless);
  clockState.available=clockDevice->supported();
  try {clockLastSync=readJson(options.state/"Time/last-sync.json").value("utc",0LL);}catch(...){}
  if(clockState.available) {
    auto device=clockDevice;bool wanted=store.data.value("ntp_auto",true);
    clockRun("startup",[device,wanted]{return wanted?device->restart():device->enable(false);});
  }
  diagnosticRunner = runner;
  configureDiagnostics();
  record("ready");
}
// Library search folds ASCII and Cyrillic without depending on the host locale.
static std::string searchFold(std::string text) {
  for(size_t i=0;i<text.size();++i) {
    auto c=static_cast<unsigned char>(text[i]);
    if(c>='A' && c<='Z')text[i]=char(c+32);
    else if(i+1<text.size()) {
      auto next=static_cast<unsigned char>(text[i+1]);
      if(c==0xd0 && next==0x81) {text[i]=char(0xd1);text[++i]=char(0x91);}
      else if(c==0xd0 && next>=0x90 && next<=0xaf) {
        if(next<0xa0)text[++i]=char(next+32);
        else {text[i]=char(0xd1);text[++i]=char(next-32);}
      }
    }
  }
  return text;
}
static void eraseGlyph(std::string &text) {
  if(text.empty())return;
  auto i=text.size()-1;
  while(i && (static_cast<unsigned char>(text[i])&0xc0)==0x80)--i;
  text.erase(i);
}
std::vector<std::vector<std::string>> App::searchKeys() const {
  const std::vector<std::string> rows=searchRussian?
    std::vector<std::string>{"1234567890", "йцукенгшщзх", "фывапролджэ", "ячсмитьбюё-"}:
    std::vector<std::string>{"1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm._'"};
  std::vector<std::vector<std::string>> keys;
  for(const auto &row:rows) {
    std::vector<std::string> cells;
    for(size_t i=0;i<row.size();) {
      size_t n=static_cast<unsigned char>(row[i])<128?1:2;
      cells.push_back(row.substr(i,n));i+=n;
    }
    keys.push_back(cells);
  }
  keys.push_back({"space","language","delete","apply"});
  return keys;
}
void App::searchText(const std::string &text) {
  if(screen!=Screen::Search)return;
  for(size_t i=0;i<text.size();) {
    auto c=static_cast<unsigned char>(text[i]);
    size_t n=c<128?1:c>=0xc2 && c<=0xdf?2:c>=0xe0 && c<=0xef?3:c>=0xf0 && c<=0xf4?4:0;
    if(!n || i+n>text.size())break;
    bool valid=c>=32 && c!=127;
    for(size_t j=1;j<n;++j)valid=valid && (static_cast<unsigned char>(text[i+j])&0xc0)==0x80;
    if(valid && searchDraft.size()+n<=96)searchDraft+=text.substr(i,n);
    i+=n;
  }
}
void App::searchAction(const std::string &action) {
  if(action=="B") {SDL_StopTextInput();screen=Screen::Library;return;}
  if(action=="SEARCH_DELETE" || action=="X") {eraseGlyph(searchDraft);return;}
  if(action=="Y") {searchRussian=!searchRussian;searchColumn=0;return;}
  if(action=="SEARCH_SUBMIT" || action=="MENU") {
    searchQuery=searchFold(searchDraft);selected=0;scroll=0;
    SDL_StopTextInput();screen=Screen::Library;return;
  }
  if(action=="UP")searchRow=std::max(0,searchRow-1);
  if(action=="DOWN")searchRow=std::min(4,searchRow+1);
  auto keys=searchKeys();
  searchColumn=std::min(searchColumn,int(keys[searchRow].size())-1);
  if(action=="LEFT")searchColumn=std::max(0,searchColumn-1);
  if(action=="RIGHT")searchColumn=std::min(int(keys[searchRow].size())-1,searchColumn+1);
  if(action=="A") {
    auto key=keys[searchRow][searchColumn];
    if(key=="apply")searchAction("SEARCH_SUBMIT");
    else if(key=="delete")eraseGlyph(searchDraft);
    else if(key=="language") {searchRussian=!searchRussian;searchColumn=0;}
    else searchText(key=="space"?" ":key);
  }
}
void App::searchDraw() {
  auto tr=[&](const std::string &key){return locale.tr(key);};
  ui->frame(tr("search.title"),section=="All"?tr("section.all"):section=="Favorites"?tr("screen.favorites"):section=="Recent"?tr("section.recent_title"):section,store.data.value("timezone","Etc/UTC"));
  ui->row(searchDraft.empty()?tr("search.placeholder"):searchDraft,128,false,672,"",true,48);
  auto keys=searchKeys();
  for(int y=0;y<5;++y)for(size_t x=0;x<keys[y].size();++x) {
    int cell=672/int(keys[y].size()),left=24+int(x)*cell;
    bool focused=y==searchRow && int(x)==searchColumn;
    ui->rect(left,190+y*43,cell-5,38,focused?ui->focus:ui->bg,5);
    auto key=keys[y][x];
    auto label=key=="language"?(searchRussian?"EN":"РУ"):key.size()>2?tr("search.key."+key):key;
    ui->text(label,left+(cell-5-ui->measure(label,21))/2,196+y*43,21);
  }
  if(ui->keyboardHints)ui->hints({{"Enter",tr("search.apply")},{"Esc",tr("action.cancel")},{"Backspace",tr("wifi.delete")}});
  else ui->hints({{"A",tr("wifi.choose")},{"B",tr("action.cancel")},{"X",tr("wifi.delete")},{"Y",tr("search.language")}});
}
std::vector<int> App::visible() const {
  std::vector<int> v;
  auto favorites = store.data.value("favorites", Json::array());
  if (section == "Recent") {
    for (const auto &entry : store.data.at("recent_games")) {
      const auto &id=entry.at("id").get_ref<const std::string &>();
      for (size_t i=0;i<games.size();++i)
        if (games[i].id==id && (searchQuery.empty() ||
            (i<searchNames.size() && searchNames[i].find(searchQuery)!=std::string::npos))) {
          v.push_back(int(i));break;
        }
    }
    return v;
  }
  for (size_t i = 0; i < games.size(); i++) {
    auto &g = games[i];
    bool fav = std::find(favorites.begin(), favorites.end(), g.id) != favorites.end();
    if (g.available && (section == "All" || (section == "Favorites" && fav) || section == g.system) &&
        (searchQuery.empty() || (i<searchNames.size() && searchNames[i].find(searchQuery)!=std::string::npos)))
      v.push_back(i);
  }
  return v;
}
std::string App::countryLabel(const std::string &id) const {
  if (id == "UTC") return "UTC";
  const auto &names = timezoneNames.at(locale.current == "ru" ? "ru" : "en");
  return names.at("countries").value(id, id);
}
std::string App::zoneLabel(const std::string &id) const {
  if (id == "Etc/UTC") return "UTC";
  const auto &names = timezoneNames.at(locale.current == "ru" ? "ru" : "en");
  auto fallback = id.substr(id.find_last_of('/') + 1);
  std::replace(fallback.begin(), fallback.end(), '_', ' ');
  return names.at("cities").value(id, fallback);
}
std::string App::timezoneSortKey(const std::string &label) const {
  return timezoneNames.at(locale.current == "ru" ? "ru" : "en").at("sort").value(label, label);
}
std::vector<std::string> App::regions() const {
  std::vector<std::string> out;
  for (auto &z : zones.zones)
    if (std::find(out.begin(), out.end(), z.country) == out.end()) out.push_back(z.country);
  std::sort(out.begin(), out.end(), [&](const auto &a, const auto &b) {
    if (a == "UTC" || b == "UTC") return a == "UTC" && b != "UTC";
    return std::make_pair(timezoneSortKey(countryLabel(a)), a) < std::make_pair(timezoneSortKey(countryLabel(b)), b);
  });
  return out;
}
std::vector<Zone> App::locations() const {
  std::vector<Zone> out;
  for (auto &z : zones.zones)
    if (z.country == region) out.push_back(z);
  std::sort(out.begin(), out.end(), [&](const auto &a, const auto &b) {
    return std::make_pair(timezoneSortKey(zoneLabel(a.id)), a.id) < std::make_pair(timezoneSortKey(zoneLabel(b.id)), b.id);
  });
  return out;
}
void App::openTimezones() {
  screen = Screen::Regions;
  row = 0;
  scroll = 0;
  const auto current = store.data.value("timezone", "Etc/UTC");
  const auto countries = regions();
  for (const auto &z : zones.zones)
    if (z.id == current) {
      auto found = std::find(countries.begin(), countries.end(), z.country);
      if (found != countries.end()) row = int(found - countries.begin());
      break;
    }
}
std::string App::screenName() const {
  switch (screen) {
  case Screen::Clock: return "clock";
  case Screen::ClockEdit: return "clock_edit";
  case Screen::Bluetooth: return "bluetooth";
  case Screen::BluetoothDevices: return "bluetooth_devices";
  case Screen::BluetoothDevice: return "bluetooth_device";
  case Screen::BluetoothForget: return "bluetooth_forget";
  case Screen::Wifi: return "wifi";
  case Screen::WifiNetworks: return "wifi_networks";
  case Screen::WifiNetwork: return "wifi_network";
  case Screen::WifiPassword: return "wifi_password";
  case Screen::Search: return "search";
  case Screen::Library:
    return "library";
  case Screen::Settings:
    return "global_settings";
  case Screen::SettingsSection: return "settings_section";
  case Screen::LogClear: return "log_clear";
  case Screen::SystemConfirm: return "system_confirm";
  case Screen::SystemProgress: return "system_progress";
  case Screen::SavingAutomatic: return "saving_automatic";
  case Screen::Language:
    return "language";
  case Screen::Regions:
    return "timezone_regions";
  case Screen::Zones:
    return "timezone_list";
  case Screen::StartupResume: return "startup_resume";
  case Screen::ResumeChoice:
    return "resume_choice";
  case Screen::Launching:
    return "launching";
  case Screen::Running:
    return "running";
  case Screen::GameMenu:
    return "game_menu";
  case Screen::GameSettings:
    return "game_settings";
  case Screen::SaveSlots:
    return "save_slots";
  case Screen::LoadSlots:
    return "load_slots";
  case Screen::DeleteSlot: return "delete_slot";
  case Screen::Overwrite:
    return "overwrite_confirm";
  case Screen::LoadConfirm:
    return "load_confirm";
  case Screen::ExitConfirm:
    return "exit_confirm";
  case Screen::Recovery:
    return "recovery";
  }
  return "unknown";
}
void App::record(const std::string &event) {
  // Do not journal typed Wi-Fi characters, capture paths or user content.
  if (screen != Screen::WifiPassword && event.find("capture:") != 0)
    batterySample(event.find("failure") != std::string::npos ? "operation_failed" : "action:" + event);
  trace.push_back({{"event", event},
                   {"screen", screenName()},
                   {"row", row},
                   {"section", section},
                   {"artwork", artwork},
                   {"locale", locale.current},
                   {"notice", notice}});
}
void App::persist(Json next) {
  volumeTick(true); // Serialize other settings with the volume authority.
  // A failed disk write must not roll the live mixer back on a later setting change.
  next["volume"] = previewVolume;
  try {
    if (requestSettings) {
      if (!requestSettings(next))
        throw std::runtime_error("Settings authority rejected change");
      // The callback installs the authority-acknowledged revision.
    } else
      store.commit(std::move(next));
  } catch (const std::exception &e) {
    notice = locale.tr("notice.not_persisted");
    std::cerr << "Preference write: " << e.what() << "\n";
    batterySample("settings_write_failed");
    throw;
  }
}
void App::applyVolume(int value) {
  previewVolume = std::clamp(value, 0, 100);
  if (core) core->volume = previewVolume; // Keep queued audio flowing across gain changes.
  volumeFeedback.confirmed = previewVolume;
}
bool App::changeVolume(int value) {
  applyVolume(value);
  if(screen==Screen::Running && requestVolume) {
    queuedVolume=previewVolume;volumeSaveAt=monotonicMs()+200;
    return true;
  }
  auto next = store.data;
  next["volume"] = previewVolume;
  try { persist(next); return true; }
  catch (const std::exception &) { return false; }
}
void App::volumeTick(bool drain) {
  for(;;) {
    if(volumeSaveJob.valid()) {
      if(!drain && volumeSaveJob.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
      try {
        store.data=volumeSaveJob.get();
        if(notice==locale.tr("notice.not_persisted"))notice.clear();
      }catch(const std::exception &error) {
        notice=locale.tr("notice.not_persisted");
        std::cerr<<"Volume persistence: "<<error.what()<<"\n";
        batterySample("settings_write_failed");
      }
    }
    if(!queuedVolume || (!drain && monotonicMs()<volumeSaveAt))return;
    int target=*queuedVolume;queuedVolume.reset();
    auto writer=requestVolume;
    volumeSaveJob=std::async(std::launch::async,[writer,target]{return writer(target);});
    if(!drain)return;
  }
}
static bool anySlots(App &a) {
  for (int i = 0; i < 3; i++)
    if (a.slotView(i).compatible)
      return true;
  return false;
}
bool App::usableSlot(int i) const {
  if(core)return i==3?core->compatibleAutomatic():core->compatible(i);
  try {
    auto coreHash=hashFile(options.coreFor(game.system)),romHash=hashFile(game.path);
    const std::vector<std::string> paths=i==3?std::vector<std::string>{"autosave.json","autosave.json.previous"}:
      std::vector<std::string>{"slot-"+std::to_string(i)+".json"};
    for(const auto &name:paths)try {
      auto meta=readJson(store.gameDir(game)/name);
      if(meta.at("core")==coreHash && meta.at("rom")==romHash &&
         meta.at("sha256")==hashBytes(meta.at("payload").get<std::vector<uint8_t>>()))return true;
    }catch(...){}
  }catch(...){}
  return false;
}
int App::latestResumeSlot() const {
  int selected=-1;long long bestSequence=-1,bestUtc=0;
  try {
    auto coreHash=core?core->identity:hashFile(options.coreFor(game.system)),romHash=core?core->romHash:hashFile(game.path);
    for(int candidate=0;candidate<4;++candidate) {
      const auto paths=candidate==3?std::vector<std::string>{"autosave.json","autosave.json.previous"}:
        std::vector<std::string>{"slot-"+std::to_string(candidate)+".json"};
      for(const auto &name:paths)try {
        auto meta=readJson(store.gameDir(game)/name);
        if(meta.at("core")!=coreHash || meta.at("rom")!=romHash ||
           meta.at("sha256")!=hashBytes(meta.at("payload").get<std::vector<uint8_t>>()))continue;
        auto sequence=saveSequence(meta),utc=meta.value("utc",0LL);
        if(selected<0 || sequence>bestSequence || (sequence==bestSequence && utc>bestUtc)) {
          selected=candidate;bestSequence=sequence;bestUtc=utc;
        }
        // Automatic loading uses the first valid generation, matching Core.
        if(candidate==3)break;
      }catch(...){}
    }
  }catch(...){}
  return selected;
}

void App::applyZone(const std::string &id) {
  std::string previous = store.data.value("timezone", "Etc/UTC");
  zones.applyHost(id);
  auto next = store.data;
  next["timezone"] = id;
  try {
    persist(next);
    screen = Screen::Clock;
    row = 3;
    notice.clear();
  } catch (...) {
    if (zones.valid(previous))
      zones.applyHost(previous);
    throw;
  }
}
uint16_t App::gameButtons(uint16_t held) {
  if(screen!=Screen::Running || (core && core->paused))return 0;
  if(gameInputBlocked) {
    if(held==0)gameInputBlocked=false;
    return 0;
  }
  return held;
}
void App::action(const std::string &a) {
  if (stop || screen == Screen::SystemProgress || screen==Screen::SavingAutomatic) return;
  if(activity())return;
  // Gameplay buttons are sampled as a held bitmask, not shell actions. In
  // particular, do not run diagnostic snapshots/journaling for every press.
  if(screen==Screen::Running && a!="MENU" && a!="SLEEP" &&
     a!="VOLUME_UP" && a!="VOLUME_DOWN")return;
  for (auto id : {"notice.language_changed", "notice.favorite_added", "notice.favorite_removed",
                  "notice.saved", "notice.loaded"})
    if (notice == locale.tr(id))
      notice.clear();
  const std::string priorNotice = notice;
  const auto inputOrigin=screen;
  try {
    if (a == "SLEEP") { sleep(); record(a); return; }
    if (a == "VOLUME_UP" || a == "VOLUME_DOWN") {
      volumeFeedback.begin(monotonicMs());
      if(screen!=Screen::Running)draw();
      bool saved = changeVolume(previewVolume + (a == "VOLUME_UP" ? 5 : -5));
      volumeFeedback.result(volumeFeedback.sequence, previewVolume, true,
                            monotonicMs());
      if (saved) notice.clear();
      record(a);
      return;
    }
    if (infoOpen) {
      if (a == "B" || a == "X") infoOpen = false;
      if (a == "UP") infoScroll = std::max(0, infoScroll - 1);
      if (a == "DOWN") ++infoScroll;
      record("info:" + a);
      return;
    }
    if (a == "X" && !infoKey().empty()) {
      infoOpen = true;
      infoScroll = 0;
      record("info:open");
      return;
    }
    if(screen==Screen::Search) {searchAction(a);record("search:"+a);return;}
    if(screen==Screen::Bluetooth || screen==Screen::BluetoothDevices || screen==Screen::BluetoothDevice || screen==Screen::BluetoothForget){btAction(a);record("bluetooth:"+a);return;}
    if(screen==Screen::Clock || screen==Screen::ClockEdit){clockAction(a);record("clock:"+a);return;}
    if (screen==Screen::Wifi || screen==Screen::WifiNetworks || screen==Screen::WifiNetwork || screen==Screen::WifiPassword) {
      wifiAction(a);record("wifi:"+a);return;
    }
    if (screen == Screen::Library) {
      if(a=="Y") {
        searchNames.clear();for(const auto &entry:games)searchNames.push_back(searchFold(entry.title));
        searchDraft=searchQuery;searchRow=searchColumn=0;
        screen=Screen::Search;SDL_StartTextInput();return;
      }
      if(a=="B" && !searchQuery.empty()) {searchQuery.clear();selected=0;scroll=0;return;}
      auto v = visible();
      if (!v.empty())
        selected = std::clamp(selected, 0, int(v.size()) - 1);
      if (a == "UP" && !v.empty())
        selected = std::max(0, selected - 1);
      if (a == "DOWN" && !v.empty())
        selected = std::min(int(v.size()) - 1, selected + 1);
      if (a == "LEFT" || a == "RIGHT") {
        std::vector<std::string> modes = {"cover", "title", "screenshot"};
        int i = int(std::find(modes.begin(), modes.end(), artwork) - modes.begin());
        artwork = modes[(i + (a == "RIGHT" ? 1 : 2)) % 3];
        auto next = store.data;
        next["artwork"] = artwork;
        persist(next);
      }
      if (a == "L1" || a == "R1") {
        int i = int(std::find(sections.begin(), sections.end(), section) - sections.begin());
        section = sections[(i + sections.size() + (a == "R1" ? 1 : -1)) % sections.size()];
        selected = 0;
        scroll = 0;
      }
      if (a == "X" && !v.empty()) {
        auto next = store.data;
        auto &f = next["favorites"];
        auto id = games[v[selected]].id;
        auto i = std::find(f.begin(), f.end(), id);
        bool removed = i != f.end();
        if (removed)
          f.erase(i);
        else
          f.push_back(id);
        persist(next);
        notice = locale.tr(removed ? "notice.favorite_removed" : "notice.favorite_added");
        auto updated = visible();
        selected = updated.empty() ? 0 : std::min(selected, int(updated.size()) - 1);
      }
      if (a == "MENU") {
        returnToSettings();
      }
      if (a == "A" && !v.empty()) {
        game = games[v[selected]];
        if (!game.available || !fs::is_regular_file(game.path)) {
          notice = locale.tr("recent.unavailable");
        } else if (!fs::is_regular_file(options.coreFor(game.system))) {
          screen = Screen::Recovery;
          notice = locale.tr("error.core_unavailable");
        } else if (latestResumeSlot()>=0) {
          screen = Screen::ResumeChoice;
          row = 0;
        } else {
          screen = Screen::Launching;
          launch = true;
          resume = false;
        }
      }
      if (a == "UP" || a == "DOWN" || a == "L1" || a == "R1" || a == "A") {
        auto vv = visible();
        auto next = store.data;
        next["section"] = section;
        if (!vv.empty())
          next["focus"] = games[vv[std::clamp(selected, 0, int(vv.size()) - 1)]].id;
        persist(next);
      }
    } else if (screen == Screen::SystemConfirm) {
      if (a == "UP") row = 0;
      if (a == "DOWN") row = 1;
      if (a == "B" || (a == "A" && row == 0)) {
        returnToSettings();
      } else if (a == "A" && row == 1) {
        // Failure must leave the application running; no shutdown code escapes.
        screen = Screen::SystemProgress;
        notice.clear();
        try {
          draw(); // Submit feedback before filesystem flush and application teardown.
          saveOnExit();
        } catch (...) {
          screen = Screen::SystemConfirm;
          notice = locale.tr("error.operation");
          throw;
        }
        systemExitCode = restartRequested ? 65 : 64;
        stop = true;
      }
    } else if (screen == Screen::LogClear) {
      if (a == "B") { screen = Screen::SettingsSection; category = 4; row = 2; }
      if (a == "A") {
        batteryLog.reset(); // Drain the writer before deleting its files.
        clearDiagnosticLogs(options.state / "Logs", options.headless ? options.state / "ram-logs" : fs::path("/tmp/rondo-diagnostics/Logs"));
        clearDiagnosticLogs(options.state / "Battery");
        fs::remove(options.state / "launcher.log");
        if (!options.headless)
          for (const auto *name : {"launcher.log", "process-0.log", "process-1.log"})
            fs::remove(fs::path("/tmp/rondo-output") / name);
        notice = locale.tr("debug.cleared");
        screen = Screen::SettingsSection; category = 4; row = 2;
        configureDiagnostics();
      }
    } else if (screen == Screen::Settings) {
      if (a == "UP") settingsFocus = std::max(0, settingsFocus - 1);
      if (a == "DOWN") settingsFocus = std::min(8, settingsFocus + 1);
      row = settingsFocus;
      if (a == "B") screen = Screen::Library;
      if (a == "A" && settingsFocus >= 7) {
        restartRequested = settingsFocus == 7;
        screen = Screen::SystemConfirm; row = 0;
        record("system:confirm"); return;
      }
      if (a == "A") {
        static const int categories[] = {0, 1, 2, 6, 5, 3, 4};
        category = categories[settingsFocus];
        screen = category == 6 ? Screen::Clock : Screen::SettingsSection;
        row = 0;
        if (category == 6) clockPollAt = 0;
      }
    } else if (screen == Screen::SettingsSection) {
      int count = category==3 ? 6 : (category == 4 || category == 1) ? 3 : (category == 0 || category == 5) ? 2 : 1;
      if (a == "UP") row = std::max(0, row - 1);
      if (a == "DOWN") row = std::min(count - 1, row + 1);
      if (a == "B") { returnToSettings(); return; }
      if (category == 4 && (a == "A" || a == "LEFT" || a == "RIGHT")) {
        if (row == 2) { if (a == "A") screen = Screen::LogClear; }
        else if (row == 0 || store.data.value("debug_enabled", false)) {
          auto next = store.data;
          if (row == 0) next["debug_enabled"] = a == "A" ? !next.value("debug_enabled", false) : a == "RIGHT";
          else {
            std::vector<std::string> levels{"errors", "events", "detailed"};
            auto it = std::find(levels.begin(), levels.end(), next.value("debug_level", "events"));
            int index = int(it - levels.begin());
            next["debug_level"] = levels[(index + (a == "LEFT" ? 2 : 1)) % 3];
          }
          persist(next); configureDiagnostics(); notice.clear();
        }
      }
      if (category == 0 && row == 1 && (a == "A" || a == "LEFT" || a == "RIGHT")) {
        if (!bootLogoSupported()) {
          notice = locale.tr("bootlogo.unsupported");
          return;
        }
        if (!bootPending() && bootAcknowledged)
          requestBootLogo(a == "LEFT" ? false : a == "RIGHT" ? true : !*bootAcknowledged);
      }
      if ((a == "LEFT" || a == "RIGHT") && category == 2) {
        if (changeVolume(previewVolume+(a=="RIGHT"?5:-5))) notice.clear();
      }
      if ((a == "LEFT" || a == "RIGHT") && category == 1 && row == 0 && brightness) {
        int old=*brightness, desired=std::clamp(old+(a=="RIGHT"?5:-5),0,100);
        if (desired!=old) {
          device->brightness(desired); brightness=device->brightness();
          if (!brightness || *brightness!=desired) throw std::runtime_error("Brightness not acknowledged");
          auto next=store.data; next["brightness"]=desired;
          try { persist(next); } catch (...) { device->brightness(old);brightness=device->brightness();throw; }
        }
        notice.clear();
      }
      if (category == 1 && row > 0 && (a == "A" || a == "LEFT" || a == "RIGHT")) {
        auto next = store.data;
        if (row == 1) {
          bool current = next.value("integer_scaling", false);
          next["integer_scaling"] = a == "LEFT" ? false : a == "RIGHT" ? true : !current;
        } else {
          const std::vector<std::string> colors{"black", "charcoal", "navy", "theme"};
          auto current = next.value("game_background", "black");
          int i = int(std::find(colors.begin(), colors.end(), current) - colors.begin());
          next["game_background"] = colors[(i + (a == "LEFT" ? 3 : 1)) % 4];
        }
        if (next != store.data) persist(next);
        notice.clear();
      }
      if (category == 3 && row == 0 && (a == "A" || a == "LEFT" || a == "RIGHT")) {
        bool current = store.data.value("show_battery_percent", true);
        bool desired = a == "LEFT" ? false : a == "RIGHT" ? true : !current;
        if (desired != current) {
          auto next = store.data;
          next["show_battery_percent"] = desired;
          persist(next);
        }
        notice.clear();
      }
      if (category == 3 && row == 1 && device->canSleep() && (a == "LEFT" || a == "RIGHT" || a == "A")) {
        bool current = store.data.value("lid_sleep", true);
        bool desired = a == "LEFT" ? false : a == "RIGHT" ? true : !current;
        if (desired != current) { auto next = store.data; next["lid_sleep"] = desired; persist(next); }
        notice.clear();
      }
      if(category==3 && row>=2 && (a=="A" || a=="LEFT" || a=="RIGHT")) {
        auto next=store.data;
        if(row==4) {
          const std::vector<int> values{0,1,2,5,10,15};
          auto index=std::find(values.begin(),values.end(),next.value("idle_screen_minutes",5))-values.begin();
          next["idle_screen_minutes"]=values[(index+(a=="LEFT"?5:1))%values.size()];
        } else {
          std::string key=row==2?"autosave_enabled":row==3?"startup_resume":"battery_warning";
          next[key]=a=="A"?!next.value(key,true):a=="RIGHT";
        }
        if(next!=store.data)persist(next);
        notice.clear();
      }
      if (a == "A" && category == 5) { screen = row == 0 ? Screen::Wifi : Screen::Bluetooth; row = 0; scroll = 0; wifiPollAt = 0; btPollAt = 0; }
      if (a == "A" && category == 0 && row == 0) {
        screen = Screen::Language;
        for (size_t i = 0; i < locale.registry["locales"].size(); i++)
          if (locale.registry["locales"][i]["code"] == locale.current) row = i;
      }
    } else if (screen == Screen::Language) {
      int count = locale.registry["locales"].size();
      if (a == "UP")
        row = std::max(0, row - 1);
      if (a == "DOWN")
        row = std::min(count - 1, row + 1);
      if (a == "B") {
        screen = Screen::SettingsSection;
        category = 0;
        row = 0;
      }
      if (a == "A") {
        auto choice = locale.registry["locales"][row];
        if (choice.value("direction", "ltr") != "ltr")
          throw std::runtime_error("RTL layout is not available");
        auto code = choice.at("code").get<std::string>();
        auto old = locale.current;
        locale.select(options.assets, code);
        if (locale.current != code)
          throw std::runtime_error("Locale catalog unavailable");
        auto next = store.data;
        next["locale"] = code;
        try {
          persist(next);
        } catch (...) {
          locale.select(options.assets, old);
          throw;
        }
        screen = Screen::SettingsSection;
        category = 0;
        row = 0;
        notice = locale.tr("notice.language_changed");
      }
    } else if (screen == Screen::Regions || screen == Screen::Zones) {
      auto r = regions();
      auto l = locations();
      int count = screen == Screen::Regions ? r.size() : l.size();
      if (a == "UP")
        row = std::max(0, row - 1);
      if (a == "DOWN")
        row = std::min(count - 1, row + 1);
      if (a == "L1")
        row = std::max(0, row - 5);
      if (a == "R1")
        row = std::min(count - 1, row + 5);
      if ((a == "LEFT" || a == "RIGHT") && count > 0) {
        auto initial = [&](int index) {
          auto text = timezoneSortKey(screen == Screen::Regions ? countryLabel(r[index]) : zoneLabel(l[index].id));
          auto first = static_cast<unsigned char>(text[0]);
          size_t size = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
          return text.substr(0, size);
        };
        int direction = a == "RIGHT" ? 1 : -1;
        const auto letter = initial(row);
        int target = row;
        while (target + direction >= 0 && target + direction < count) {
          target += direction;
          if (initial(target) != letter) break;
        }
        if (direction < 0)
          while (target > 0 && initial(target - 1) == initial(target)) --target;
        row = target;
      }
      if (a == "B") {
        if (screen == Screen::Zones) {
          screen = Screen::Regions;
          row = regionFocus;
          scroll = regionScroll;
        } else {
          screen = Screen::Clock;
          row = 3;
        }
        if (screen != Screen::Regions)
          scroll = 0;
      }
      if (a == "A" && count > 0) {
        if (screen == Screen::Regions) {
          region = r[row];
          regionFocus = row;
          regionScroll = scroll;
          if (region == "UTC")
            applyZone("Etc/UTC");
          else {
            screen = Screen::Zones;
            row = 0;
            scroll = 0;
            auto ll = locations();
            for (size_t i = 0; i < ll.size(); i++)
              if (ll[i].id == store.data["timezone"])
                row = i;
          }
        } else
          applyZone(l[row].id);
      }
    } else if(screen==Screen::StartupResume) {
      if(a=="UP")row=0;
      if(a=="DOWN")row=1;
      if(a=="B" || (a=="A" && row==1)) {screen=Screen::Library;row=0;}
      else if(a=="A" && row==0) {resume=true;screen=Screen::Launching;launch=true;}
    } else if (screen == Screen::ResumeChoice) {
      if (a == "UP")
        row = 0;
      if (a == "DOWN")
        row = 1;
      if (a == "B")
        screen = Screen::Library;
      if (a == "A") {
        resume = row == 0;
        screen = Screen::Launching;
        launch = true;
      }
    } else if (screen == Screen::Running) {
      if (a == "MENU") {
        screen = Screen::GameMenu;
        row = 0;
        core->paused = true;
        core->buttons = 0;
        core->silence();
      }
    } else if (screen == Screen::GameMenu) {
      const int chosen = row;
      if (a == "UP")
        row = std::max(0, row - 1);
      if (a == "DOWN")
        row = std::min(4, row + 1);
      if (a == "B" || (a == "A" && chosen == 0)) {
        screen = Screen::Running;
        core->paused = false;
        notice.clear();
      }
      if (a == "A" && chosen == 1 && core->canSave()) {
        screen = Screen::SaveSlots;
        row = slot;
      }
      if (a == "A" && chosen == 2 && anySlots(*this)) {
        screen = Screen::LoadSlots;
        row = slot;
      }
      if (a == "A" && chosen == 3) {
        screen = Screen::GameSettings;
        row = 0;
      }
      if (a == "A" && chosen == 4) {
        screen = Screen::ExitConfirm;
        autosaveError.clear();
        row = 0;
      }
    } else if (screen == Screen::GameSettings) {
      if (a == "B") {
        screen = Screen::GameMenu;
        row = 3;
      }
      if (a == "A" || a == "LEFT" || a == "RIGHT") {
        scalingLastError.clear();
        auto next = store.data;
        auto &options = next["game_options"];
        int current = !options.contains(game.id) || !options[game.id].contains("integer_scaling")
            ? 0 : options[game.id]["integer_scaling"].get<bool>() ? 1 : 2;
        int desired = (current + (a == "LEFT" ? 2 : 1)) % 3;
        if (desired == 0) {
          options[game.id].erase("integer_scaling");
          if (options[game.id].empty()) options.erase(game.id);
        } else options[game.id]["integer_scaling"] = desired == 1;
        persist(next);
      }
    } else if (screen == Screen::SaveSlots || screen == Screen::LoadSlots) {
      if (a == "UP")
        row = std::max(0, row - 1);
      if (a == "DOWN")
        row = std::min(2, row + 1);
      if (a == "B") {
        bool saving = screen == Screen::SaveSlots;
        screen = Screen::GameMenu;
        row = saving ? 1 : 2;
      }
      if (a == "Y" && slotView(row).present) {
        slot = row; deleteOrigin = screen; screen = Screen::DeleteSlot; row = 0;
      }
      if (a == "A") {
        slot = row;
        if (screen == Screen::LoadSlots) {
          if (core->compatible(slot)) {
            screen = Screen::LoadConfirm;
            row = 0;
          }
        } else if (fs::exists(store.gameDir(game) / ("slot-" + std::to_string(slot) + ".json"))) {
          screen = Screen::Overwrite;
          row = 0;
        } else {
          core->save(slot);
          slotCache.clear();
          screen = Screen::GameMenu;
          row = 1;
          notice = locale.tr("notice.saved");
          noticeUntil = monotonicMs() + 2000;
        }
      }
    } else if (screen == Screen::DeleteSlot) {
      if (a == "UP") row = 0;
      if (a == "DOWN") row = 1;
      if (a == "B" || (a == "A" && row == 0)) { screen = deleteOrigin; row = slot; }
      else if (a == "A" && row == 1) {
        core->deleteSlot(slot); slotCache.clear(); screen = deleteOrigin; row = slot;
        notice = locale.tr("slot.deleted"); noticeUntil = monotonicMs() + 2000;
      }
    } else if (screen == Screen::Overwrite || screen == Screen::LoadConfirm ||
               screen == Screen::ExitConfirm) {
      if (a == "UP")
        row = 0;
      if (a == "DOWN")
        row = screen==Screen::ExitConfirm && !autosaveError.empty()?std::min(2,row+1):1;
      auto origin = screen;
      bool confirmed = a == "A" && (row == 1 || (origin==Screen::ExitConfirm && row==2 && !autosaveError.empty()));
      if (a == "B" || (a == "A" && row == 0)) {
        screen = origin == Screen::Overwrite     ? Screen::SaveSlots
                 : origin == Screen::LoadConfirm ? Screen::LoadSlots
                                                 : Screen::GameMenu;
        row = origin == Screen::ExitConfirm ? 4 : slot;
      }
      if (confirmed) {
        if (origin == Screen::Overwrite) {
          core->save(slot);
          slotCache.clear();
          screen = Screen::GameMenu;
          row = 1;
          notice = locale.tr("notice.saved");
          noticeUntil = monotonicMs() + 2000;
        } else if (origin == Screen::LoadConfirm) {
          core->loadState(slot);
          screen = Screen::Running;
          core->paused = false;
          notice = locale.tr("notice.loaded");
          noticeUntil = monotonicMs() + 2000;
        } else {
          saveOnExit(row==2);
          gameClosed = true;
          stop = true;
        }
      }
    } else if (screen == Screen::Recovery) {
      if (a == "B")
        screen = Screen::Library;
    }
  } catch (const std::exception &e) {
    if (a == "VOLUME_UP" || a == "VOLUME_DOWN")
      volumeFeedback.result(volumeFeedback.sequence, volumeFeedback.confirmed, false,
                            monotonicMs());
    if (notice == priorNotice)
      notice = locale.tr("error.operation");
    std::cerr << "Operation: " << e.what() << "\n";
    batterySample("operation_failed");
    if (screen == Screen::GameSettings) scalingLastError = notice;
  }
  if(inputOrigin!=Screen::Running && screen==Screen::Running) {
    gameInputBlocked=true;
    if(core)core->buttons=0;
  }
  record(a);
}
std::string App::infoKey() const {
  if(screen==Screen::Bluetooth || screen==Screen::BluetoothDevices || screen==Screen::BluetoothDevice || screen==Screen::BluetoothForget)return "bluetooth";
  if(screen==Screen::Clock || screen==Screen::ClockEdit)return "clock";
  if (screen==Screen::WifiPassword) return "";
  if (screen==Screen::Wifi || screen==Screen::WifiNetworks || screen==Screen::WifiNetwork) return "wifi";
  if (screen == Screen::GameSettings) return "scaling";
  if (screen == Screen::SaveSlots || screen == Screen::LoadSlots) return "slots";
  if (screen == Screen::Regions || screen == Screen::Zones) return "timezone";
  if (screen == Screen::Settings && settingsFocus >= 7) return "system_power";
  if (screen != Screen::SettingsSection) return "";
  if (category == 4) return "debug";
  if (category == 0) return row == 0 ? "language" : "boot";
  if (category == 5) return row == 0 ? "wifi" : "bluetooth";
  if(category==3 && row>=2)return row==2?"autosave":row==3?"startup_resume":row==4?"idle_screen":"battery_warning";
  return category == 1 ? (row == 0 ? "brightness" : row == 1 ? "scaling" : "game_background") : category == 2 ? "sound" : row == 0 ? "battery" : "lid";
}
void App::rememberLaunch() {
  auto next=store.data;
  next["recent_games"]=recentAfterLaunch(next.at("recent_games"),game,options.volumes);
  next["resume_game"]=game.id;
  persist(next);
  if (section=="Recent") { selected=0;scroll=0; }
}
std::string App::libraryTitle() const {
  if (section == "All") return locale.tr("section.all_title");
  if (section == "Favorites") return locale.tr("section.favorites_title");
  if (section == "Recent") return locale.tr("section.recent_title");
  static const std::map<std::string,std::vector<std::string>> aliases{
    {"NES",{"NES"}}, {"SNES",{"SNES"}}, {"MD",{"Mega Drive","MD"}},
    {"PSX",{"PS1"}}, {"GB",{"Game Boy","GB"}},
    {"GBC",{"Game Boy Color","GBC"}}, {"GBA",{"Game Boy Advance","GBA"}}};
  auto found = aliases.find(section);
  auto names = found == aliases.end() ? std::vector<std::string>{section} : found->second;
  for (const auto &name : names) {
    auto title = locale.tr("section.system_title", {{"system", name}});
    if (ui->measure(title,29,700) <= 414) return title;
  }
  return ui->fit(locale.tr("section.system_title",{{"system",names.back()}}),414,29,700);
}
SDL_Color App::gameBackground() const {
  auto color = store.data.value("game_background", "black");
  if (color == "theme") return ui->bg;
  if (color == "charcoal") return {16,16,16,255};
  if (color == "navy") return {10,16,24,255};
  return {0,0,0,255};
}
void App::returnToSettings() {
  screen = Screen::Settings;
  row = settingsFocus;
}
void App::requestBootLogo(bool desired) {
  if (bootPending() || (bootAcknowledged && *bootAcknowledged == desired)) return;
  bootTarget = desired;
  bootLastError.clear();
  bootJob = std::async(std::launch::async, [desired] {
    try {
      applyBootLogo(desired);
      return std::make_pair(std::optional<bool>(desired), std::string());
    } catch (const std::exception &e) {
      return std::make_pair(readBootLogo(), std::string(e.what()));
    }
  });
}
void App::configureDiagnostics() {
  if (!store.data.value("debug_enabled", false)) { batteryLog.reset(); return; }
  if (!batteryLog) {
    batteryLog = std::make_unique<BatteryLog>(options.state / "Logs", diagnosticRunner ? "runner" : "shell",
        "/sys", "/proc", 2 * 1024 * 1024,
        options.headless ? options.state / "ram-logs" : fs::path("/tmp/rondo-diagnostics/Logs"), true);
    batteryLogWarning = false;diagnosticSampleAt = 0;
    batterySample("start");
  }
}
void App::observeFrame(double inputMs,double updateMs,double drawMs) {
  if(!store.data.value("debug_enabled",false) || store.data.value("debug_level","events")!="detailed")return;
  const auto now=monotonicMs();
  if(!frameWindow)frameWindow=now;
  ++frameCount;frameInput+=inputMs;frameUpdate+=updateMs;frameDraw+=drawMs;
  auto total=inputMs+updateMs+drawMs;frameMaximum=std::max(frameMaximum,total);frameSlow+=total>16.667;
  if(now-frameWindow<2000)return;
  frameDiagnostic={{"frames",frameCount},{"window_ms",now-frameWindow},
    {"fps",1000.0*frameCount/double(now-frameWindow)},{"input_ms",frameInput/frameCount},
    {"update_ms",frameUpdate/frameCount},{"draw_ms",frameDraw/frameCount},
    {"max_work_ms",frameMaximum},{"over_16ms",frameSlow}};
  frameWindow=now;frameCount=frameSlow=0;frameInput=frameUpdate=frameDraw=frameMaximum=0;
}
void App::batterySample(const std::string &event) {
  if (!batteryLog || (!ui && event.empty())) return;
  auto level = store.data.value("debug_level", "events");
  bool failure = event.find("failed") != std::string::npos;
  if ((level == "errors" && !failure) || (level != "detailed" && event.empty())) return;
  if (!ui && event.find("action:") == 0) return;
  const auto now=monotonicMs();
  if(event.empty() && now<diagnosticSampleAt)return;
  diagnosticSampleAt=now+5000;
  bool ok = batteryLog->sample(now, {{"screen", screenName()},
      {"brightness_percent", brightness ? Json(*brightness) : Json(nullptr)},
      {"volume_percent", previewVolume},
      {"stored_volume_percent", store.data.value("volume", 80)},
      {"battery_display", ui ? ui->batteryDisplay : Json(nullptr)},
      {"paused", core ? Json(core->paused) : Json(nullptr)},
      {"bluetooth", btDiagnostic}, {"frames",frameDiagnostic},
      {"controller_input",ui?ui->controllerDiagnostics():Json(nullptr)}}, event);
  if (!ok && !batteryLogWarning) {
    std::cerr << "Battery diagnostic journal unavailable; gameplay continues: "
              << batteryLog->lastError.dump() << "\n";
    batteryLogWarning = true;
  }
  if (ok) batteryLogWarning = false;
}
void App::saveOnExit(bool skipAutomatic) {
  volumeTick(true);
  if(!core)return;
  if(!skipAutomatic && store.data.value("autosave_enabled",true)) {
    auto origin=screen;
    try {
      if(origin!=Screen::SystemProgress) {notice.clear();screen=Screen::SavingAutomatic;draw();}
      core->saveAutomatic();autosaveError.clear();
    } catch(...) {screen=origin;autosaveError=locale.tr("autosave.failed");notice=autosaveError;throw;}
  } else core->flushSram();
}
void App::rememberClosedGame() {
  auto next=store.data;
  next["resume_game"]="";
  persist(next);
}
void App::offerStartupResume() {
  if(!store.data.value("startup_resume",true) || !store.warning.empty())return;
  // Recent history survives an explicit close; only an unfinished session is offered.
  auto id=store.data.value("resume_game",std::string());
  if(id.empty())return;
  auto found=std::find_if(games.begin(),games.end(),[&](const Game &candidate){return candidate.id==id && candidate.available;});
  if(found==games.end() || !fs::is_regular_file(options.coreFor(found->system)) || !fs::is_regular_file(found->path))return;
  game=*found;
  if(latestResumeSlot()>=0) {screen=Screen::StartupResume;row=0;}
}
void App::wakeScreen() {
  if(!screenBlank)return;
  if(device->canBlank())device->blank(false);
  screenBlank=false;lastActivity=monotonicMs();
}
bool App::activity() {
  auto now=monotonicMs();lastActivity=now;
  if(now<wakeConsumeUntil)return true;
  if(!screenBlank)return false;
  try {wakeScreen();wakeConsumeUntil=now+150;}
  catch(const std::exception &error) {notice=locale.tr("power.screen_error");std::cerr<<error.what()<<"\n";}
  return true;
}
void App::batteryWarning(const Telemetry &telemetry,uint64_t now) {
  if(!batteryWarningsLoaded) {
    try {
      auto state=readJson(options.state/"battery-warning.json");
      if(state.at("schema")==1 && state.at("shown").is_number_integer())
        batteryWarningMask=std::clamp(state.at("shown").get<int>(),0,3);
    }catch(...){}
    batteryWarningsLoaded=true;
  }
  auto remember=[&](int mask) {
    batteryWarningMask=mask;
    try {
      auto text=Json({{"schema",1},{"shown",mask}}).dump();
      atomicWrite(options.state/"battery-warning.json",{text.begin(),text.end()});
    }catch(const std::exception &error) {
      std::cerr<<"Battery warning state: "<<error.what()<<"\n";
      batterySample("battery_warning_state_failed");
    }
  };
  bool known=telemetry.capacity && *telemetry.capacity>=0 && *telemetry.capacity<=100;
  bool charging=telemetry.batteryStatus=="Charging" || telemetry.batteryStatus=="Full";
  if(charging) {
    batteryWarningPercent=-1;batteryNoticeUntil=0;
    // Only sustained, system-confirmed charging rearms warnings, not gauge jumps.
    if(known && *telemetry.capacity>=20 && batteryWarningMask) {
      if(!batteryChargeAt)batteryChargeAt=now;
      if(now>=batteryChargeAt+30000)remember(0);
    }else batteryChargeAt=0;
    return;
  }
  batteryChargeAt=0;
  if(!known || screen!=Screen::Running || !store.data.value("battery_warning",true)) {
    batteryWarningPercent=-1;batteryNoticeUntil=0;return;
  }
  int mask=*telemetry.capacity<=5?3:*telemetry.capacity<=10?1:0;
  if(mask && (batteryWarningMask&mask)!=mask) {
    remember(batteryWarningMask|mask);
    batteryWarningPercent=*telemetry.capacity;batteryNoticeUntil=now+3000;
  }
}
void App::powerTick(uint64_t now) {
  if(sleeping || stop)return;
  bool menu=screen!=Screen::Running && screen!=Screen::Launching && screen!=Screen::SystemConfirm && screen!=Screen::SystemProgress && screen!=Screen::SavingAutomatic && !launch;
  int minutes=store.data.value("idle_screen_minutes",5);
  if(!screenBlank && menu && minutes>0 && now>=lastActivity+uint64_t(minutes)*60000) {
    try {if(device->canBlank())device->blank(true);screenBlank=true;}
    catch(const std::exception &error) {lastActivity=now;notice=locale.tr("power.screen_error");std::cerr<<error.what()<<"\n";}
  }
  if(screen!=Screen::Running || (batteryNoticeUntil && now>=batteryNoticeUntil)) {batteryWarningPercent=-1;batteryNoticeUntil=0;}
  if(now>=powerSampleAt) {powerSampleAt=now+15000;if(device->canSleep() || brightness)batteryWarning(readTelemetry(),now);}
}
void App::ticks(unsigned n) {
  volumeTick();
  if(ui)powerTick(monotonicMs());
  wifiTick();
  btTick();
  clockTick();
  if (ui && !sleeping && monotonicMs()>=lidCheckAt) {
    lidCheckAt=monotonicMs()+100;
    auto open=device->lidOpen();
    bool closed=open && !*open && lastLidOpen && *lastLidOpen;
    lastLidOpen=open;
    if (closed && store.data.value("lid_sleep",true)) sleep();
  }
  if (noticeUntil && monotonicMs() >= noticeUntil) {
    if (notice == locale.tr("notice.saved") || notice == locale.tr("notice.loaded")) notice.clear();
    noticeUntil = 0;
  }
  if (bootPending() && bootJob.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
    auto result = bootJob.get();
    bootAcknowledged = result.first;
    if (result.second.empty()) {
      try {
        if (store.data.value("replace_boot_logo", true) != bootTarget) {
          auto next = store.data;
          next["replace_boot_logo"] = bootTarget;
          persist(next);
        }
        notice = locale.tr("boot.saved");
        bootMessageUntil = monotonicMs() + 2000;
      } catch (...) {
        // The boot file may have changed: retain the reread hardware value.
        bootAcknowledged = readBootLogo();
        bootMessageUntil = monotonicMs() + 4000;
      }
    } else {
      notice = locale.tr("binary.error", {{"value", bootAcknowledged ? locale.tr(*bootAcknowledged ? "value.on" : "value.off") : "—"}});
      bootMessageUntil = monotonicMs() + 4000;
    }
    if (!result.second.empty() || notice.find(locale.tr("notice.not_persisted")) == 0)
      bootLastError = notice;
    bootNotice = notice;
  }
  if (bootMessageUntil && monotonicMs() >= bootMessageUntil) {
    if (notice == bootNotice) notice.clear();
    bootMessageUntil = 0;
  }
  if (core)
    for (unsigned i = 0; i < n; i++)
      core->tick();
}
void App::sleep() {
  if (sleeping) return;
  try {wakeScreen();}catch(...){notice=locale.tr("power.screen_error");return;}
  if (!device->canSleep()) { notice=locale.tr("sleep.unsupported");return; }
  sleeping=true;
  clockSleep();
  btSleep();
  wifiQueued={};wifiQueuedTask.clear();wifiToggle.reset();
  if(wifiJob.valid()) {wifiDevice->cancelled=true;wifiJob.wait();wifiTick();}
  wifiDevice->cancelled=false;wifiPollAt=0;
  auto origin=screen; bool wasPaused=core?core->paused:true;
  try {
    while (bootPending()) { ticks(0);SDL_Delay(5); }
    if (core) { core->paused=true;core->buttons=0;core->silence();core->flushSram();core->closeAudio(); }
    volumeTick(true); // Audio/emulation are paused; finish the last requested gain.
    batterySample("sleep_before");
    record("sleep:prepared");
    if(batteryLog)batteryLog->drain(); // The game/audio are already paused.
    device->sleep(store.data.value("wake_on_lid_open",true) && device->canWakeOnLid(),
                  store.data.value("lid_sleep",true));
    if(ui) {
      ui->handheldKeys=std::make_unique<H700Keys>();ui->volumeKeys=std::make_unique<VolumeKeys>();
      ui->powerKeys=std::make_unique<PowerKeys>();SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
    }
    if(core) { core->openAudio();core->paused=wasPaused;gameInputBlocked=true; }
    lastLidOpen=device->lidOpen();lidCheckAt=monotonicMs()+300;
    batterySample("sleep_after");
    btResume();
    lastActivity=monotonicMs();
    notice.clear();record("sleep:resumed");
  } catch (const std::exception &e) {
    batterySample("sleep_failed");
    std::cerr<<"Sleep: "<<e.what()<<"\n";
    if(core) {
      core->paused=true;core->buttons=0;
      try { core->openAudio(); } catch(const std::exception &audio) { std::cerr<<audio.what()<<"\n"; }
      if(origin==Screen::Running) { screen=Screen::GameMenu;row=0; }
    }
    notice=locale.tr("sleep.error");record("sleep:failed");
  }
  sleeping=false;
}
const App::SlotView &App::slotView(int index) {
  auto path=store.gameDir(game)/("slot-"+std::to_string(index)+".json");
  std::error_code error;auto stamp=fs::last_write_time(path,error);
  std::string key=game.id+(error?":missing":std::to_string(static_cast<long long>(stamp.time_since_epoch().count()))+":"+std::to_string(fs::file_size(path,error)));
  auto &cached=slotCache[index]; if(cached.first==key)return cached.second;
  cached.first=key;cached.second=SlotView{};auto &view=cached.second;
  view.present=fs::is_regular_file(path,error);
  if (!view.present) return view;
  try {
    auto meta=readJson(path); view.utc=meta.value("utc",0LL);
    view.compatible=usableSlot(index);
    if(meta.contains("preview")) {
      auto p=meta.at("preview");int w=p.at("width"),h=p.at("height");
      if(w>0 && h>0 && w<=1024 && h<=1024 && p.at("pixels").size()==size_t(w)*h) {
        auto pixels=p.at("pixels").get<std::vector<uint32_t>>();auto bytes=reinterpret_cast<const uint8_t *>(pixels.data());
        if(hashBytes({bytes,bytes+pixels.size()*sizeof(uint32_t)})==p.at("sha256")) {view.width=w;view.height=h;view.pixels=std::move(pixels);}
      }
    }
  } catch (...) {}
  return view;
}
std::string App::slotDate(long long utc) const {
  auto old=getenv("TZ");std::string previous=old?old:"";bool had=old;
  zones.applyHost(store.data.value("timezone","Etc/UTC"));
  time_t time=utc;tm local{};localtime_r(&time,&local);char text[64];
  strftime(text,sizeof(text),locale.current=="ru"?"%d.%m.%Y  %H:%M:%S":"%Y-%m-%d  %H:%M:%S",&local);
  if(had)setenv("TZ",previous.c_str(),1);else unsetenv("TZ");tzset();return text;
}
void App::capture(const std::string &name) {
  draw();
  ui->save(options.output / (name + ".png"));
  record("capture:" + name);
}
void App::draw() {
  ui->bluetoothEnabled=bt.enabled;
  // A game frame without a header must never inherit a previous menu's 100%.
  ui->batteryDisplay = {{"screen", screenName()}, {"header_visible", false},
      {"percentage_visible", false}, {"source_percent", nullptr},
      {"displayed_percent", nullptr}, {"label", nullptr},
      {"rendered_monotonic_ms", nullptr}, {"submitted_monotonic_ms", nullptr}};
  if(screenBlank) {ui->rect(0,0,720,480,{0,0,0,255});ui->present();batterySample();return;}
  ui->wifiOverride.reset();
  if (wifi.available && wifi.ready) {
    Telemetry confirmed;
    confirmed.wifiEnabled = wifi.enabled;
    confirmed.wifiState = !wifi.connectedSsid.empty() ? "connected" :
      (wifi.phase=="connecting" || wifi.phase=="address" || wifi.phase=="scanning") ? "connecting" : "disconnected";
    confirmed.wifiSignalDbm = wifi.signalDbm;
    ui->wifiOverride = confirmed;
  }
  ui->showBatteryPercent = store.data.value("show_battery_percent", true);
  auto tr = [this](auto key) { return locale.tr(key); };
  auto zone = store.data.value("timezone", "Etc/UTC");
  if (screen == Screen::Running) {
    ui->rect(0, 0, 720, 480, gameBackground());
    int w = 720, h = 480;
    if(game.system=="GB" || game.system=="GBC" || game.system=="GBA") {
      double scale=std::min(720.0/core->width,480.0/core->height);
      w=int(core->width*scale);h=int(core->height*scale);
    }
    if(game.system=="SNES") {
      h=480;w=int(h*core->aspectRatio);
      if(w>720) {w=720;h=int(w/core->aspectRatio);}
    }
    if (store.integerScaling(game.id)) {
      int scale = std::max(1, std::min(720 / core->width, 480 / core->height));
      w = core->width * scale;
      h = core->height * scale;
    }
    ui->pixels(core->frame, core->width, core->height, (720 - w) / 2, (480 - h) / 2, w, h);
    if (!notice.empty()) ui->notice(notice,"folder");
    if(batteryWarningPercent>=0 && monotonicMs()<batteryNoticeUntil && store.data.value("battery_warning",true)) {
      ui->rect(584,24,112,40,ui->focus,8);
      ui->icon("battery",596,32,24);
      ui->text(std::to_string(batteryWarningPercent)+"%",630,30,22,500,ui->cream,56);
    }
    ui->volumeOverlay(volumeFeedback, locale, !notice.empty());
    ui->present();
    batterySample();
    return;
  }
  if (screen == Screen::Library) {
    auto v = visible();
    ui->frame(libraryTitle(), "", zone);
    int x = 24;
    for (auto &s : sections) {
      std::string label = s == "All" ? tr("section.all") : s;
      int w = s == "Favorites" || s == "Recent" ? 22 : ui->measure(label, 19, s == section ? 700 : 500);
      if (x + w > 680)
        break;
      if (s == "Favorites" || s == "Recent")
        ui->icon(s == "Favorites" ? "star-filled" : "clock", x, 86, 22);
      else
        ui->text(label, x, 85, 19, s == section ? 700 : 500);
      if (s == section)
        ui->rect(x, 115, w, 2, ui->accent);
      x += w + 30;
    }
    if(!searchQuery.empty())ui->text(locale.tr("search.filter",{{"query",searchQuery}}),x,85,19,500,ui->secondary,std::max(1,672-x));
    if (v.empty()) {
      ui->text(tr(!searchQuery.empty()?"search.empty":section == "Favorites" ? "empty.favorites" : section == "Recent" ? "empty.recent" : "empty.library"), 32, 182, 27, 700);
      ui->text(!searchQuery.empty()?tr("search.empty_help"):section == "Favorites"
                   ? locale.tr("empty.favorites_help", {{"button", ui->buttonLabel("X")}})
                   : tr(section == "Recent" ? "empty.recent_help" : "empty.library_help"), 32, 224,
               17, 500, ui->secondary);
      std::vector<std::pair<std::string,std::string>> hints={{"Y",tr("search.title")},{"MENU",tr("screen.settings")}};
      if(!searchQuery.empty())hints.push_back({"B",tr("search.clear")});
      else hints.push_back({"L1/R1",tr("action.section")});
      ui->hints(hints);
    } else {
      selected = std::clamp(selected, 0, int(v.size()) - 1);
      auto &g = games[v[selected]];
      auto favs = store.data["favorites"];
      bool longRow = ui->measure(g.title, 23, 700) > 384;
      double content = v.size() * 54 + (longRow ? 26 : 0), viewport = 280, top = selected * 54,
             bottom = top + (longRow ? 78 : 52);
      if (top < scroll)
        scroll = top;
      if (bottom > scroll + viewport)
        scroll = bottom - viewport;
      scroll = std::clamp(scroll, 0.0, std::max(0.0, content - viewport));
      SDL_Rect clip{16, 128, 452, 280};
      SDL_RenderSetClipRect(ui->renderer, &clip);
      int y = 128 - int(scroll);
      for (size_t i = 0; i < v.size(); i++) {
        int h = i == size_t(selected) && longRow ? 78 : 52;
        if (y + h > 128 && y < 408) {
          bool fav = std::find(favs.begin(), favs.end(), games[v[i]].id) != favs.end();
          ui->row(games[v[i]].title, y, i == size_t(selected), 452, "", games[v[i]].available, h,
                  i == size_t(selected) ? (fav ? "star-filled" : "star") : fav ? "star" : "");
        }
        y += h + 2;
      }
      SDL_RenderSetClipRect(ui->renderer, nullptr);
      ui->rect(16, 408, 452, 24, ui->bg);
      ui->scrollbar(content, viewport, scroll, true);
      auto path = g.path.parent_path() / (artwork + ".png");
      if (!g.available || !ui->image(path, 492, 140, 204, 204, artwork == "cover")) {
        ui->rect(492, 140, 204, 204, ui->focus, 9);
        ui->icon("folder", 578, 193, 32);
        ui->text(tr(!g.available ? "recent.unavailable_short" : artwork == "cover"   ? "art.missing_cover"
                    : artwork == "title" ? "art.missing_title"
                                         : "art.missing_gameplay"),
                 504, 244, 17, 500, ui->secondary, 180);
      }
      for (int i = 0; i < 3; i++) {
        bool on = (artwork == "cover" && i == 0) || (artwork == "title" && i == 1) ||
                  (artwork == "screenshot" && i == 2);
        ui->circle(570 + i * 20, 356, 8, on ? ui->cream : ui->secondary, on);
      }
      ui->text("‹", 546, 345, 23);
      ui->text("›", 630, 345, 23);
      auto label = tr(artwork == "cover"   ? "art.cover"
                      : artwork == "title" ? "art.title"
                                           : "art.gameplay");
      ui->text(label, 594 - ui->measure(label, 18) / 2, 372, 18);
      auto source = g.system + " · " + g.volume;
      ui->text(source,594-ui->measure(source,17)/2,399,17,500,ui->secondary,204);
      std::vector<std::pair<std::string,std::string>> hints;
      if (g.available) hints.push_back({"A",tr("action.play")});
      if(searchQuery.empty())hints.push_back({"X",tr("screen.favorites")});
      hints.push_back({"Y",tr("search.title")});
      hints.push_back({"MENU",tr("screen.settings")});
      if(!searchQuery.empty())hints.push_back({"B",tr("search.clear")});
      ui->hints(hints);
    }
  } else if(screen==Screen::Search) {
    searchDraw();
  } else if(screen==Screen::Clock || screen==Screen::ClockEdit) {
    clockDraw();
  } else if(screen==Screen::Bluetooth || screen==Screen::BluetoothDevices || screen==Screen::BluetoothDevice || screen==Screen::BluetoothForget) {
    btDraw();
  } else if(screen==Screen::Wifi || screen==Screen::WifiNetworks || screen==Screen::WifiNetwork || screen==Screen::WifiPassword) {
    wifiDraw();
  } else if (screen == Screen::Settings) {
    const std::vector<std::string> labels = {"section.interface", "section.display", "section.sound",
      "clock.title", "section.network", "section.power", "section.debug", "power.restart", "power.shutdown"};
    const std::vector<std::string> icons = {"panels-top-left","monitor","volume-2","clock","wifi","battery","bug","rotate-cw","power"};
    ui->frame(tr("screen.settings"), "", zone, "settings");
    constexpr double viewport = 324, content = 9 * 54 + 16;
    const double top = settingsFocus * 54 + (settingsFocus >= 7 ? 16 : 0);
    if (top < settingsScroll) settingsScroll = top;
    if (top + 54 > settingsScroll + viewport) settingsScroll = top + 54 - viewport;
    settingsScroll = std::clamp(settingsScroll, 0.0, content - viewport);
    SDL_Rect clip{16, 84, 672, 324};
    SDL_RenderSetClipRect(ui->renderer, &clip);
    const int divider = 84 + 7 * 54 + 7 - int(settingsScroll);
    ui->line(32,divider,672,divider,ui->separator);
    for (int i = 0; i < 9; ++i) {
      int y = 84 + i * 54 + (i >= 7 ? 16 : 0) - int(settingsScroll);
      if (y + 52 > 84 && y < 408)
        ui->row(tr(labels[i]), y, i == settingsFocus, 672, "", true, 52, "chevron-right", icons[i]);
    }
    SDL_RenderSetClipRect(ui->renderer, nullptr);
    ui->scrollbar(content, viewport, settingsScroll, false, 91, 406);
    std::vector<std::pair<std::string,std::string>> hints{{"A",tr("action.open")},{"B",tr("action.back")}};
    if(settingsFocus >= 7) hints.push_back({"X",tr("footer.info")});
    ui->hints(hints);
  } else if (screen == Screen::SettingsSection) {
    const std::vector<std::string> labels = {"section.interface", "section.display", "section.sound",
      "section.power", "section.debug", "section.network"};
    ui->frame(tr(labels.at(category)), tr("screen.settings"), zone);
    if (category == 0) {
      std::string name = locale.current;
      for (auto &l : locale.registry["locales"])
        if (l["code"] == locale.current) name = l["native_name"];
      ui->row(tr("screen.language"),128,row==0,672,name,true,52,"chevron-right");
      ui->binaryRow(tr("boot.label"),182,row==1,bootAcknowledged,locale,bootLogoSupported());
    } else if (category == 4) {
      bool enabled = store.data.value("debug_enabled", false);
      ui->binaryRow(tr("debug.enabled"),128,row==0,enabled,locale);
      ui->row(tr("debug.level"),182,row==1,672,tr("debug."+store.data.value("debug_level","events")),enabled);
      ui->row(tr("debug.clear"),236,row==2,672,"",true,52,"chevron-right");
    } else if (category == 3) {
      const int top=128,viewport=284,content=322;
      if(row*54<powerScroll)powerScroll=row*54;
      if(row*54+52>powerScroll+viewport)powerScroll=row*54+52-viewport;
      powerScroll=std::clamp(powerScroll,0.0,double(content-viewport));
      SDL_Rect clip{16,top,688,viewport};SDL_RenderSetClipRect(ui->renderer,&clip);
      const std::vector<std::string> keys{"show_battery_percent","lid_sleep","autosave_enabled","startup_resume","idle_screen_minutes","battery_warning"};
      const std::vector<std::string> labels{"setting.battery_percent","setting.lid_sleep","setting.autosave","setting.startup_resume","setting.idle_screen","setting.battery_warning"};
      for(int i=0;i<6;++i) {
        int y=top+i*54-int(powerScroll);
        if(i==4) {
          int minutes=store.data.value(keys[i],5);
          ui->row(tr(labels[i]),y,row==i,672,minutes?locale.tr("power.minutes",{{"minutes",minutes}}):tr("value.off"));
        } else ui->binaryRow(tr(labels[i]),y,row==i,store.data.value(keys[i],true),locale,i!=1 || device->canSleep());
      }
      SDL_RenderSetClipRect(ui->renderer,nullptr);
      ui->rect(16,412,688,20,ui->bg);
      ui->scrollbar(content,viewport,powerScroll,false,128,412);
    } else if (category == 5) {
      ui->row("Wi-Fi",128,row==0,672,"",true,52,"chevron-right");
      ui->row("Bluetooth",182,row==1,672,"",true,52,"chevron-right");
    } else if (category == 1) {
      ui->row(tr("setting.brightness"),128,row==0,672,brightness ? std::to_string(*brightness)+"%" : "—",brightness.has_value());
      ui->binaryRow(tr("setting.integer_scaling"),182,row==1,store.data.value("integer_scaling",false),locale);
      ui->row(tr("setting.game_background"),236,row==2);
      auto backgroundName=tr("background."+store.data.value("game_background","black"));
      ui->text(backgroundName,640-ui->measure(backgroundName,23),248,23);
      ui->circle(656,250,24,ui->secondary,true);
      ui->circle(658,252,20,gameBackground(),true);
    } else {
      bool supported = category == 2 || brightness.has_value();
      std::string value = category == 2 ? std::to_string(previewVolume)+"%" : brightness ? std::to_string(*brightness)+"%" : "—";
      ui->row(tr(category==1?"setting.brightness":"setting.volume"),128,true,672,value,supported);
    }
    const bool opens = (category == 0 && row == 0) || category == 5 || (category == 4 && row == 2);
    std::vector<std::pair<std::string,std::string>> hints;
    if (opens) hints.push_back({"A",tr("action.open")});
    else {
      hints.push_back({"LEFT/RIGHT",tr("footer.value")});
      if (category == 0 || (category == 3 && row!=4) || (category == 1 && row == 1) || (category == 4 && row == 0))
        hints.push_back({"A",tr("footer.toggle")});
    }
    hints.push_back({"B",tr("action.back")});
    hints.push_back({"X",tr("footer.info")});
    const bool disabled = (category == 0 && row == 1 && (!bootLogoSupported() || bootPending() || !bootAcknowledged)) ||
      (category == 1 && row == 0 && !brightness) || (category == 3 && row == 1 && !device->canSleep()) ||
      (category == 4 && row == 1 && !store.data.value("debug_enabled",false));
    ui->hints(hints,disabled);
  } else if (screen == Screen::SystemProgress || screen==Screen::SavingAutomatic) {
    ui->rect(0,0,720,480,ui->bg);
    ui->icon(screen==Screen::SavingAutomatic?"save":restartRequested ? "rotate-cw" : "power",328,142,64);
    auto title = tr(screen==Screen::SavingAutomatic?"autosave.progress":restartRequested ? "power.restarting" : "power.shutting_down");
    ui->text(title,(720-ui->measure(title,29,700))/2,242,29,700);
  } else if (screen == Screen::SystemConfirm) {
    ui->frame(tr(restartRequested?"power.restart":"power.shutdown"),tr("screen.settings"),zone);
    ui->text(tr(restartRequested?"power.restart_confirm":"power.shutdown_confirm"),32,128,23,500,ui->cream,656);
    ui->row(tr("action.cancel"),204,row==0);
    ui->row(tr(restartRequested?"power.restart":"power.shutdown"),258,row==1,672,"",true,52,"",restartRequested?"rotate-cw":"power");
    ui->hints({{"A",tr("action.select")},{"B",tr("action.back")}});
  } else if (screen == Screen::LogClear) {
    ui->frame(tr("debug.clear"),tr("section.debug"),zone);
    std::istringstream lines(tr("debug.confirm"));std::string line;int y=150;
    while(std::getline(lines,line)) { ui->text(line,32,y,23,500,ui->cream,650);y+=42; }
    ui->hints({{"A",tr("debug.clear")},{"B",tr("action.back")}});
  } else if (screen == Screen::Language) {
    ui->frame(tr("screen.language"), tr("editor.path_general"), zone);
    auto ls = locale.registry["locales"];
    for (size_t i = 0; i < ls.size(); i++)
      ui->row(ls[i]["native_name"], 128 + i * 54, i == size_t(row), 672, "", true, 52,
              ls[i]["code"] == locale.current ? "check" : "");
    ui->hints({{"A", tr("action.apply")}, {"B", tr("action.cancel")}});
  } else if (screen == Screen::Regions || screen == Screen::Zones) {
    ui->frame(tr("setting.timezone"),
              screen == Screen::Regions ? tr("timezone.pick_country") : countryLabel(region), zone);
    auto rs = regions();
    auto ls = locations();
    int count = screen == Screen::Regions ? rs.size() : ls.size();
    row = std::clamp(row, 0, std::max(0, count - 1));
    double content = count * 54, viewport = 252, top = row * 54;
    if (top < scroll)
      scroll = top;
    if (top + 52 > scroll + viewport)
      scroll = top + 52 - viewport;
    scroll = std::clamp(scroll, 0.0, std::max(0.0, content - viewport));
    SDL_Rect clip{16, 128, 672, 252};
    SDL_RenderSetClipRect(ui->renderer, &clip);
    auto now = time(nullptr);
    for (int i = 0; i < count; i++) {
      int y = 128 + i * 54 - int(scroll);
      if (y + 52 <= 128 || y >= 380)
        continue;
      const auto label = screen == Screen::Regions ? countryLabel(rs[i]) : zoneLabel(ls[i].id);
      bool saved = screen == Screen::Regions ? false : ls[i].id == zone;
      if (screen == Screen::Regions)
        for (const auto &z : zones.zones) if (z.id == zone && z.country == rs[i]) saved = true;
      ui->row(label, y, i == row, 672,
              screen == Screen::Zones ? zones.offset(ls[i].id, now) : "›",
              true, 52, "", saved ? "check" : "");
    }
    SDL_RenderSetClipRect(ui->renderer, nullptr);
    ui->rect(16, 380, 672, 52, ui->bg);
    ui->scrollbar(content, viewport, scroll, false);

    ui->hints({{"A", tr(screen == Screen::Regions ? "action.open" : "action.apply")},
               {"B", tr("action.back")},
               {"LEFT/RIGHT",tr("timezone.letter")},{"X",tr("footer.info")}});
  } else if (screen == Screen::GameMenu) {
    ui->frame(tr("screen.game_menu"), game.title, zone);
    std::vector<std::string> labels = {"action.resume", "action.save_state", "action.load_state",
                                       "action.game_settings", "action.return_library"};
    const std::vector<std::string> icons = {"play", "save", "folder", "settings", "arrow-left"};
    for (int i = 0; i < 5; i++)
      ui->row(tr(labels[i]), 128 + i * 54, row == i, 452, "",
              !(i == 2 && !anySlots(*this)) && !(i == 1 && !core->canSave()), 52, "", icons[i]);
    ui->pixels(core->frame, core->width, core->height, 492, 140, 204, 204);
    ui->text(tr("game.paused"), 492, 365, 19);
    ui->text(game.system,492,396,18,400,ui->secondary);
    ui->hints({{"A", tr("action.select")}, {"B", tr("action.resume")}});
  } else if (screen == Screen::GameSettings) {
    ui->frame(tr("screen.game_settings"), tr("scope.game"), zone);
    const auto &options = store.data.at("game_options");
    bool inherited = !options.contains(game.id) || !options.at(game.id).contains("integer_scaling");
    auto effective = tr(store.integerScaling(game.id)?"value.on":"value.off");
    auto value = inherited ? locale.tr("value.global_state",{{"value",effective}}) : effective;
    ui->row(tr("setting.integer_scaling"),128,true,672,value);
    ui->hints({{"LEFT/RIGHT", tr("footer.value")}, {"B", tr("action.back")}, {"X", tr("footer.info")}});
  } else if (screen == Screen::SaveSlots || screen == Screen::LoadSlots) {
    ui->frame(tr(screen == Screen::SaveSlots ? "screen.save_slots" : "screen.load_slots"),
              game.title, zone);
    for (int i=0;i<3;++i) {
      const auto &view=slotView(i);
      auto label=view.present?(view.utc>0?slotDate(view.utc):tr("slot.saved")):tr("slot.empty");
      ui->row(label,128+i*54,row==i,452,"",screen==Screen::SaveSlots || view.compatible,52,"",
              screen==Screen::SaveSlots?"save":"folder");
    }
    const auto &view=slotView(row);
    if (!view.pixels.empty()) ui->pixels(view.pixels,view.width,view.height,492,140,204,204);
    else {
      ui->rect(492,140,204,204,ui->focus,9);
      ui->icon("save",578,194,32);
      auto label=tr(view.present?"slot.no_preview":"slot.empty");
      ui->text(label,594-ui->measure(label,19)/2,252,19,400,ui->secondary,192);
    }
    if (view.present && !view.compatible) ui->text(tr("slot.incompatible"),492,365,19,400,ui->secondary,204);
    std::vector<std::pair<std::string,std::string>> hints{{"A",tr("action.select")},{"B",tr("action.back")}};
    if (view.present) hints.push_back({"Y",tr("action.delete")});
    hints.push_back({"X",tr("footer.info")});
    ui->hints(hints,screen==Screen::LoadSlots && !view.compatible);
  } else if(screen==Screen::StartupResume) {
    ui->frame(tr("startup.title"),game.title,zone);
    ui->row(tr("action.resume"),160,row==0);
    ui->row(tr("startup.library"),214,row==1);
    ui->hints({{"A",tr("action.select")},{"B",tr("startup.library")}});
  } else if (screen == Screen::ResumeChoice || screen == Screen::Overwrite || screen == Screen::DeleteSlot ||
             screen == Screen::LoadConfirm || screen == Screen::ExitConfirm) {
    std::string title = tr(screen == Screen::DeleteSlot ? "slot.delete_confirm" : screen == Screen::ResumeChoice  ? "resume.title"
                           : screen == Screen::Overwrite   ? "slot.overwrite"
                           : screen == Screen::LoadConfirm ? "slot.load_confirm"
                                                           : "exit.title");
    ui->frame(title, game.title, zone);
    ui->row(tr(screen == Screen::ResumeChoice  ? "action.resume"
               : screen == Screen::ExitConfirm ? "action.stay"
                                               : "action.cancel"),
            screen==Screen::ExitConfirm?204:128, row == 0);
    ui->row(tr(screen == Screen::ResumeChoice  ? "resume.new"
               : screen == Screen::ExitConfirm ? "action.exit"
               : screen == Screen::DeleteSlot  ? "action.delete"
               : screen == Screen::Overwrite   ? "action.overwrite"
                                               : "action.confirm"),
            screen==Screen::ExitConfirm?258:182, row == 1);
    if (screen == Screen::DeleteSlot)
      { std::istringstream lines(tr("slot.delete_body"));std::string line;int y=270;
        while(std::getline(lines,line)){ui->text(line,32,y,22,400,ui->secondary,656);y+=34;} }
    if (screen == Screen::ExitConfirm)
      ui->text(autosaveError.empty()?tr(store.data.value("autosave_enabled",true)?"exit.autosave":"exit.warning"):tr("autosave.failed"),32,136,22,400,ui->secondary,656);
    if(screen==Screen::ExitConfirm && !autosaveError.empty())ui->row(tr("autosave.skip"),312,row==2);
    ui->hints({{"A", tr("action.select")}, {"B", tr("action.back")}});
  } else {
    ui->frame(tr(screen == Screen::Launching ? "progress.launch" : "screen.recovery"), game.title,
              zone);
    ui->text(notice, 32, 164, 23, 500, ui->cream, 650);
    ui->hints({{"B", tr("action.back")}});
  }
  if (!notice.empty() && screen!=Screen::Recovery)
    ui->notice(notice,notice==tr("notice.saved")?"save":"settings",screen==Screen::Library);
  if (infoOpen) {
    auto key = infoKey();
    auto body = key == "timezone"
        ? locale.tr("info.timezone.body", {{"pages", ui->buttonLabel("L1/R1")}})
        : tr("info." + key + ".body");
    if(key=="wifi" && !wifi.address.empty())body+="\nIP: "+wifi.address;
    if (key=="brightness" && !brightness) body+="\n"+tr("binary.unsupported");
    if (key=="clock") body+="\n"+tr("clock.last")+": "+(clockLastSync>0 && zones.valid(zone)?slotDate(clockLastSync):tr("clock.never"));
    if (key=="lid" && !device->canSleep()) body+="\n"+tr("binary.unsupported");
    if (screen==Screen::Zones) { auto locations=this->locations();if(row<int(locations.size()))body+="\n"+locations[row].id; }
    auto error = key == "boot" ? bootLastError : key == "scaling" ? scalingLastError : "";
    if (key == "boot" && bootPending()) body += "\n" + tr("binary.applying");
    else if (!error.empty()) body += "\n" + error;
    else if (!notice.empty()) body += "\n" + notice;
    Json sections;
    if (key == "boot" || key == "scaling") {
      sections = Json::object();
      for (auto field : {"intro", "on", "off", "timing", "note"})
        sections[field] = tr("info." + key + "." + field);
      sections["on_label"] = tr("value.on");
      sections["off_label"] = tr("value.off");
      sections["status"] = key == "boot" && !bootLogoSupported() ? tr("binary.unsupported") : key == "boot" && bootPending() ? tr("binary.applying") : error.empty() ? notice : error;
    }
    infoScroll = ui->info(tr("info." + key + ".title"), body, infoScroll, sections);
    ui->hints({{"B", tr("footer.close")}});
  }
  ui->volumeOverlay(volumeFeedback, locale, screen == Screen::Library && !notice.empty() && !infoOpen);
  ui->present();
  batterySample();
}
} // namespace rondo
