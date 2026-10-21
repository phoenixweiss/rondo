// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "clock.hpp"
#include "platform.hpp"
#include <algorithm>
#include <ctime>
#include <sstream>
namespace rondo {
bool clockBaseosPlatformSupported(const std::string &release, const std::string &os) {
  std::istringstream platformInfo(release), osInfo(os);
  std::string line;
  bool target = false, h700 = false, baseos = false;
  while (std::getline(platformInfo, line)) {
    target |= line == "BASEOS_TARGET=rg34xxsp";
    h700 |= line == "BASEOS_PLATFORM=h700";
  }
  while (std::getline(osInfo, line))
    baseos |= line == "ID=baseos";
  return target && h700 && baseos;
}
int clockDays(int year, int month) {
  if (month < 1 || month > 12)
    throw ClockError("invalid");
  static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return days[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
long long clockUtc(const ClockFields &f) {
  if (f.year < 2020 || f.year > 2099 || f.month < 1 || f.month > 12 || f.day < 1 ||
      f.day > clockDays(f.year, f.month) || f.hour < 0 || f.hour > 23 || f.minute < 0 || f.minute > 59)
    throw ClockError("invalid");
  std::vector<time_t> candidates;
  for (int dst : {-1, 0, 1}) {
    tm local{};
    local.tm_year = f.year - 1900; local.tm_mon = f.month - 1; local.tm_mday = f.day;
    local.tm_hour = f.hour; local.tm_min = f.minute; local.tm_isdst = dst;
    time_t utc = mktime(&local);
    tm check{};
    if (utc != time_t(-1) && localtime_r(&utc, &check) && check.tm_year == f.year - 1900 &&
        check.tm_mon == f.month - 1 && check.tm_mday == f.day && check.tm_hour == f.hour &&
        check.tm_min == f.minute && check.tm_sec == 0 &&
        std::find(candidates.begin(), candidates.end(), utc) == candidates.end())
      candidates.push_back(utc);
  }
  if (candidates.empty()) throw ClockError("invalid");
  if (candidates.size() != 1) throw ClockError("ambiguous");
  return candidates.front();
}
} // namespace rondo
#ifdef __linux__
#include <fcntl.h>
#include <fstream>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/timex.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
namespace rondo {
static std::string readClock(const fs::path &path) {
  std::ifstream input(path);
  std::string value;
  std::getline(input, value);
  return value;
}
class BaseClock final : public ClockAdapter {
  fs::path notify;
  std::vector<std::string> environment;
  void command(bool enabled) {
    std::vector<char *> values;
    for (auto &value : environment)
      values.push_back(value.data());
    values.push_back(nullptr);
    posix_spawn_file_actions_t files;
    posix_spawn_file_actions_init(&files);
    std::vector<int> descriptors;
    {
      std::error_code error;
      for (auto &entry : fs::directory_iterator("/proc/self/fd", error))
        try {
          int fd = std::stoi(entry.path().filename().string());
          if (fd > 2)
            descriptors.push_back(fd);
        } catch (...) {
        }
    }
    for (int fd : descriptors)
      if (fcntl(fd, F_GETFD) >= 0)
        posix_spawn_file_actions_addclose(&files, fd);
    for (int fd = 0; fd < 3; ++fd)
      posix_spawn_file_actions_addopen(&files, fd, "/dev/null", fd == 0 ? O_RDONLY : O_WRONLY, 0);
    char program[] = "/usr/bin/timedatectl", mode[] = "set-ntp", yes[] = "true", no[] = "false";
    char *argv[] = {program, mode, enabled ? yes : no, nullptr};
    pid_t pid = -1;
    int rc = posix_spawn(&pid, program, &files, nullptr, argv, values.data());
    posix_spawn_file_actions_destroy(&files);
    if (rc)
      throw ClockError("service");
    auto deadline = monotonicMs() + 3000;
    int status = 0;
    for (;;) {
      auto result = waitpid(pid, &status, WNOHANG);
      if (result == pid)
        break;
      if (result < 0 && errno != EINTR)
        throw ClockError("service");
      if (monotonicMs() >= deadline) {
        kill(pid, SIGTERM);
        usleep(50000);
        if (waitpid(pid, &status, WNOHANG) != pid) {
          kill(pid, SIGKILL);
          while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
          }
        }
        throw ClockError("timeout");
      }
      usleep(20000);
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status))
      throw ClockError("service");
    if (!enabled) {
      auto deadline = monotonicMs() + 3000;
      while (fs::exists("/run/baseos-ntp.lock")) {
        if (monotonicMs() >= deadline)
          throw ClockError("timeout");
        usleep(20000);
      }
    }
    if ((readClock("/data/ntp-enabled") == "yes") != enabled)
      throw ClockError("readback");
  }

public:
  explicit BaseClock(const fs::path &path) : notify("/run/rondo-ntp-notify.sh") {
    // Snapshot on construction, before the UI can temporarily change TZ.
    for (char **p = environ; *p; ++p)
      if (std::string(*p).find("BASEOS_NTP_NOTIFY=") != 0)
        environment.emplace_back(*p);
    environment.push_back("BASEOS_NTP_NOTIFY=" + notify.string());
    // FAT may mask executable bits. Keep the callback executable in RAM;
    // BaseOS owns the service, so it must survive a frontend restart.
    auto temporary = fs::path("/run/.rondo-ntp-notify-" + std::to_string(getpid()));
    try {
      atomicWrite(temporary, readBytes(path));
      if (chmod(temporary.c_str(), 0700))
        throw ClockError("service");
      fs::rename(temporary, notify);
    } catch (...) {
      std::error_code error;
      fs::remove(temporary, error);
      throw;
    }
  }
  bool supported() const override { return true; }
  ClockState status() override {
    ClockState state;
    state.available = true;
    state.enabled = readClock("/data/ntp-enabled") == "yes";
    timex query{};
    int result = adjtimex(&query); // modes=0: query only, never set the clock here.
    try {
      auto value = readClock("/run/baseos-ntp-synchronized");
      size_t end = 0;
      auto stamp = std::stoll(value, &end);
      if (state.enabled && result >= 0 && result != TIME_ERROR &&
          !(query.status & (STA_UNSYNC | STA_CLOCKERR)) && end == value.size() && stamp > 0 &&
          stamp <= time(nullptr) + 5) {
        state.synchronized = true;
        state.syncUtc = stamp;
      }
    } catch (...) {
    }
    return state;
  }
  ClockState enable(bool enabled) override {
    command(enabled);
    return status();
  }
  ClockState restart() override {
    command(false);
    command(true);
    return status();
  }
  ClockState setTime(long long utc) override {
    if (utc < 1577746800LL || utc > 4102534800LL)
      throw ClockError("invalid");
    // Stop and confirm the NTP worker before changing CLOCK_REALTIME.
    command(false);
    timespec requested{static_cast<time_t>(utc), 0}, actual{};
    if (clock_settime(CLOCK_REALTIME, &requested))
      throw ClockError("set");
    if (clock_gettime(CLOCK_REALTIME, &actual) || actual.tv_sec < utc || actual.tv_sec > utc + 2)
      throw ClockError("clock_readback");
    ClockState result;
    result.available = true;
    result.manualUtc = utc;
    // BaseOS boots from a UTC RTC. Verify its value; do not report a mere attempt as success.
    int fd = open("/dev/rtc0", O_RDWR | O_CLOEXEC);
    if (fd < 0) fd = open("/dev/rtc", O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
      time_t stamp = utc;
      tm value{};
      gmtime_r(&stamp, &value);
      rtc_time rtc{};
      rtc.tm_sec=value.tm_sec; rtc.tm_min=value.tm_min; rtc.tm_hour=value.tm_hour;
      rtc.tm_mday=value.tm_mday; rtc.tm_mon=value.tm_mon; rtc.tm_year=value.tm_year;
      rtc_time readback{};
      if (ioctl(fd, RTC_SET_TIME, &rtc) == 0 && ioctl(fd, RTC_RD_TIME, &readback) == 0) {
        tm got{};
        got.tm_sec=readback.tm_sec; got.tm_min=readback.tm_min; got.tm_hour=readback.tm_hour;
        got.tm_mday=readback.tm_mday; got.tm_mon=readback.tm_mon; got.tm_year=readback.tm_year;
        time_t saved = timegm(&got);
        result.rtcSaved = saved >= utc && saved <= utc + 2;
      }
      close(fd);
    }
    return result;
  }
};
} // namespace rondo
#endif
namespace rondo {
std::shared_ptr<ClockAdapter> clockAdapter(const fs::path &notify, bool enabled) {
#ifdef __linux__
  std::ifstream release("/etc/baseos-release"), os("/etc/os-release");
  const std::string releaseInfo((std::istreambuf_iterator<char>(release)), {});
  const std::string osInfo((std::istreambuf_iterator<char>(os)), {});
  if (enabled && clockBaseosPlatformSupported(releaseInfo, osInfo) &&
      fs::is_regular_file(notify) && access("/usr/bin/timedatectl", X_OK) == 0 &&
      access("/usr/sbin/baseos-ntp", X_OK) == 0 && access("/usr/sbin/ntpd", X_OK) == 0)
    try {
      return std::make_shared<BaseClock>(notify);
    } catch (...) {
      return std::make_shared<ClockAdapter>();
    }
#else
  (void)notify;
  (void)enabled;
#endif
  return std::make_shared<ClockAdapter>();
}
} // namespace rondo
