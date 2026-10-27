#include "state.hpp"
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include "psx.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include <cstring>
using namespace rondo;
class PsxDevice:public DeviceControl {public:int sleeps=0;bool canSleep()const override{return true;}void sleep(bool,bool)override{++sleeps;}};
int main(int argc,char **argv) {
 assert(argc==6);Options o;o.assets=argv[1];o.state=fs::path(argv[2])/"state";o.output=argv[2];o.core=argv[3];o.psxCore=argv[4];o.psxTestHle=false;o.headless=true;o.volumes={{"PSXFIXTURE",fs::absolute(argv[5])}};
 {Store initial(o.state);auto cfg=initial.data;cfg["psx_firmware"]="hle";initial.commit(cfg);}
 auto games=scan(o.volumes);assert(games.size()==1&&games[0].system=="PSX");auto game=games[0];assert(psxDiscs(game.path).size()==2);assert(o.coreFor("PSX")==o.psxCore);
 {bool rejected=false;try{psxBiosIdentity(o.state/"missing-bios");}catch(...){rejected=true;}assert(rejected);}
 {auto bytes=readBytes(game.path);auto p=o.state/"bad.m3u";std::string bad="../outside.chd\n";atomicWrite(p,{bad.begin(),bad.end()});bool rejected=false;try{psxDiscs(p);}catch(...){rejected=true;}assert(rejected);}
 {auto list=o.state/"normalized/normalized.M3U";fs::create_directories(list.parent_path());auto images=psxDiscs(game.path);
  for(size_t i=0;i<images.size();++i)fs::copy_file(images[i],list.parent_path()/("disc"+std::to_string(i)+".CHD"));
  std::string text="\xef\xbb\xbf# comment\r\n  disc0.CHD  \r\n\r\ndisc1.CHD\r\n";atomicWrite(list,{text.begin(),text.end()});
  Game normalized=game;normalized.path=list;normalized.id="normalized-playlist";App a(o,true,normalized);a.ticks(2);assert(a.core->discCount()==2);a.core->changeDisc(1);
 }
 std::vector<uint8_t> card;
 {App a(o,true,game);a.ticks(45);assert(a.core->usesHle());assert(a.core->audioFrames>0&&a.core->width>=256&&a.core->height>=224&&a.core->discCount()==2);
  assert(!fs::exists(a.store.gameDir(game)/"pcsx-card2.mcd"));
  auto handle=dlopen(o.psxCore.c_str(),RTLD_NOW|RTLD_LOCAL);assert(handle);auto memory=reinterpret_cast<void *(*)(unsigned)>(dlsym(handle,"retro_get_memory_data"));auto size=reinterpret_cast<size_t (*)(unsigned)>(dlsym(handle,"retro_get_memory_size"));assert(memory&&size);
  auto pressed=static_cast<unsigned short *>(dlsym(handle,"in_keystate"));// GNU builds export only the public libretro ABI. The optional native
  // probe additionally checks the core's internal digital pad translation.
  a.gameButtons(0);
  for(auto mapping:{std::pair<unsigned,unsigned>{RETRO_DEVICE_ID_JOYPAD_B,14},{RETRO_DEVICE_ID_JOYPAD_A,13},{RETRO_DEVICE_ID_JOYPAD_Y,15},{RETRO_DEVICE_ID_JOYPAD_X,12},{RETRO_DEVICE_ID_JOYPAD_UP,4},{RETRO_DEVICE_ID_JOYPAD_RIGHT,5},{RETRO_DEVICE_ID_JOYPAD_DOWN,6},{RETRO_DEVICE_ID_JOYPAD_LEFT,7},{RETRO_DEVICE_ID_JOYPAD_START,3},{RETRO_DEVICE_ID_JOYPAD_SELECT,0},{RETRO_DEVICE_ID_JOYPAD_L,10},{RETRO_DEVICE_ID_JOYPAD_R,11},{RETRO_DEVICE_ID_JOYPAD_L2,8},{RETRO_DEVICE_ID_JOYPAD_R2,9}}) {
   a.core->buttons=a.gameButtons(uint16_t(1u<<mapping.first));a.ticks(1);if(pressed)assert(pressed[0]==(1u<<mapping.second));a.core->buttons=0;a.ticks(1);if(pressed)assert(pressed[0]==0);
  }
  std::cout<<(pressed?"PASS: 14 digital pad translations observed in core.\n":"INFO: core hides internal pad state; hardware input acceptance remains required.\n");
  // The native pinned core exports these probes; Linux hides internal symbols.
  auto analogLeft=reinterpret_cast<int (*)[2]>(dlsym(handle,"in_analog_left"));
  auto analogRight=reinterpret_cast<int (*)[2]>(dlsym(handle,"in_analog_right"));
  a.core->psxController(true);a.gameInputBlocked=false;
  a.gameInput(0,{-32768,32767,24000,-24000});a.ticks(1);
  if(analogLeft&&analogRight){assert(analogLeft[0][0]==0&&analogLeft[0][1]>=254);assert(analogRight[0][0]>200&&analogRight[0][1]<40);}
  a.action("MENU");assert((a.core->axes==std::array<int16_t,4>{}));a.action("B");
  a.gameInput(0,{20000,0,0,0});assert(a.gameInputBlocked&&a.core->axes[0]==0);
  a.gameInput(0,{});assert(!a.gameInputBlocked);a.gameInput(0,{20000,0,0,0});assert(a.core->axes[0]==20000);
  a.gameInput(0,{});a.ticks(1);if(analogLeft)assert(analogLeft[0][0]==128);
  a.core->psxController(false);
  int oldWidth=a.core->width,oldHeight=a.core->height;
  for(auto size:{std::pair<int,int>{256,240},{320,240},{512,240},{640,480},{640,512},{320,256}}) {
    a.core->width=size.first;a.core->height=size.second;
    auto cfg=a.store.data;cfg["integer_scaling"]=false;a.store.commit(cfg);
    auto rect=a.gameViewport();assert(rect.x==40&&rect.y==0&&rect.w==640&&rect.h==480);
    cfg["integer_scaling"]=true;a.store.commit(cfg);rect=a.gameViewport();
    assert(rect.x==40&&rect.y==0&&rect.w==640&&rect.h==480);
    cfg["game_options"][game.id]["integer_scaling"]=true;a.store.commit(cfg);rect=a.gameViewport();
    assert(rect.x==40&&rect.y==0&&rect.w==640&&rect.h==480);
  }
  a.core->width=oldWidth;a.core->height=oldHeight;
  auto cfg=a.store.data;cfg["integer_scaling"]=false;a.store.commit(cfg);
  auto originalFrame=a.core->frame;
  for(auto size:{std::pair<int,int>{256,240},{320,240},{512,240},{640,480},{640,512}}) {
    a.core->width=size.first;a.core->height=size.second;a.core->frame.assign(size.first*size.second,0xff123456);
    a.notice.clear();a.draw();
    auto surface=a.ui->surface;
    auto pixel=[&](int x,int y){return reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(surface->pixels)+y*surface->pitch)[x];};
    // Assert rendered pixels, not just the intended rectangle: the old nested
    // raw-aspect fit passed geometry assertions but letterboxed 512x240 again.
    assert(pixel(40,0)==0xff123456&&pixel(679,479)==0xff123456);
    assert(pixel(39,240)==0xff000000&&pixel(680,240)==0xff000000);
  }
  a.capture("psx-aspect-512-corrected");
  a.core->width=oldWidth;a.core->height=oldHeight;a.core->frame=std::move(originalFrame);
  auto geometry=a.core->videoGeometry();assert(geometry["aspect_ratio"].get<double>()>1.3&&geometry.contains("pitch_bytes"));

  auto *ram=static_cast<uint8_t *>(memory(RETRO_MEMORY_SYSTEM_RAM));assert(ram);auto counter=[&]{return std::vector<uint8_t>(ram+0x20000,ram+0x20004);};auto current=counter();assert(current!=std::vector<uint8_t>(4,0));
  a.core->save(0);assert(a.core->compatible(0));a.ticks(3);assert(counter()!=current);a.core->loadState(0);assert(counter()==current);
  auto *m=static_cast<uint8_t *>(memory(RETRO_MEMORY_SAVE_RAM));assert(m&&size(RETRO_MEMORY_SAVE_RAM)==128*1024);m[1234]=0x5a;a.core->flushSram();card={m,m+size(RETRO_MEMORY_SAVE_RAM)};
  a.action("MENU");a.row=int(GameMenuItem::Settings);a.action("A");
  a.action("DOWN");assert(a.row==1);a.action("RIGHT");assert(a.store.data["game_options"][game.id]["psx_controller"]=="digital");a.action("RIGHT");assert(a.psxDualshock());
  a.capture("psx-game-controller");a.action("RIGHT");assert(a.store.data["game_options"][game.id]["integer_scaling"]==true&&!a.store.data["game_options"][game.id].contains("psx_controller"));
  a.row=0;assert(a.infoKey()=="disc");a.action("RIGHT");assert(a.core->discIndex()==1);assert(a.core->paused);a.locale.select(o.assets,"ru");a.capture("psx-disc-ru");a.locale.select(o.assets,"en");a.capture("psx-disc-en");a.action("X");a.capture("psx-disc-info");a.action("B");
  a.core->save(1);assert(readState(a.store.gameDir(game)/"slot-1.state",StateRead::Metadata).meta["disc"]==1);a.core->changeDisc(0);a.core->loadState(1);assert(a.core->discIndex()==1);a.core->saveAutomatic();
  auto device=std::make_unique<PsxDevice>();auto *d=device.get();a.device=std::move(device);a.sleep();assert(d->sleeps==1&&a.core->discIndex()==1);
  a.core->deleteSlot(0);assert(!fs::exists(a.store.gameDir(game)/"slot-0.state"));dlclose(handle);
 }
 {App a(o,true,game);assert(a.core->discIndex()==0);a.core->loadAutomatic();assert(a.core->discIndex()==1&&a.core->compatible(1));assert(readBytes(a.store.gameDir(game)/"sram.bin")==card);}
 {App shell(o);shell.game=game;shell.rememberLaunch();}
 {App shell(o);assert(shell.screen==Screen::StartupResume);shell.action("B");shell.action("A");assert(shell.screen==Screen::ResumeChoice);shell.action("B");shell.rememberClosedGame();}
 // Automatic fallback and warning persistence use ordinary launch options.
 {auto missing=o;missing.state=o.state/"missing-bios-state";
  {Store st(missing.state);auto cfg=st.data;cfg["psx_firmware"]="hle";st.commit(cfg);}
  {App a(missing);a.locale.select(o.assets,"ru");a.action("A");assert(a.screen==Screen::PsxWarning&&!a.launch);a.capture("psx-hle-warning-ru");a.locale.select(o.assets,"en");a.capture("psx-hle-warning-en");
   a.action("B");assert(a.screen==Screen::Library&&!a.store.data.value("psx_hle_warning_acknowledged",false));
   a.action("A");assert(a.screen==Screen::PsxWarning);a.action("A");assert(a.launch&&a.store.data.at("psx_hle_warning_acknowledged")==true);}
  {App a(missing);a.action("A");assert(a.launch&&a.screen==Screen::Launching);}
  // A failed settings write must not silently consume the warning.
  missing.state=o.state/"warning-write-failed";{Store st(missing.state);auto cfg=st.data;cfg["psx_firmware"]="hle";st.commit(cfg);}App a(missing);a.action("A");assert(a.screen==Screen::PsxWarning);
  a.requestSettings=[](const Json &){return false;};a.action("A");assert(!a.launch&&a.screen==Screen::PsxWarning&&!a.store.data.value("psx_hle_warning_acknowledged",false));
 }
 {auto startup=o;startup.state=o.state/"startup-warning";
  {Store st(startup.state);auto cfg=st.data;cfg["psx_firmware"]="hle";st.commit(cfg);}
  {App a(startup,true,game);a.ticks(2);a.core->saveAutomatic();}
  {App a(startup);a.game=game;a.rememberLaunch();}
  {App a(startup);assert(a.screen==Screen::StartupResume);a.action("A");assert(a.screen==Screen::PsxWarning&&!a.launch&&a.resume);a.action("A");assert(a.launch&&a.resume);}
 }
 {auto bios=o.state/"bios-signature-test";fs::create_directories(bios);auto hle=psxContentHash(game.path,bios,true);
  std::vector<uint8_t> dummy(512*1024);atomicWrite(bios/"scph5501.bin",dummy);assert(!psxBiosAvailable(bios)&&psxContentHash(game.path,bios,true)==hle);
  // Original synthetic bytes only: test selection/identity, never execute this
  // structural fixture as a BIOS or claim retail BIOS boot verification.
  dummy[2]=8;dummy[3]=0x3c;dummy[4]=0x3f;atomicWrite(bios/"scph5501.bin",dummy);
  assert(psxBiosAvailable(bios)&&psxContentHash(game.path,bios)!=hle&&psxContentHash(game.path,bios,true)==hle);
  auto real=o;real.state=o.state/"bios-present-shell";real.psxBios=bios;{Store st(real.state);auto cfg=st.data;cfg["psx_firmware"]="bios";st.commit(cfg);}App a(real);a.action("A");assert(a.launch&&a.screen==Screen::Launching&&!a.store.data.value("psx_hle_warning_acknowledged",false));
 }
 {auto opts=o;opts.state=o.state/"openbios-runtime";
  {App a(opts,true,game);assert(!a.core->usesHle());a.ticks(1200);
   auto handle=dlopen(o.psxCore.c_str(),RTLD_NOW|RTLD_LOCAL);assert(handle);auto memory=reinterpret_cast<void *(*)(unsigned)>(dlsym(handle,"retro_get_memory_data"));assert(memory);
   auto *ram=static_cast<uint8_t *>(memory(RETRO_MEMORY_SYSTEM_RAM));assert(ram);uint32_t color=0;memcpy(&color,ram+0x20004,4);assert(color==255);assert(a.core->audioFrames>0);
   a.core->save(0);auto saved=std::vector<uint8_t>(ram+0x20000,ram+0x20004);a.ticks(5);a.core->loadState(0);assert(std::vector<uint8_t>(ram+0x20000,ram+0x20004)==saved);
   a.core->saveAutomatic();auto device=std::make_unique<PsxDevice>();a.device=std::move(device);a.sleep();dlclose(handle);
  }
  {App a(opts,true,game);assert(a.core->compatibleAutomatic());a.core->loadAutomatic();a.ticks(2);}
  {Store st(opts.state);auto cfg=st.data;cfg["psx_firmware"]="hle";st.commit(cfg);}
  {App a(opts,true,game);assert(a.core->usesHle()&&!a.core->compatible(0)&&!a.core->compatibleAutomatic());}
 }
 {auto opts=o;opts.state=o.state/"core-options-ui";
  {App a(opts);a.action("MENU");if(a.screen==Screen::LibraryMenu)a.action("A");a.settingsFocus=7;a.action("A");assert(a.screen==Screen::CoreSettings);a.locale.select(o.assets,"ru");a.capture("core-settings-ru");a.action("A");assert(a.screen==Screen::PsxCoreSettings);a.capture("psx-core-settings-ru");
   assert(a.store.data.at("psx_firmware")=="openbios");a.action("RIGHT");assert(a.store.data.at("psx_firmware")=="bios");a.action("RIGHT");assert(a.store.data.at("psx_firmware")=="hle");a.action("LEFT");assert(a.store.data.at("psx_firmware")=="bios");
   a.action("DOWN");assert(a.infoKey()=="psx_controller");a.action("RIGHT");assert(a.store.data.at("psx_controller")=="dualshock");
   a.capture("psx-controls-ru");
   a.action("X");a.capture("psx-core-info-ru");a.action("B");a.locale.select(o.assets,"en");a.capture("psx-core-settings-en");a.action("B");assert(a.screen==Screen::CoreSettings);a.action("DOWN");a.action("LEFT");assert(!a.store.data.at("left_stick_dpad").get<bool>());a.capture("left-stick-ru");a.action("B");assert(a.screen==Screen::Settings&&a.settingsFocus==7);}
  {App a(opts);assert(a.store.data.at("psx_firmware")=="bios");a.action("A");assert(a.screen==Screen::PsxFirmwareError&&!a.launch);a.locale.select(o.assets,"ru");a.capture("psx-user-bios-missing-ru");a.action("A");assert(a.screen==Screen::PsxCoreSettings);}
 }
 {auto opts=o;opts.state=o.state/"bios-picker";opts.psxBios=opts.state/"BIOS/PSX";
  fs::create_directories(opts.psxBios);std::vector<uint8_t> fixture(512*1024);fixture[2]=8;fixture[3]=0x3c;fixture[4]=0x3f;
  for(int i=0;i<9;++i){fixture[20]=uint8_t(i);atomicWrite(opts.psxBios/("scph-test-"+std::to_string(i)+".bin"),fixture);}
  atomicWrite(opts.psxBios/"scph-short.bin",{1,2,3});assert(psxBiosFiles(opts.psxBios).size()==9);
  auto selected=opts.psxBios/"scph-test-0.bin";auto identity=psxBiosIdentity(selected);
  auto isolated=psxPrepareBios(selected,opts.state/"BIOSSelection");assert(psxBiosFiles(isolated).size()==1&&psxBiosIdentity(isolated)==identity);
  fixture[20]=42;atomicWrite(opts.psxBios/"scph-test-8.bin",fixture);assert(psxBiosIdentity(selected)==identity);
  atomicWrite(isolated/"scph-other.bin",fixture);bool rejected=false;try{psxPrepareBios(selected,opts.state/"BIOSSelection");}catch(...){rejected=true;}assert(rejected);fs::remove(isolated/"scph-other.bin");
  {Store st(opts.state);auto next=st.data;next["psx_firmware"]="bios";st.commit(next);
   next=st.data;next["psx_bios_file"]="../escape.bin";rejected=false;try{st.commit(next);}catch(...){rejected=true;}assert(rejected&&st.data["psx_bios_file"]=="auto");}
  {App a(opts);a.screen=Screen::PsxCoreSettings;a.row=2;a.locale.select(o.assets,"ru");a.capture("bios-file-setting-ru");
   a.action("A");assert(a.screen==Screen::PsxBiosFiles&&a.row==0);a.capture("bios-files-ru");auto before=a.store.data;
   a.action("DOWN");a.action("B");assert(a.screen==Screen::PsxCoreSettings&&a.row==2&&a.store.data==before);
   a.action("A");a.action("R1");assert(a.row==5);a.capture("bios-files-scrolled-ru");a.action("L1");a.action("DOWN");
   a.requestSettings=[](const Json &){return false;};a.action("A");assert(a.screen==Screen::PsxBiosFiles&&a.store.data==before);a.requestSettings={};
   a.action("A");assert(a.screen==Screen::PsxCoreSettings&&a.store.data["psx_bios_file"]=="scph-test-0.bin");
   a.locale.select(o.assets,"en");a.capture("bios-file-setting-en");a.action("A");assert(a.row==1);a.capture("bios-files-en");a.action("B");}
  {App a(opts);assert(a.store.data["psx_bios_file"]=="scph-test-0.bin");fs::remove(selected);a.action("A");assert(a.screen==Screen::PsxFirmwareError&&!a.launch&&a.store.data["psx_bios_file"]=="scph-test-0.bin");a.locale.select(o.assets,"ru");a.capture("bios-selected-missing-ru");}
  {Store st(opts.state);auto next=st.data;next["psx_firmware"]="hle";st.commit(next);}
  {App a(opts,true,game);assert(a.core->usesHle()&&!fs::exists(selected));a.ticks(2);}
 }
 std::cout<<"PASS: PSX CHD-only playlist, bounded content identity, MIPS video/RAM execution, audio callback, state/disc roundtrip, memory card restart, pause/sleep, startup continuation, OpenBIOS MIPS boot/state/suspend, one-time HLE warning, persistent core settings and firmware state isolation.\n";
}
