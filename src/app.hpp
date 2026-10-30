// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "core.hpp"
#include "preferences.hpp"
#include "battery_log.hpp"
#include "clock.hpp"
#include "wifi.hpp"
#include "bluetooth.hpp"
#include "ui.hpp"
#include "system_info.hpp"
#include "screenshots.hpp"
#include "game_statistics.hpp"
#include "backup.hpp"
#include "progress.hpp"
#include <functional>
#include <future>
namespace rondo {
enum class GameMenuItem { Resume, Save, Load, Screenshot, Settings, Info, Exit, Count };
enum class Screen {
  Systems, LibraryPreferences, LibraryMenu,
  ScreenshotSettings, ScreenshotCapture,
  Themes, CoreCatalog, MappingCores, Mapping, MappingReset, Wizard, WizardExit,
  Library,
  Search, SearchResults, GameDetails,
  Settings,
  SettingsSection,
  InputTest,
  SystemInfo,
  ProgressBusy, ProgressSources, ProgressConfirm, ProgressDone,
  Backups, BackupDetails, BackupGames, BackupDelete, BackupConfirm, BackupBusy, BackupRestart,
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
  ExitConfirm, DiscardConfirm,
  CoreSettings,
  PsxCoreSettings,
  PsxBiosFiles,
  PsxFirmwareError,
  PsxWarning,
  Recovery
};
// A route snapshot stores navigation only; confirmed preferences and service
// results remain authoritative when a parent is restored.
struct NavigationFrame {
  Screen route=Screen::Library;
  std::string collection,query,draft,selectedGame,region,wifiIdentity,btIdentity;
  std::string infoPage;
  int regionFocus=0,wifiFocus=0,wifiKeyRow=0,wifiKeyColumn=0,btFocus=0,systemInfoRow=0;
  double btScroll=0,systemInfoScroll=0;
  int selected=0,row=0,category=0,system=0,quick=0,settings=0,keyRow=0,keyColumn=0;
  double scroll=0,regionScroll=0,settingsScroll=0,featureScroll=0,interfaceScroll=0;
  bool actions=false,russian=false;
};
struct Options {
  fs::path assets, state, core, gbCore, gbaCore, snesCore, mdCore, psxCore, psxBios, executable, script, output;
  bool psxTestHle=false, shellReload=false;
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
  bool headless = false, setup = false;
};
class App {
public:
  std::vector<NavigationFrame> navigation;
  std::optional<NavigationFrame> gameOrigin;
  size_t gameOriginDepth=0;
  uint64_t navigationRevision=0;
  bool navigationActions=false;
  int navigationFocus=0;
  NavigationFrame navigationFrame() const;
  void navigationRestore(const NavigationFrame &);
  void navigationPush(Screen);
  void navigationBack();
  void navigationGameReturn();
  void navigationSettings();
  void openSystems();
  bool explicitSystemsRoot=false;
  bool libraryActionsAvailable() const;
  bool navigationAction(const std::string &);
  void navigationDraw();
  void actionImpl(const std::string &);
  struct NavigationAction {std::string id,label,icon;};
  std::vector<NavigationAction> navigationItems() const;
  Options options;
  Store store;
  mutable std::optional<std::vector<CoreEntry>> coreCatalogCache;
  std::vector<CoreEntry> installedCores() const;
  bool coreEnabled(const std::string &) const;
  std::string themeName(const std::string &) const;
  void openThemes();
  void preferenceAction(const std::string &);
  void preferenceDraw(const std::string &);
  double featureScroll=0;
  bool systemsQuick=false, gamesQuick=false;
  int systemFocus=0,quickFocus=0,librarySettingsReturn=5;
  Json libraryDraft;
  double interfaceScroll=0;
  std::map<std::string,std::string> collectionFocus;
  std::vector<std::string> librarySystems() const;
  void openSearch(bool global=false);
  void libraryOpen(const std::string &);
  void libraryRemember();
  void libraryRestore();
  void libraryConfigure(Screen origin);
  void libraryPreferenceAction(const std::string &);
  void libraryPreferenceDraw(const std::string &);
  std::string libraryValue(const std::string &,const Json &) const;
  void libraryCycle(Json &,int,int);
  bool libraryAction(const std::string &);
  void systemsAction(const std::string &);
  void systemsDraw(const std::string &);
  void gameTilesDraw(const std::string &);
  void gamePositionDraw(const Game &,size_t);
  void libraryQuickDraw();
  std::string mappingCore;
  int mappingCoreRow=0,mappingReturnRow=0;
  bool mappingTest=false;
  bool wizardActive=false,wizardRepeat=false,wizardExternal=false,wizardPending=false;
  int wizardStep=0,wizardRow=0,wizardExternalRow=0;
  Screen wizardChildRoot=Screen::Library;
  bool wizardPendingNetworks=false;
  Json wizardChildBaseline;
  int wizardNextRow() const;
  void wizardOpenChild(Screen);
  void wizardReturn();
  void wizardMaybeReturn();
  Json wizardDraft;
  SDL_AudioDeviceID soundTest=0;
  uint64_t soundTestUntil=0;
  void startWizard(bool repeat=false);
  void wizardAction(const std::string &);
  void wizardDraw(const std::string &);
  void wizardRestore();
  void wizardEnter(int);
  void wizardAdvance(bool);
  void wizardTick();
  void wizardTestSound();
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
  ScreenshotWriter screenshotWriter;
  ScreenshotChord screenshotKeys,interfaceScreenshotKeys;
  std::string screenshotMessage;
  uint64_t screenshotMessageUntil=0;
  bool screenshotRecordingArmed=false;
  uint16_t screenshotRecording=0;
  void takeScreenshot();
  void screenshotTick();
  void playScreenshotSound();
  void screenshotDraw();
  void screenshotAction(const std::string &);
  void screenshotSettingsDraw(const std::string &);
  void screenshotRecord(uint16_t);
  uint16_t gameButtons(uint16_t);
  void gameInput(uint16_t, const std::array<int16_t,4> &);
  void sampleInput();
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
  std::future<Json> backupJob;
  std::shared_ptr<BackupProgress> backupProgress;
  std::vector<fs::path> backupList;
  fs::path backupSelected;
  Json backupSummary=Json::object(), backupGameList=Json::array();
  std::map<std::string,Json> backupMetadata;
  double backupScroll=0;
  bool backupDeleting=false, backupRestoring=false, restoreInputFence=false;
  std::string backupError;
  std::future<Json> progressJob;
  std::shared_ptr<BackupProgress> progressStatus;
  Json progressSources=Json::array(),progressSelected=Json::object();
  bool progressAutomatic=false,progressCopying=false;
  std::string progressError,progressFingerprintValue,progressLegacyRom,progressLegacyCore;
  int progressFocus=0;double progressScroll=0;
  void progressOpen(bool automatic=false);
  void progressCopy();
  void progressTick();
  void progressAction(const std::string &);
  void progressDraw(const std::string &);
  void backupOpen();
  void backupStart(bool,bool deleting=false);
  void backupTick();
  void backupAction(const std::string &);
  void backupDraw(const std::string &);
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
  void rememberLaunch(const std::string &fingerprint={});
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
  std::unique_ptr<GameStatistics> statistics;
  Json gameInfoStatistics=Json::object();
  void statisticsTick(bool flush=false);
  void startStatistics();
  void openGameInfo();
  void prepareGame(const Game &,bool checkProgress=true);

  Game game;
  bool stop = false, launch = false, resume = false;
  // Handled by the H700 launcher only after process teardown.
  int systemExitCode = 0;
  bool restartRequested = false;
  Json trace = Json::array();
  std::future<std::pair<std::optional<bool>, std::string>> bootJob;
  std::optional<bool> bootAcknowledged;
  bool bootTarget = true;
  std::string bootRequestedTheme;
  bool infoOpen = false;
  int infoScroll = 0;
  std::string bootLastError, scalingLastError, bootNotice;
  std::string infoKey() const;
  uint64_t bootMessageUntil = 0, noticeUntil = 0;
  bool bootPending() const { return bootJob.valid(); }
  void requestBootLogo(bool, bool force=false);
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
  void btResume(bool applyDisabled=false);

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
