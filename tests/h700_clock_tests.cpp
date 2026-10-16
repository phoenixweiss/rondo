// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "clock.hpp"
#include "platform.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sys/timex.h>
#include <unistd.h>
#include <cstdarg>
#include <linux/rtc.h>
#include <sys/ioctl.h>
using namespace rondo;
static bool synchronized = false;
static time_t manualTime=0;
static bool setFails=false, readFails=false, rtcFails=false, rtcMismatch=false;
static rtc_time savedRtc{};
static int rtcWrites=0;
extern "C" int __real_clock_gettime(clockid_t, struct timespec *);
extern "C" int __wrap_clock_settime(clockid_t clock, const struct timespec *value) {
  assert(clock==CLOCK_REALTIME && value->tv_nsec==0);
  assert(!fs::exists("/run/baseos-ntp.lock"));
  std::ifstream pref("/data/ntp-enabled");std::string choice;std::getline(pref,choice);
  assert(choice=="no");
  if (setFails) {errno=EPERM;return -1;}
  manualTime=value->tv_sec;return 0;
}
extern "C" int __wrap_clock_gettime(clockid_t clock, struct timespec *value) {
  if (clock!=CLOCK_REALTIME || !manualTime) return __real_clock_gettime(clock,value);
  if(readFails){errno=EIO;return -1;}
  value->tv_sec=manualTime;value->tv_nsec=0;return 0;
}
extern "C" int __wrap_ioctl(int, unsigned long request, ...) {
  va_list args;va_start(args,request);auto value=va_arg(args,rtc_time*);va_end(args);
  assert(request==RTC_SET_TIME || request==RTC_RD_TIME);
  if(rtcFails){errno=EIO;return -1;}
  if(request==RTC_SET_TIME){savedRtc=*value;++rtcWrites;}
  else {*value=savedRtc;if(rtcMismatch)value->tm_year--;}
  return 0;
}
extern "C" int __wrap_adjtimex(struct timex *query) {
  assert(query->modes == 0);
  query->status = synchronized ? 0 : STA_UNSYNC;
  return synchronized ? TIME_OK : TIME_ERROR;
}
static void write(const fs::path &path, const std::string &text) {
  fs::create_directories(path.parent_path());
  std::ofstream(path) << text;
}
int main() {
  assert(readJson("/tmp/clock-test-guard.json").value("synthetic", false));
  fs::create_directories("/tmp/clock-bin");
  write("/tmp/clock-bin/hwclock", "#!/bin/sh\nprintf '%s\n' \"$*\" > /tmp/clock-rtc-request\n");
  write("/tmp/clock-bin/ntpd",
        "#!/bin/sh\nsleep 0.1\n\"$BASEOS_NTP_NOTIFY\" step\nexec sleep 300\n");
  fs::permissions("/tmp/clock-bin/hwclock", fs::perms::owner_all);
  fs::permissions("/tmp/clock-bin/ntpd", fs::perms::owner_all);
  setenv("PATH", "/tmp/clock-bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
  setenv("BASEOS_NTP_LOG", "/tmp/clock-service.log", 1);
  setenv("BASEOS_NTP_DAEMON", "/tmp/clock-bin/ntpd", 1);
  setenv("BASEOS_NTP_SKIP_NETWORK_CHECK", "1", 1);
  setenv("BASEOS_NTP_RETRY_SECONDS", "1", 1);
  fs::permissions("/tmp/app/tools/ntp-notify.sh",
                  fs::perms::owner_read); // Simulate FAT masking executable bits.
  auto device = clockAdapter("/tmp/app/tools/ntp-notify.sh", true);
  assert(device->supported());
  assert((fs::status("/run/rondo-ntp-notify.sh").permissions() & fs::perms::owner_exec) !=
         fs::perms::none);
  assert(!device->enable(false).enabled);
  assert(device->restart().enabled);
  auto deadline = monotonicMs() + 5000;
  while (!fs::exists("/run/baseos-ntp-synchronized") && monotonicMs() < deadline)
    usleep(20000);
  assert(fs::exists("/run/baseos-ntp-synchronized"));
  assert(!device->status().synchronized);
  synchronized = true;
  assert(device->status().synchronized);
  std::string arguments;
  do {
    std::ifstream rtc("/tmp/clock-rtc-request");
    std::getline(rtc, arguments);
    if (arguments == "-u -w")
      break;
    usleep(20000);
  } while (monotonicMs() < deadline);
  assert(arguments == "-u -w");
  write("/run/baseos-ntp-synchronized", std::to_string(time(nullptr) + 86400));
  assert(!device->status().synchronized);
  assert(!device->enable(false).enabled);
  assert(!fs::exists("/run/baseos-ntp.lock"));
  assert(system("stratum=16 /bin/sh /tmp/app/tools/ntp-notify.sh periodic") == 0);
  assert(!fs::exists("/run/baseos-ntp-synchronized"));
  write("/dev/rtc0",""); // Private tmpfs only; never a host device.
  auto set=device->setTime(1767225600);
  assert(set.manualUtc==1767225600 && set.rtcSaved && !set.enabled && !set.synchronized);
  assert(savedRtc.tm_year==126 && savedRtc.tm_mon==0 && savedRtc.tm_mday==1 && savedRtc.tm_hour==0);
  rtcFails=true;
  assert(!device->setTime(1767225600).rtcSaved);
  rtcFails=false;rtcMismatch=true;
  assert(!device->setTime(1767225600).rtcSaved);
  rtcMismatch=false;
  auto expectFailure=[&](const std::string &error) {
    int before=rtcWrites;
    try {device->setTime(1767225600);assert(false);}
    catch(const ClockError &e){assert(e.what()==error);}
    assert(rtcWrites==before);
  };
  setFails=true;expectFailure("set");setFails=false;
  readFails=true;expectFailure("clock_readback");readFails=false;
  assert(system("stratum=2 /bin/sh /tmp/app/tools/ntp-notify.sh stratum") == 0);
  assert(fs::exists("/run/baseos-ntp-synchronized"));
  assert(system("/bin/sh /tmp/app/tools/ntp-notify.sh unsync") == 0);
  assert(!fs::exists("/run/baseos-ntp-synchronized"));
  std::cout << "PASS: exact BaseOS time service, filtered callback, synchronized-kernel "
               "acknowledgement, future-marker rejection, UTC RTC request and stop/restart "
               "cleanup, offline clock write/readback and UTC RTC verification/failures. "
               "Daemon/clock/RTC substituted; no real clock set.\n";
}
