#include "app.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace rondo;
struct FakeDevice : DeviceControl {
  int level=60,sleeps=0;bool open=true,fail=false,wakeAvailable=true;
  std::vector<std::pair<bool,bool>> policies;
  std::function<void()> before;
  std::optional<int> brightness() override { return level; }
  void brightness(int n) override { level=n; }
  bool canSleep() const override { return true; }
  bool canWakeOnLid() const override { return wakeAvailable; }
  std::optional<bool> lidOpen() override { return open; }
  void sleep(bool wake, bool close) override { policies.emplace_back(wake,close);++sleeps;if(before)before();if(fail)throw std::runtime_error("Injected suspend failure");open=true; }
};
int main(int argc,char **argv) {
  assert(argc==5);Options o;o.assets=argv[1];o.state=argv[2];o.core=argv[3];o.headless=true;o.output=o.state/"renders";
  auto root=o.state;
  PowerInputState key;
  assert(key.event(1,116,0).empty());
  assert(key.event(1,116,1).empty());assert(key.event(1,116,2).empty());
  assert(key.event(1,116,0)=="SLEEP");assert(key.event(1,116,0).empty());
  key.event(1,116,1);key.event(0,3,0);assert(key.event(1,116,0).empty());
  {
    App a(o);auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);a.brightness=60;
    auto group=[&](int index) { a.returnToSettings();a.settingsFocus=index;a.row=index;a.action("A"); };
    a.action("Y");group(1);a.action("RIGHT");assert(d->level==65 && a.store.data["brightness"]==65);a.capture("brightness-inline");
    a.action("DOWN");assert(a.row==0); // Display contains brightness only.
    a.requestSettings=[](Json){return false;};a.action("LEFT");assert(d->level==65 && a.brightness==65 && a.store.data["brightness"]==65);a.requestSettings={};
    group(5);a.action("LEFT");assert(a.row==0 && !a.store.data["show_battery_percent"].get<bool>() && d->level==65);
    a.capture("battery-percent-off");
    a.requestSettings=[](Json){return false;};a.action("RIGHT");assert(!a.store.data["show_battery_percent"].get<bool>() && d->level==65);
    a.requestSettings={};a.action("RIGHT");assert(a.store.data["show_battery_percent"].get<bool>());
    group(2);a.action("RIGHT");assert(a.screen==Screen::SettingsSection && a.store.data["volume"]==85);
    a.action("A");assert(a.screen==Screen::SettingsSection);a.capture("sound-inline");a.action("LEFT");assert(a.store.data["volume"]==80);
    group(5);a.action("DOWN");a.action("LEFT");assert(a.store.data["lid_sleep"]==false);
    d->open=false;a.lidCheckAt=0;a.ticks(0);assert(d->sleeps==0);
    a.action("RIGHT");d->open=true;a.lidCheckAt=0;a.ticks(0);d->open=false;a.lidCheckAt=0;a.ticks(0);assert(d->sleeps==1);
    a.action("SLEEP");assert(d->sleeps==2);
    // A supervised shell without SDL must not independently suspend its child.
    a.ui.reset();d->open=false;a.lastLidOpen=true;a.lidCheckAt=0;a.ticks(0);assert(d->sleeps==2);
  }
  {
    o.state=root/"settings-navigation";App a(o);
    const auto original=a.store.data;
    for(const auto *language:{"en","ru"}) {
      a.locale.select(o.assets,language);a.screen=Screen::Library;a.settingsFocus=0;a.settingsScroll=0;
      a.action("Y");a.action("UP");a.action("LEFT");a.action("L1");a.action("R1");
      assert(a.screen==Screen::Settings && a.row==0);
      a.capture(std::string("settings-top-")+language);assert(a.settingsScroll==0);
      a.ui->inputHints(true);a.capture(std::string("settings-keyboard-")+language);a.ui->inputHints(false);
      a.action("A");assert(a.screen==Screen::SettingsSection && a.category==0);
      a.action("A");assert(a.screen==Screen::Language);a.action("B");assert(a.screen==Screen::SettingsSection);
      a.action("B");assert(a.screen==Screen::Settings && a.row==0);
      for(int i=0;i<6;++i)a.action("DOWN");
      assert(a.row==6);a.capture(std::string("settings-bottom-")+language);assert(a.settingsScroll>0);
      auto offset=a.settingsScroll;a.action("A");assert(a.category==4 && a.row==0);
      a.action("B");assert(a.row==6 && a.settingsScroll==offset);
      a.action("B");assert(a.screen==Screen::Library);a.action("Y");assert(a.row==6 && a.settingsScroll==offset);
      for(int i=0;i<3;++i) { a.action("UP"); }
      a.action("A");assert(a.screen==Screen::Clock);
      a.capture(std::string("settings-clock-")+language);a.action("B");assert(a.screen==Screen::Settings && a.row==3);
      a.action("DOWN");a.action("A");assert(a.category==5 && a.screen==Screen::SettingsSection);
      a.action("A");assert(a.screen==Screen::Wifi);a.action("B");assert(a.screen==Screen::SettingsSection && a.category==5 && a.row==0);
      a.action("B");assert(a.row==4 && a.screen==Screen::Settings);
      assert(a.store.data==original); // Navigation never changes preferences.
    }
  }
  {
    for(const auto *language:{"en","ru"}) {
      for(int target:{7,8}) {
        o.state=root/(std::string("system-power-")+language+std::to_string(target));App a(o);
        a.locale.select(o.assets,language);a.settingsFocus=0;a.screen=Screen::Settings;
        a.capture(std::string("settings-icons-top-")+language);
        for(int i=0;i<target;++i)a.action("DOWN");
        assert(a.row==target && a.infoKey()=="system_power");
        a.capture(std::string("power-menu-")+language+std::to_string(target));
        const auto before=a.store.data;
        a.action("LEFT");a.action("RIGHT");assert(!a.stop && a.store.data==before);
        a.action("X");assert(a.infoOpen);a.action("B");assert(!a.infoOpen);
        a.action("A");assert(a.screen==Screen::SystemConfirm && a.row==0);
        a.capture(std::string("power-confirm-")+language+std::to_string(target));
        a.action("A");assert(a.screen==Screen::Settings && a.row==target && !a.stop);
        a.action("A");a.action("DOWN");a.action("B");assert(!a.stop && a.row==target);
        a.action("A");a.action("DOWN");a.action("A");
        assert(a.stop && a.systemExitCode==(target==8?64:65) && a.store.data==before);
      }
    }
  }
  {
    o.state=root/"library-search";App a(o);a.games.clear();
    for(int i=0;i<204;++i)a.games.push_back({std::to_string(i),"test","NES","Game "+std::to_string(i),{}});
    a.games.push_back({"contra","test","NES","Contra",{}});
    a.games.push_back({"duck","test","NES","Чёрный Плащ",{}});
    a.games.push_back({"world","test","SNES","Super Mario World",{}});
    a.sections={"All","Favorites","NES","SNES"};
    a.selected=87;const auto original=a.store.data;
    a.action("MENU");assert(a.screen==Screen::Search);a.searchText("CONTRA");
    a.ui->inputHints(true);a.capture("search-physical-en");a.action("B");
    assert(a.screen==Screen::Library && a.selected==87 && a.visible().size()==207);
    a.action("MENU");assert(a.searchDraft.empty());a.searchText("CONTRA");a.action("SEARCH_SUBMIT");
    assert(a.visible().size()==1 && a.games[a.visible()[0]].id=="contra");a.capture("search-found-en");
    a.action("B");assert(a.visible().size()==207);
    a.locale.select(o.assets,"ru");a.ui->inputHints(false);a.action("MENU");
    a.searchText("ЧЁРНЫЙ ПЛАЩ");a.capture("search-physical-ru");a.action("SEARCH_SUBMIT");
    assert(a.visible().size()==1 && a.games[a.visible()[0]].id=="duck");a.capture("search-found-ru");
    a.action("MENU");a.action("SEARCH_DELETE");assert(a.searchDraft=="чёрный пла");
    a.action("B");assert(a.visible().size()==1);a.action("B");
    a.action("MENU");a.action("Y");a.capture("search-keyboard-ru");
    a.action("DOWN");a.action("A");assert(a.searchDraft=="й");a.action("X");assert(a.searchDraft.empty());
    a.searchText(std::string(120,'a'));assert(a.searchDraft.size()==96);a.action("SEARCH_SUBMIT");
    assert(a.visible().empty());a.capture("search-empty-ru");a.action("B");
    a.section="NES";a.action("MENU");a.searchText("mario");a.action("SEARCH_SUBMIT");assert(a.visible().empty());
    a.action("B");a.section="Favorites";a.store.data["favorites"]={"contra"};
    a.action("MENU");a.searchText("tra");a.action("SEARCH_SUBMIT");assert(a.visible().size()==1);
    a.action("X");assert(a.visible().empty());a.action("B");
    a.store.data=original;assert(a.store.data==original);
    // Search is session-only; a fresh app opens the unfiltered library.
    App restarted(o);assert(restarted.searchQuery.empty());
  }
  {
    o.state=root/"lid-options";App a(o);auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);
    assert(a.store.data["lid_sleep"]==true && a.store.data["wake_on_lid_open"]==true);
    a.action("Y");for(int i=0;i<5;++i)a.action("DOWN");a.action("A");
    a.action("DOWN");
    assert(a.row==1 && a.infoKey()=="lid"); // No wake-on-open row, even with a capable adapter.
    a.action("LEFT");assert(a.store.data["wake_on_lid_open"]==true && a.store.data["lid_sleep"]==false);
    a.requestSettings=[](Json){return false;};a.action("RIGHT");assert(a.store.data["lid_sleep"]==false);a.requestSettings={};a.action("RIGHT");a.notice.clear();
    for(const auto *language:{"en","ru"}) { a.locale.select(o.assets,language);a.capture(std::string("lid-options-")+language);a.action("X");a.capture(std::string("lid-sleep-info-")+language);a.action("B"); }
    for(bool close:{false,true})for(bool wake:{false,true}) {
      auto next=a.store.data;next["lid_sleep"]=close;next["wake_on_lid_open"]=wake;a.persist(next);
      int before=d->sleeps;d->open=false;a.lastLidOpen=true;a.lidCheckAt=0;a.ticks(0);
      assert(d->sleeps==before+int(close));
      a.action("SLEEP");assert(d->policies.back()==std::make_pair(wake,close));
    }
    Store restart(o.state);assert(restart.data["wake_on_lid_open"]==true && restart.data["lid_sleep"]==true);
    auto bad=Json{{"schema",1},{"wake_on_lid_open","true"}}.dump();
    atomicWrite(root/"invalid-lid/config.json",{bad.begin(),bad.end()});
    Store invalid(root/"invalid-lid");assert(!invalid.warning.empty() && invalid.data["wake_on_lid_open"].is_boolean());
    d->wakeAvailable=false;a.row=1;a.action("DOWN");assert(a.row==1 && a.infoKey()=="lid" && a.store.data["wake_on_lid_open"]==true);
    a.capture("lid-adapter-unavailable");a.action("SLEEP");assert(d->policies.back()==std::make_pair(false,true));
  }
  {
    o.state=root/"runner";Game game{"device-game","TEST","NES","Fixture",argv[4]};App a(o,true,game);
    auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);
    a.ticks(90);auto hash=a.core->stateHash();auto frames=a.core->frames;
    Telemetry shown;shown.capacity=100;a.ui->battery(shown,true);
    a.draw();assert(a.ui->batteryDisplay["header_visible"]==false && a.ui->batteryDisplay["displayed_percent"].is_null());
    d->before=[&]{assert(a.core->paused && a.core->buttons==0);};
    a.core->buttons=1;a.action("SLEEP");assert(d->sleeps==1 && !a.core->paused && a.screen==Screen::Running);
    assert(a.core->frames==frames && a.core->stateHash()==hash);
    a.action("MENU");a.row=3;a.action("X");assert(!a.infoOpen); // game menu X has no hidden mutation.
    a.draw();assert(a.ui->batteryDisplay["header_visible"]==true && a.ui->batteryDisplay["screen"]=="game_menu");
    a.action("SLEEP");assert(a.core->paused && a.row==3 && a.screen==Screen::GameMenu);
    a.action("B");d->fail=true;a.action("SLEEP");assert(a.core->paused && a.screen==Screen::GameMenu && !a.notice.empty());
    d->fail=false;
    auto dir=a.store.gameDir(game);fs::rename(dir,dir.string()+".saved");std::ofstream(dir)<<"blocked";
    int count=d->sleeps;a.action("SLEEP");assert(d->sleeps==count && a.core->paused); // SRAM failure aborts sleep.
    fs::remove(dir);fs::rename(dir.string()+".saved",dir);
    a.core->save(0);a.screen=Screen::LoadSlots;a.row=0;a.capture("slot-preview");auto meta=readJson(dir/"slot-0.json");assert(meta.contains("preview"));
    auto &view=a.slotView(0);assert(view.compatible && view.pixels==a.core->frame && view.width==a.core->width && view.utc>0);
    meta.erase("preview");auto text=meta.dump();atomicWrite(dir/"slot-1.json",{text.begin(),text.end()});
    auto &old=a.slotView(1);assert(old.compatible && old.pixels.empty() && old.utc==view.utc);
    meta["core"]="wrong-core";text=meta.dump();atomicWrite(dir/"slot-2.json",{text.begin(),text.end()});assert(!a.slotView(2).compatible);a.row=1;a.capture("slot-legacy");a.row=2;a.capture("slot-incompatible");a.action("X");assert(a.infoOpen);a.capture("slots-info");
    // Updating time backwards must not make Continue choose the older slot.
    auto legacy=readJson(dir/"slot-0.json");legacy.erase("save_sequence");legacy["utc"]=static_cast<long long>(time(nullptr))+86400;
    auto encoded=legacy.dump();atomicWrite(dir/"slot-0.json",{encoded.begin(),encoded.end()});
    a.core->save(1);auto recent=readJson(dir/"slot-1.json");
    assert(recent["utc"]<legacy["utc"] && saveSequence(recent)>0 && a.latestResumeSlot()==1);
    a.core->save(0);assert(a.latestResumeSlot()==0 && saveSequence(readJson(dir/"slot-0.json"))>saveSequence(recent));
    // Deletion is confirmed, targets only one slot and preserves SRAM / neighbors.
    a.screen=Screen::LoadSlots;a.row=2;a.notice.clear();a.infoOpen=false;
    auto kept0=readBytes(dir/"slot-0.json"),kept1=readBytes(dir/"slot-1.json");
    std::ofstream(dir/"slot-2.json.previous")<<"old backup";
    auto sramBefore=readBytes(dir/"sram.bin");
    auto target=readBytes(dir/"slot-2.json");
    a.action("Y");assert(a.screen==Screen::DeleteSlot && a.row==0);a.capture("delete-slot-confirm-en");
    a.locale.select(o.assets,"ru");a.capture("delete-slot-confirm-ru");
    a.action("B");assert(a.screen==Screen::LoadSlots && a.row==2 && readBytes(dir/"slot-2.json")==target);
    fs::remove(dir/"slot-2.json.previous");fs::create_directories(dir/"slot-2.json.previous");
    std::ofstream(dir/"slot-2.json.previous/block")<<"failure fixture";
    a.action("Y");a.action("DOWN");a.action("A");assert(a.screen==Screen::DeleteSlot && readBytes(dir/"slot-2.json")==target);
    fs::remove_all(dir/"slot-2.json.previous");a.action("A");
    assert(a.screen==Screen::LoadSlots && a.row==2 && !fs::exists(dir/"slot-2.json") && !a.slotView(2).present);
    assert(readBytes(dir/"slot-0.json")==kept0 && readBytes(dir/"slot-1.json")==kept1 && readBytes(dir/"sram.bin")==sramBefore);
    a.capture("deleted-slot-ru");a.action("Y");assert(a.screen==Screen::LoadSlots);
    bool invalid=false;try{a.core->deleteSlot(-1);}catch(...){invalid=true;}assert(invalid);
    Store reload(o.state);assert(reload.data.value("lid_sleep",true));
  }
  {
    o.state=root/"volume-write-failure";Game game{"volume-failure","TEST","NES","Fixture",argv[4]};App a(o,true,game);
    auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);
    a.requestSettings=[](Json){throw std::runtime_error("Read-only file system");return false;};
    a.action("SLEEP");assert(d->sleeps==1 && !a.core->paused);
    a.action("VOLUME_DOWN");assert(a.core->volume==75 && a.previewVolume==75 && a.store.data["volume"]==80 && !a.notice.empty());
    a.action("VOLUME_DOWN");assert(a.core->volume==70 && a.volumeFeedback.confirmed==70);
    a.action("VOLUME_UP");assert(a.core->volume==75 && a.previewVolume==75);
    a.screen=Screen::SettingsSection;a.category=2;a.row=0;
    a.action("LEFT");assert(a.core->volume==70 && a.previewVolume==70 && !a.notice.empty());
    a.action("RIGHT");assert(a.core->volume==75 && a.previewVolume==75);
    for(int i=0;i<25;++i)a.action("VOLUME_DOWN");assert(a.core->volume==0);
    for(int i=0;i<25;++i)a.action("VOLUME_UP");assert(a.core->volume==100);
    Store untouched(o.state);assert(untouched.data["volume"]==80);
    a.requestSettings={};a.action("VOLUME_DOWN");assert(a.core->volume==95 && a.store.data["volume"]==95 && a.notice.empty());
    Store restart(o.state);assert(restart.data["volume"]==95);
  }
  {
    o.state=root/"debug-settings";App a(o);
    assert(!a.store.data.value("debug_enabled",true) && !a.batteryLog && !fs::exists(o.state/"Logs"));
    a.action("Y");for(int i=0;i<6;++i)a.action("DOWN");a.action("A");assert(a.category==4);a.action("DOWN");
    a.action("RIGHT");assert(a.store.data["debug_level"]=="events" && !a.batteryLog);
    a.action("UP");a.requestSettings=[](Json){return false;};a.action("RIGHT");assert(!a.batteryLog && !a.store.data["debug_enabled"].get<bool>());
    a.requestSettings={};a.action("RIGHT");assert(a.batteryLog && fs::exists(o.state/"Logs/shell.jsonl"));
    for(const auto *language:{"en","ru"}) {
      a.locale.select(o.assets,language);a.capture(std::string("debug-on-")+language);
      a.action("X");a.capture(std::string("debug-info-")+language);a.action("B");
    }
    a.action("DOWN");a.action("RIGHT");assert(a.store.data["debug_level"]=="detailed");
    a.action("LEFT");a.action("LEFT");assert(a.store.data["debug_level"]=="errors");
    auto path=o.state/"Logs/shell.jsonl";auto size=fs::file_size(path);
    a.record("safe-event");a.batterySample();assert(fs::file_size(path)==size);
    a.batterySample("operation_failed");assert(fs::file_size(path)>size);
    a.action("DOWN");a.action("A");assert(a.screen==Screen::LogClear);
    a.capture("debug-clear-confirm");a.action("B");assert(fs::exists(path));
    std::ofstream(o.state/"keep-me")<<"user data";
    a.action("A");a.action("A");assert(!fs::exists(path) && fs::exists(o.state/"keep-me"));
    a.action("UP");a.action("UP");a.action("LEFT");assert(!a.batteryLog);
    a.capture("debug-off-ru");
    Store restart(o.state);assert(!restart.data["debug_enabled"].get<bool>() && restart.data["debug_level"]=="errors");
    Json bad=restart.data;bad["debug_level"]="unknown";
    auto text=bad.dump();atomicWrite(root/"debug-invalid/config.json",{text.begin(),text.end()});
    Store invalid(root/"debug-invalid");assert(!invalid.warning.empty());
  }
  {
    o.state=root/"battery-owner";
    auto log=o.state/"Battery";
    {
      App a(o);assert(!a.batteryLog); // Headless checks never enable hardware logging.
      a.batteryLog=std::make_unique<BatteryLog>(log,"shell",root/"fake-sys",root/"fake-proc");
      a.batterySample("start");
      auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);
      a.sleep();d->fail=true;a.sleep();
      auto size=fs::file_size(log/"shell.jsonl");
      a.ui.reset();a.batterySample();assert(fs::file_size(log/"shell.jsonl")==size);
    }
    std::ifstream input(log/"shell.jsonl");std::string line;std::vector<std::string> events;
    while(std::getline(input,line)) { auto event=Json::parse(line).at("event").get<std::string>();if(event.find("action:")!=0)events.push_back(event); }
    assert((events==std::vector<std::string>{"start","sleep_before","sleep_after","sleep_before","sleep_failed","stop"}));
  }
  {
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);UI hidden(o.assets,true);
    Game game{"audio-wake","TEST","NES","Fixture",argv[4]};Core c(argv[3],game,root/"audio-wake",false);
    for(int i=0;i<60;++i)c.tick();
    auto hash=c.stateHash();auto audio=c.audioFrames;
    c.closeAudio();c.openAudio();assert(c.stateHash()==hash);
    c.tick();assert(c.audioFrames>audio);
  }
  {
    // SDL virtual devices test the actual controller API without host hardware.
    UI ui(o.assets,true);
    int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);assert(index>=0);
    auto *joystick=SDL_JoystickOpen(index);assert(joystick);
    assert(ui.controllerMappingsLoaded>=1);
    // The shipped Linux Bluetooth profile must be recognized by the real SDL
    // mapping API even when this test runs on the native macOS renderer.
    auto *micro=SDL_GameControllerMappingForGUID(SDL_JoystickGetGUIDFromString("05000000c82d00002090000000010000"));
    if(std::string(SDL_GetPlatform())=="Linux") {assert(micro && std::string(micro).find("a:b1,")!=std::string::npos);}
    SDL_free(micro);
    char guid[64];SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick),guid,sizeof(guid));
    std::string mapping=std::string(guid)+",Rondo test controller,a:b0,b:b1,back:b4,start:b6,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,";
    assert(SDL_GameControllerAddMapping(mapping.c_str())>=0);
    SDL_PumpEvents();ui.refreshControllers();assert(ui.controllers.size()==1);
    SDL_JoystickSetVirtualButton(joystick,0,1);SDL_JoystickSetVirtualButton(joystick,4,1);SDL_GameControllerUpdate();
    SDL_Event observed{};observed.type=SDL_JOYBUTTONDOWN;observed.jbutton.which=SDL_JoystickInstanceID(joystick);ui.controllerEvent(observed);
    observed.type=SDL_CONTROLLERBUTTONDOWN;observed.cbutton.which=SDL_JoystickInstanceID(joystick);ui.controllerEvent(observed);
    ui.inputDiagnosticAt=0;auto passport=ui.controllerDiagnostics();
    assert(passport["devices"].size()==1 && passport["devices"][0]["mapped"]==true);
    assert(passport["devices"][0]["activity"]["raw_buttons"]==1 && passport["devices"][0]["activity"]["mapped_buttons"]==1);
    assert(passport["devices"][0]["bindings"]["a"]=="button:0");
    assert(passport.dump().find("Rondo test controller")==std::string::npos);
    auto mask=ui.controllerButtons();assert((mask&(1<<RETRO_DEVICE_ID_JOYPAD_A)) && (mask&(1<<RETRO_DEVICE_ID_JOYPAD_SELECT)));
    SDL_JoystickSetVirtualAxis(joystick,1,20000);SDL_GameControllerUpdate();
    assert(ui.controllerButtons()&(1<<RETRO_DEVICE_ID_JOYPAD_DOWN));assert(ui.controllerNavigation()==std::vector<std::string>{"DOWN"});
    assert(ui.controllerNavigation().empty());
    SDL_JoystickSetVirtualAxis(joystick,1,4000);SDL_GameControllerUpdate();assert(ui.controllerNavigation().empty());
    SDL_JoystickClose(joystick);assert(SDL_JoystickDetachVirtual(index)==0);SDL_PumpEvents();ui.refreshControllers();
    assert(ui.controllers.empty() && ui.controllerButtons()==0 && ui.axisRepeats.empty() && ui.controllerActivity.empty());
    // An unmapped joystick stays observable and is promoted after a mapping is
    // supplied; unplugging it releases both the handle and activity counters.
    index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,2,3,1);assert(index>=0);
    auto *unknown=SDL_JoystickOpen(index);assert(unknown);
    auto id=SDL_JoystickInstanceID(unknown);
    SDL_PumpEvents();ui.refreshControllers();
    assert(ui.unmappedControllers.count(id) && ui.controllers.empty());
    observed.type=SDL_JOYHATMOTION;observed.jhat.which=id;ui.controllerEvent(observed);
    ui.inputDiagnosticAt=0;passport=ui.controllerDiagnostics();
    assert(passport["devices"][0]["mapped"]==false && passport["devices"][0]["activity"]["raw_hats"]==1);
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(unknown),guid,sizeof(guid));
    mapping=std::string(guid)+",temporary fixture,a:b0,b:b1,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,";
    assert(SDL_GameControllerAddMapping(mapping.c_str())>=0);ui.refreshControllers();
    assert(ui.controllers.count(id) && ui.unmappedControllers.empty());
    SDL_JoystickClose(unknown);assert(SDL_JoystickDetachVirtual(index)==0);SDL_PumpEvents();ui.refreshControllers();
    assert(ui.controllers.empty() && ui.unmappedControllers.empty() && ui.controllerActivity.empty());
  }
  std::cout<<"PASS: scrollable settings EN/RU, root focus restoration, nested back navigation, hidden lid-wake controls, fresh Power release, independent lid defaults/four combinations/persistence/failure/unavailable fallback, lid policy, inline volume/brightness with rollback, active-process ownership, paused core continuity, suspend failure, SRAM failure abort, preview integrity and legacy states.\n";
}
