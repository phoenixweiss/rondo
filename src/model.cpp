// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "model.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
namespace rondo {
std::vector<uint8_t> readBytes(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    throw std::runtime_error("Cannot read " + p.string());
  return {std::istreambuf_iterator<char>(f), {}};
}
std::string hashBytes(const std::vector<uint8_t> &b) { return picosha2::hash256_hex_string(b); }
std::string hashFile(const fs::path &p) { return hashBytes(readBytes(p)); }
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
                                            ".md",  ".gen", ".cue", ".m3u", ".chd"};
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
          if (sibling.path().filename().string().front() != '.' && sibling.path().extension() == ".m3u") {
            std::ifstream f(sibling.path());
            std::string line;
            while (std::getline(f, line)) {
              if (!line.empty() && line.back() == '\r')
                line.pop_back();
              if (!line.empty() && line[0] != '#' &&
                  (path.parent_path() / line).lexically_normal() == path.lexically_normal())
                listed = true;
            }
          }
        if (listed)
          continue;
      }
      auto rel = path.lexically_relative(v.root);
      std::string folder = rel.begin()->string();
      std::string system = systems.count(folder) ? systems.at(folder) : folder;
      if (ext == ".nes")
        system = "NES";
      auto identity = v.id + "\n" + rel.generic_string();
      std::vector<uint8_t> bytes(identity.begin(), identity.end());
      std::string title = path.stem().string();
      if (path.parent_path() != v.root && path.parent_path().filename() != folder)
        title = path.parent_path().filename().string();
      out.push_back({hashBytes(bytes), v.id, system, title, path});
    }
  }
  std::sort(out.begin(), out.end(), [](auto &a, auto &b) {
    return std::tie(a.system, a.title, a.id) < std::tie(b.system, b.title, b.id);
  });
  return out;
}
static void validateConfig(const Json &candidate, const Json &defaults) {
  if (!candidate.is_object() || candidate.value("schema", 0) != 1)
    throw std::runtime_error("Unsupported configuration");
  for (const auto &key : {"revision", "locale", "artwork", "section", "focus", "timezone", "volume",
                          "favorites", "game_options", "replace_boot_logo", "brightness", "lid_sleep", "wake_on_lid_open", "show_battery_percent"})
    if (candidate.contains(key) && candidate[key].type() != defaults[key].type() &&
        !(candidate[key].is_number() && defaults[key].is_number()))
      throw std::runtime_error("Invalid configuration field");
}
Store::Store(fs::path p) : root(std::move(p)) {
  data = {{"schema", 1},
          {"revision", 0},
          {"locale", "en"},
          {"volume", 80},
          {"brightness", 70},
          {"lid_sleep", true},
          {"wake_on_lid_open", true},
          {"show_battery_percent", true},
          {"replace_boot_logo", true},
          {"artwork", "cover"},
          {"section", "All"},
          {"focus", ""},
          {"timezone", "Etc/UTC"},
          {"favorites", Json::array()},
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
  if (data["artwork"] != "cover" && data["artwork"] != "title" && data["artwork"] != "screenshot")
    data["artwork"] = "cover";
}
void Store::commit(Json next) {
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
