// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "screenshots.hpp"
#include "stb_image_write.h"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
namespace rondo {
uint16_t screenshotChord(const Json &data) {
  auto mode=data.value("screenshot_shortcut","sticks");
  if(mode=="sticks")return 0xc000;
  if(mode=="triggers")return 0x3000;
  if(mode=="select_r1")return (1u<<2)|(1u<<11);
  if(mode=="custom")return uint16_t(data.value("screenshot_custom",0xc000));
  return 0;
}
std::string screenshotTimestamp(const Timezones &zones,const std::string &zone,time_t now) {
  struct Restore {bool had;std::string value;~Restore(){if(had)setenv("TZ",value.c_str(),1);else unsetenv("TZ");tzset();}} restore{getenv("TZ")!=nullptr,getenv("TZ")?getenv("TZ"):""};
  tm local{};if(zones.valid(zone)){zones.applyHost(zone);localtime_r(&now,&local);}else gmtime_r(&now,&local);
  char stamp[32];strftime(stamp,sizeof(stamp),"%Y-%m-%d_%H-%M-%S",&local);return stamp;
}
void ScreenshotChord::reset(){candidate=swallowed=0;since=0;passed=false;}
uint16_t ScreenshotChord::sample(uint16_t held,uint16_t chord,uint64_t now,bool &trigger) {
  trigger=false;
  if(!chord){reset();return held;}
  if(swallowed){if(!(held&swallowed))reset();else return held&~swallowed;}
  auto pressed=uint16_t(held&chord);
  if(!pressed){auto tap=passed?0:candidate;candidate=0;passed=false;return held|tap;}
  if(!candidate){candidate=pressed;since=now;passed=false;}
  if(pressed==chord){trigger=true;swallowed=chord;candidate=0;return held&~chord;}
  if(now-since>=70)passed=true;
  return passed?held:uint16_t(held&~chord);
}
static std::string screenshotComponent(const std::string &value,const std::string &fallback) {
  std::string name;for(unsigned char c:value){if(std::string("/\\:*?\"<>|").find(char(c))!=std::string::npos||c<32||c==127)name+=' ';else name+=char(c);}
  while(!name.empty()&&(name.back()==' '||name.back()=='.'))name.pop_back();
  while(!name.empty()&&(name.front()==' '||name.front()=='.'))name.erase(name.begin());
  if(name.empty()||name=="."||name=="..")name=fallback;
  if(name.size()>96){size_t n=96;while(n&&(uint8_t(name[n])&0xc0)==0x80)--n;name.resize(n);}return name;
}
static fs::path screenshotFolder(const fs::path &root,const Game &game) {
  auto parent=root/screenshotComponent(game.system,"Games");fs::create_directories(parent);
  auto title=screenshotComponent(game.title,"Game");auto owner=hashBytes({game.id.begin(),game.id.end()})+"\n";
  for(int number=1;number<1000;++number) {
    auto folder=parent/(title+(number==1?"":" ("+std::to_string(number)+")"));
    if(fs::is_symlink(folder))continue;
    std::error_code creationError;bool created=fs::create_directory(folder,creationError);
    if(creationError&&creationError!=std::errc::file_exists)throw fs::filesystem_error("Screenshot directory",folder,creationError);
    if(created) {
      try{atomicWrite(folder/".game-id",{owner.begin(),owner.end()});}catch(...){std::error_code ignored;fs::remove(folder,ignored);throw;}
      return folder;
    }
    if(!fs::is_directory(folder))continue;
    auto marker=folder/".game-id";std::error_code error;
    if(fs::is_regular_file(marker,error)&&!fs::is_symlink(marker)&&fs::file_size(marker,error)==owner.size()) {
      std::ifstream file(marker);std::string value(owner.size(),' ');file.read(value.data(),value.size());if(value==owner)return folder;
    }
  }
  throw std::runtime_error("Screenshot folder unavailable");
}
static fs::path writeScreenshot(const fs::path &root,const Game &game,const std::vector<uint32_t> &frame,int width,int height,bool interface,std::string timestamp,double displayAspect) {
  int sourceWidth=width,sourceHeight=height;
  if(displayAspect>0) {
    // Preserve all source detail: expand the non-square pixel axis instead of
    // shrinking a high-resolution frame. The worker owns no SDL/Core objects.
    width=int(std::lround(height*displayAspect));
    if(width<sourceWidth){width=sourceWidth;height=int(std::lround(width/displayAspect));}
    if(width<1||height<1||width>4096||height>4096)throw std::runtime_error("Screenshot display geometry out of range");
  }
  std::vector<unsigned char> rgba(size_t(width)*height*4);
  for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
    auto sx=std::min(sourceWidth-1,int(int64_t(2*x+1)*sourceWidth/(2*width)));
    auto sy=std::min(sourceHeight-1,int(int64_t(2*y+1)*sourceHeight/(2*height)));
    auto p=frame[size_t(sy)*sourceWidth+sx];auto i=(size_t(y)*width+x)*4;
    rgba[i]=p>>16;rgba[i+1]=p>>8;rgba[i+2]=p;rgba[i+3]=255;
  }
  std::vector<uint8_t> bytes;
  auto collect=[](void *context,void *data,int length){auto &out=*static_cast<std::vector<uint8_t> *>(context);auto p=static_cast<uint8_t *>(data);out.insert(out.end(),p,p+length);};
  if(!stbi_write_png_to_func(collect,&bytes,width,height,4,rgba.data(),width*4))throw std::runtime_error("Screenshot encoding failed");
  auto folder=interface?root/"Interface":screenshotFolder(root,game);
  fs::create_directories(folder);
  if(timestamp.empty()){auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());tm local{};localtime_r(&now,&local);char stamp[32];strftime(stamp,sizeof(stamp),"%Y-%m-%d_%H-%M-%S",&local);timestamp=stamp;}
  static std::atomic<unsigned> sequence{0};
  fs::path path;int fd=-1;
  for(int attempt=0;attempt<100;++attempt){
    path=folder/(timestamp+"-"+std::to_string(sequence++)+".png");
    fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0644);
    if(fd>=0)break;
    if(errno!=EEXIST)throw std::runtime_error("Screenshot file creation failed");
  }
  if(fd<0)throw std::runtime_error("Screenshot filename unavailable");
  try {
    size_t done=0;while(done<bytes.size()){
      auto n=write(fd,bytes.data()+done,bytes.size()-done);
      if(n<0&&errno==EINTR)continue;
      if(n<=0)throw std::runtime_error("Screenshot write failed");done+=size_t(n);
    }
    int flushed;do {flushed=fsync(fd);}while(flushed&&errno==EINTR);
    if(flushed)throw std::runtime_error("Screenshot flush failed");
    int status=close(fd);fd=-1;if(status!=0)throw std::runtime_error("Screenshot close failed");
  }catch(...){if(fd>=0)close(fd);std::error_code error;fs::remove(path,error);throw;}
  // A complete PNG also needs a durable directory entry. A barrier failure
  // after close is ambiguous: report it, but retain the complete image.
  syncDirectory(folder);
  return path;
}
bool ScreenshotWriter::submit(const fs::path &root,const Game &game,const std::vector<uint32_t> &frame,int width,int height,const std::string &timestamp,double displayAspect) {
  if(busy())return false;
  if(width<1||height<1||width>4096||height>4096||size_t(width)*height!=frame.size())throw std::runtime_error("No game frame");
  if(!std::isfinite(displayAspect)||displayAspect<0||(displayAspect>0&&displayAspect<0.125)||displayAspect>8)throw std::runtime_error("Invalid screenshot display aspect");
  // Copy the immutable raw frame once. Resampling, encoding and SD I/O never touch SDL/Core.
  job=std::async(std::launch::async,writeScreenshot,root,game,frame,width,height,false,timestamp,displayAspect);return true;
}
bool ScreenshotWriter::submitInterface(const fs::path &root,const std::vector<uint32_t> &frame,int width,int height,const std::string &timestamp) {
  if(busy())return false;
  if(width<1||height<1||width>4096||height>4096||size_t(width)*height!=frame.size())throw std::runtime_error("No interface frame");
  job=std::async(std::launch::async,writeScreenshot,root,Game{},frame,width,height,true,timestamp,0.0);return true;
}
std::optional<fs::path> ScreenshotWriter::poll() {
  if(!job.valid()||job.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return {};
  return job.get();
}
} // namespace rondo
