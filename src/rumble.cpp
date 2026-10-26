// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "rumble.hpp"
#ifdef __linux__
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace rondo {
Rumble::Rumble(std::function<bool(bool)> writer):write(std::move(writer)),worker(&Rumble::loop,this) {}
Rumble::~Rumble() {
  {std::lock_guard<std::mutex> lock(mutex);quitting=true;}
  changed.notify_all();
  worker.join();
}
bool Rumble::available() const {std::lock_guard<std::mutex> lock(mutex);return healthy&&!quitting;}
void Rumble::update(uint16_t strong,uint16_t weak) {
  std::lock_guard<std::mutex> lock(mutex);
  if(!healthy||quitting)return;
  bool next=strong||weak;
  deadline=Clock::now()+std::chrono::milliseconds(250);
  if(requested!=next){requested=next;++sequence;changed.notify_all();}
}
void Rumble::stop() {
  std::unique_lock<std::mutex> lock(mutex);
  requested=false;forceOff=true;auto target=++sequence;changed.notify_all();
  // Pause/suspend must request Off before blocking on saves or OS sleep.
  changed.wait_for(lock,std::chrono::milliseconds(100),[&]{return completed>=target||!healthy;});
}
void Rumble::loop() {
  std::unique_lock<std::mutex> lock(mutex);
  bool output=false,initial=true;
  for(;;) {
    bool next=!quitting&&healthy&&requested&&Clock::now()<deadline;
    if(!next&&requested&&Clock::now()>=deadline)requested=false;
    auto current=sequence;
    if(initial||next!=output||quitting||forceOff) {
      forceOff=false;
      lock.unlock();bool ok=false;try{ok=write(next);}catch(...){}
      // A failed On may have reached the driver: always attempt Off once.
      if(!ok&&next)try{write(false);}catch(...){}
      lock.lock();healthy=healthy&&ok;output=next&&ok;initial=false;
    }
    completed=current;changed.notify_all();
    if(quitting)break;
    if(output)changed.wait_until(lock,deadline,[&]{return quitting||sequence!=completed;});
    else changed.wait(lock,[&]{return quitting||sequence!=completed;});
  }
}
std::shared_ptr<Rumble> h700Rumble(bool enabled) {
#ifdef __linux__
  if(enabled) {
    int fd=open("/sys/class/power_supply/axp2202-battery/moto",O_WRONLY|O_CLOEXEC);
    if(fd<0)return {};
    auto file=std::shared_ptr<int>(new int(fd),[](int *p){close(*p);delete p;});
    return std::make_shared<Rumble>([file](bool on){
      if(lseek(*file,0,SEEK_SET)<0)return false;
      const char *text=on?"1\n":"0\n";ssize_t count;
      do{count=::write(*file,text,2);}while(count<0&&errno==EINTR);
      return count==2;
    });
  }
#else
  (void)enabled;
#endif
  return {};
}
}
