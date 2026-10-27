// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <cassert>
#include <iostream>
#include <dlfcn.h>
using namespace rondo;
class MdDevice:public DeviceControl {
public:int sleeps=0;bool canSleep()const override{return true;}
 void sleep(bool,bool)override{++sleeps;}
};
int main(int argc,char **argv) {
 assert(argc==6);Options o;o.assets=argv[1];o.state=fs::path(argv[2])/"state";o.output=argv[2];o.core=argv[3];o.mdCore=argv[4];o.headless=true;o.volumes={{"md-test",fs::absolute(argv[5])}};
 auto games=scan(o.volumes);assert(games.size()==1);auto game=games[0];assert(game.system=="MD" && o.coreFor("MD")==o.mdCore && o.coreFor("NES")==o.core);
 auto dir=o.state/"games"/game.id;std::vector<uint8_t> seed(16384,0x5a);atomicWrite(dir/"sram.bin",seed);
 {App a(o,true,game);a.ticks(90);
  std::cout<<"MD frame "<<a.core->width<<"x"<<a.core->height<<" aspect "<<a.core->aspectRatio<<" rate "<<a.core->sampleRate<<" fps "<<a.core->fps<<"\n";
  assert(a.core->width>=256 && a.core->height>=224 && a.core->audioFrames>0);
  auto neutral=a.core->frame.at(0);a.gameButtons(0);
  for(unsigned button:{RETRO_DEVICE_ID_JOYPAD_A,RETRO_DEVICE_ID_JOYPAD_B,RETRO_DEVICE_ID_JOYPAD_X,RETRO_DEVICE_ID_JOYPAD_Y,RETRO_DEVICE_ID_JOYPAD_L,RETRO_DEVICE_ID_JOYPAD_R,RETRO_DEVICE_ID_JOYPAD_START,RETRO_DEVICE_ID_JOYPAD_SELECT,RETRO_DEVICE_ID_JOYPAD_UP,RETRO_DEVICE_ID_JOYPAD_DOWN,RETRO_DEVICE_ID_JOYPAD_LEFT,RETRO_DEVICE_ID_JOYPAD_RIGHT}) {
   a.core->buttons=a.gameButtons(uint16_t(1u<<button));a.ticks(3);if(a.core->frame[0]==neutral) {std::cerr<<"No response for button "<<button<<"\n";}assert(a.core->frame[0]!=neutral);
   a.core->buttons=0;a.ticks(3);assert(a.core->frame[0]==neutral);
  }
  auto handle=dlopen(o.mdCore.c_str(),RTLD_NOW|RTLD_LOCAL);assert(handle);
  auto memory=reinterpret_cast<void *(*)(unsigned)>(dlsym(handle,"retro_get_memory_data"));assert(memory);
  auto ramHash=[&]{auto *ram=static_cast<uint8_t *>(memory(RETRO_MEMORY_SYSTEM_RAM));assert(ram);return hashBytes({ram,ram+65536});};
  // PicoDrive rebuilds transient renderer data on load; compare emulated RAM,
  // not a reserialized blob that includes those transient fields.
  a.core->save(0);assert(a.core->compatible(0));auto saved=ramHash();a.ticks(3);assert(ramHash()!=saved);a.core->loadState(0);assert(ramHash()==saved);
  a.core->saveAutomatic();assert(a.core->compatibleAutomatic());a.ticks(3);a.core->loadAutomatic();assert(ramHash()==saved);dlclose(handle);
  auto device=std::make_unique<MdDevice>();auto *d=device.get();a.device=std::move(device);a.sleep();assert(d->sleeps==1 && !a.core->paused);
  a.store.data["integer_scaling"]=true;a.capture("md-integer");a.store.data["integer_scaling"]=false;a.capture("md-aspect");
  a.action("MENU");a.row=int(GameMenuItem::Exit);a.action("A");a.row=1;a.action("A");assert(a.stop && a.gameClosed);a.core->flushSram();assert(readBytes(dir/"sram.bin")==seed);
 }
 {App a(o,true,game);a.core->flushSram();assert(readBytes(dir/"sram.bin")==seed);a.core->loadAutomatic();assert(a.core->compatible(0));}
 {App shell(o);shell.game=game;shell.rememberLaunch();}
 {App shell(o);assert(shell.screen==Screen::StartupResume);shell.action("B");shell.locale.select(o.assets,"ru");shell.capture("md-library-ru");shell.rememberClosedGame();}
 {App shell(o);assert(shell.screen==Screen::Library);}
 {auto bytes=readBytes(games[0].path);bytes[0x1f0]='E';bytes[0x1f1]=' ';bytes[0x1f2]=' ';
  auto path=o.state/"pal-card/technical.MD";atomicWrite(path,bytes);Options pal=o;pal.volumes={{"pal",path.parent_path()}};pal.state=o.state/"pal-state";
  auto entries=scan(pal.volumes);assert(entries.size()==1 && entries[0].system=="MD");App a(pal,true,entries[0]);a.ticks(5);assert(a.core->fps>49 && a.core->fps<51 && a.core->audioFrames>0);
 }
 {Options missing=o;missing.state=o.state/"missing-state";missing.mdCore=missing.state/"missing-core";App shell(missing);shell.action("A");assert(shell.screen==Screen::Recovery && !shell.launch);}
 std::cout<<"PASS: PicoDrive MD video/audio delivery, all 12 buttons, state roundtrip, SRAM restart, suspend, scaling and startup policy.\n";
}
