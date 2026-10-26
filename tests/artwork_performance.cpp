// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "ui.hpp"
#include <chrono>
#include <iostream>
#include <sys/resource.h>
using namespace rondo;
int main(int argc,char **argv){
 if(argc!=4)return 2;Json result;
 for(int restart=0;restart<2;++restart){
  UI ui(argv[1],true);ui.artworkCache=std::make_unique<ArtworkCache>(argv[3]);Json calls=Json::array(),ready=Json::array();
  for(int round=0;round<2;++round)for(int i=0;i<8;++i){
   ArtworkRequest r{fs::path(argv[2])/(std::to_string(i)+".png"),204,204,true};auto start=std::chrono::steady_clock::now();
   bool ok=ui.artworkImage({r},0,0,204,204);
   double initial=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
   for(int spin=0;!ok&&spin<5000;++spin){SDL_Delay(1);ok=ui.artworkImage({r},0,0,204,204);}
   if(!ok)return 3;
   calls.push_back(initial);ready.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  }
  result[restart?"persistent":"first"]={{"initial_call_ms",calls},{"ready_ms",ready},{"cache",ui.artworkCache->statistics()}};
 }
 rusage usage{};getrusage(RUSAGE_SELF,&usage);
#ifdef __APPLE__
 result["max_rss_bytes"]=usage.ru_maxrss;
#else
 result["max_rss_bytes"]=usage.ru_maxrss*1024;
#endif
 std::cout<<result.dump(2)<<"\n";
}
