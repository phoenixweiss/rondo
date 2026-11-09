// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "json.hpp"
#include "picosha2.h"
#include <ctime>
#include <functional>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace rondo {
constexpr int GamepadPlayerCount=4;
constexpr int GamepadControllerCount=10;
namespace fs = std::filesystem;
using Json = nlohmann::json;
std::string systemName(const std::string &);
long long saveSequence(const Json &);
std::vector<uint8_t> readBytes(const fs::path &);
std::string hashBytes(const std::vector<uint8_t> &);
std::string hashFile(const fs::path &);
void syncDirectory(const fs::path &);
void atomicWrite(const fs::path &, const std::vector<uint8_t> &);
Json readJson(const fs::path &);
std::string utf8Prefix(std::string, size_t);
std::string searchFold(std::string);
struct Volume {
  std::string id;
  fs::path root;
};
struct Game {
  std::string id, volume, system, title;
  fs::path path;
  bool available = true;
};
std::vector<Game> scan(const std::vector<Volume> &);
Json recentAfterLaunch(const Json &, const Game &, const std::vector<Volume> &);
std::vector<Game> recentUnavailable(const Json &, const std::vector<Game> &);
// Config stays on TF1; category paths are resolved afresh at operation boundaries.
struct StatePaths {
  fs::path config;
  std::function<fs::path(const std::string &,bool)> resolve;
  std::function<Json()> identity;
  StatePaths(fs::path p):config(std::move(p)){}
  fs::path category(const std::string &,bool writing=false) const;
  fs::path settings(bool writing=false) const;
  Json binding() const;
  std::function<void()> guard(const std::string &,bool writing=true) const;
};
class Store {
public:
  fs::path root;
  StatePaths paths;
  Json data;
  std::string warning;
  bool readOnly = false;
  bool fresh = false;
  explicit Store(fs::path);
  explicit Store(StatePaths);
  void commit(Json);
  void validateDocument(const Json &) const;
  fs::path gameDir(const Game &,bool writing=false) const;
  void migrateLegacyTf1(const std::vector<Game> &,const std::vector<Volume> &);
  bool integerScaling(const std::string &gameId) const;
};
class Locale {
  Json source, catalog;

public:
  Json registry;
  std::string current = "en";
  explicit Locale(const fs::path &);
  void select(const fs::path &, const std::string &);
  std::string tr(const std::string &, const Json &args = Json::object()) const;
};
struct Zone {
  std::string id, region, city, country;
};
class Timezones {
public:
  fs::path root;
  std::vector<Zone> zones;
  explicit Timezones(fs::path = "/usr/share/zoneinfo");
  bool valid(const std::string &) const;
  std::string offset(const std::string &, time_t) const;
  void applyHost(const std::string &) const;
};
struct Scroll {
  double thumb, position;
  bool visible;
};
Scroll scrollGeometry(double content, double viewport, double offset, double track,
                      double minimum = 24);
} // namespace rondo
