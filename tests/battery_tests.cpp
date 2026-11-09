// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "ui.hpp"
#include <cassert>
#include <iostream>
using namespace rondo;
int main(int argc, char **argv) {
  assert(argc == 3); fs::path out=argv[2];fs::create_directories(out);
  UI ui(argv[1],true); Locale locale(argv[1]);
  BatteryDisplayFilter filter;
  auto sample = [&](int raw, uint64_t time, const char *status = "Discharging", bool enabled = true) {
    Telemetry t;
    t.capacity = raw;
    t.batteryStatus = status;
    return filter.update(t, enabled, time);
  };
  assert(sample(92, 0, "Charging") == 92);
  assert(sample(100, 5000, "Full") == 92);
  assert(sample(100, 19999, "Full") == 92);
  for (int i=0; i<8; ++i) assert(sample(100, 20000 + 5000*i, "Full") == 93+i);
  // The confirmed 100% holds for one minute, then a persistent change ramps down.
  assert(sample(89, 60000) == 100);
  assert(sample(89, 114999) == 100);
  assert(sample(89, 115000) == 99);
  assert(sample(5, 115001) == 5);
  assert(sample(100, 120000, "Full", false) == 100);
  assert(sample(100, 125000) == 100);
  assert(sample(89, 130000) == 100);
  assert(sample(100, 135000) == 100);
  assert(sample(99, 140000) == 99);
  assert(!sample(101, 145000));
  assert(sample(89, 150000) == 89);

  Telemetry observed; observed.capacity=100;
  ui.status("12:34", observed);
  observed.capacity=89;
  ui.status("12:34", observed);
  assert(ui.batteryDisplay["source_percent"]==89 && ui.batteryDisplay["displayed_percent"]==100);
  ui.smoothBatteryLevel=false;
  ui.status("12:34", observed);
  assert(ui.batteryDisplay["source_percent"]==89 && ui.batteryDisplay["displayed_percent"]==89);
  observed.capacity=100;
  ui.battery(observed,true);ui.present();
  assert(ui.batteryDisplay["displayed_percent"]==100 && ui.batteryDisplay["label"]=="100%");
  observed.capacity=10;ui.battery(observed,true);ui.present();
  assert(ui.batteryDisplay["source_percent"]==10 && ui.batteryDisplay["displayed_percent"]==10 && ui.batteryDisplay["label"]=="10%");
  assert(ui.batteryDisplay["submitted_monotonic_ms"].is_number_unsigned());
  ui.battery(observed,false);ui.present();
  assert(ui.batteryDisplay["source_percent"]==10 && ui.batteryDisplay["displayed_percent"].is_null() && ui.batteryDisplay["label"].is_null());
  observed.capacity.reset();ui.battery(observed,true);ui.present();
  assert(ui.batteryDisplay["source_percent"].is_null() && ui.batteryDisplay["displayed_percent"].is_null() && ui.batteryDisplay["label"]=="—");
  auto pixels=[&](int x,int y,int w,int h) {
    std::vector<Uint32> result;
    for(int row=y;row<y+h;++row) {
      auto *p=reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(ui.surface->pixels)+row*ui.surface->pitch);
      result.insert(result.end(),p+x,p+x+w);
    }
    return result;
  };
  for(const auto *language:{"en","ru"}) {
    locale.select(argv[1],language);
    std::map<int,std::vector<Uint32>> images;
    for(int level:{0,5,15,25,50,75,100}) {
      Telemetry telemetry;telemetry.capacity=level;
      ui.frame(locale.tr("screen.library"),"","Etc/UTC");
      ui.rect(575,24,125,32,ui.bg);ui.battery(telemetry,true);
      images[level]=pixels(599,34,32,14);ui.save(out/(std::string(language)+"-"+std::to_string(level)+".png"));
      auto icon=images[level];ui.rect(575,24,125,32,ui.bg);ui.battery(telemetry,false);
      assert(pixels(599,34,32,14)==icon);
      auto blank=SDL_MapRGBA(ui.surface->format,ui.bg.r,ui.bg.g,ui.bg.b,ui.bg.a);
      auto percentage=pixels(639,24,60,32);assert(std::all_of(percentage.begin(),percentage.end(),[&](auto p){return p==blank;}));
      telemetry.batteryStatus="Charging";ui.rect(575,24,125,32,ui.bg);ui.battery(telemetry,false);
      assert(pixels(599,34,32,14)!=icon);
      auto bolt=pixels(606,29,16,25);
      auto cream=SDL_MapRGBA(ui.surface->format,ui.cream.r,ui.cream.g,ui.cream.b,ui.cream.a);
      assert(std::count(bolt.begin(),bolt.end(),cream)>60);
      auto tip=pixels(606,29,16,4), tail=pixels(606,48,16,6);
      assert(std::count(tip.begin(),tip.end(),cream)>0 && std::count(tail.begin(),tail.end(),cream)>0);
      assert(std::all_of(bolt.begin(),bolt.end(),[&](auto p){return p==blank || p==cream || (level<=15 && p==SDL_MapRGBA(ui.surface->format,ui.accent.r,ui.accent.g,ui.accent.b,ui.accent.a));}));
      ui.save(out/(std::string(language)+"-charging-"+std::to_string(level)+".png"));
    }
    assert(images[0]!=images[5] && images[25]!=images[50] && images[50]!=images[100]);
    Telemetry unknown;ui.rect(575,24,125,32,ui.bg);ui.battery(unknown,false);
    assert(pixels(599,34,32,14)!=images[0]);ui.save(out/(std::string(language)+"-unknown.png"));
  }
  // Optional elements occupy space only while visible. The whole group stays right-aligned.
  Telemetry telemetry;telemetry.capacity=50;
  ui.rect(0,0,720,68,ui.bg);ui.showBatteryPercent=true;
  int plain=ui.status("12:34",telemetry);
  const int clockX=696-ui.measure("12:34",21);auto clock=pixels(clockX,29,ui.measure("12:34",21),27);
  telemetry.wifiEnabled=true;
  ui.rect(0,0,720,68,ui.bg);int wifi=ui.status("12:34",telemetry);
  assert(plain-wifi==34 && pixels(clockX,29,ui.measure("12:34",21),27)==clock);
  ui.save(out/"status-wifi-on.png");
  telemetry.wifiState="connected";
  std::vector<std::vector<Uint32>> signals;
  for(int dbm:{-90,-75,-60,-45,0}) {
    telemetry.wifiSignalDbm=dbm==0?std::nullopt:std::optional<int>(dbm);
    ui.rect(0,0,720,68,ui.bg);ui.status("12:34",telemetry);
    signals.push_back(pixels(wifi,29,22,25));
    ui.save(out/("status-signal-"+std::to_string(dbm)+".png"));
  }
  for(size_t i=0;i<signals.size();++i)for(size_t j=0;j<i;++j)assert(signals[i]!=signals[j]);
  telemetry.wifiEnabled=false;ui.showBatteryPercent=false;
  ui.rect(0,0,720,68,ui.bg);int compact=ui.status("12:34",telemetry);
  assert(compact>plain && pixels(clockX,29,ui.measure("12:34",21),27)==clock);
  ui.save(out/"status-wifi-off-percent-off.png");
  telemetry.batteryStatus="Charging";
  ui.rect(0,0,720,68,ui.bg);int charging=ui.status("12:34",telemetry);
  assert(compact==charging);
  ui.save(out/"status-charging-percent-off.png");
  Store legacy(out/"state");assert(legacy.data["show_battery_percent"]==true && legacy.data["smooth_battery_level"]==true);
  auto config=legacy.data;config["show_battery_percent"]=false;config["smooth_battery_level"]=false;legacy.commit(config);
  Store restart(out/"state");assert(restart.data["show_battery_percent"]==false && restart.data["smooth_battery_level"]==false);
  auto bad=Json{{"schema",1},{"show_battery_percent","false"}}.dump();atomicWrite(out/"state/config.json",{bad.begin(),bad.end()});
  Store invalid(out/"state");assert(!invalid.warning.empty() && invalid.data["show_battery_percent"].is_boolean());
  std::cout<<"PASS: real battery fill, zero/low/full/charging/unknown, hidden percentage with unchanged icon, readable monochrome charging mark, compact optional status items, legacy default, persistence and invalid-type rejection.\n";
}
