// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
#include <iostream>
namespace rondo {
App::App(Options opts, bool runner, const Game &entry)
    : options(std::move(opts)), store(options.state), locale(options.assets) {
  locale.select(options.assets, store.data.value("locale", "en"));
  timezoneNames = readJson(options.assets / "Timezones/names.json");
  notice = store.warning.empty() ? "" : locale.tr("notice.config_recovered");
  if (!store.warning.empty()) std::cerr << "Configuration: " << store.warning << "\n";
  artwork = store.data.value("artwork", "cover");
  section = store.data.value("section", "All");
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
    core = std::make_unique<Core>(options.core, game, store.gameDir(game), options.headless);
    core->volume = store.data.value("volume", 80);
    screen = Screen::Running;
  } else {
    games = scan(options.volumes);
    sections = {"All", "Favorites"};
    for (auto &g : games)
      if (std::find(sections.begin(), sections.end(), g.system) == sections.end())
        sections.push_back(g.system);
    if (std::find(sections.begin(), sections.end(), section) == sections.end())
      section = "All";
    auto v = visible();
    auto id = store.data.value("focus", "");
    for (size_t i = 0; i < v.size(); ++i)
      if (games[v[i]].id == id)
        selected = int(i);
  }
  volumeFeedback.confirmed = store.data.value("volume", 80);
  record("ready");
}
std::vector<int> App::visible() const {
  std::vector<int> v;
  auto favorites = store.data.value("favorites", Json::array());
  for (size_t i = 0; i < games.size(); i++) {
    auto &g = games[i];
    bool fav = std::find(favorites.begin(), favorites.end(), g.id) != favorites.end();
    if (section == "All" || (section == "Favorites" && fav) || section == g.system)
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
  case Screen::Library:
    return "library";
  case Screen::Settings:
    return "global_settings";
  case Screen::Language:
    return "language";
  case Screen::Regions:
    return "timezone_regions";
  case Screen::Zones:
    return "timezone_list";
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
  trace.push_back({{"event", event},
                   {"screen", screenName()},
                   {"row", row},
                   {"section", section},
                   {"artwork", artwork},
                   {"locale", locale.current},
                   {"notice", notice}});
}
void App::persist(Json next) {
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
    throw;
  }
}
static bool anySlots(App &a) {
  for (int i = 0; i < 3; i++)
    if (a.slotView(i).compatible)
      return true;
  return false;
}
bool App::usableSlot(int i) const {
  if (core)
    return core->compatible(i);
  try {
    auto j = readJson(store.gameDir(game) / ("slot-" + std::to_string(i) + ".json"));
    auto b = j.at("payload").get<std::vector<uint8_t>>();
    return j.at("core") == hashFile(options.core) && j.at("rom") == hashFile(game.path) &&
           j.at("sha256") == hashBytes(b);
  } catch (...) {
    return false;
  }
}
int App::latestResumeSlot() const {
  int selected = -1;
  long long bestUtc = 0;
  fs::file_time_type bestModified = fs::file_time_type::min();
  for (int candidate = 0; candidate < 3; ++candidate)
    if (usableSlot(candidate)) {
      auto path = store.gameDir(game) / ("slot-" + std::to_string(candidate) + ".json");
      auto meta = readJson(path);
      auto utc = meta.value("utc", 0LL);
      auto modified = fs::last_write_time(path);
      if (selected < 0 || utc > bestUtc || (utc == bestUtc && modified > bestModified)) {
        selected = candidate;
        bestUtc = utc;
        bestModified = modified;
      }
    }
  return selected;
}
void App::applyZone(const std::string &id) {
  std::string previous = store.data.value("timezone", "Etc/UTC");
  zones.applyHost(id);
  auto next = store.data;
  next["timezone"] = id;
  try {
    persist(next);
    screen = Screen::Settings;
    row = 1;
    notice.clear();
  } catch (...) {
    if (zones.valid(previous))
      zones.applyHost(previous);
    throw;
  }
}
void App::action(const std::string &a) {
  for (auto id : {"notice.language_changed", "notice.favorite_added", "notice.favorite_removed",
                  "notice.saved", "notice.loaded"})
    if (notice == locale.tr(id))
      notice.clear();
  const std::string priorNotice = notice;
  try {
    if (a == "SLEEP") { sleep(); record(a); return; }
    if (a == "VOLUME_UP" || a == "VOLUME_DOWN") {
      volumeFeedback.begin(monotonicMs());
      draw();
      auto next = store.data;
      next["volume"] =
          std::clamp(store.data.value("volume", 80) + (a == "VOLUME_UP" ? 5 : -5), 0, 100);
      persist(next);
      if (core) {
        core->volume = store.data.value("volume", 80);
        core->silence();
      }
      previewVolume = store.data.value("volume", 80);
      volumeFeedback.result(volumeFeedback.sequence, store.data.value("volume", 80), true,
                            monotonicMs());
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
    if (screen == Screen::Library) {
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
      if (a == "Y") {
        screen = Screen::Settings;
        row = 0;
        category = 0;
      }
      if (a == "A" && !v.empty()) {
        game = games[v[selected]];
        if (game.system != "NES" || !fs::is_regular_file(options.core)) {
          screen = Screen::Recovery;
          notice = locale.tr("error.core_unavailable");
        } else if (anySlots(*this)) {
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
    } else if (screen == Screen::Settings) {
      int count = category == 0 ? 3 : (category == 1 || category == 3) ? 2 : 1;
      if (a == "UP")
        row = std::max(0, row - 1);
      if (a == "DOWN")
        row = std::min(count - 1, row + 1);
      if (a == "L1" || a == "R1") {
        category = (category + 4 + (a == "R1" ? 1 : -1)) % 4;
        row = 0;
      }
      if (a == "B") {
        screen = Screen::Library;
        row = 0;
      }
      if (category == 0 && row == 2 && (a == "A" || a == "LEFT" || a == "RIGHT")) {
        if (!bootLogoSupported()) {
          notice = locale.tr("bootlogo.unsupported");
          return;
        }
        if (!bootPending() && bootAcknowledged)
          requestBootLogo(a == "LEFT" ? false : a == "RIGHT" ? true : !*bootAcknowledged);
      }
      if ((a == "LEFT" || a == "RIGHT") && category == 2) {
        auto next=store.data;
        int desired=std::clamp(store.data.value("volume",80)+(a=="RIGHT"?5:-5),0,100);
        if (desired!=store.data.value("volume",80)) { next["volume"]=desired;persist(next); }
        notice.clear();
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
      if (category == 1 && row == 1 && (a == "A" || a == "LEFT" || a == "RIGHT")) {
        bool current = store.data.value("show_battery_percent", true);
        bool desired = a == "LEFT" ? false : a == "RIGHT" ? true : !current;
        if (desired != current) {
          auto next = store.data;
          next["show_battery_percent"] = desired;
          persist(next);
        }
        notice.clear();
      }
      if (category==3 && (row==0 ? device->canSleep() : device->canWakeOnLid()) && (a=="LEFT" || a=="RIGHT" || a=="A")) {
        const auto setting=row==0?"lid_sleep":"wake_on_lid_open";
        bool current=store.data.value(setting,true);
        bool desired=a=="LEFT"?false:a=="RIGHT"?true:!current;
        if (desired!=current) { auto next=store.data;next[setting]=desired;persist(next); }
        notice.clear();
      }
      if (a == "A" && category == 0 && row != 2) {
        if (row == 0) {
          screen = Screen::Language;
          row = 0;
          for (size_t i = 0; i < locale.registry["locales"].size(); i++)
            if (locale.registry["locales"][i]["code"] == locale.current)
              row = i;
        } else {
          if (zones.zones.empty())
            notice = locale.tr("timezone.unavailable");
          else {
            openTimezones();
          }
        }
      }
    } else if (screen == Screen::Language) {
      int count = locale.registry["locales"].size();
      if (a == "UP")
        row = std::max(0, row - 1);
      if (a == "DOWN")
        row = std::min(count - 1, row + 1);
      if (a == "B") {
        screen = Screen::Settings;
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
        screen = Screen::Settings;
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
          screen = Screen::Settings;
          row = 1;
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
        bool current = next["game_options"].contains(game.id) && next["game_options"][game.id].value("integer_scaling", false);
        bool desired = a == "LEFT" ? false : a == "RIGHT" ? true : !current;
        if (current != desired) {
          next["game_options"][game.id]["integer_scaling"] = desired;
          persist(next);
        }
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
    } else if (screen == Screen::Overwrite || screen == Screen::LoadConfirm ||
               screen == Screen::ExitConfirm) {
      if (a == "UP")
        row = 0;
      if (a == "DOWN")
        row = 1;
      auto origin = screen;
      bool confirmed = a == "A" && row == 1;
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
          core->flushSram();
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
    if (screen == Screen::GameSettings) scalingLastError = notice;
  }
  record(a);
}
std::string App::infoKey() const {
  if (screen == Screen::GameSettings) return "scaling";
  if (screen == Screen::SaveSlots || screen == Screen::LoadSlots) return "slots";
  if (screen == Screen::Regions || screen == Screen::Zones) return "timezone";
  if (screen != Screen::Settings) return "";
  if (category == 0) return row == 0 ? "language" : row == 1 ? "timezone" : "boot";
  return category == 1 ? (row == 1 ? "battery" : "brightness") : category == 2 ? "sound" : row==0 ? "lid" : "lid_wake";
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
void App::ticks(unsigned n) {
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
  if (!device->canSleep()) { notice=locale.tr("sleep.unsupported");return; }
  sleeping=true;
  auto origin=screen; bool wasPaused=core?core->paused:true;
  try {
    while (bootPending()) { ticks(0);SDL_Delay(5); }
    if (core) { core->paused=true;core->buttons=0;core->silence();core->flushSram();core->closeAudio(); }
    record("sleep:prepared");
    device->sleep(store.data.value("wake_on_lid_open",true) && device->canWakeOnLid(),
                  store.data.value("lid_sleep",true));
    if(ui) {
      ui->handheldKeys=std::make_unique<H700Keys>();ui->volumeKeys=std::make_unique<VolumeKeys>();
      ui->powerKeys=std::make_unique<PowerKeys>();SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
    }
    if(core) { core->openAudio();core->paused=wasPaused; }
    lastLidOpen=device->lidOpen();lidCheckAt=monotonicMs()+300;
    notice.clear();record("sleep:resumed");
  } catch (const std::exception &e) {
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
  ui->showBatteryPercent = store.data.value("show_battery_percent", true);
  auto tr = [this](auto key) { return locale.tr(key); };
  auto zone = store.data.value("timezone", "Etc/UTC");
  if (screen == Screen::Running) {
    ui->rect(0, 0, 720, 480, ui->bg);
    int w = 720, h = 480;
    if (store.data["game_options"].contains(game.id) &&
        store.data["game_options"][game.id].value("integer_scaling", false)) {
      int scale = std::max(1, std::min(720 / core->width, 480 / core->height));
      w = core->width * scale;
      h = core->height * scale;
    }
    ui->pixels(core->frame, core->width, core->height, (720 - w) / 2, (480 - h) / 2, w, h);
    if (!notice.empty()) ui->notice(notice,"folder");
    ui->volumeOverlay(volumeFeedback, locale, !notice.empty());
    ui->present();
    return;
  }
  if (screen == Screen::Library) {
    auto v = visible();
    ui->frame(section == "Favorites" ? tr("screen.favorites") : tr("screen.library"), "", zone);
    int x = 24;
    for (auto &s : sections) {
      std::string label = s == "All" ? tr("section.all") : s;
      int w = s == "Favorites" ? 22 : ui->measure(label, 19, s == section ? 700 : 500);
      if (x + w > 680)
        break;
      if (s == "Favorites")
        ui->icon("star", x, 86, 22);
      else
        ui->text(label, x, 85, 19, s == section ? 700 : 500);
      if (s == section)
        ui->rect(x, 115, w, 2, ui->accent);
      x += w + 30;
    }
    if (v.empty()) {
      ui->text(tr(section == "Favorites" ? "empty.favorites" : "empty.library"), 32, 182, 27, 700);
      ui->text(section == "Favorites"
                   ? locale.tr("empty.favorites_help", {{"button", ui->buttonLabel("X")}})
                   : tr("empty.library_help"), 32, 224,
               17, 500, ui->secondary);
      ui->hints({{"Y", tr("screen.settings")}, {"L1/R1", tr("action.section")}});
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
          ui->row(games[v[i]].title, y, i == size_t(selected), 452, "", true, h,
                  fav ? "star" : "");
        }
        y += h + 2;
      }
      SDL_RenderSetClipRect(ui->renderer, nullptr);
      ui->rect(16, 408, 452, 24, ui->bg);
      ui->scrollbar(content, viewport, scroll, true);
      auto path = g.path.parent_path() / (artwork + ".png");
      if (!ui->image(path, 492, 140, 204, 204, artwork == "cover")) {
        ui->rect(492, 140, 204, 204, ui->focus, 9);
        ui->icon("folder", 578, 193, 32);
        ui->text(tr(artwork == "cover"   ? "art.missing_cover"
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
      ui->hints({{"A", tr("action.play")},
                 {"X", tr("screen.favorites")},
                 {"Y", tr("screen.settings")},
                 {"L1/R1", tr("action.section")}});
    }
  } else if (screen == Screen::Settings) {
    std::vector<std::string> cat = {"section.general", "section.display", "section.sound",
                                    "section.power"};
    ui->frame(tr("screen.settings"), "", zone);
    int x = 24;
    for (int i = 0; i < 4; i++) {
      auto label = tr(cat[i]);
      ui->text(label, x, 85, 19, i == category ? 700 : 500);
      if (i == category)
        ui->rect(x, 115, ui->measure(label, 19), 2, ui->accent);
      x += ui->measure(label, 19) + 38;
    }
    if (category == 0) {
      std::string name = locale.current;
      for (auto &l : locale.registry["locales"])
        if (l["code"] == locale.current)
          name = l["native_name"];
      ui->row(tr("screen.language"), 128, row == 0, 672, name + "  ›");
      ui->row(tr("setting.timezone"), 182, row == 1, 672, zoneLabel(zone) + "  ›");
      ui->binaryRow(tr("boot.label"), 236, row == 2, bootAcknowledged, locale, bootLogoSupported());
    } else if (category==3) {
      ui->binaryRow(tr("setting.lid_sleep"),128,row==0,store.data.value("lid_sleep",true),locale,device->canSleep());
      ui->binaryRow(tr("setting.lid_wake"),182,row==1,store.data.value("wake_on_lid_open",true),locale,device->canWakeOnLid());
    } else {
      bool supported=category==2 || brightness.has_value();
      std::string value=category==2?std::to_string(store.data.value("volume",80))+"%":brightness?std::to_string(*brightness)+"%":"—";
      ui->row(tr(category==1?"setting.brightness":"setting.volume"),128,row==0,672,value,supported);
      if (category == 1)
        ui->binaryRow(tr("setting.battery_percent"),182,row==1,store.data.value("show_battery_percent",true),locale);
    }
    if (category==0 && row!=2)
      ui->hints({{"A",tr("action.change")},{"B",tr("action.back")},{"L1/R1",tr("action.section")},{"X",tr("footer.info")}});
    else {
      std::vector<std::pair<std::string,std::string>> hints;
      if ((category==0 && row==2) || category==3 || (category==1 && row==1)) hints.push_back({"A",tr("footer.toggle")});
      hints.push_back({"LEFT/RIGHT",tr("footer.value")});hints.push_back({"B",tr("action.back")});
      if((category==1 && row==0) || category==2) hints.push_back({"L1/R1",tr("action.section")});
      hints.push_back({"X",tr("footer.info")});
      ui->hints(hints,(category==0 && (!bootLogoSupported() || bootPending() || !bootAcknowledged)) ||
                      (category==1 && row==0 && !brightness) || (category==3 && !(row==0?device->canSleep():device->canWakeOnLid())));
    }
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
    bool scaling = false;
    if (store.data["game_options"].contains(game.id))
      scaling = store.data["game_options"][game.id].value("integer_scaling", false);
    ui->binaryRow(tr("setting.integer_scaling"), 128, true, scaling, locale);
    ui->hints({{"A", tr("footer.toggle")}, {"B", tr("action.back")}, {"LEFT/RIGHT", tr("footer.value")}, {"X", tr("footer.info")}});
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
    ui->hints({{"A",tr("action.select")},{"B",tr("action.back")},{"X",tr("footer.info")}},screen==Screen::LoadSlots && !view.compatible);
  } else if (screen == Screen::ResumeChoice || screen == Screen::Overwrite ||
             screen == Screen::LoadConfirm || screen == Screen::ExitConfirm) {
    std::string title = tr(screen == Screen::ResumeChoice  ? "resume.title"
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
               : screen == Screen::Overwrite   ? "action.overwrite"
                                               : "action.confirm"),
            screen==Screen::ExitConfirm?258:182, row == 1);
    if (screen == Screen::ExitConfirm)
      ui->text(tr("exit.warning"), 32, 136, 22, 400, ui->secondary,656);
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
    if (key=="brightness" && !brightness) body+="\n"+tr("binary.unsupported");
    if (key=="lid_wake" && !device->canWakeOnLid()) body+="\n"+tr("binary.unsupported");
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
}
} // namespace rondo
