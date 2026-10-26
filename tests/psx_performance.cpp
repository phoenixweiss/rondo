// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "core.hpp"
#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
using namespace rondo;
// Explicitly supplied technical disc; no user BIOS or ROM is distributed.
// Its MIPS loop writes 255 at RAM 0x20004 after reaching the executable.
int main(int argc,char **argv){
 if(argc!=7)return 2;
 fs::path output=argv[6];Game game{"benchmark","fixture","PSX","Technical",fs::absolute(argv[2])};
 using Clock=std::chrono::steady_clock;auto start=Clock::now();
 auto ms=[&]{return std::chrono::duration<double,std::milli>(Clock::now()-start).count();};
 Core core(argv[1],game,output.parent_path()/"state",true,argv[3],std::string(argv[4])=="hle");
 Json result={{"init_ms",ms()},{"mode",argv[4]},{"technical_disc",true},{"unpaced",true},{"audio_device",false}};
 auto handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!handle)return 3;
 auto memory=reinterpret_cast<void *(*)(unsigned)>(dlsym(handle,"retro_get_memory_data"));if(!memory)return 4;
 bool booted=false;
 for(unsigned frame=0;frame<3600;++frame){core.tick();uint32_t marker=0;
  auto ram=static_cast<const uint8_t *>(memory(RETRO_MEMORY_SYSTEM_RAM));
  if(ram)memcpy(&marker,ram+0x20004,4);
  if(marker==255){booted=true;break;}}
 result["booted"]=booted;result["boot_frames"]=core.frames;result["boot_total_ms"]=ms();
 if(booted){int count=std::stoi(argv[5]);if(count<=0||count>100000)return 5;
  for(int i=0;i<60;++i)core.tick();start=Clock::now();double slowest=0;Json samples=Json::array();
  for(int i=0;i<count;++i){auto frame=Clock::now();core.tick();double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-frame).count();
   samples.push_back(elapsed);slowest=std::max(slowest,elapsed);}
  result["steady_ms"]=ms();result["steady_frames"]=count;result["frame_ms"]=samples;result["max_frame_ms"]=slowest;
  result["video"]=core.videoGeometry();}
 dlclose(handle);auto text=result.dump(2);atomicWrite(output,{text.begin(),text.end()});
 return booted?0:6;
}
