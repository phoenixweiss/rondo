// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "artwork.hpp"
#include "platform.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>
#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_image.h"
#pragma GCC diagnostic pop
namespace rondo {
std::string ArtworkRequest::key() const {
  return path.lexically_normal().string()+"#"+std::to_string(width)+"x"+std::to_string(height)+"#"+std::to_string(smooth);
}
static bool cacheName(const std::string &name) {
  return name.size()==69&&name.substr(64)==".rgba"&&
    std::all_of(name.begin(),name.begin()+64,[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
ArtworkCache::ArtworkCache(fs::path root,size_t memoryBytes,size_t diskBytes)
 :directory(std::move(root)),memoryBudget(memoryBytes),diskBudget(diskBytes),worker(&ArtworkCache::loop,this) {}
ArtworkCache::~ArtworkCache() {
  {std::lock_guard<std::mutex> lock(mutex);quitting=true;queue.clear();wanted.clear();}
  changed.notify_all();worker.join();
}
void ArtworkCache::select(const std::vector<ArtworkRequest> &requests) {
  std::lock_guard<std::mutex> lock(mutex);
  std::set<std::string> next;
  for(size_t i=0;i<std::min(size_t(5),requests.size());++i) {
    auto &r=requests[i];if(r.width<1||r.height<1||r.width>512||r.height>512)continue;
    next.insert(r.key());
  }
  wanted=std::move(next);
  queue.erase(std::remove_if(queue.begin(),queue.end(),[&](const auto &r){return !wanted.count(r.key());}),queue.end());
  for(size_t i=0;i<std::min(size_t(5),requests.size());++i) {
    auto &r=requests[i];auto id=r.key();if(!wanted.count(id)||running==id)continue;
    auto found=records.find(id);
    if(found!=records.end()&&monotonicMs()<found->second.checked+1000)continue;
    if(std::any_of(queue.begin(),queue.end(),[&](const auto &q){return q.key()==id;}))continue;
    queue.push_back(r);
  }
  // Reprioritize the visible image even after a rapid direction reversal.
  if(!requests.empty()) {
    auto it=std::find_if(queue.begin(),queue.end(),[&](const auto &r){return r.key()==requests.front().key();});
    if(it!=queue.end())std::rotate(queue.begin(),it,it+1);
  }
  changed.notify_one();
}
std::shared_ptr<const ArtworkImage> ArtworkCache::get(const ArtworkRequest &r) {
  std::lock_guard<std::mutex> lock(mutex);auto it=records.find(r.key());
  if(it==records.end())return {};it->second.used=++clock;return it->second.image;
}
bool ArtworkCache::stillWanted(const std::string &id) {
  std::lock_guard<std::mutex> lock(mutex);return !quitting&&wanted.count(id);
}
bool ArtworkCache::missing(const ArtworkRequest &r) const {
  std::lock_guard<std::mutex> lock(mutex);auto id=r.key();auto found=records.find(id);
  return found!=records.end()&&!found->second.image&&running!=id&&
    !std::any_of(queue.begin(),queue.end(),[&](const auto &q){return q.key()==id;});
}
Json ArtworkCache::statistics() const {
  std::lock_guard<std::mutex> lock(mutex);
  return {{"decodes",decodes},{"disk_hits",hits},{"cancelled",cancelled},
    {"resident_bytes",resident},{"queued",queue.size()},{"busy",!running.empty()},{"records",records.size()}};
}
void ArtworkCache::openDisk() {
  if(directory.empty())return;
  try {
    if(fs::is_symlink(directory)||fs::is_symlink(directory.parent_path()))return;
    fs::create_directories(directory);writable=true;
    std::vector<std::pair<fs::file_time_type,fs::path>> files;
    for(auto &e:fs::directory_iterator(directory)) {
      {std::lock_guard<std::mutex> lock(mutex);if(quitting)return;}
      if(e.is_symlink()||!e.is_regular_file())continue;
      auto name=e.path().filename().string();
      if(cacheName(name))files.emplace_back(e.last_write_time(),e.path());
      else if(name.size()>74&&cacheName(name.substr(0,69))&&name.substr(69,5)==".tmp.")fs::remove(e.path());
    }
    std::sort(files.begin(),files.end());
    for(auto &e:files)disk[e.second.filename().string()]={fs::file_size(e.second),++diskClock};
    {std::lock_guard<std::mutex> lock(mutex);clock=std::max(clock,diskClock);}
    trimDisk();
  }catch(...){writable=false;}
}
void ArtworkCache::trimDisk(size_t extra) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    for(auto &entry:records)if(entry.second.image) {
      auto found=disk.find(entry.second.image->identity+".rgba");
      if(found!=disk.end())found->second.used=std::max(found->second.used,entry.second.used);
    }
  }
  uintmax_t bytes=0;for(auto &e:disk)bytes+=e.second.size;
  while(bytes+extra>diskBudget&&!disk.empty()) {
    auto oldest=std::min_element(disk.begin(),disk.end(),[](const auto &a,const auto &b){return a.second.used<b.second.used;});
    fs::remove(directory/oldest->first);bytes-=oldest->second.size;disk.erase(oldest);
  }
}
void ArtworkCache::touchDisk(const std::string &name,size_t bytes) {
  std::lock_guard<std::mutex> lock(mutex);disk[name]={bytes,++clock};
}
static void put32(std::vector<uint8_t> &out,uint32_t value){for(int i=0;i<4;++i)out.push_back(uint8_t(value>>(i*8)));}
static uint32_t get32(const std::vector<uint8_t> &in,size_t offset){uint32_t n=0;for(int i=0;i<4;++i)n|=uint32_t(in[offset+i])<<(i*8);return n;}
std::shared_ptr<ArtworkImage> ArtworkCache::load(const ArtworkRequest &r) {
  std::error_code error;auto stamp=fs::last_write_time(r.path,error);if(error)return {};
  auto size=fs::file_size(r.path,error);if(error||size>16*1024*1024||!fs::is_regular_file(r.path))return {};
  auto fingerprint=r.key()+"#v1#"+std::to_string(static_cast<long long>(stamp.time_since_epoch().count()))+"#"+std::to_string(size);
  auto image=std::make_shared<ArtworkImage>();image->identity=hashBytes({fingerprint.begin(),fingerprint.end()});
  auto failed=failedSources.find(r.key());
  if(failed!=failedSources.end()&&failed->second==image->identity)return {};
  {
    std::lock_guard<std::mutex> lock(mutex);auto it=records.find(r.key());
    if(it!=records.end()&&it->second.image&&it->second.image->identity==image->identity)return std::const_pointer_cast<ArtworkImage>(it->second.image);
  }
  auto name=image->identity+".rgba";auto path=directory/name;
  if(writable)try {
    if(!fs::is_symlink(path)&&fs::is_regular_file(path)&&fs::file_size(path)<=80+size_t(r.width)*r.height*4) {
      auto bytes=readBytes(path);
      if(bytes.size()>=80&&std::memcmp(bytes.data(),"RART0001",8)==0) {
        int w=int(get32(bytes,8)),h=int(get32(bytes,12));
        if(w>0&&h>0&&w<=r.width&&h<=r.height&&bytes.size()==80+size_t(w)*h*4) {
          std::vector<uint8_t> pixels(bytes.begin()+80,bytes.end());
          if(hashBytes(pixels)==std::string(bytes.begin()+16,bytes.begin()+80)) {
            image->width=w;image->height=h;image->rgba=std::move(pixels);
            touchDisk(name,bytes.size());std::lock_guard<std::mutex> lock(mutex);++hits;return image;
          }
        }
      }
      fs::remove(path);disk.erase(name);
    }
  }catch(...){}
  int width=0,height=0,channels=0;
  auto *raw=stbi_load(r.path.c_str(),&width,&height,&channels,4);
  std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(raw,stbi_image_free);
  {std::lock_guard<std::mutex> lock(mutex);++decodes;}
  if(!raw||width<1||height<1) {
    failedSources[r.key()]=image->identity;
    while(failedSources.size()>64)failedSources.erase(failedSources.begin());
    return {};
  }
  failedSources.erase(r.key());
  if(!stillWanted(r.key()))return {};
  double scale=std::min({1.0,double(r.width)/width,double(r.height)/height});
  image->width=std::max(1,int(width*scale));image->height=std::max(1,int(height*scale));
  image->rgba.resize(size_t(image->width)*image->height*4);
  for(int y=0;y<image->height;++y)for(int x=0;x<image->width;++x) {
    double sx=std::clamp((x+0.5)*width/image->width-0.5,0.0,double(width-1));
    double sy=std::clamp((y+0.5)*height/image->height-0.5,0.0,double(height-1));
    auto *out=image->rgba.data()+(y*image->width+x)*4;
    if(!r.smooth){std::memcpy(out,raw+(int(std::lround(sy))*width+int(std::lround(sx)))*4,4);continue;}
    int x0=int(sx),y0=int(sy),x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);
    double fx=sx-x0,fy=sy-y0;const int locations[]={y0*width+x0,y0*width+x1,y1*width+x0,y1*width+x1};
    const double weights[]={(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};double alpha=0,rgb[3]{};
    for(int i=0;i<4;++i){auto *p=raw+locations[i]*4;alpha+=p[3]*weights[i];for(int c=0;c<3;++c)rgb[c]+=p[c]*p[3]*weights[i];}
    out[3]=uint8_t(std::lround(alpha));for(int c=0;c<3;++c)out[c]=alpha>0?uint8_t(std::clamp(std::lround(rgb[c]/alpha),0L,255L)):0;
  }
  if(!stillWanted(r.key())||fs::last_write_time(r.path,error)!=stamp||error||fs::file_size(r.path,error)!=size||error)return {};
  if(writable&&image->rgba.size()+80<=diskBudget)try {
    trimDisk(image->rgba.size()+80);
    std::vector<uint8_t> bytes{'R','A','R','T','0','0','0','1'};put32(bytes,image->width);put32(bytes,image->height);
    auto digest=hashBytes(image->rgba);bytes.insert(bytes.end(),digest.begin(),digest.end());bytes.insert(bytes.end(),image->rgba.begin(),image->rgba.end());
    static std::atomic<unsigned> number{0};auto temp=fs::path(path.string()+".tmp."+std::to_string(getpid())+"."+std::to_string(++number));
    // Disposable cache: atomic rename, no fsync of data or directory. Corruption
    // simply regenerates a thumbnail; ROMs and original artwork are untouched.
    int fd=open(temp.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);
    if(fd>=0) {
      size_t done=0;while(done<bytes.size()){auto n=::write(fd,bytes.data()+done,bytes.size()-done);if(n<=0)break;done+=size_t(n);}close(fd);
      if(done==bytes.size()) {
        std::error_code renameError;fs::rename(temp,path,renameError);
        if(!renameError)touchDisk(name,bytes.size());else fs::remove(temp,renameError);
      }
      else fs::remove(temp);
    }
  }catch(...){}
  return image;
}
void ArtworkCache::loop() {
  openDisk();
  std::unique_lock<std::mutex> lock(mutex);
  while(!quitting) {
    changed.wait(lock,[&]{return quitting||!queue.empty();});if(quitting)break;
    auto request=queue.front();queue.pop_front();running=request.key();
    lock.unlock();std::shared_ptr<ArtworkImage> image;try{image=load(request);}catch(...){}lock.lock();
    if(!quitting&&wanted.count(running)) {
      auto &entry=records[running];if(entry.image)resident-=entry.image->rgba.size();
      entry={image,monotonicMs(),++clock};if(image)resident+=image->rgba.size();
      while(resident>memoryBudget||records.size()>64) {
        auto oldest=records.end();for(auto it=records.begin();it!=records.end();++it)
          if(oldest==records.end()||it->second.used<oldest->second.used)oldest=it;
        if(oldest->second.image)resident-=oldest->second.image->rgba.size();records.erase(oldest);
      }
    } else ++cancelled;
    running.clear();
  }
}
}
