// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include "stb_image_write.h"
#include <chrono>
#include <iostream>
#include <numeric>
#include <sys/resource.h>
using namespace rondo;
using Clock=std::chrono::steady_clock;
static Json summary(std::vector<double> values) {
  std::sort(values.begin(),values.end());return {{"mean_ms",std::accumulate(values.begin(),values.end(),0.0)/values.size()},
    {"p50_ms",values[values.size()/2]},{"p95_ms",values[values.size()*95/100]},{"max_ms",values.back()}};
}
int main(int argc,char **argv) {
  if(argc!=3)return 2;fs::path work=argv[2];fs::create_directories(work);
  // Representative large PNGs, generated locally; no ROMs or game artwork.
  for(int cover=0;cover<32;++cover) {
    auto dir=work/"covers"/std::to_string(cover);fs::create_directories(dir);
    if(fs::exists(dir/"cover.png"))continue;
    std::vector<uint32_t> pixels(800*1200);
    for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|uint32_t((i*13+cover*111111)&0xffffffu);
    if(!stbi_write_png((dir/"cover.png").c_str(),800,1200,4,pixels.data(),800*4))return 3;
  }
  Options o;o.assets=argv[1];o.state=work/"state";o.headless=true;App a(o);
  a.games.clear();a.sections={"All","Favorites","Recent","NES","GBA","SNES"};
  const char *systems[]={"NES","GBA","SNES"};
  for(int i=0;i<3000;++i) {
    char title[80];snprintf(title,sizeof(title),"Game %04d - Library performance",i);
    a.games.push_back({"game-"+std::to_string(i),"TF1",systems[i%3],title,work/"covers"/std::to_string(i%32)/"test.rom"});
  }
  a.store.data["favorites"]=Json::array();a.store.data["recent_games"]=Json::array();
  for(int i=0;i<1000;++i)a.store.data["favorites"].push_back(a.games[i*3].id);
  for(int i=0;i<20;++i)a.store.data["recent_games"].push_back({{"id",a.games[2999-i].id}});
  Json result={{"games",3000},{"favorites",1000},{"recent",20},{"synthetic",true},{"headless",true}};
  for(const auto *section:{"All","NES","Favorites","Recent"}) {
    a.section=section;std::vector<double> timings;size_t count=0;
    for(int i=0;i<30;++i){auto start=Clock::now();auto visible=a.visible();count=visible.size();timings.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());}
    result["visible"][section]=summary(timings);result["visible"][section]["count"]=count;
  }
  a.section="All";a.screen=Screen::Library;
  for(const auto *view:{"list","tiles"}) {
    a.store.data["game_view"]=view;
    for(int round=0;round<2;++round) {
      a.selected=0;a.scroll=0;std::vector<double> timings;
      for(int i=0;i<60;++i) {
        auto start=Clock::now();a.action("DOWN");a.draw();timings.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
      }
      result["navigation"][view][round?"warm":"cold"]=summary(timings);
      for(int i=0;i<2000;++i){auto stats=a.ui->artworkCache->statistics();if(stats["queued"]==0&&!stats["busy"].get<bool>())break;SDL_Delay(1);}
    }
  }
  result["artwork_cache"]=a.ui->artworkCache->statistics();rusage use{};getrusage(RUSAGE_SELF,&use);
#ifdef __APPLE__
  result["max_rss_bytes"]=use.ru_maxrss;
#else
  result["max_rss_bytes"]=use.ru_maxrss*1024;
#endif
  auto text=result.dump(2);atomicWrite(work/"result.json",{text.begin(),text.end()});std::cout<<text<<"\n";
}
