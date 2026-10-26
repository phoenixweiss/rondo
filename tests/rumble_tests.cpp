// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "core.hpp"
#include <cassert>
#include <algorithm>
#include <dlfcn.h>
#include <iostream>
using namespace rondo;
struct Probe {
  std::mutex mutex;
  std::condition_variable changed;
  std::vector<bool> writes;
  size_t observed=0;
  bool write(bool on){std::lock_guard<std::mutex> lock(mutex);writes.push_back(on);changed.notify_all();return true;}
  void expect(bool on){std::unique_lock<std::mutex> lock(mutex);
    // A short On pulse may already have ended when a busy test thread resumes.
    // Remember edges since the last observation, while allowing unchanged state.
    assert(changed.wait_for(lock,std::chrono::seconds(2),[&]{return
      std::find(writes.begin()+observed,writes.end(),on)!=writes.end()||(!writes.empty()&&writes.back()==on);}));
    observed=writes.size();
  }
  size_t count(){std::lock_guard<std::mutex> lock(mutex);return writes.size();}
};
int main(int argc,char **argv){
  assert(argc==2||argc==3);fs::path root=argv[1];fs::create_directories(root);
  Probe probe;
  {
    Rumble motor([&](bool on){return probe.write(on);});probe.expect(false);
    motor.update(65535,0);probe.expect(true);auto count=probe.count();
    for(int i=0;i<10;++i){motor.update(65535,1);std::this_thread::sleep_for(std::chrono::milliseconds(40));}
    assert(probe.count()==count); // Continuous rumble does not write per frame.
    motor.stop();probe.expect(false);
    motor.update(0,123);std::this_thread::sleep_for(std::chrono::milliseconds(350));
    probe.expect(true);probe.expect(false); // Observe the whole pulse after watchdog expiry.
    motor.update(1,0);probe.expect(true); // A new pulse after watchdog expiry.
  }
  probe.expect(false);
  {Rumble failed([&](bool on){probe.write(on);return !on;});failed.update(1,0);
   for(int i=0;i<100&&failed.available();++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));
   assert(!failed.available());probe.expect(false);failed.update(1,0);}
  auto library=fs::absolute(argv[0]).parent_path()/"rumble-fixture.so";
  auto handle=dlopen(library.c_str(),RTLD_NOW|RTLD_LOCAL);assert(handle);
  auto pulse=reinterpret_cast<bool (*)(unsigned,unsigned,uint16_t)>(dlsym(handle,"rondo_test_rumble"));assert(pulse);
  auto rom=root/"fixture.bin";atomicWrite(rom,{0});
  Game game{"rumble","fixture","NES","Rumble fixture",rom};
  auto motor=std::make_shared<Rumble>([&](bool on){return probe.write(on);});
  {
    Core core(library,game,root/"save",true);core.rumble=motor;
    assert(!pulse(1,RETRO_RUMBLE_STRONG,1)&&!pulse(0,42,1));
    assert(pulse(0,RETRO_RUMBLE_STRONG,40000));core.tick();probe.expect(true);
    assert(pulse(0,RETRO_RUMBLE_WEAK,1)&&pulse(0,RETRO_RUMBLE_STRONG,0));core.tick();probe.expect(true);
    assert(pulse(0,RETRO_RUMBLE_WEAK,0));core.silence();probe.expect(false);
    assert(pulse(0,RETRO_RUMBLE_WEAK,1));core.tick();probe.expect(true);
    core.paused=true;core.silence();probe.expect(false);assert(!pulse(0,RETRO_RUMBLE_WEAK,1));
    core.paused=false;core.rumbleEnabled=false;assert(!pulse(0,RETRO_RUMBLE_STRONG,1));
    core.tick();probe.expect(false);core.rumbleEnabled=true;
    assert(pulse(0,RETRO_RUMBLE_STRONG,1));core.tick();probe.expect(true);
  }
  probe.expect(false);dlclose(handle);
  if(argc==3) {
    // Only the isolated BaseOS preflight supplies this guard and fake sysfs.
    assert(std::string(argv[2])=="--synthetic-sysfs");
    assert(readBytes("/tmp/rondo-synthetic-hardware")==std::vector<uint8_t>({'t','e','s','t','-','o','n','l','y','\n'}));
    auto device=h700Rumble(true);assert(device&&device->available());
    auto expect=[&](bool on){for(int i=0;i<100;++i){auto bytes=readBytes("/sys/class/power_supply/axp2202-battery/moto");if(!bytes.empty()&&bytes[0]==(on?'1':'0'))return;SDL_Delay(5);}assert(false);};
    device->update(1,0);expect(true);device->stop();expect(false);
    device->update(0,1);expect(true);device.reset();expect(false);
  }
  std::cout<<"PASS: rumble ABI, independent channels, pause/disable/destruction, driver failure, watchdog and coalesced writes.\n";
}
