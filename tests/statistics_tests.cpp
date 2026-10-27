// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "game_statistics.hpp"
#include <cassert>
#include <iostream>
using namespace rondo;
int main(int argc,char **argv){
 assert(argc==2);fs::path root=argv[1],path=root/"game/statistics.json";
 {GameStatistics stats(path,"stable-id",1000);stats.tick(1000,true);stats.tick(2500,false);stats.tick(100000,false);stats.tick(100000,true);stats.tick(101000,false);
  assert(stats.snapshot()["launches"]==1&&stats.snapshot()["played_ms"]==2500);assert(stats.flush(101000));}
 auto saved=GameStatistics::read(path,"stable-id");assert(saved["played_ms"]==2500&&saved["historical_time_unknown"]==true);
 {GameStatistics stats(path,"stable-id",200000);stats.tick(200000,true);for(int i=1;i<1000;++i)stats.tick(200000+i,true);assert(stats.writesStarted==1);assert(stats.flush(201000));}
 saved=GameStatistics::read(path,"stable-id");assert(saved["launches"]==2&&saved["played_ms"]==3500);
 auto hash=hashFile(path);bool rejected=false;try{GameStatistics stats(path,"wrong-id",0);}catch(...){rejected=true;}assert(rejected&&hashFile(path)==hash);
 auto blocked=root/"blocked";atomicWrite(blocked,{1});{GameStatistics stats(blocked/"statistics.json","retry",0);stats.tick(0,true);assert(!stats.flush(1000));assert(!stats.error.empty());fs::remove(blocked);assert(stats.flush(1000));}
 assert(GameStatistics::read(blocked/"statistics.json","retry")["launches"]==1);
 assert(GameStatistics::read(root/"absent","none").empty());
 std::cout<<"PASS: versioned portable counters, monotonic active time, pause/sleep exclusion, throttled writes, identity checks and idempotent retry.\n";
}
