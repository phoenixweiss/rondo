#include "state.hpp"
#include "app.hpp"
#include <cassert>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace rondo;
struct FakeDevice : DeviceControl {
  int level=60,sleeps=0;bool open=true,fail=false,wakeAvailable=true;
  bool blanked=false,failBlank=false;int blankCalls=0;
  bool canBlank() const override {return true;}
  void blank(bool value) override {if(failBlank)throw std::runtime_error("Injected blank failure");blanked=value;++blankCalls;}
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
    a.action("MENU");group(1);a.action("RIGHT");assert(d->level==65 && a.store.data["brightness"]==65);a.capture("brightness-inline");
    a.action("DOWN");assert(a.row==1);a.action("DOWN");assert(a.row==2);a.action("UP");a.action("UP");assert(a.row==0); // Display: brightness, global scaling, background.
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
      a.action("MENU");a.action("UP");a.action("LEFT");a.action("L1");a.action("R1");
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
      a.action("B");assert(a.screen==Screen::Library);a.action("MENU");assert(a.row==6 && a.settingsScroll==offset);
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
      for(int target:{9,10}) {
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
        assert(a.stop && a.systemExitCode==(target==10?64:65) && a.store.data==before);
        assert(a.screen==Screen::SystemProgress);
        a.capture(std::string("power-progress-")+language+std::to_string(target));
        a.action("B");a.action("A");a.action("VOLUME_UP");a.action("SLEEP");
        assert(a.screen==Screen::SystemProgress && a.store.data==before);
      }
    }
  }
  {
    o.state=root/"display-qol";App a(o);
    a.device=std::make_unique<FakeDevice>();a.brightness=60;
    assert(!a.store.integerScaling("new") && a.store.data["game_background"]=="black");
    a.store.data["game_options"]["old-on"]={{"integer_scaling",true},{"other",17}};
    a.store.data["game_options"]["old-off"]={{"integer_scaling",false}};
    a.screen=Screen::SettingsSection;a.category=1;a.row=1;a.action("RIGHT");
    assert(a.store.integerScaling("new") && a.store.integerScaling("old-on") && !a.store.integerScaling("old-off"));
    a.requestSettings=[](Json){return false;};a.action("LEFT");
    assert(a.store.integerScaling("new"));a.requestSettings={};a.notice.clear();
    a.row=2;a.action("RIGHT");assert(a.store.data["game_background"]=="charcoal");
    a.action("LEFT");assert(a.store.data["game_background"]=="black");
    Store reload(o.state);assert(reload.integerScaling("new") && !reload.integerScaling("old-off"));
    for(const auto *language:{"en","ru"}) {
      a.locale.select(o.assets,language);a.row=1;a.capture(std::string("display-global-")+language);
      a.action("X");a.capture(std::string("display-global-info-")+language);a.action("B");
      a.row=2;a.capture(std::string("display-background-")+language);
      for(const auto *system:{"All","Favorites","NES","SNES","GB","GBC","GBA","MD","PSX"}) {
        a.section=system;a.screen=Screen::Library;
        auto title=a.libraryTitle();assert(a.ui->measure(title,29,700)<=414);
        if(std::string(system)=="NES")assert(title==(std::string(language)=="ru"?"Игры для NES":"NES games"));
        a.capture(std::string("library-title-")+system+"-"+language);
      }
      a.screen=Screen::SettingsSection;a.category=1;
    }
    // A runner reads the same persisted global default; per-game reset retains other options.
    Game entry{"old-on","test","NES","QOL fixture",argv[4]};
    a.ui.reset();
    o.state=root/"display-runner";Store initial(o.state);auto config=initial.data;
    config["integer_scaling"]=true;config["game_options"][entry.id]={{"integer_scaling",true},{"other",17}};initial.commit(config);
    App runner(o,true,entry);runner.ticks(3);
    assert(runner.store.integerScaling(entry.id));
    for(const auto *language:{"en","ru"}) {
      runner.locale.select(o.assets,language);runner.screen=Screen::GameSettings;
      runner.capture(std::string("display-override-on-")+language);
      runner.action("RIGHT");assert(!runner.store.integerScaling(entry.id));
      runner.capture(std::string("display-override-off-")+language);
      runner.action("RIGHT");assert(runner.store.integerScaling(entry.id));
      assert(!runner.store.data["game_options"][entry.id].contains("integer_scaling"));
      assert(runner.store.data["game_options"][entry.id]["other"]==17);
      runner.capture(std::string("display-override-global-")+language);
      runner.screen=Screen::Running;runner.capture(std::string("display-game-black-")+language);
      auto *pixel=static_cast<uint32_t *>(runner.ui->surface->pixels);Uint8 r,g,b,alpha;
      SDL_GetRGBA(pixel[0],runner.ui->surface->format,&r,&g,&b,&alpha);assert(r==0 && g==0 && b==0);
      runner.screen=Screen::GameSettings;runner.action("RIGHT");assert(runner.store.integerScaling(entry.id));
    }
    auto before=runner.store.data;runner.requestSettings=[](Json){return false;};
    runner.action("RIGHT");assert(runner.store.data==before && !runner.notice.empty());
    // A failed SRAM flush must restore confirmation, never request system power.
    runner.requestSettings={};auto dir=runner.store.gameDir(entry);
    fs::rename(dir,dir.string()+".kept");std::ofstream(dir)<<"blocked";
    runner.screen=Screen::SystemConfirm;runner.row=1;runner.restartRequested=false;
    runner.action("A");assert(!runner.stop && runner.systemExitCode==0 && runner.screen==Screen::SystemConfirm && !runner.notice.empty());
    fs::remove(dir);fs::rename(dir.string()+".kept",dir);
  }
  {
    o.state=root/"recents";auto library=root/"recent-card";
    fs::create_directories(library/"NES/First");fs::create_directories(library/"NES/Second");
    fs::copy_file(argv[4],library/"NES/First/game.nes");fs::copy_file(argv[4],library/"NES/Second/game.nes");
    o.volumes={{"recent-volume",library}};
    { App a(o);a.section="Recent";assert(a.visible().empty());
      for(const auto *language:{"en","ru"}) { a.locale.select(o.assets,language);a.capture(std::string("recents-empty-")+language); }
      a.game=a.games[0];auto firstId=a.game.id;a.rememberLaunch();assert(a.visible().size()==1);
      a.game=a.games[1];a.rememberLaunch();assert(a.visible().size()==2 && a.games[a.visible()[0]].id==a.game.id);
      a.game=a.games[0];a.rememberLaunch();assert(a.store.data["recent_games"].size()==2 && a.games[a.visible()[0]].id==firstId);
      const auto before=a.store.data;
      a.requestSettings=[](Json){return false;};a.game=a.games[1];
      bool failed=false;try{a.rememberLaunch();}catch(...){failed=true;}
      assert(failed && a.store.data==before);a.requestSettings={};a.notice.clear();
      for(const auto *language:{"en","ru"}) { a.locale.select(o.assets,language);a.capture(std::string("recents-list-")+language); }
      a.screen=Screen::Library;a.selected=0;
      a.action("A");assert(a.launch);a.launch=false; // Request is not an acknowledged launch.
      assert(a.store.data["recent_games"]==before["recent_games"]);
    }
    fs::rename(library,library.string()+".away");
    { App offline(o);offline.section="Recent";auto visible=offline.visible();assert(visible.size()==2);
      assert(!offline.games[visible[0]].available);
      for(const auto *language:{"en","ru"}) { offline.locale.select(o.assets,language);offline.capture(std::string("recents-missing-")+language); }
      auto before=offline.store.data["recent_games"];offline.action("A");
      assert(!offline.launch && offline.screen==Screen::Library && !offline.notice.empty());
      assert(offline.store.data["recent_games"]==before);offline.notice.clear();
      offline.action("Y");offline.searchText("First");offline.action("SEARCH_SUBMIT");assert(offline.visible().size()==1);
      offline.action("B");offline.section="All";assert(offline.visible().empty());
    }
    fs::rename(library.string()+".away",library);
    { App online(o);online.section="Recent";assert(online.visible().size()==2 && online.games[online.visible()[0]].available); }
    o.volumes.clear();
  }
  {
    o.state=root/"background-preview";App a(o);a.screen=Screen::SettingsSection;a.category=1;a.row=2;
    for(const auto *language:{"en","ru"})for(const auto *color:{"black","charcoal","navy","theme"}) {
      a.locale.select(o.assets,language);a.store.data["game_background"]=color;
      a.capture(std::string("background-swatch-")+language+"-"+color);
    }
  }
  {
    o.state=root/"power-qol";App a(o);auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);
    a.screen=Screen::SettingsSection;a.category=3;a.row=0;a.lastActivity=1000;a.powerSampleAt=9999999;
    a.powerTick(300999);assert(!a.screenBlank);a.powerTick(301000);assert(a.screenBlank && d->blanked);
    a.store.data["debug_enabled"]=true;a.store.data["debug_level"]="detailed";
    a.batteryLog=std::make_unique<BatteryLog>(o.state/"Logs","shell",root/"fake-sys",root/"fake-proc");
    a.capture("menu-screen-off");assert(fs::exists(o.state/"Logs/shell.jsonl"));
    assert(!a.ui->batteryDisplay.at("percentage_visible").get<bool>());
    a.store.data["debug_enabled"]=false;a.batteryLog.reset();
    a.action("A");assert(!a.screenBlank && !d->blanked && a.row==0 && a.store.data["show_battery_percent"]==true);
    a.wakeConsumeUntil=0;a.store.data["idle_screen_minutes"]=0;a.lastActivity=0;a.powerTick(900000);assert(!a.screenBlank);
    a.store.data["idle_screen_minutes"]=1;a.screen=Screen::Running;a.powerTick(900000);assert(!a.screenBlank);
    a.screen=Screen::SettingsSection;a.lastActivity=0;d->failBlank=true;a.powerTick(900000);assert(!a.screenBlank && !a.notice.empty());d->failBlank=false;a.notice.clear();
    for(const auto *language:{"en","ru"}) {
      a.locale.select(o.assets,language);a.row=0;a.capture(std::string("power-options-top-")+language);
      a.row=5;a.capture(std::string("power-options-bottom-")+language);assert(a.powerScroll>0);
      a.action("X");a.capture(std::string("battery-warning-info-")+language);a.action("B");
    }
    Telemetry t;t.capacity=10;t.batteryStatus="Discharging";
    a.screen=Screen::Library;a.batteryWarning(t,1000);assert(a.batteryWarningPercent<0 && a.batteryWarningMask==0);
    a.screen=Screen::Running;a.batteryWarning(t,2000);assert(a.batteryWarningPercent==10 && a.batteryWarningMask==1 && a.batteryNoticeUntil==5000);
    a.powerTick(5000);assert(a.batteryWarningPercent<0);
    t.capacity=80;a.batteryWarning(t,6000);t.capacity=10;a.batteryWarning(t,7000);assert(a.batteryWarningPercent<0);
    t.capacity=5;a.batteryWarning(t,8000);assert(a.batteryWarningPercent==5 && a.batteryWarningMask==3 && !a.stop);
    a.screen=Screen::GameMenu;a.batteryWarning(t,9000);assert(a.batteryWarningPercent<0);
    a.batteryWarningsLoaded=false;a.batteryWarningMask=0;a.screen=Screen::Running;
    a.batteryWarning(t,10000);assert(a.batteryWarningMask==3 && a.batteryWarningPercent<0);
    t.capacity=100;a.batteryWarning(t,11000);assert(a.batteryWarningMask==3);
    t.batteryStatus="Charging";t.capacity=25;a.batteryWarning(t,12000);a.batteryWarning(t,41999);assert(a.batteryWarningMask==3);
    a.batteryWarning(t,42000);assert(a.batteryWarningMask==0 && a.batteryWarningPercent<0);
    t.batteryStatus="Discharging";t.capacity.reset();a.batteryWarning(t,100000);assert(a.batteryWarningPercent<0);
    a.store.data["battery_warning"]=false;t.capacity=5;a.batteryWarning(t,110000);assert(a.batteryWarningMask==0 && a.batteryWarningPercent<0);
    a.store.data["battery_warning"]=true;a.batteryWarning(t,120000);assert(a.batteryWarningMask==3 && a.batteryWarningPercent==5);

  }
  {
    o.state=root/"autosave";o.volumes={{"autosave-card",root/"autosave-card"}};
    fs::create_directories(o.volumes[0].root/"NES/Game");fs::copy_file(argv[4],o.volumes[0].root/"NES/Game/test.nes");
    auto entry=scan(o.volumes)[0];
    { App input(o,true,entry);
      auto A=uint16_t(1<<RETRO_DEVICE_ID_JOYPAD_A),B=uint16_t(1<<RETRO_DEVICE_ID_JOYPAD_B);
      assert(input.gameButtons(A)==0);assert(input.gameButtons(0)==0);assert(input.gameButtons(A)==A);
      input.action("MENU");input.action("A");assert(input.screen==Screen::Running && !input.core->paused);
      for(int i=0;i<10;++i)assert(input.gameButtons(A)==0);
      assert(input.gameButtons(A|B)==0);assert(input.gameButtons(B)==0);
      assert(input.gameButtons(0)==0);assert(input.gameButtons(A)==A);
      input.action("MENU");input.action("B");assert(input.gameButtons(B)==0);
      input.gameButtons(0);assert(input.gameButtons(B)==B);
      input.core->save(0);input.screen=Screen::LoadConfirm;input.row=1;input.slot=0;input.action("A");
      assert(input.gameButtons(A)==0);input.gameButtons(0);assert(input.gameButtons(A)==A);
    }
    { App preview(o,true,entry);preview.ticks(5);Telemetry low;low.capacity=10;low.batteryStatus="Discharging";
      preview.batteryWarning(low,monotonicMs());preview.capture("battery-game-10");
      preview.volumeFeedback.confirmed=50;preview.volumeFeedback.phase="confirmed";preview.volumeFeedback.deadline=monotonicMs()+10000;
      preview.capture("battery-and-volume");preview.volumeFeedback.deadline=0;
      low.capacity=5;preview.batteryWarning(low,monotonicMs());preview.capture("battery-game-5");
      preview.screen=Screen::GameMenu;preview.capture("battery-menu-no-warning");
    }
    { App a(o,true,entry);a.ticks(5);a.core->save(0);auto manual=readBytes(a.store.gameDir(entry)/"slot-0.state");
      a.screen=Screen::ExitConfirm;a.row=1;a.action("A");assert(a.stop && a.core->compatibleAutomatic());
      assert(readBytes(a.store.gameDir(entry)/"slot-0.state")==manual && a.latestResumeSlot()==3);
      a.rememberLaunch();
    }
    { App startup(o);assert(startup.screen==Screen::StartupResume && !startup.launch);
      for(const auto *language:{"en","ru"}) {startup.locale.select(o.assets,language);startup.capture(std::string("startup-resume-")+language);}
      startup.action("B");assert(startup.screen==Screen::Library && !startup.launch);
      startup.offerStartupResume();startup.action("A");assert(startup.launch && startup.resume);
    }
    { App failed(o,true,entry);failed.ticks(3);auto dir=failed.store.gameDir(entry);
      fs::rename(dir,dir.string()+".saved");std::ofstream(dir)<<"blocked";
      failed.screen=Screen::ExitConfirm;failed.row=1;failed.action("A");assert(!failed.stop && !failed.gameClosed && !failed.autosaveError.empty());
      for(const auto *language:{"en","ru"}) {failed.locale.select(o.assets,language);failed.notice=failed.locale.tr("autosave.failed");failed.capture(std::string("autosave-error-")+language);}
      fs::remove(dir);fs::rename(dir.string()+".saved",dir);
      auto prior=readBytes(dir/"autosave.state");failed.action("DOWN");assert(failed.row==2);failed.action("A");
      assert(failed.stop && failed.gameClosed && readBytes(dir/"autosave.state")==prior);
    }
    { App closed(o);auto recent=closed.store.data["recent_games"];
      closed.rememberClosedGame();assert(closed.store.data["recent_games"]==recent);
      assert(closed.latestResumeSlot()>=0);
    }
    { App closed(o);assert(closed.screen==Screen::Library);
      closed.game=entry;closed.rememberLaunch();
    }
    { App poweredOff(o);assert(poweredOff.screen==Screen::StartupResume);
      auto old=poweredOff.store.data;old.erase("resume_game");poweredOff.store.commit(old);
    }
    { App legacy(o);assert(legacy.screen==Screen::Library); }
    { Store settings(o.state);auto config=settings.data;config["startup_resume"]=false;config["autosave_enabled"]=false;settings.commit(config);
      App disabled(o);assert(disabled.screen==Screen::Library);
    }
    o.volumes.clear();
  }
  {
    o.state=root/"library-search";App a(o);a.games.clear();
    for(int i=0;i<204;++i)a.games.push_back({std::to_string(i),"test","NES","Game "+std::to_string(i),{}});
    a.games.push_back({"contra","test","NES","Contra",{}});
    a.games.push_back({"duck","test","NES","Чёрный Плащ",{}});
    a.games.push_back({"world","test","SNES","Super Mario World",{}});
    a.sections={"All","Favorites","NES","SNES"};
    a.selected=87;const auto original=a.store.data;
    a.action("Y");assert(a.screen==Screen::Search);a.searchText("CONTRA");
    a.ui->inputHints(true);a.capture("search-physical-en");a.action("B");
    assert(a.screen==Screen::Library && a.selected==87 && a.visible().size()==207);
    a.action("Y");assert(a.searchDraft.empty());a.searchText("CONTRA");a.action("SEARCH_SUBMIT");
    assert(a.visible().size()==1 && a.games[a.visible()[0]].id=="contra");a.capture("search-found-en");
    a.action("B");assert(a.visible().size()==207);
    a.locale.select(o.assets,"ru");a.ui->inputHints(false);a.action("Y");
    a.searchText("ЧЁРНЫЙ ПЛАЩ");a.capture("search-physical-ru");a.action("SEARCH_SUBMIT");
    assert(a.visible().size()==1 && a.games[a.visible()[0]].id=="duck");a.capture("search-found-ru");
    a.action("Y");a.action("SEARCH_DELETE");assert(a.searchDraft=="чёрный пла");
    a.action("B");assert(a.visible().size()==1);a.action("B");
    a.action("Y");a.action("Y");a.capture("search-keyboard-ru");
    a.action("DOWN");a.action("A");assert(a.searchDraft=="й");a.action("X");assert(a.searchDraft.empty());
    a.searchText(std::string(120,'a'));assert(a.searchDraft.size()==96);a.action("SEARCH_SUBMIT");
    assert(a.visible().empty());a.capture("search-empty-ru");a.action("B");
    a.section="NES";a.action("Y");a.searchText("mario");a.action("SEARCH_SUBMIT");assert(a.visible().empty());
    a.action("B");a.section="Favorites";a.store.data["favorites"]={"contra"};
    a.action("Y");a.searchText("tra");a.action("SEARCH_SUBMIT");assert(a.visible().size()==1);
    a.action("X");assert(a.visible().empty());a.action("B");
    a.store.data=original;assert(a.store.data==original);
    // Search is session-only; a fresh app opens the unfiltered library.
    App restarted(o);assert(restarted.searchQuery.empty());
  }
  {
    o.state=root/"lid-options";App a(o);auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);
    assert(a.store.data["lid_sleep"]==true && a.store.data["wake_on_lid_open"]==true);
    a.action("MENU");for(int i=0;i<5;++i)a.action("DOWN");a.action("A");
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
    d->wakeAvailable=false;a.row=1;a.action("DOWN");assert(a.row==2 && a.infoKey()=="autosave" && a.store.data["wake_on_lid_open"]==true);a.row=1;
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
    a.core->save(0);a.screen=Screen::LoadSlots;a.row=0;a.capture("slot-preview");auto state=readState(dir/"slot-0.state");assert(!state.pixels.empty());
    auto &view=a.slotView(0);assert(view.compatible && view.width<=192 && view.height<=144 && view.utc>0);
    state.pixels.clear();state.meta["preview_width"]=0;state.meta["preview_height"]=0;writeState(dir/"slot-1.state",state);
    auto &old=a.slotView(1);assert(old.compatible && old.pixels.empty() && old.utc==view.utc);
    state.meta["core"]="wrong-core";writeState(dir/"slot-2.state",state);assert(!a.slotView(2).compatible);a.row=1;a.capture("slot-no-preview");a.row=2;a.capture("slot-incompatible");a.action("X");assert(a.infoOpen);a.capture("slots-info");
    // Updating time backwards must not make Continue choose the older slot.
    auto future=readState(dir/"slot-0.state");future.meta["utc"]=static_cast<long long>(time(nullptr))+86400;
    writeState(dir/"slot-0.state",future);
    a.core->save(1);auto recent=readState(dir/"slot-1.state",StateRead::Metadata).meta;
    assert(recent["utc"]<future.meta["utc"] && saveSequence(recent)>0 && a.latestResumeSlot()==1);
    a.core->save(0);assert(a.latestResumeSlot()==0 && saveSequence(readState(dir/"slot-0.state",StateRead::Metadata).meta)>saveSequence(recent));
    // Deletion is confirmed, targets only one slot and preserves SRAM / neighbors.
    a.screen=Screen::LoadSlots;a.row=2;a.notice.clear();a.infoOpen=false;
    auto kept0=readBytes(dir/"slot-0.state"),kept1=readBytes(dir/"slot-1.state");
    std::ofstream(dir/"slot-2.state.previous")<<"old backup";
    auto sramBefore=readBytes(dir/"sram.bin");
    auto target=readBytes(dir/"slot-2.state");
    a.action("Y");assert(a.screen==Screen::DeleteSlot && a.row==0);a.capture("delete-slot-confirm-en");
    a.locale.select(o.assets,"ru");a.capture("delete-slot-confirm-ru");
    a.action("B");assert(a.screen==Screen::LoadSlots && a.row==2 && readBytes(dir/"slot-2.state")==target);
    fs::remove(dir/"slot-2.state.previous");fs::create_directories(dir/"slot-2.state.previous");
    std::ofstream(dir/"slot-2.state.previous/block")<<"failure fixture";
    a.action("Y");a.action("DOWN");a.action("A");assert(a.screen==Screen::DeleteSlot && readBytes(dir/"slot-2.state")==target);
    fs::remove_all(dir/"slot-2.state.previous");a.action("A");
    assert(a.screen==Screen::LoadSlots && a.row==2 && !fs::exists(dir/"slot-2.state") && !a.slotView(2).present);
    assert(readBytes(dir/"slot-0.state")==kept0 && readBytes(dir/"slot-1.state")==kept1 && readBytes(dir/"sram.bin")==sramBefore);
    a.capture("deleted-slot-ru");a.action("Y");assert(a.screen==Screen::LoadSlots);
    bool invalid=false;try{a.core->deleteSlot(-1);}catch(...){invalid=true;}assert(invalid);
    Store reload(o.state);assert(reload.data.value("lid_sleep",true));
  }
  {
    o.state=root/"volume-background";Game game{"volume-background","TEST","NES","Fixture",argv[4]};App a(o,true,game);
    auto release=std::make_shared<std::promise<void>>();auto gate=release->get_future().share();
    auto started=std::make_shared<std::promise<void>>();auto ready=started->get_future();std::atomic<int> writes{0};
    const auto state=o.state;
    auto writer=[state](int value) {Store authority(state);auto next=authority.data;next["volume"]=value;authority.commit(next);return authority.data;};
    a.requestVolume=[&,writer,gate,started](int value) {
      if(++writes==1) {started->set_value();gate.wait();}
      return writer(value);
    };
    a.action("VOLUME_DOWN");assert(a.previewVolume==75 && writes==0);
    a.volumeSaveAt=0;a.ticks(0);assert(ready.wait_for(std::chrono::seconds(2))==std::future_status::ready);
    auto frames=a.core->frames;auto audio=a.core->audioFrames;auto began=std::chrono::steady_clock::now();
    for(int i=0;i<10;++i) {a.action("VOLUME_DOWN");a.ticks(1);}
    auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
    assert(elapsed<500 && writes==1 && a.previewVolume==25 && a.core->volume==25);
    assert(a.core->frames==frames+10 && a.core->audioFrames>audio);
    release->set_value();a.volumeTick(true);assert(writes==2 && a.store.data["volume"]==25 && a.previewVolume==25);
    assert(Store(state).data["volume"]==25);
    a.requestVolume=[](int)->Json {throw std::runtime_error("Injected delayed volume write failure");};
    a.action("VOLUME_UP");a.volumeTick(true);assert(a.previewVolume==30 && a.core->volume==30 && a.store.data["volume"]==25 && !a.notice.empty());
    a.requestVolume=writer;a.action("VOLUME_UP");
    auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);
    d->before=[&]{assert(a.core->paused && Store(state).data["volume"]==35);};
    a.sleep();assert(d->sleeps==1 && a.previewVolume==35 && !a.core->paused);
    std::cout<<"PASS: game/audio progress during blocked volume persistence, ten steps in "<<elapsed<<" ms, coalesced final gain, failure and sleep drain.\n";
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
    o.state=root/"gameplay-input-fast-path";App a(o);a.screen=Screen::Running;
    for(const auto *level:{"errors","events","detailed"}) {
      a.store.data["debug_enabled"]=true;a.store.data["debug_level"]=level;
      const auto trace=a.trace.size();
      for(int i=0;i<1000;++i)
        for(const auto *button:{"A","B","X","Y","UP","DOWN","LEFT","RIGHT","L1","R1","START","SELECT"})a.action(button);
      assert(a.screen==Screen::Running && a.trace.size()==trace);
    }
  }
  {
    o.state=root/"debug-settings";App a(o);
    for(int i=0;i<1000;++i)a.record("bounded-trace");assert(a.trace.size()==256);
    assert(!a.store.data.value("debug_enabled",true) && !a.batteryLog && !fs::exists(o.state/"Logs"));
    a.action("MENU");for(int i=0;i<6;++i)a.action("DOWN");a.action("A");assert(a.category==4);a.action("DOWN");
    a.action("RIGHT");assert(a.store.data["debug_level"]=="events" && !a.batteryLog);
    a.action("UP");a.requestSettings=[](Json){return false;};a.action("RIGHT");assert(!a.batteryLog && !a.store.data["debug_enabled"].get<bool>());
    a.requestSettings={};a.action("RIGHT");assert(a.batteryLog);a.batteryLog->drain();assert(fs::exists(o.state/"Logs/shell.log"));
    for(const auto *language:{"en","ru"}) {
      a.locale.select(o.assets,language);a.capture(std::string("debug-on-")+language);
      a.action("X");a.capture(std::string("debug-info-")+language);a.action("B");
    }
    a.action("DOWN");a.action("RIGHT");assert(a.store.data["debug_level"]=="detailed");
    a.action("LEFT");a.action("LEFT");assert(a.store.data["debug_level"]=="errors");
    a.batteryLog->drain();
    auto path=o.state/"Logs/shell.log";auto size=fs::file_size(path);
    a.record("safe-event");a.batterySample();assert(fs::file_size(path)==size);
    a.batterySample("operation_failed");a.batteryLog->drain();assert(fs::file_size(path)>size);
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
      auto size=fs::file_size(log/"shell.log");
      a.ui.reset();a.batterySample();assert(fs::file_size(log/"shell.log")==size);
    }
    std::ifstream input(log/"shell.log");std::string line;std::vector<std::string> events;
    while(std::getline(input,line)) { auto event=Json::parse(line.substr(line.find('\t')+1)).at("event").get<std::string>();if(event.find("action:")!=0)events.push_back(event); }
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
    std::string mapping=std::string(guid)+",Rondo test controller,a:b0,b:b1,back:b4,start:b6,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,leftstick:b7,rightstick:b8,";
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
    assert(!(ui.controllerButtons(false)&(1<<RETRO_DEVICE_ID_JOYPAD_DOWN)));
    assert(ui.controllerAxes()[1]>12000);
    SDL_JoystickSetVirtualAxis(joystick,0,-32768);SDL_JoystickSetVirtualAxis(joystick,2,32767);
    SDL_JoystickSetVirtualAxis(joystick,3,-24000);SDL_JoystickSetVirtualAxis(joystick,5,32767);
    SDL_JoystickSetVirtualButton(joystick,7,1);SDL_JoystickSetVirtualButton(joystick,8,1);SDL_GameControllerUpdate();
    auto axes=ui.controllerAxes();assert(axes[0]==-32768&&axes[2]==32767&&axes[3]<-12000);
    auto full=ui.controllerButtons(false);assert(full&(1<<RETRO_DEVICE_ID_JOYPAD_R2));
    assert(full&(1<<RETRO_DEVICE_ID_JOYPAD_L3));assert(full&(1<<RETRO_DEVICE_ID_JOYPAD_R3));
    SDL_JoystickSetVirtualAxis(joystick,0,0);SDL_JoystickSetVirtualAxis(joystick,2,0);SDL_JoystickSetVirtualAxis(joystick,3,0);
    SDL_JoystickSetVirtualAxis(joystick,1,4000);SDL_GameControllerUpdate();assert(ui.controllerNavigation().empty());
    SDL_JoystickClose(joystick);assert(SDL_JoystickDetachVirtual(index)==0);SDL_PumpEvents();ui.refreshControllers();
    assert(ui.controllers.empty() && ui.controllerButtons()==0 && ui.axisRepeats.empty() && ui.controllerActivity.empty());
    assert((ui.controllerAxes()==std::array<int16_t,4>{}));
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
  std::cout<<"PASS: scrollable settings EN/RU, root focus restoration, nested back navigation, hidden lid-wake controls, fresh Power release, independent lid defaults/four combinations/persistence/failure/unavailable fallback, lid policy, inline volume/brightness with rollback, active-process ownership, paused core continuity, suspend failure, SRAM failure abort, compact preview integrity and binary states.\n";
}
