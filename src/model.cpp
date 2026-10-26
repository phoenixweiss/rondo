// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "model.hpp"
#include "psx.hpp"
#include <limits>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
namespace rondo {
long long saveSequence(const Json &meta) {
  if(!meta.contains("save_sequence"))return 0;
  const auto &value=meta["save_sequence"];
  if(!value.is_number_integer() || value<0 || value>=std::numeric_limits<long long>::max())return 0;
  return value.get<long long>();
}

std::vector<uint8_t> readBytes(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    throw std::runtime_error("Cannot read " + p.string());
  return {std::istreambuf_iterator<char>(f), {}};
}
std::string hashBytes(const std::vector<uint8_t> &b) { return picosha2::hash256_hex_string(b); }
std::string hashFile(const fs::path &p) {
  std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read "+p.string());
  picosha2::hash256_one_by_one hasher;char block[65536];
  while(f){f.read(block,sizeof(block));hasher.process(block,block+f.gcount());}
  if(f.bad())throw std::runtime_error("Read failed: "+p.string());hasher.finish();return picosha2::get_hash_hex_string(hasher);
}
Json readJson(const fs::path &p) {
  auto b = readBytes(p);
  return Json::parse(b);
}
void atomicWrite(const fs::path &p, const std::vector<uint8_t> &bytes) {
  fs::create_directories(p.parent_path());
  auto temp = p.string() + ".tmp.XXXXXX";
  std::vector<char> name(temp.begin(), temp.end());
  name.push_back(0);
  int fd = mkstemp(name.data());
  if (fd < 0)
    throw std::runtime_error(std::strerror(errno));
  bool renamed = false;
  try {
    size_t n = 0;
    while (n < bytes.size()) {
      auto r = write(fd, bytes.data() + n, bytes.size() - n);
      if (r < 0 && errno == EINTR)
        continue;
      if (r <= 0)
        throw std::runtime_error(std::strerror(errno));
      n += size_t(r);
    }
    if (fsync(fd))
      throw std::runtime_error(std::strerror(errno));
    if (close(fd)) {
      fd = -1;
      throw std::runtime_error(std::strerror(errno));
    }
    fd = -1;
    if (rename(name.data(), p.c_str()))
      throw std::runtime_error(std::strerror(errno));
    renamed = true;
    int dir = open(p.parent_path().c_str(), O_RDONLY);
    if (dir < 0)
      throw std::runtime_error(std::strerror(errno));
    int r = fsync(dir), e = errno;
#ifdef __linux__
    // Vendor FAT kernels may not make allocation-table changes durable via
    // directory fsync alone. Flush the entire owning filesystem after rename.
    if (syncfs(dir)) {
      int error = errno;
      close(dir);
      throw std::runtime_error(std::strerror(error));
    }
#endif
    close(dir);
    if (r && e != EINVAL && e != ENOTSUP)
      throw std::runtime_error(std::strerror(e));
  } catch (...) {
    if (fd >= 0)
      close(fd);
    if (!renamed)
      unlink(name.data());
    throw;
  }
}
std::string utf8Prefix(std::string s, size_t n) {
  if (n >= s.size())
    return s;
  while (n && (static_cast<unsigned char>(s[n]) & 0xc0) == 0x80)
    --n;
  return s.substr(0, n);
}
// Fold ASCII and Cyrillic without depending on the host locale.
std::string searchFold(std::string text) {
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
static bool inside(const fs::path &p, const fs::path &root) {
  auto r = p.lexically_relative(root);
  return !r.empty() && *r.begin() != "..";
}
std::vector<Game> scan(const std::vector<Volume> &volumes) {
  std::vector<Game> out;
  std::set<std::string> volumeIds;
  const std::map<std::string, std::string> systems = {
      {"NES", "NES"},
      {"Nintendo - Nintendo Entertainment System (NES)", "NES"},
      {"Nintendo - Game Boy (GB)", "GB"},
      {"Nintendo - Game Boy Color (GBC)", "GBC"},
      {"Nintendo - Game Boy Advance (GBA)", "GBA"},
      {"Nintendo - Super Nintendo Entertainment System (SNES)", "SNES"},
      {"Sega - Mega Drive - Genesis (MD)", "MD"},
      {"Sony - PlayStation (PS)", "PSX"}};
  const std::set<std::string> extensions = {".nes", ".gb",  ".gbc", ".gba", ".sfc", ".smc",
                                            ".md",  ".gen", ".m3u", ".chd"};
  for (auto v : volumes) {
    if (v.id.empty() || !volumeIds.insert(v.id).second)
      throw std::runtime_error("Volume IDs must be unique and nonempty");
    if (!fs::is_directory(v.root))
      continue;
    v.root = fs::canonical(v.root);
    for (auto it = fs::recursive_directory_iterator(v.root,
                                                    fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it) {
      // macOS stores resource forks as AppleDouble ._ROM.nes sidecars on FAT.
      // Hidden metadata directories must not contribute games or playlists.
      if (it->path().filename().string().front() == '.') {
        if (it->is_directory())
          it.disable_recursion_pending();
        continue;
      }
      if (it->is_symlink()) {
        if (it->is_directory())
          it.disable_recursion_pending();
        continue;
      }
      if (!it->is_regular_file())
        continue;
      auto path = it->path();
      auto ext = path.extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      if (!extensions.count(ext) || !inside(fs::canonical(path), v.root))
        continue;
      // A playlist owns its discs; a cue owns raw bins. Never launch
      // sidecars/tracks separately.
      if (ext != ".m3u") {
        bool listed = false;
        for (const auto &sibling : fs::directory_iterator(path.parent_path()))
          if (sibling.path().filename().string().front() != '.' && searchFold(sibling.path().extension().string()) == ".m3u") {
            try {for(const auto &disc:psxDiscs(sibling.path()))if(disc==fs::canonical(path))listed=true;}catch(...){}
          }
        if (listed)
          continue;
      }
      auto rel = path.lexically_relative(v.root);
      std::string folder = rel.begin()->string();
      std::string system = systems.count(folder) ? systems.at(folder) : folder;
      if (ext == ".nes")system = "NES";
      else if(ext==".gb")system="GB";
      else if(ext==".gbc")system="GBC";
      else if(ext==".gba")system="GBA";
      else if(ext==".sfc" || ext==".smc")system="SNES";
      else if(ext==".md" || ext==".gen")system="MD";
      else if(ext==".chd" || ext==".m3u")system="PSX";
      auto identity = v.id + "\n" + rel.generic_string();
      std::vector<uint8_t> bytes(identity.begin(), identity.end());
      std::string title = path.stem().string();
      if (path.parent_path() != v.root && path.parent_path().filename() != folder)
        title = path.parent_path().filename().string();
      out.push_back({hashBytes(bytes), v.id, system, title, path});
    }
  }
  // Build collation keys once while scanning, never in the render loop. Russian
  // Yo sorts with Ye; search keeps their distinction. Equal titles have a stable
  // system/identity tie-breaker across volumes and filesystem enumeration order.
  std::map<std::string, std::string> titles;
  for (const auto &game : out) {
    auto key = searchFold(game.title);
    for (size_t at = 0; (at = key.find("ё", at)) != std::string::npos; at += 2)
      key.replace(at, 2, "е");
    titles.emplace(game.id, std::move(key));
  }
  std::sort(out.begin(), out.end(), [&](const auto &a, const auto &b) {
    return std::tie(titles.at(a.id), a.system, a.id) <
           std::tie(titles.at(b.id), b.system, b.id);
  });
  return out;
}
static bool validRecent(const Json &entry) {
  if (!entry.is_object()) return false;
  for (const auto *key : {"id","volume","relative_path","system","title"})
    if (!entry.contains(key) || !entry.at(key).is_string() || entry.at(key).get<std::string>().empty() ||
        entry.at(key).get<std::string>().size() > 4096) return false;
  const auto relative = entry.at("relative_path").get<std::string>();
  fs::path path(relative);
  if (path.is_absolute() || path.has_root_name()) return false;
  for (const auto &part : path) if (part == ".." || part == ".") return false;
  auto identity = entry.at("volume").get<std::string>() + "\n" + relative;
  return entry.at("id") == hashBytes({identity.begin(),identity.end()});
}
static Json cleanRecents(const Json &source) {
  Json result=Json::array();std::set<std::string> ids;
  if (source.is_array()) for (const auto &entry : source) {
    if (validRecent(entry) && ids.insert(entry.at("id").get<std::string>()).second)
      result.push_back(entry);
    if (result.size() == 20) break;
  }
  return result;
}
Json recentAfterLaunch(const Json &history, const Game &game, const std::vector<Volume> &volumes) {
  auto volume=std::find_if(volumes.begin(),volumes.end(),[&](const Volume &v){return v.id==game.volume;});
  if (volume==volumes.end() || !game.available) throw std::runtime_error("Unknown recent-game volume");
  auto relative=fs::weakly_canonical(game.path).lexically_relative(fs::weakly_canonical(volume->root)).generic_string();
  Json entry={{"id",game.id},{"volume",game.volume},{"relative_path",relative},
              {"system",game.system},{"title",game.title}};
  if (!validRecent(entry)) throw std::runtime_error("Invalid recent-game identity");
  Json next=Json::array({entry});
  for (const auto &old:cleanRecents(history))
    if (old.at("id")!=game.id && next.size()<20) next.push_back(old);
  return next;
}
std::vector<Game> recentUnavailable(const Json &history, const std::vector<Game> &games) {
  std::set<std::string> available;for(const auto &game:games)available.insert(game.id);
  std::vector<Game> result;
  for (const auto &entry:cleanRecents(history))
    if (!available.count(entry.at("id").get<std::string>()))
      result.push_back({entry.at("id"),entry.at("volume"),entry.at("system"),entry.at("title"),{},false});
  return result;
}
static void validateConfig(const Json &candidate, const Json &defaults) {
  if (!candidate.is_object() || candidate.value("schema", 0) != 1)
    throw std::runtime_error("Unsupported configuration");
  for (const auto &key : {"revision", "locale", "artwork", "section", "focus", "timezone", "volume",
                          "favorites", "game_options", "psx_hle_warning_acknowledged", "psx_firmware", "psx_bios_file", "psx_controller", "rumble_enabled", "left_stick_dpad", "replace_boot_logo", "brightness", "lid_sleep", "wake_on_lid_open", "show_battery_percent", "wifi_enabled", "bluetooth_enabled", "bluetooth_device", "ntp_auto", "debug_enabled", "debug_level", "integer_scaling", "game_background", "autosave_enabled", "startup_resume", "resume_game", "idle_screen_minutes", "battery_warning"})
    if (candidate.contains(key) && candidate[key].type() != defaults[key].type() &&
        !(candidate[key].is_number() && defaults[key].is_number()))
      throw std::runtime_error("Invalid configuration field");
  if(candidate.contains("psx_bios_file")&&!psxBiosFilename(candidate.at("psx_bios_file").get<std::string>()))
    throw std::runtime_error("Invalid PSX BIOS filename");
  if(candidate.contains("psx_controller")&&candidate["psx_controller"]!="digital"&&candidate["psx_controller"]!="dualshock")throw std::runtime_error("Invalid PSX controller");
  if(candidate.contains("psx_firmware")&&candidate["psx_firmware"]!="openbios"&&candidate["psx_firmware"]!="bios"&&candidate["psx_firmware"]!="hle")throw std::runtime_error("Invalid PSX firmware mode");
  if (candidate.contains("debug_level") && candidate["debug_level"] != "errors" &&
      candidate["debug_level"] != "events" && candidate["debug_level"] != "detailed")
    throw std::runtime_error("Invalid diagnostic level");
  if(candidate.contains("idle_screen_minutes")) {
    const std::set<int> values{0,1,2,5,10,15};
    if(!candidate.at("idle_screen_minutes").is_number_integer() ||
       !values.count(candidate.at("idle_screen_minutes").get<int>()))
      throw std::runtime_error("Invalid screen idle interval");
  }
  if (candidate.contains("game_background")) {
    const std::set<std::string> colors{"black", "charcoal", "navy", "theme"};
    if (!colors.count(candidate.at("game_background").get<std::string>()))
      throw std::runtime_error("Invalid game background");
  }
  if (candidate.contains("game_options"))
    for (const auto &entry : candidate.at("game_options").items()) {
      if (!entry.value().is_object() || (entry.value().contains("integer_scaling") &&
          !entry.value().at("integer_scaling").is_boolean()))
        throw std::runtime_error("Invalid game display option");
      if(entry.value().contains("psx_controller") && entry.value()["psx_controller"]!="digital" && entry.value()["psx_controller"]!="dualshock")
        throw std::runtime_error("Invalid game PSX controller");
    }
}
Store::Store(fs::path p) : root(std::move(p)) {
  data = {{"schema", 1},
          {"revision", 0},
          {"locale", "en"},
          {"volume", 80},
          {"debug_enabled", false},
          {"debug_level", "events"},
          {"brightness", 70},
          {"integer_scaling", false},
          {"game_background", "black"},
          {"autosave_enabled", true},
          {"startup_resume", true},
          {"resume_game", ""},
          {"idle_screen_minutes", 5},
          {"battery_warning", true},
          {"lid_sleep", true},
          {"wake_on_lid_open", true},
          {"show_battery_percent", true},
          {"wifi_enabled", false},
          {"ntp_auto", true},
          {"replace_boot_logo", true},
          {"psx_hle_warning_acknowledged", false},
          {"psx_firmware", "openbios"},
          {"psx_controller", "digital"},
          {"rumble_enabled", true},
          {"psx_bios_file", "auto"},
          {"left_stick_dpad", true},
          {"artwork", "cover"},
          {"section", "All"},
          {"focus", ""},
          {"timezone", "Etc/UTC"},
          {"bluetooth_enabled", false},
          {"bluetooth_device", ""},
          {"favorites", Json::array()},
          {"recent_games", Json::array()},
          {"game_options", Json::object()}};
  auto file = root / "config.json";
  if (fs::exists(file)) {
    try {
      auto d = readJson(file);
      if (d.is_object() && d.value("schema", 0) != 1)
        readOnly = true;
      validateConfig(d, data);
      data.update(d);
    } catch (const std::exception &e) {
      warning = e.what();
      try {
        auto d = readJson(root / "config.previous.json");
        validateConfig(d, data);
        if (d.value("schema", 0) == 1) {
          data.update(d);
          warning = "Recovered previous configuration";
        }
      } catch (...) {
      }
    }
  }
  data["recent_games"] = cleanRecents(data.value("recent_games",Json::array()));
  if (data["artwork"] != "cover" && data["artwork"] != "title" && data["artwork"] != "screenshot")
    data["artwork"] = "cover";
}
void Store::commit(Json next) {
  if(next.contains("rumble_enabled")&&!next.at("rumble_enabled").is_boolean())
    throw std::runtime_error("Invalid vibration setting");
  if(next.contains("psx_bios_file")&&(!next.at("psx_bios_file").is_string()||!psxBiosFilename(next.at("psx_bios_file").get<std::string>())))
    throw std::runtime_error("Invalid PSX BIOS filename");
  if (readOnly)
    throw std::runtime_error("Configuration schema requires a newer Rondo");
  // Preserve unknown keys; one authority writes the versioned document.
  next["schema"] = 1;
  next["revision"] = data.value("revision", 0) + 1;
  if (fs::exists(root / "config.json")) {
    auto previous = readBytes(root / "config.json");
    bool valid = false;
    try {
      validateConfig(Json::parse(previous), data);
      valid = true;
    } catch (...) {
    }
    // A rejected current file must not replace the last valid recovery generation.
    atomicWrite(valid ? root / "config.previous.json"
                      : root / ("config.rejected-" + hashBytes(previous) + ".json"),
                previous);
  }
  auto str = next.dump(2) + "\n";
  try {
    atomicWrite(root / "config.json", {str.begin(), str.end()});
  } catch (...) {
    // Rename may already have happened: resolve actual durable state before
    // another attempt.
    try {
      data = readJson(root / "config.json");
    } catch (...) {
    }
    throw;
  }
  data = std::move(next);
}
fs::path Store::gameDir(const Game &g) const { return root / "games" / g.id; }
void Store::migrateLegacyTf1(const std::vector<Game> &games,const std::vector<Volume> &volumes) {
  if(readOnly)return;
  auto volume=std::find_if(volumes.begin(),volumes.end(),[](const auto &v){return v.id=="TF1";});
  if(volume==volumes.end())return;
  std::map<std::string,std::string> aliases;
  auto identity=[](const std::string &id,const std::string &relative){auto text=id+"\n"+relative;return hashBytes({text.begin(),text.end()});};
  // Compatibility with early development launchers only. Normal scans and new
  // saves use TF1; no legacy namespace is retained as the active identity.
  const std::set<std::string> legacy{"RONDO-TEST","RONDO-DEMO"};
  auto base=fs::weakly_canonical(volume->root);
  for(const auto &g:games)if(g.volume=="TF1") {
    auto relative=g.path.lexically_relative(base).generic_string();
    for(auto &old:legacy)aliases[identity(old,relative)]=g.id;
  }
  auto next=data;
  for(auto &entry:next["recent_games"])if(legacy.count(entry.value("volume",""))) {
    auto old=entry.at("id").get<std::string>();auto id=identity("TF1",entry.at("relative_path").get<std::string>());
    aliases[old]=id;entry["volume"]="TF1";entry["id"]=id;
  }
  for(auto key:{"focus","resume_game"}) {
    auto found=aliases.find(next.value(key,""));if(found!=aliases.end())next[key]=found->second;
  }
  Json favorites=Json::array();std::set<std::string> seen;
  for(const auto &value:next["favorites"]) {
    auto id=value.get<std::string>();auto found=aliases.find(id);if(found!=aliases.end())id=found->second;
    if(seen.insert(id).second)favorites.push_back(id);
  }
  next["favorites"]=favorites;next["recent_games"]=cleanRecents(next["recent_games"]);
  for(auto &alias:aliases)if(next["game_options"].contains(alias.first)) {
    auto old=next["game_options"][alias.first];auto &current=next["game_options"][alias.second];
    if(!current.is_object())current=Json::object();
    for(auto &option:old.items())if(!current.contains(option.key()))current[option.key()]=option.value();
    next["game_options"].erase(alias.first);
  }
  // Enumerate names once, without reading SRAM/state contents. This also allows
  // a previously absent ROM to migrate when it returns on a later boot.
  std::set<std::string> occupied,directories;auto parent=root/"games";
  if(fs::is_directory(parent))for(auto &entry:fs::directory_iterator(parent)) {
    auto name=entry.path().filename().string();occupied.insert(name);
    if(!entry.is_symlink()&&entry.is_directory())directories.insert(name);
  }
  bool moved=false;
  for(auto &alias:aliases)if(directories.count(alias.first)&&!occupied.count(alias.second)) {
    fs::rename(parent/alias.first,parent/alias.second);directories.erase(alias.first);occupied.erase(alias.first);occupied.insert(alias.second);moved=true;
  }
  if(next!=data||moved)commit(next); // Existing atomic config write flushes the owning filesystem.
}
bool Store::integerScaling(const std::string &gameId) const {
  const auto &options = data.at("game_options");
  if (options.contains(gameId) && options.at(gameId).contains("integer_scaling"))
    return options.at(gameId).at("integer_scaling").get<bool>();
  return data.value("integer_scaling", false);
}
Locale::Locale(const fs::path &assets) {
  registry = readJson(assets / "Locales/registry.json");
  source = readJson(assets / "Locales/en.json");
  catalog = source;
}
void Locale::select(const fs::path &assets, const std::string &code) {
  current = "en";
  catalog = source;
  try {
    for (auto &l : registry.at("locales"))
      if (l.at("code") == code && l.value("direction", "ltr") == "ltr") {
        auto c = readJson(assets / "Locales" / (code + ".json"));
        if (!c.is_object())
          return;
        // Malformed values fall back per-message, rather than crashing a
        // screen.
        for (auto it = c.begin(); it != c.end(); ++it)
          if (it.value().is_string() && source.contains(it.key())) {
            auto placeholders = [](const std::string &s) {
              std::set<std::string> out;
              size_t p = 0;
              while ((p = s.find('{', p)) != s.npos) {
                auto e = s.find('}', p);
                if (e == s.npos)
                  break;
                out.insert(s.substr(p, e - p + 1));
                p = e + 1;
              }
              return out;
            };
            if (placeholders(it.value()) == placeholders(source[it.key()]))
              catalog[it.key()] = it.value();
          }
        current = code;
        return;
      }
  } catch (...) {
    current = "en";
    catalog = source;
  }
}
std::string Locale::tr(const std::string &id, const Json &args) const {
  std::string s = catalog.value(id, source.value(id, id));
  for (auto it = args.begin(); it != args.end(); ++it) {
    std::string key = "{" + it.key() + "}",
                v = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
    size_t p = 0;
    while ((p = s.find(key, p)) != s.npos) {
      s.replace(p, key.size(), v);
      p += v.size();
    }
  }
  return s;
}
Timezones::Timezones(fs::path p) : root(std::move(p)) {
  if (valid("Etc/UTC"))
    zones.push_back({"Etc/UTC", "UTC", "UTC", "UTC"});
  std::ifstream f(root / "zone.tab");
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream ss(line);
    std::string country, coords, id;
    ss >> country >> coords >> id;
    if (!valid(id))
      continue;
    auto slash = id.find('/');
    if (slash == id.npos)
      continue;
    auto city = id.substr(slash + 1);
    std::replace(city.begin(), city.end(), '_', ' ');
    zones.push_back({id, id.substr(0, slash), city, country});
  }
  std::sort(zones.begin(), zones.end(), [](auto &a, auto &b) { return a.id < b.id; });
}
bool Timezones::valid(const std::string &id) const {
  return !id.empty() && id[0] != '/' && id.find("..") == id.npos && fs::is_regular_file(root / id);
}
struct TZGuard {
  std::string previous;
  bool had = false;
  TZGuard(const std::string &id) {
    const char *p = getenv("TZ");
    had = p;
    if (p)
      previous = p;
    setenv("TZ", id.c_str(), 1);
    tzset();
  }
  ~TZGuard() {
    if (had)
      setenv("TZ", previous.c_str(), 1);
    else
      unsetenv("TZ");
    tzset();
  }
};
std::string Timezones::offset(const std::string &id, time_t now) const {
  if (!valid(id))
    return "—";
  TZGuard guard(":" + (root / id).string());
  tm time{};
  localtime_r(&now, &time);
  char buf[32];
  strftime(buf, sizeof(buf), "%z", &time);
  std::string s = buf;
  return s.size() == 5 ? "UTC" + s.substr(0, 3) + ":" + s.substr(3) : "UTC";
}
void Timezones::applyHost(const std::string &id) const {
  if (!valid(id))
    throw std::runtime_error("Time zone data unavailable");
  if (setenv("TZ", (":" + (root / id).string()).c_str(), 1))
    throw std::runtime_error("Cannot apply time zone");
  tzset();
}
Scroll scrollGeometry(double c, double v, double off, double track, double min) {
  if (c <= v || c <= 0)
    return {0, 0, false};
  double thumb = std::min(track, std::max(min, track * v / c));
  return {thumb, (track - thumb) * std::clamp(off / (c - v), 0.0, 1.0), true};
}
} // namespace rondo
