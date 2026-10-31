// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "ui.hpp"
#include "stb_image.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace rondo;
using Clock=std::chrono::steady_clock;
int main(int argc,char **argv) {
  assert(argc==3||argc==4);
  const bool display=argc==4&&std::string(argv[3])=="--display";
  SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP,"1");
  UI ui(argv[1],!display,true);fs::path work=argv[2];fs::create_directories(work);
  auto *target=SDL_CreateRGBSurfaceWithFormat(0,720,480,32,SDL_PIXELFORMAT_ARGB8888);
  auto *renderer=SDL_CreateSoftwareRenderer(target);assert(renderer);
  auto same=[&] {
    SDL_RenderFlush(ui.renderer);SDL_RenderFlush(renderer);
    for(int y=0;y<480;++y)assert(!memcmp(static_cast<uint8_t *>(ui.surface->pixels)+y*ui.surface->pitch,
      static_cast<uint8_t *>(target->pixels)+y*target->pitch,720*4));
  };
  {
    GameFrame frame(renderer);
    for(auto size:std::vector<std::pair<int,int>>{{368,480},{512,240},{640,480},{256,240},{160,144},{240,160},{368,480}}) {
      int w=size.first,h=size.second;std::vector<uint32_t> pixels(size_t(w)*h);
      for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|((i*7919)&0xffffffu);
      SDL_Color bg{23,31,41,255};SDL_Rect rect{40,0,640,480};
      ui.beginFrame();ui.rect(0,0,720,480,bg);ui.pixels(pixels,w,h,rect.x,rect.y,rect.w,rect.h,false);
      assert(frame.draw(pixels,w,h,rect,bg)&&frame.redraw());same();
      pixels.assign(pixels.size(),0xffaabbccu);assert(frame.draw(pixels,w,h,rect,bg)&&frame.redraw());
      ui.rect(0,0,720,480,bg);ui.pixels(pixels,w,h,rect.x,rect.y,rect.w,rect.h,false);same();
      assert(!frame.draw({},w,h,rect,bg));
    }
  }
  SDL_DestroyRenderer(renderer);SDL_FreeSurface(target);
  if(display) {
    std::vector<uint32_t> pixels(368*480,0xff4676a6u);SDL_Rect viewport{40,0,640,480};SDL_Color bg{0,0,0,255};
    auto overlay=[&] {ui.rect(24,24,176,40,ui.focus,8);ui.icon("volume-2",36,32,24);ui.text("Test",76,30,22);};
    auto paint=[&](bool direct,bool showOverlay) {
      ui.beginFrame();
      if(direct)ui.gamePixels(pixels,368,480,viewport,bg,showOverlay);
      else {ui.rect(0,0,720,480,bg);ui.pixels(pixels,368,480,40,0,640,480,false);}
      if(showOverlay)overlay();
      ui.present();
    };
    for(bool showOverlay:{false,true}) {
      paint(false,showOverlay);ui.save(work/(showOverlay?"legacy-overlay.png":"legacy.png"));
      paint(true,showOverlay);assert(std::string(ui.renderPath())=="display-game-texture");
      ui.save(work/(showOverlay?"direct-overlay.png":"direct.png"));
      int w,h,n;auto *a=stbi_load((work/(showOverlay?"legacy-overlay.png":"legacy.png")).c_str(),&w,&h,&n,4);
      auto *b=stbi_load((work/(showOverlay?"direct-overlay.png":"direct.png")).c_str(),&w,&h,&n,4);assert(a&&b);
      int maxError=0;size_t differing=0;
      for(int i=0;i<720*480;++i)for(int c=0;c<3;++c){int e=abs(int(a[i*4+c])-int(b[i*4+c]));maxError=std::max(maxError,e);differing+=e>2;}
      stbi_image_free(a);stbi_image_free(b);assert(maxError<=3&&differing<100);
      std::cout<<"Overlay "<<showOverlay<<": max channel error "<<maxError<<"\n";
    }
    Json result;result["hidden_display"]=true;result["vsync_requested"]=false;result["synthetic_frame"]=true;
    for(bool direct:{false,true})for(bool showOverlay:{false,true}) {
      for(int i=0;i<30;++i)paint(direct,showOverlay);
      double cpu=0,wait=0;auto start=Clock::now();
      for(int i=0;i<600;++i){auto before=Clock::now();paint(direct,showOverlay);
        auto total=std::chrono::duration<double,std::milli>(Clock::now()-before).count();
        cpu+=total-ui.presentationMs;wait+=ui.presentationMs;}
      result[std::string(direct?"direct":"legacy")+(showOverlay?"_overlay":"")]=Json{
        {"total_ms",std::chrono::duration<double,std::milli>(Clock::now()-start).count()/600},
        {"prepare_ms",cpu/600},{"present_ms",wait/600}};
    }
    auto text=result.dump(2);atomicWrite(work/"benchmark.json",{text.begin(),text.end()});std::cout<<text<<"\n";
  }
  std::cout<<"PASS: display game texture colors, geometry, updates and resolution changes.\n";
}
