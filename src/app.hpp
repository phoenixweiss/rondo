// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "core.hpp"
#include "ui.hpp"
#include <functional>
#include <future>
namespace rondo {
enum class Screen {
  Library,
  Settings,
  Language,
  Regions,
  Zones,
  ResumeChoice,
  Launching,
  Running,
  GameMenu,
  GameSettings,
  SaveSlots,
  LoadSlots,
  Overwrite,
  LoadConfirm,
  ExitConfirm,
  Recovery
};
struct Options {
  fs::path assets, state, core, executable, script, output;
  std::vector<Volume> volumes;
  bool headless = false;
};
class App {
public:
  Options options;
  Store store;
  Locale locale;
  Timezones zones;
  Json timezoneNames;
  std::string countryLabel(const std::string &) const;
  std::string zoneLabel(const std::string &) const;
  std::string timezoneSortKey(const std::string &) const;
  void openTimezones();
  std::unique_ptr<UI> ui;
  std::unique_ptr<DeviceControl> device;
  std::optional<int> brightness;
  std::optional<bool> lastLidOpen = true;
  uint64_t lidCheckAt = 0;
  bool sleeping = false;
  void sleep();
  struct SlotView {
    bool present=false, compatible=false;
    long long utc=0;
    int width=0,height=0;
    std::vector<uint32_t> pixels;
  };
  std::map<int,std::pair<std::string,SlotView>> slotCache;
  const SlotView &slotView(int);
  std::string slotDate(long long) const;
  std::vector<Game> games;
  std::vector<std::string> sections;
  Screen screen = Screen::Library;
  int selected = 0, row = 0, slot = 0, category = 0, regionFocus = 0;
  double scroll = 0, regionScroll = 0;
  int previewVolume = 80;
  VolumeFeedback volumeFeedback;
  std::string section = "All", artwork = "cover", notice, region;
  std::unique_ptr<Core> core;
  Game game;
  bool stop = false, launch = false, resume = false;
  Json trace = Json::array();
  std::future<std::pair<std::optional<bool>, std::string>> bootJob;
  std::optional<bool> bootAcknowledged;
  bool bootTarget = true;
  bool infoOpen = false;
  int infoScroll = 0;
  std::string bootLastError, scalingLastError, bootNotice;
  std::string infoKey() const;
  uint64_t bootMessageUntil = 0, noticeUntil = 0;
  bool bootPending() const { return bootJob.valid(); }
  void requestBootLogo(bool);
  std::function<bool(Json)> requestSettings;
  App(Options, bool runner = false, const Game &entry = {});
  std::vector<int> visible() const;
  std::vector<std::string> regions() const;
  std::vector<Zone> locations() const;
  void action(const std::string &);
  void draw();
  void ticks(unsigned);
  void capture(const std::string &);
  void persist(Json);
  void record(const std::string &);
  std::string screenName() const;
  bool usableSlot(int) const;
  int latestResumeSlot() const;
  void applyZone(const std::string &);
};
} // namespace rondo
