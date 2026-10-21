// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <cassert>
#include <iostream>
using namespace rondo;
class SnesDevice:public DeviceControl {
public:int sleeps=0;bool canSleep()const override{return true;}
 void sleep(bool,bool)override{++sleeps;}
};
int main(int argc,char **argv) {
 assert(argc==6);Options o;o.assets=argv[1];o.state=fs::path(argv[2])/"state";o.output=argv[2];o.core=argv[3];o.snesCore=argv[4];o.headless=true;o.volumes={{"snes-test",fs::absolute(argv[5])}};
 auto games=scan(o.volumes);assert(games.size()==1);auto game=games[0];assert(game.system=="SNES" && o.coreFor("SNES")==o.snesCore && o.coreFor("NES")==o.core);
 auto dir=o.state/"games"/game.id;std::vector<uint8_t> seed(8192,0x5a);atomicWrite(dir/"sram.bin",seed);
 {App a(o,true,game);a.ticks(90);
  std::cout<<"SNES frame "<<a.core->width<<"x"<<a.core->height<<" aspect "<<a.core->aspectRatio<<" rate "<<a.core->sampleRate<<" fps "<<a.core->fps<<"\n";
  assert(a.core->width>=256 && a.core->height>=224 && a.core->audioFrames>0);
  auto neutral=a.core->frame.at(0);a.gameButtons(0);
  for(unsigned button:{RETRO_DEVICE_ID_JOYPAD_A,RETRO_DEVICE_ID_JOYPAD_B,RETRO_DEVICE_ID_JOYPAD_X,RETRO_DEVICE_ID_JOYPAD_Y,RETRO_DEVICE_ID_JOYPAD_L,RETRO_DEVICE_ID_JOYPAD_R,RETRO_DEVICE_ID_JOYPAD_START,RETRO_DEVICE_ID_JOYPAD_SELECT,RETRO_DEVICE_ID_JOYPAD_UP,RETRO_DEVICE_ID_JOYPAD_DOWN,RETRO_DEVICE_ID_JOYPAD_LEFT,RETRO_DEVICE_ID_JOYPAD_RIGHT}) {
   a.core->buttons=a.gameButtons(uint16_t(1u<<button));a.ticks(3);assert(a.core->frame[0]!=neutral);
   a.core->buttons=0;a.ticks(3);assert(a.core->frame[0]==neutral);
  }
  a.core->save(0);assert(a.core->compatible(0));auto saved=a.core->stateHash();a.ticks(3);a.core->loadState(0);assert(a.core->stateHash()==saved);
  a.core->saveAutomatic();assert(a.core->compatibleAutomatic());a.ticks(3);a.core->loadAutomatic();assert(a.core->stateHash()==saved);
  auto device=std::make_unique<SnesDevice>();auto *d=device.get();a.device=std::move(device);a.sleep();assert(d->sleeps==1 && !a.core->paused);
  a.store.data["integer_scaling"]=true;a.capture("snes-integer");a.store.data["integer_scaling"]=false;a.capture("snes-aspect");
  a.action("MENU");a.row=4;a.action("A");a.row=1;a.action("A");assert(a.stop && a.gameClosed);a.core->flushSram();assert(readBytes(dir/"sram.bin")==seed);
 }
 {App a(o,true,game);a.core->flushSram();assert(readBytes(dir/"sram.bin")==seed);a.core->loadAutomatic();assert(a.core->compatible(0));}
 {App shell(o);shell.game=game;shell.rememberLaunch();}
 {App shell(o);assert(shell.screen==Screen::StartupResume);shell.action("B");shell.locale.select(o.assets,"ru");shell.capture("snes-library-ru");shell.rememberClosedGame();}
 {App shell(o);assert(shell.screen==Screen::Library);}
 {auto source=readBytes(games[0].path);std::vector<uint8_t> headered(512,0);headered.insert(headered.end(),source.begin(),source.end());
  auto path=o.state/"headered-card/technical.SMC";atomicWrite(path,headered);Options alias=o;alias.volumes={{"headered",path.parent_path()}};alias.state=o.state/"headered-state";
  auto entries=scan(alias.volumes);assert(entries.size()==1 && entries[0].system=="SNES");App a(alias,true,entries[0]);a.ticks(10);assert(a.core->width>=256 && a.core->height>=224);
 }
 {auto bytes=readBytes(games[0].path);bytes[0x7fd9]=2;bytes[0x7fdc]=bytes[0x7fdd]=bytes[0x7fde]=bytes[0x7fdf]=0;
  unsigned sum=510;for(auto byte:bytes)sum+=byte;uint16_t check=sum;bytes[0x7fdc]=uint8_t(~check);bytes[0x7fdd]=uint8_t((~check)>>8);bytes[0x7fde]=uint8_t(check);bytes[0x7fdf]=uint8_t(check>>8);
  auto path=o.state/"pal-card/technical.sfc";atomicWrite(path,bytes);Options pal=o;pal.volumes={{"pal",path.parent_path()}};pal.state=o.state/"pal-state";
  auto entries=scan(pal.volumes);App a(pal,true,entries[0]);a.ticks(5);assert(a.core->fps>49 && a.core->fps<51 && a.core->audioFrames>0);
 }
 {Options missing=o;missing.state=o.state/"missing-state";missing.snesCore=missing.state/"missing-core";App shell(missing);shell.action("A");assert(shell.screen==Screen::Recovery && !shell.launch);}
 std::cout<<"PASS: Supafaust SNES video/audio delivery, all 12 buttons, state roundtrip, SRAM restart, suspend, scaling and startup policy.\n";
}
