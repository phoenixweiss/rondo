// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <chrono>
#include <iostream>
using namespace rondo;
int main(int argc,char **argv){
 if(argc!=7)return 2;
 Options o;o.assets=argv[1];o.state=fs::path(argv[2])/"state";o.output=argv[2];o.core=argv[3];o.psxCore=argv[4];o.headless=true;o.volumes={{"PERF",fs::absolute(argv[5])}};
 {Store store(o.state);auto cfg=store.data;cfg["psx_firmware"]="hle";store.commit(cfg);}
 auto games=scan(o.volumes);if(games.size()!=1)return 3;
 using Clock=std::chrono::steady_clock;
 Json result={{"host_only",true},{"technical_rom",true},{"controlled_preview_512x240",true},{"runs",Json::array()}};
 for(int trial=0;trial<3;++trial){Json run;auto start=Clock::now();App a(o,true,games[0]);
 auto ms=[&]{return std::chrono::duration<double,std::milli>(Clock::now()-start).count();};run["core_init_ms"]=ms();a.ticks(45);a.core->width=512;a.core->height=240;a.core->frame.assign(512*240,0xff4f7061);
 for(int slot=0;slot<3;++slot)a.core->save(slot);a.core->saveAutomatic();a.core->saveAutomatic();
 start=Clock::now();a.core->save(0);run["manual_overwrite_ms"]=ms();
 start=Clock::now();a.core->saveAutomatic();run["autosave_ms"]=ms();
 start=Clock::now();a.latestResumeSlot();run["resume_selection_ms"]=ms();
 start=Clock::now();for(int slot=0;slot<3;++slot)a.slotView(slot);run["three_previews_ms"]=ms();
 start=Clock::now();a.core->loadState(0);run["load_ms"]=ms();
 auto path=a.store.gameDir(games[0])/"slot-0.state";if(!fs::exists(path))path.replace_extension(".json");run["state_bytes"]=fs::file_size(path);
 result["runs"].push_back(run);}
 auto text=result.dump(2);atomicWrite(argv[6],{text.begin(),text.end()});
}
