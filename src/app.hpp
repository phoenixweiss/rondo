// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "core.hpp"
#include "battery_log.hpp"
#include "clock.hpp"
#include "wifi.hpp"
#include "bluetooth.hpp"
#include "ui.hpp"
#include "system_info.hpp"
#include <functional>
#include <future>
namespace rondo {
enum class Screen {
  Library,
  Search,
  Settings,
  SettingsSection,
  InputTest,
  SystemInfo,
  LogClear,
  SystemConfirm,
  SystemProgress,
  SavingAutomatic,
  Clock,
  ClockEdit,
  Bluetooth,
  BluetoothDevices,
  BluetoothDevice,
  BluetoothForget,
  Wifi,
  WifiNetworks,
  WifiNetwork,
  WifiPassword,
  Language,
  Regions,
  Zones,
  ResumeChoice,
  StartupResume,
  Launching,
  Running,
  GameMenu,
  GameSettings,
  SaveSlots,
  LoadSlots,
  DeleteSlot,
  Overwrite,
  LoadConfirm,
  ExitConfirm,
  CoreSettings,
  PsxCoreSettings,
  PsxBiosFiles,
  PsxFirmwareError,
  PsxWarning,
  Recovery
};
struct Options {
  fs::path assets, state, core, gbCore, gbaCore, snesCore, mdCore, psxCore, psxBios, executable, script, output;
  bool psxTestHle=false;
  fs::path coreFor(const std::string &system) const {
    if(system=="NES")return core;
    if(system=="GB" || system=="GBC") {
      if(!gbCore.empty())return gbCore;
      if(!core.empty())return core.parent_path()/("gambatte_libretro"+core.extension().string());
    }
    if(system=="GBA") {
      if(!gbaCore.empty())return gbaCore;
      if(!core.empty())return core.parent_path()/("gpsp_libretro"+core.extension().string());
    }
    if(system=="SNES") {
      if(!snesCore.empty())return snesCore;
      if(!core.empty())return core.parent_path()/("mednafen_supafaust_libretro"+core.extension().string());
    }
    if(system=="MD") {
      if(!mdCore.empty())return mdCore;
      if(!core.empty())return core.parent_path()/("picodrive_libretro"+core.extension().string());
    }
    if(system=="PSX") {if(!psxCore.empty())return psxCore;if(!core.empty())return core.parent_path()/("pcsx_rearmed_libretro"+core.extension().string());}
    return {};
  }
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
  void configureArtwork();
  std::unique_ptr<DeviceControl> device;
  std::shared_ptr<Rumble> motor;
  uint16_t testButtons=0;
  std::array<int16_t,4> testAxes{};
  std::set<std::string> testExtras;
  bool testChordHeld=false,testInputArmed=false;
  bool testFromCores=false;
  uint64_t testFeedbackUntil=0;
  std::string testFeedback;
  void openInputTest(bool fromCores);
  void testInput(uint16_t,const std::array<int16_t,4> &,const std::set<std::string> &);
  void drawInputTest(const std::string &zone);
  std::optional<int> brightness;
  std::optional<bool> lastLidOpen = true;
  uint64_t lidCheckAt = 0;
  bool sleeping = false;
  bool gameInputBlocked=true;
  uint16_t gameButtons(uint16_t);
  void gameInput(uint16_t, const std::array<int16_t,4> &);
  bool psxDualshock() const;
  bool screenBlank=false, batteryWarningsLoaded=false;
  uint64_t wakeConsumeUntil=0, lastActivity=0, powerSampleAt=0, batteryChargeAt=0;
  uint64_t batteryNoticeUntil=0;
  int batteryWarningMask=0, batteryWarningPercent=-1;
  std::string autosaveError;
  double powerScroll=0;
  bool activity();
  void wakeScreen();
  void powerTick(uint64_t);
  void batteryWarning(const Telemetry &,uint64_t);
  void saveOnExit(bool skipAutomatic=false);
  void offerStartupResume();
  void rememberClosedGame();
  bool gameClosed=false;
  std::unique_ptr<BatteryLog> batteryLog;
  bool batteryLogWarning = false;
  uint64_t diagnosticSampleAt = 0;
  bool diagnosticRunner = false;
  Json frameDiagnostic=Json::object();
  uint64_t frameWindow=0,frameCount=0,frameSlow=0;
  double frameInput=0,frameUpdate=0,frameDraw=0,frameMaximum=0;
  void observeFrame(double inputMs,double updateMs,double drawMs);
  void configureDiagnostics();
  void batterySample(const std::string &event = "");
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
  Screen deleteOrigin = Screen::LoadSlots;
  int selected = 0, row = 0, slot = 0, category = 0, regionFocus = 0;
  double scroll = 0, regionScroll = 0;
  std::vector<std::string> psxBiosChoices;
  double psxBiosScroll=0;
  int settingsFocus = 0;
  double settingsScroll = 0;
  void returnToSettings();
  struct SystemInfoRow { std::string label,value,source,state,page; };
  SystemInfoProcess systemInfoJob;
  Json systemInfoData=Json::object();
  std::string systemInfoPage="root",systemInfoError;
  int systemInfoRow=0,systemInfoSourceScroll=0;
  double systemInfoScroll=0;
  bool systemInfoSource=false;
  std::vector<std::tuple<std::string,int,double>> systemInfoStack;
  std::vector<SystemInfoRow> systemInfoRows() const;
  void systemInfoRefresh();
  void systemInfoTick();
  void systemInfoAction(const std::string &);
  void systemInfoDraw();
  std::string libraryTitle() const;
  void rememberLaunch();
  SDL_Color gameBackground() const;
  int previewVolume = 80;
  void applyVolume(int);
  bool changeVolume(int);
  // Only the dedicated volume worker calls this; it must not touch SDL/App.
  std::function<Json(int)> requestVolume;
  std::future<Json> volumeSaveJob;
  std::optional<int> queuedVolume;
  uint64_t volumeSaveAt = 0;
  void volumeTick(bool drain = false);
  VolumeFeedback volumeFeedback;
  std::string section = "All", artwork = "cover", notice, region;
  std::unique_ptr<Core> core;
  Game game;
  bool stop = false, launch = false, resume = false;
  // Handled by the H700 launcher only after process teardown.
  int systemExitCode = 0;
  bool restartRequested = false;
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
  ~App();
  std::shared_ptr<ClockAdapter> clockDevice;
  ClockState clockState;
  std::future<ClockState> clockJob;
  std::function<ClockState()> clockQueued;
  std::string clockTask,clockQueuedTask,clockError;
  std::optional<bool> clockToggle;
  uint64_t clockPollAt=0,clockDeadline=0;
  long long clockLastSync=0;
  ClockFields clockFields;
  bool clockLocalUpdated=false;
  bool clockManual=false,clockJustUpdated=false,clockNetworkWasOnline=false,clockReadback=false;
  bool clockBusy()const{return bool(clockQueued)||(clockJob.valid()&&clockTask!="poll");}
  void clockRun(const std::string &,std::function<ClockState()>);
  void clockTick();
  void clockAction(const std::string &);
  void clockDraw();
  void clockSleep();
  std::string clockPhase()const;
  std::shared_ptr<WifiAdapter> wifiDevice;
  WifiState wifi;
  WifiNetwork wifiSelected;
  std::future<WifiState> wifiJob;
  std::string wifiTask, wifiPassword, wifiQueuedTask;
  std::function<WifiState()> wifiQueued;
  bool wifiBusy() const {return bool(wifiQueued) || (wifiJob.valid() && wifiTask!="poll");}
  std::optional<bool> wifiToggle;
  uint64_t wifiPollAt=0;
  int wifiKeyRow=0,wifiKeyColumn=0,wifiListFocus=0;
  bool wifiUpper=false,wifiSymbols=false,wifiReadback=false;
  void wifiRun(const std::string &,std::function<WifiState()>);
  void wifiTick();
  void wifiAction(const std::string &);
  void wifiDraw();
  void wifiConnect();
  void wifiText(const std::string &);
  std::vector<std::vector<std::string>> wifiKeys() const;

  std::shared_ptr<BluetoothAdapter> btDevice;
  BluetoothState bt;
  BluetoothDevice btSelected;
  std::future<BluetoothState> btJob;
  std::string btTask, btError, btQueuedTask;
  std::function<BluetoothState()> btQueued;
  uint64_t btStarted=0, btDiscoveryUntil=0;
  Json btDiagnostic=Json::object();
  std::optional<bool> btToggle;
  uint64_t btPollAt = 0;
  int btFocus = 0;
  double btScroll = 0;
  bool btBusy() const { return bool(btQueued) || (btJob.valid() && btTask!="poll"); }
  void btRun(const std::string &, std::function<BluetoothState()>);
  void btTick();
  void btAction(const std::string &);
  void btDraw();
  void btSleep();
  void btResume();

  std::string searchQuery, searchDraft;
  std::vector<std::string> searchNames;
  int searchRow=0, searchColumn=0;
  bool searchRussian=false;
  void searchText(const std::string &);
  void searchAction(const std::string &);
  void searchDraw();
  std::vector<std::vector<std::string>> searchKeys() const;
  std::vector<int> visible() const;
  std::vector<std::string> regions() const;
  std::vector<Zone> locations() const;
  void action(const std::string &);
  void draw();
  SDL_Rect gameViewport() const;
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
