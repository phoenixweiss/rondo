#include "app.hpp"
#include <cassert>
#include <iostream>
using namespace rondo;
class GbaDevice:public DeviceControl {
public:int sleeps=0;bool canSleep()const override{return true;}
 void sleep(bool,bool)override{++sleeps;}
};
int main(int argc,char **argv) {
 assert(argc==6);Options o;o.assets=argv[1];o.state=fs::path(argv[2])/"state";o.output=argv[2];o.core=argv[3];o.gbaCore=argv[4];o.headless=true;o.volumes={{"gba-test",fs::absolute(argv[5])}};
 auto games=scan(o.volumes);assert(games.size()==1);auto game=games[0];assert(game.system=="GBA" && o.coreFor("GBA")==o.gbaCore && o.coreFor("NES")==o.core);
 auto dir=o.state/"games"/game.id;std::vector<uint8_t> seed(131072,0xff);seed[0]=0x5a;atomicWrite(dir/"sram.bin",seed);
 {App a(o,true,game);a.ticks(90);assert(a.core->width==240 && a.core->height==160 && a.core->audioFrames>0 && a.core->audioPeak>0 && a.core->sampleRate==32768);
  auto neutral=a.core->frame[0];a.gameButtons(0);
  for(unsigned button:{RETRO_DEVICE_ID_JOYPAD_A,RETRO_DEVICE_ID_JOYPAD_B,RETRO_DEVICE_ID_JOYPAD_L,RETRO_DEVICE_ID_JOYPAD_R,RETRO_DEVICE_ID_JOYPAD_START,RETRO_DEVICE_ID_JOYPAD_SELECT}) {
   a.core->buttons=a.gameButtons(uint16_t(1u<<button));a.ticks(2);assert(a.core->frame[0]!=neutral);
   a.core->buttons=0;a.ticks(2);assert(a.core->frame[0]==neutral);
  }
  a.core->save(0);assert(a.core->compatible(0));auto saved=a.core->stateHash();a.ticks(3);a.core->loadState(0);assert(a.core->stateHash()==saved);
  a.core->saveAutomatic();assert(a.core->compatibleAutomatic());a.ticks(3);a.core->loadAutomatic();assert(a.core->stateHash()==saved);
  a.core->volume=0;a.core->audioPeak=0;a.ticks(2);assert(a.core->audioPeak==0);
  auto device=std::make_unique<GbaDevice>();auto *d=device.get();a.device=std::move(device);a.sleep();assert(d->sleeps==1 && !a.core->paused);
  a.store.data["integer_scaling"]=true;a.capture("gba-integer");a.store.data["integer_scaling"]=false;a.capture("gba-aspect");
  a.rememberLaunch();
  a.action("MENU");a.row=4;a.action("A");a.row=1;a.action("A");assert(a.stop);a.core->flushSram();assert(readBytes(dir/"sram.bin")==seed);
 }
 {App a(o,true,game);a.core->flushSram();assert(readBytes(dir/"sram.bin")==seed);a.core->loadAutomatic();assert(a.core->compatible(0));}
 {App shell(o);assert(shell.screen==Screen::StartupResume);shell.action("B");shell.action("MENU");assert(shell.screen==Screen::Settings);shell.action("B");shell.action("Y");assert(shell.screen==Screen::Search);shell.action("B");shell.locale.select(o.assets,"ru");shell.capture("gba-library-ru");shell.action("MENU");shell.capture("settings-menu-key-ru");shell.action("B");shell.action("Y");shell.capture("search-y-key-ru");}
 std::cout<<"PASS: actual gpSP GBA video/audio, A/B/L/R/Start/Select, manual/auto states, SRAM restart, mute, suspend, scaling and swapped library keys.\n";
}
