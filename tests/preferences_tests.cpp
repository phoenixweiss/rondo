// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
using namespace rondo;
static void next(App &a){a.wizardRow=a.wizardNextRow();a.action("A");}
struct SetupWifi:WifiAdapter {
  bool on=false,fail=false;std::atomic<int> calls{0},scans{0};
  bool supported()const override{return true;}
  WifiState status()override{WifiState s;s.available=s.ready=true;s.enabled=on;s.phase=on?"disconnected":"off";return s;}
  WifiState scan()override{++scans;return status();}
  WifiState enable(bool value)override{++calls;SDL_Delay(30);if(fail)throw WifiError("transport");on=value;return status();}
};
struct SetupClock:ClockAdapter {
  bool on=true,fail=false;std::atomic<int> calls{0},sets{0};
  bool supported()const override{return true;}
  ClockState status()override{ClockState s;s.available=true;s.enabled=on;return s;}
  ClockState enable(bool value)override{++calls;SDL_Delay(30);if(fail)throw ClockError("service");on=value;return status();}
  ClockState restart()override{return enable(true);}
  ClockState setTime(long long utc)override{++sets;on=false;auto state=status();state.manualUtc=utc;state.rtcSaved=true;return state;}
};
int main(int argc,char **argv) {
  assert(argc==4);Options o;o.assets=argv[1];o.state=fs::path(argv[2])/"state";o.output=fs::path(argv[2])/"screens";o.core=argv[3];o.headless=true;
  // Only NES is executed here; other paths are presence fixtures for the catalog.
  o.gbCore=o.gbaCore=o.snesCore=o.mdCore=o.psxCore=o.core;
  assert(themeId("slate")=="nord"&&themeId("moss")=="pine"&&themeId("unknown")=="rondo");
  {App a(o);assert(!a.wizardActive);assert(a.store.fresh);auto before=a.store.data;
    a.screen=Screen::SettingsSection;a.category=0;a.row=1;a.action("A");assert(a.screen==Screen::Themes);
    a.action("DOWN");assert(a.ui->currentTheme=="nord"&&a.store.data==before);
    a.action("B");assert(a.ui->currentTheme=="rondo"&&a.screen==Screen::SettingsSection&&a.row==1);
    a.openThemes();a.action("DOWN");a.requestSettings=[](const Json &){return false;};a.action("A");assert(a.ui->currentTheme=="rondo"&&a.store.data==before&&!a.notice.empty());a.requestSettings={};
    a.openThemes();a.action("DOWN");a.action("A");assert(a.store.data["theme_id"]=="nord");
    auto preserved=a.store.data;for(auto language:{"en","ru"}) {a.locale.select(o.assets,language);
      for(const auto &t:a.ui->themes["themes"]){a.notice.clear();a.ui->setTheme(t["id"]);a.screen=Screen::SettingsSection;a.category=0;a.row=1;a.capture(std::string("interface-")+t["id"].get<std::string>()+"-"+language);}
    }
    a.ui->setTheme("nord");a.screen=Screen::CoreCatalog;a.row=1;a.action("LEFT");assert(!a.coreEnabled("GB")&&!a.coreEnabled("GBC"));assert(a.store.data["favorites"]==preserved["favorites"]&&a.store.data["game_options"]==preserved["game_options"]);
    a.action("RIGHT");assert(a.coreEnabled("GB"));
    auto c=a.installedCores();for(const auto &entry:c){assert(entry.available()&&entry.enabled(a.store.data));}
    CoreEntry experimental{"test","Test","TEST",o.core,true,{}};assert(!experimental.enabled(a.store.data));auto choices=a.store.data;choices["enabled_cores"]["test"]=true;assert(experimental.enabled(choices));
    CoreEntry missing{"absent","Absent","NONE",o.state/"absent.so",false,{}};assert(!missing.enabled(choices));
    auto input=a.store.data;input["input_mapping"]["fceumm"]["8"]=0;assert(mappedButtons(input,"NES",1)==((1<<0)|(1<<8)));assert(mappedButtons(input,"GB",1)==1);input["input_mapping"]["fceumm"]["8"]=-1;assert(mappedButtons(input,"NES",1<<8)==0);
    a.screen=Screen::MappingCores;a.row=0;a.action("A");assert(a.screen==Screen::Mapping);a.row=4;a.action("LEFT");assert(mappedSource(a.store.data,"fceumm",8)==7);
    a.action("X");assert(a.screen==Screen::InputTest);a.testInput(0,{},{});a.testInput((1<<3)|(1<<2),{},{});assert(a.screen==Screen::Mapping&&a.row==4);
    a.action("Y");assert(a.screen==Screen::MappingReset);a.action("B");assert(mappedSource(a.store.data,"fceumm",8)==7);a.action("Y");a.action("DOWN");a.action("A");assert(mappedSource(a.store.data,"fceumm",8)==8);
    for(auto lang:{"en","ru"}){a.locale.select(o.assets,lang);a.screen=Screen::MappingCores;a.capture(std::string("mapping-cores-")+lang);a.screen=Screen::Mapping;a.mappingCore="pcsx_rearmed";a.row=15;a.featureScroll=0;a.capture(std::string("mapping-psx-")+lang);a.screen=Screen::CoreCatalog;a.row=5;a.capture(std::string("core-catalog-")+lang);}
  }
  {App a(o);assert(a.ui->currentTheme=="nord");a.startWizard(true);assert(a.wizardActive&&a.wizardStep==0);auto old=a.store.data;
    a.action("RIGHT");assert(a.locale.current!=old["locale"]);a.action("Y");assert(a.wizardStep==1&&a.locale.current==old["locale"]);a.action("Y");assert(a.wizardStep==2);
    a.wizardRow=1;a.action("RIGHT");assert(a.ui->currentTheme!="nord");a.action("Y");assert(a.wizardStep==3&&a.store.data["theme_id"]=="nord"&&a.ui->currentTheme=="nord");
    int volume=a.previewVolume;a.action("LEFT");assert(a.previewVolume==volume-5);a.action("MENU");assert(a.screen==Screen::WizardExit);a.action("B");a.action("Y");assert(a.previewVolume==volume&&a.wizardStep==4);
    a.wizardRow=1;a.action("A");assert(a.screen==Screen::Wizard&&a.wizardStep==4&&!a.notice.empty());a.action("Y");
    a.wizardRow=1;a.action("A");assert(a.screen==Screen::Regions);a.action("B");assert(a.screen==Screen::Wizard&&a.wizardRow==1);a.action("Y");
    a.wizardRow=1;a.action("LEFT");next(a);assert(!a.coreEnabled("GB")&&!a.coreEnabled("GBC")&&a.wizardStep==7);
    a.action("Y");assert(a.wizardStep==8);a.action("A");assert(!a.wizardActive&&a.store.data["wizard"]["completed"]==true);
    a.startWizard(true);a.action("MENU");a.action("A");assert(!a.wizardActive&&a.screen==Screen::Library&&a.store.data["wizard"]["deferred"]==true);
    auto data=a.store.data;data["wizard"]={{"schema",1},{"completed",false},{"deferred",true},{"step",2}};a.store.commit(data);
  }
  o.setup=true;
  {App a(o);assert(a.screen==Screen::Wizard&&a.wizardStep==3);a.action("LEFT");a.requestSettings=[](const Json &){return false;};next(a);assert(a.wizardStep==3&&a.previewVolume==a.store.data["volume"]&&!a.notice.empty());a.requestSettings={};
    for(auto lang:{"en","ru"})for(int step=0;step<9;++step){a.wizardEnter(step);a.locale.select(o.assets,lang);a.capture(std::string("wizard-")+std::to_string(step)+"-"+lang);}
  }
  o.state=fs::path(argv[2])/"fresh";
  {App a(o);assert(a.screen==Screen::Wizard&&a.previewVolume==30);auto before=a.store.data;a.action("Y");assert(a.store.data["volume"]==30);a.wizardEnter(3);a.action("X");assert(a.soundTest);assert(SDL_GetQueuedAudioSize(a.soundTest)>44100);a.soundTestUntil=0;a.ticks(0);assert(!a.soundTest);}
  o.setup=false;o.state=fs::path(argv[2])/"services";
  {App a(o);a.startWizard(true);auto radio=std::make_shared<SetupWifi>();a.wifiDevice=radio;a.wifi=radio->status();a.wifiPollAt=monotonicMs()+60000;
    a.wizardEnter(4);a.action("RIGHT");next(a);assert(a.wizardPending&&a.wizardStep==4);
    for(int i=0;i<100&&a.wizardStep==4;++i){a.ticks(0);SDL_Delay(5);}assert(a.wizardStep==5&&radio->on&&a.store.data["wifi_enabled"]==true);
    auto clock=std::make_shared<SetupClock>();a.clockDevice=clock;a.clockState=clock->status();a.clockPollAt=monotonicMs()+60000;
    a.action("LEFT");next(a);assert(a.wizardPending&&a.wizardStep==5);
    for(int i=0;i<100&&a.wizardStep==5;++i){a.ticks(0);SDL_Delay(5);}assert(a.wizardStep==6&&!clock->on&&!a.store.data["ntp_auto"].get<bool>());
    a.wizardEnter(4);radio->fail=true;a.action("LEFT");next(a);
    for(int i=0;i<100&&a.wizardPending;++i){a.ticks(0);SDL_Delay(5);}assert(a.wizardStep==4&&a.store.data["wifi_enabled"]==true&&!a.notice.empty());
    radio->fail=false;a.wizardEnter(4);a.wizardRow=1;a.action("A");assert(a.screen==Screen::WifiNetworks);
    for(int i=0;i<100&&a.wifiBusy();++i){a.ticks(0);SDL_Delay(5);}
    a.action("B");assert(a.screen==Screen::Wizard&&a.wizardRow==1);a.action("Y");assert(a.store.data["wifi_enabled"].get<bool>());
  }
  o.setup=false;o.state=fs::path(argv[2])/"focus-actions";
  {App a(o);a.startWizard(true);a.wizardEnter(3);
    auto revision=a.store.data["revision"];a.action("DOWN");assert(a.wizardRow==1);a.action("A");
    assert(a.soundTest&&a.screen==Screen::Wizard&&a.wizardStep==3&&a.store.data["revision"]==revision);
    a.action("A");assert(a.soundTest);a.action("DOWN");assert(a.wizardRow==a.wizardNextRow());a.action("A");assert(a.wizardStep==4&&!a.soundTest);
    auto radio=std::make_shared<SetupWifi>();a.wifiDevice=radio;a.wifi=radio->status();a.wifiPollAt=monotonicMs()+60000;
    a.action("RIGHT");a.action("DOWN");a.action("A");assert(a.wizardPending&&a.screen==Screen::Wizard);
    for(int i=0;i<100&&a.screen==Screen::Wizard;++i){a.ticks(0);SDL_Delay(5);}
    assert(a.screen==Screen::WifiNetworks&&a.wizardStep==4&&radio->on&&a.store.data["wifi_enabled"].get<bool>());
    for(int i=0;i<100&&a.wifiBusy();++i){a.ticks(0);SDL_Delay(5);}assert(radio->scans>0);
    a.action("B");assert(a.screen==Screen::Wizard&&a.wizardRow==1);a.action("DOWN");a.action("A");assert(a.wizardStep==5);
    auto clock=std::make_shared<SetupClock>();a.clockDevice=clock;a.clockState=clock->status();a.clockPollAt=monotonicMs()+60000;
    a.action("LEFT");a.action("DOWN");a.action("A");assert(a.screen==Screen::Regions);
    a.action("B");assert(a.screen==Screen::Wizard&&a.wizardRow==1&&!a.wizardDraft["ntp_auto"].get<bool>());
    a.action("A");a.row=0;a.action("A");assert(a.screen==Screen::Wizard&&a.wizardRow==1&&a.store.data["timezone"]=="Etc/UTC");
    a.action("DOWN");a.action("A");assert(a.screen==Screen::ClockEdit&&a.wizardStep==5);
    a.action("A");for(int i=0;i<100&&a.screen==Screen::ClockEdit;++i){a.ticks(0);SDL_Delay(5);}
    assert(a.screen==Screen::Wizard&&a.wizardRow==2&&clock->sets==1&&!a.store.data["ntp_auto"].get<bool>());
    a.action("Y");assert(a.wizardStep==6&&!a.store.data["ntp_auto"].get<bool>());
    for(auto lang:{"en","ru"})for(int step:{3,4,5,6,7}){a.wizardEnter(step);a.locale.select(o.assets,lang);a.capture(std::string("focus-actions-")+std::to_string(step)+"-"+lang);a.wizardRow=a.wizardNextRow();a.capture(std::string("next-row-")+std::to_string(step)+"-"+lang);}
  }
  o.setup=true;o.state=fs::path(argv[2])/"legacy";
  {Store legacy(o.state);auto data=legacy.data;data["volume"]=65;legacy.commit(data);}
  {App a(o);assert(!a.wizardActive&&a.previewVolume==65);a.startWizard(true);assert(a.previewVolume==65);}
  o.state=fs::path(argv[2])/"future";fs::create_directories(o.state);{std::ofstream f(o.state/"config.json");f<<"{\"schema\":2}";}
  {App a(o);assert(a.store.readOnly&&!a.wizardActive);}
  o.setup=false;o.state=fs::path(argv[2])/"render-game";
  auto fixtures=fs::path(o.assets).parent_path()/"runtime/fixtures";
  if(!fs::exists(fixtures))fixtures=fs::path(o.assets).parent_path()/"Demo/card-a";
  o.volumes={{"fixture",fixtures}};auto games=scan(o.volumes);assert(!games.empty());
  {App a(o,true,games.front());a.ticks(3);a.core->paused=true;a.core->save(0);auto pixels=a.core->frame;
    for(auto language:{"en","ru"})for(const auto &t:a.ui->themes["themes"]){
      auto id=t["id"].get<std::string>();a.locale.select(o.assets,language);a.ui->setTheme(id);a.notice.clear();
      a.screen=Screen::GameMenu;a.row=0;a.capture("game-menu-"+id+"-"+language);
      a.screen=Screen::SaveSlots;a.row=0;a.capture("slots-"+id+"-"+language);
      a.screen=Screen::Running;a.volumeFeedback.confirmed=50;a.volumeFeedback.begin(monotonicMs());a.volumeFeedback.result(a.volumeFeedback.sequence,50,true,monotonicMs());a.capture("volume-"+id+"-"+language);
      assert(a.core->frame==pixels);
    }
  }
  // Boolean settings keep their acknowledged value and never open an editor.
  o.setup=false;o.state=fs::path(argv[2])/"binary-controls";
  {App a(o);struct Setting {Screen screen;int category,row;const char *key;};
    const std::vector<Setting> settings{
      {Screen::CoreSettings,0,1,"left_stick_dpad"},{Screen::CoreSettings,0,2,"rumble_enabled"},
      {Screen::SettingsSection,1,1,"integer_scaling"},{Screen::SettingsSection,3,0,"show_battery_percent"},
      {Screen::SettingsSection,3,2,"autosave_enabled"},{Screen::SettingsSection,3,3,"startup_resume"},
      {Screen::SettingsSection,3,5,"battery_warning"},{Screen::SettingsSection,4,0,"debug_enabled"},
      {Screen::ScreenshotSettings,0,2,"screenshot_sound"}};
    for(const auto &s:settings){a.navigation.clear();a.navigationActions=false;a.screen=s.screen;a.category=s.category;a.row=s.row;
      bool expected=a.store.data.value(s.key,false);
      for(auto key:{"LEFT","LEFT","RIGHT","RIGHT","A","A"}){expected=std::string(key)=="A"?!expected:std::string(key)=="RIGHT";a.action(key);assert(a.store.data.value(s.key,!expected)==expected&&a.screen==s.screen);}
      a.action("LEFT");assert(!a.store.data.value(s.key,true));auto before=a.store.data;
      a.requestSettings=[](const Json &){return false;};a.action("RIGHT");assert(a.store.data==before&&a.screen==s.screen&&!a.notice.empty());a.requestSettings={};
    }
    for(auto lang:{"en","ru"})for(const auto &theme:a.ui->themes["themes"]){
      const auto id=theme["id"].get<std::string>();a.locale.select(o.assets,lang);a.ui->setTheme(id);a.notice.clear();
      a.screen=Screen::CoreSettings;a.row=1;a.capture("binary-cores-"+id+"-"+lang);
      a.screen=Screen::SettingsSection;a.category=3;a.row=2;a.powerScroll=0;a.capture("binary-power-"+id+"-"+lang);
      a.category=4;a.row=0;a.capture("binary-debug-"+id+"-"+lang);
      a.screen=Screen::ScreenshotSettings;a.row=2;a.capture("binary-screenshot-"+id+"-"+lang);
      // Check the selected segment surface independently of row focus.
      auto segmentPixel=[&](bool focused,std::optional<bool> value,bool supported){
        a.ui->rect(0,0,720,480,a.ui->bg);a.ui->binaryRow(a.locale.tr("setting.rumble"),128,focused,value,a.locale,supported);SDL_RenderPresent(a.ui->renderer);
        auto *p=static_cast<uint8_t *>(a.ui->surface->pixels)+140*a.ui->surface->pitch+540*4;return *reinterpret_cast<uint32_t *>(p);
      };
      auto off=segmentPixel(false,false,true);assert(off==segmentPixel(true,false,true));
      auto on=segmentPixel(false,true,true);assert(on==segmentPixel(true,true,true)&&on!=off);
      assert(segmentPixel(false,std::nullopt,true)==segmentPixel(false,false,false));
    }
  }
  for(const auto &invalid:std::vector<Json>{Json{{"enabled_cores",Json{{"fceumm","yes"}}}},Json{{"input_mapping",Json{{"fceumm",Json{{"8",16}}}}}},Json{{"wizard",Json{{"schema",1},{"step",8},{"completed",false},{"deferred",false}}}}}) {bool rejected=false;try{validatePreferences(invalid);}catch(...){rejected=true;}assert(rejected);}
  std::cout<<"PASS: theme preview/apply/cancel/failure, seven palettes, shared core selection, mapping/reset/persistence, setup skip/defer/restart/service return/rejected write, fresh defaults and explicit sound.\n";
}
