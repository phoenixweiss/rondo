#include "app.hpp"
#include <cassert>
#include <iostream>
using namespace rondo;
class GbDevice:public DeviceControl {
public:int sleeps=0;bool canSleep()const override{return true;}
 void sleep(bool,bool)override{++sleeps;}
};
int main(int argc,char **argv) {
 assert(argc==6);fs::path assets=argv[1],root=argv[2],fixtures=argv[5];fs::create_directories(root);
 Options o;o.assets=assets;o.state=root/"state";o.output=root;o.core=argv[3];o.gbCore=argv[4];o.headless=true;o.volumes={{"gb-test",fixtures}};
 auto games=scan(o.volumes);assert(games.size()==2);
 for(const auto &game:games) {
  assert(game.system=="GB" || game.system=="GBC");
  assert(o.coreFor(game.system)==o.gbCore && o.coreFor("NES")==o.core && o.coreFor("GBA").empty());
  auto dir=o.state/"games"/game.id;fs::create_directories(dir);
  std::vector<uint8_t> seed(32768,0);seed[0]=0x5a;atomicWrite(dir/"sram.bin",seed);
  { App a(o,true,game);a.ticks(60);assert(a.core->width==160 && a.core->height==144 && a.core->audioFrames>0);
    a.core->save(0);auto manual=readBytes(a.store.gameDir(game)/"slot-0.json");
    a.core->saveAutomatic();assert(a.core->compatibleAutomatic());
    a.core->flushSram();assert(readBytes(dir/"sram.bin")==seed);
    assert(readBytes(a.store.gameDir(game)/"slot-0.json")==manual);
    auto device=std::make_unique<GbDevice>();auto *d=device.get();a.device=std::move(device);
    auto frames=a.core->frames;a.sleep();assert(d->sleeps==1 && !a.core->paused);a.ticks(2);assert(a.core->frames==frames+2);
    a.store.data["integer_scaling"]=true;a.capture(game.system+"-integer");
    a.store.data["integer_scaling"]=false;a.capture(game.system+"-aspect");
    assert(fs::file_size(dir/"rtc.bin")>0);
    auto config=a.store.data;config["recent_games"]=recentAfterLaunch(config["recent_games"],game,o.volumes);a.store.commit(config);
    a.action("MENU");a.row=4;a.action("A");a.row=1;a.action("A");assert(a.stop);
  }
  auto rtc=readBytes(dir/"rtc.bin");seed[0]=0x33;atomicWrite(dir/"sram.bin",seed);
  { App restored(o,true,game);restored.core->flushSram();assert(readBytes(dir/"sram.bin")==seed);
    assert(readBytes(dir/"rtc.bin")==rtc);
    restored.core->loadState(0);restored.core->flushSram();assert(readBytes(dir/"sram.bin")[0]==0x5a);
  }
  seed[0]=0x44;atomicWrite(dir/"sram.bin",seed);
  { App restored(o,true,game);restored.core->loadAutomatic();restored.core->flushSram();
    assert(readBytes(dir/"sram.bin")[0]==0x5a && restored.core->compatible(0));
  }
  { App shell(o);assert(shell.screen==Screen::StartupResume && shell.game.id==game.id);shell.action("B");assert(shell.screen==Screen::Library); }
 }
 std::cout<<"PASS: actual Gambatte DMG/CGB, video/audio, manual/auto states, independent SRAM/RTC, resume/startup, integer/aspect, suspend continuity and NES routing.\n";
}
