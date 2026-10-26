// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "ui.hpp"
#include "stb_image_write.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace rondo;
static void png(const fs::path &path,uint8_t red) {
  fs::create_directories(path.parent_path());std::vector<uint8_t> data(600*400*4,255);
  for(size_t i=0;i<data.size();i+=4){data[i]=red;data[i+1]=20;data[i+2]=40;}
  assert(stbi_write_png(path.c_str(),600,400,4,data.data(),600*4));
}
static std::shared_ptr<const ArtworkImage> settle(ArtworkCache &cache,const ArtworkRequest &r) {
  cache.select({r});
  for(int i=0;i<1000;++i){auto s=cache.statistics();if(s["queued"]==0&&!s["busy"].get<bool>())return cache.get(r);SDL_Delay(5);}
  assert(false);return {};
}
static uintmax_t diskBytes(const fs::path &root){uintmax_t n=0;for(auto &e:fs::directory_iterator(root))if(e.is_regular_file())n+=e.file_size();return n;}
int main(int argc,char **argv) {
  assert(argc==3);fs::path root=argv[2];fs::create_directories(root);
  auto source=root/"original.png";png(source,150);auto original=hashFile(source);
  ArtworkRequest a{source,204,204,true};std::string id;
  {ArtworkCache cache(root/"cache");auto image=settle(cache,a);assert(image&&image->width==204&&image->height==136&&image->rgba[0]==150);
   id=image->identity;assert(cache.statistics()["decodes"]==1);
   for(int i=0;i<1000;++i){cache.select({a});assert(cache.get(a)==image);}assert(cache.statistics()["decodes"]==1);}
  {ArtworkCache cache(root/"cache");assert(settle(cache,a)->identity==id);assert(cache.statistics()["disk_hits"]==1&&cache.statistics()["decodes"]==0);}
  auto file=root/"cache"/(id+".rgba");auto bytes=readBytes(file);bytes.back()^=0xff;atomicWrite(file,bytes);
  {ArtworkCache cache(root/"cache");auto image=settle(cache,a);assert(image&&image->rgba[0]==150&&cache.statistics()["decodes"]==1);}
  assert(hashFile(source)==original);
  {ArtworkCache cache(root/"cache");assert(settle(cache,a));png(source,210);
   auto stamp=fs::last_write_time(source);fs::last_write_time(source,stamp+std::chrono::seconds(2));SDL_Delay(1100);
   auto image=settle(cache,a);assert(image&&image->identity!=id&&image->rgba[0]==210);
   fs::remove(source);SDL_Delay(1100);assert(!settle(cache,a));}
  png(source,150);
  {ArtworkCache cache(root/"bad-cache");atomicWrite(root/"invalid.png",{1,2,3});assert(!settle(cache,{root/"invalid.png",204,204,true}));
   auto decodes=cache.statistics()["decodes"];SDL_Delay(1100);assert(!settle(cache,{root/"invalid.png",204,204,true}));assert(cache.statistics()["decodes"]==decodes);
   png(root/"invalid.png",210);SDL_Delay(1100);assert(settle(cache,{root/"invalid.png",204,204,true}));
   assert(!settle(cache,{root/"absent.png",204,204,true}));}
  atomicWrite(root/"blocked",{0});
  {ArtworkCache cache(root/"blocked/child");assert(settle(cache,a));}
  fs::create_directory(root/"outside");atomicWrite(root/"outside/keep",{42});fs::create_directory_symlink(root/"outside",root/"linked");
  {ArtworkCache cache(root/"linked");assert(settle(cache,a));assert(diskBytes(root/"outside")==1);}
  auto b=a,c=a;b.path=root/"b.png";c.path=root/"c.png";png(b.path,20);png(c.path,40);
  size_t imageSize=204*136*4;
  {ArtworkCache cache(root/"bounded",imageSize*2,(imageSize+80)*2);auto first=settle(cache,a);auto second=settle(cache,b);
   assert(cache.get(a));auto third=settle(cache,c);assert(third);
   assert(fs::exists(root/"bounded"/(first->identity+".rgba"))&&!fs::exists(root/"bounded"/(second->identity+".rgba")));
   assert(cache.statistics()["resident_bytes"].get<size_t>()<=imageSize*2&&diskBytes(root/"bounded")<=(imageSize+80)*2);
   for(int i=0;i<200;++i)cache.select({i%2?a:b,c});assert(cache.statistics()["queued"].get<size_t>()<=5);}
  {UI ui(argv[1],true);ui.artworkCache=std::make_unique<ArtworkCache>(root/"render");assert(settle(*ui.artworkCache,a));
   ui.rect(0,0,720,480,ui.bg);assert(ui.artworkImage({a},492,140,204,204));ui.save(root/"thumbnail.png");}
  std::cout<<"PASS: bounded asynchronous artwork, unchanged originals, persistent thumbnail readback, corruption recovery, source invalidation/removal, memory/disk LRU, RAM-only fallback, symlink safety and SDL rendering.\n";
}
