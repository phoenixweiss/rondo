// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "json.hpp"
#include "picosha2.h"
#include <ctime>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace rondo {
namespace fs = std::filesystem;
using Json = nlohmann::json;
long long saveSequence(const Json &);
std::vector<uint8_t> readBytes(const fs::path &);
std::string hashBytes(const std::vector<uint8_t> &);
std::string hashFile(const fs::path &);
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
class Store {
public:
  fs::path root;
  Json data;
  std::string warning;
  bool readOnly = false;
  bool fresh = false;
  explicit Store(fs::path);
  void commit(Json);
  fs::path gameDir(const Game &) const;
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
