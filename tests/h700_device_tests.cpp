// Run only in an isolated BaseOS chroot with synthetic sysfs and a fake ioctl.
#include "platform.hpp"
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <sstream>
#include <unistd.h>
using namespace rondo;
static int raw=96,memWrites=0,readOverride=-999;static bool reject=false,failSuspendWrite=false;
extern "C" int __wrap_ioctl(int, unsigned long request, ...) {
  va_list ap;va_start(ap,request);auto *args=va_arg(ap,unsigned long *);va_end(ap);
  assert(args[0]==0);
  if(request==0x103)return readOverride==-999?raw:readOverride;
  assert(request==0x102);
  if(reject){reject=false;errno=EBUSY;return -1;}
  raw=int(args[1]);
  // Reproduce the remapping recovered from this device's vendor kernel.
  if(raw==200)raw=170;
  else if(raw==255)raw=200;
  if(raw==0 && failSuspendWrite) { failSuspendWrite=false;fs::remove("/sys/power/state");fs::create_directory("/sys/power/state"); }
  return 0;
}
extern "C" ssize_t __real_write(int,const void *,size_t);
extern "C" ssize_t __wrap_write(int fd,const void *data,size_t n) {
  if(n==3 && !memcmp(data,"mem",3))++memWrites;
  return __real_write(fd,data,n);
}
static void put(const char *path,const char *text){std::ofstream(path)<<text;}
static std::string get(const char *path){std::ifstream f(path);std::string s;std::getline(f,s);return s;}
int main() {
  assert(get("/tmp/rondo-synthetic-hardware")=="test-only"); // Fail closed outside the fixture.
  auto d=deviceControl(true);assert(d->brightness()==70 && d->canSleep() && d->lidOpen()==true);
  d->brightness(0);assert(raw==4);d->brightness(100);assert(raw==254 && d->brightness()==100);
  for(int percent=0;percent<=100;percent+=5) {d->brightness(percent);assert(d->brightness()==percent);}
  d->brightness(95);assert(raw==224 && d->brightness()==95);
  std::ostringstream diagnostic;auto *previous=std::cerr.rdbuf(diagnostic.rdbuf());
  readOverride=200;bool mismatch=false;
  try{d->brightness(100);}catch(const std::exception &e){mismatch=std::string(e.what()).find("requested_raw=254 actual_raw=200")!=std::string::npos;}
  readOverride=-999;std::cerr.rdbuf(previous);assert(mismatch);
  auto log=diagnostic.str();assert(log.find("percent=100 requested_raw=254")!=std::string::npos);
  assert(log.find("ioctl_rc=0 errno=0")!=std::string::npos && log.find("actual_raw=200 ok=0")!=std::string::npos);
  reject=true;bool failed=false;try{d->brightness(60);}catch(...){failed=true;}assert(failed && raw==254);
  d->brightness(70);d->sleep();assert(raw==96 && memWrites==1);
  assert(get("/sys/class/graphics/fb0/blank")=="0");
  assert(get("/sys/class/power_supply/axp2202-battery/work_led")=="1");
  assert(get("/sys/class/power_supply/axp2202-battery/os_sleep")=="16");
  put("/sys/class/power_supply/axp2202-battery/hallkey","0");
  std::thread lid([]{usleep(400000);put("/sys/class/power_supply/axp2202-battery/hallkey","1");});
  d->sleep();lid.join();assert(memWrites>=3 && raw==96);
  failSuspendWrite=true;
  failed=false;try{d->sleep();}catch(...){failed=true;}
  assert(failed && raw==96 && get("/sys/class/graphics/fb0/blank")=="0");
  assert(fs::exists("/tmp/rondo-mixer-restored"));
  std::cout<<"PASS: guarded H700 adapter, disp2 ioctl arguments/readback, visible minimum, brightness failure, mem suspend protocol, closed-lid re-suspend, display/mixer restoration on success and failure. Synthetic sysfs only.\n";
}
