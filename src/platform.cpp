// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "platform.hpp"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cmath>
namespace rondo {
static std::string value(const fs::path &p) {
  std::ifstream f(p);
  std::string s;
  std::getline(f, s);
  return s;
}
// The RG34XXSP vendor ioctl remaps 255 to 200 (and 200 to 170).
// Use 254 for the maximum; the 5% curve avoids both remapped raw values.
int brightnessRawLevel(int percent) {
  static constexpr int levels[11]={4,6,10,16,32,48,64,96,128,192,255};
  int step=(std::clamp(percent,0,100)+2)/5, lower=step/2;
  int raw=step%2?(levels[lower]+levels[lower+1]+1)/2:levels[lower];
  return std::min(raw,254); // Keep the existing 95% level (224) unchanged.
}
int brightnessPercent(int raw) {
  int nearest=0;
  for(int step=5;step<=100;step+=5)
    if(std::abs(brightnessRawLevel(step)-raw)<std::abs(brightnessRawLevel(nearest)-raw))nearest=step;
  return nearest;
}
int wifiSignalLevel(std::optional<int> dbm) {
  if (!dbm || *dbm < -127 || *dbm >= 0) return -1;
  return *dbm >= -55 ? 3 : *dbm >= -67 ? 2 : *dbm >= -80 ? 1 : 0;
}
Telemetry readTelemetry(const fs::path &sys, const fs::path &proc) {
  Telemetry t;
  auto battery = sys / "class/power_supply/axp2202-battery";
  try {
    auto s = value(battery / "capacity");
    size_t end = 0;
    int n = std::stoi(s, &end);
    if (end == s.size() && n >= 0 && n <= 100)
      t.capacity = n;
  } catch (...) {
  }
  t.batteryStatus = value(battery / "status");
  try {
    long n = std::stol(value(battery / "voltage_now"));
    if (n > 0)
      t.voltageMicrovolts = n;
  } catch (...) {
  }
  auto rfkill = sys / "class/rfkill";
  std::error_code e;
  if (fs::is_directory(rfkill, e))
    for (auto &entry :
         fs::directory_iterator(rfkill, fs::directory_options::skip_permission_denied, e)) {
      if (value(entry.path() / "type") == "wlan")
        t.wifiEnabled = t.wifiEnabled || (value(entry.path() / "soft") == "0" && value(entry.path() / "hard") == "0");
    }
  auto net = sys / "class/net/wlan0";
  // An unrelated, unblocked rfkill node does not mean wlan0 is enabled.
  try {
    auto flags = value(net / "flags"); size_t end = 0;
    auto bits = std::stoul(flags, &end, 0);
    t.wifiEnabled = t.wifiEnabled && end == flags.size() && (bits & 1u);
  } catch (...) { t.wifiEnabled = false; }
  if (t.wifiEnabled) {
    t.wifiState = value(net / "carrier") == "1"           ? "connected"
                  : value(net / "operstate") == "dormant" ? "connecting"
                                                          : "disconnected";
  }
  if (t.wifiState == "connected") {
    std::ifstream wireless(proc / "net/wireless"); std::string line;
    while (std::getline(wireless, line)) {
      auto colon = line.find(':'); if (colon == line.npos) continue;
      auto name = line.substr(0,colon);
      name.erase(std::remove_if(name.begin(),name.end(),[](unsigned char c){return std::isspace(c);}),name.end());
      if (name != "wlan0") continue;
      std::istringstream fields(line.substr(colon+1)); std::string status; double quality=0, dbm=0;
      if (fields >> status >> quality >> dbm && std::isfinite(dbm) && dbm >= -127 && dbm < 0)
        t.wifiSignalDbm = int(dbm);
    }
  }
  return t;
}
} // namespace rondo
#ifdef __linux__
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include <algorithm>
#include <iostream>
namespace rondo {
bool bootLogoSupported() {
#ifdef __linux__
  const char *tool = getenv("RONDO_BOOTLOGO_TOOL");
  return tool && fs::is_regular_file(tool);
#else
  return false;
#endif
}
static int bootLogoCommand(const char *mode) {
#ifdef __linux__
  if (!bootLogoSupported()) throw std::runtime_error("Boot logo adapter unavailable");
  std::string tool = getenv("RONDO_BOOTLOGO_TOOL");
  pid_t pid = fork();
  if (pid < 0) throw std::runtime_error("Cannot start boot logo adapter");
  if (!pid) {
    setpgid(0, 0);
    execl("/bin/sh", "sh", tool.c_str(), mode, nullptr);
    _exit(127);
  }
  int status = 0;
  auto deadline = monotonicMs() + 60000;
  for (;;) {
    auto result = waitpid(pid, &status, WNOHANG);
    if (result == pid) break;
    if (result < 0 && errno != EINTR) throw std::runtime_error("Boot logo adapter wait failed");
    if (monotonicMs() >= deadline) {
      kill(-pid, SIGTERM);
      usleep(500000);
      kill(-pid, SIGKILL);
      while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
      throw std::runtime_error("Boot logo adapter timed out; inspect before retry");
    }
    usleep(10000);
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
  (void)mode;
  throw std::runtime_error("Boot logo adapter unavailable");
#endif
}
void applyBootLogo(bool enabled) {
  if (bootLogoCommand(enabled ? "on" : "off") != 0)
    throw std::runtime_error("Boot logo update failed; see launcher.log");
}
std::optional<bool> readBootLogo() {
  try {
    int result = bootLogoCommand("status");
    if (result == 10) return true;
    if (result == 11) return false;
  } catch (...) {}
  return std::nullopt;
}
std::vector<std::string> H700InputState::event(unsigned type, unsigned code, int value, uint64_t now) {
  std::vector<std::string> actions;
  auto set = [&](const std::string &action, bool held) {
    if (held && down.insert(action).second) {
      actions.push_back(action);
      if (action == "UP" || action == "DOWN" || action == "LEFT" || action == "RIGHT")
        repeats[action] = now + 350;
    } else if (!held) {
      down.erase(action);
      repeats.erase(action);
    }
  };
  if (type == 0 && code == 3) { // SYN_DROPPED: never leave a game button stuck.
    down.clear();
    repeats.clear();
  } else if (type == 1 && (value == 0 || value == 1)) {
    static const std::map<unsigned, std::string> codes = {
      {103,"UP"},{108,"DOWN"},{105,"LEFT"},{106,"RIGHT"},
      {304,"A"},{305,"B"},{307,"X"},{306,"Y"},
      {310,"SELECT"},{311,"START"},{308,"L1"},{309,"R1"},{312,"MENU"}};
    auto it = codes.find(code);
    if (it != codes.end()) set(it->second, value == 1);
    // Ignore synthetic KEY_GOTO (354), POWER, lid and duplicate kernel repeats.
  } else if (type == 3 && (code == 16 || code == 17)) {
    set(code == 16 ? "LEFT" : "UP", value < 0);
    set(code == 16 ? "RIGHT" : "DOWN", value > 0);
  }
  return actions;
}
std::vector<std::string> H700InputState::repeat(uint64_t now) {
  std::vector<std::string> actions;
  for (auto &[action, deadline] : repeats)
    if (now >= deadline) {
      actions.push_back(action);
      deadline = now + 90;
    }
  return actions;
}
H700Keys::H700Keys() {
#ifdef __linux__
  const char *mode = getenv("RONDO_DEVICE_INPUT");
  if (!mode || std::string(mode) != "h700") return;
  std::error_code error;
  if (!fs::is_directory("/dev/input", error)) return;
  for (auto &entry : fs::directory_iterator("/dev/input", fs::directory_options::skip_permission_denied, error)) {
    if (entry.path().filename().string().find("event") != 0) continue;
    int fd = open(entry.path().c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;
    char name[256]{};
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    if (std::string(name) != "ANBERNIC-keys") { close(fd); continue; }
    devices.push_back({fd, {}});
    std::cerr << "H700 input: " << entry.path() << " " << name << " (raw evdev, no grab)\n";
  }
  if (devices.empty()) std::cerr << "H700 input: ANBERNIC-keys not found or unreadable\n";
#endif
}
H700Keys::~H700Keys() {
#ifdef __linux__
  for (auto &device : devices) close(device.fd);
#endif
}
bool H700Keys::pressed(const std::string &action) const {
  for (auto &device : devices) if (device.state.down.count(action)) return true;
  return false;
}
std::vector<std::string> H700Keys::poll() {
  std::vector<std::string> actions;
#ifdef __linux__
  auto now = monotonicMs();
  for (auto &device : devices) {
    input_event e;
    ssize_t count;
    while ((count = read(device.fd, &e, sizeof(e))) == sizeof(e)) {
      auto next = device.state.event(e.type, e.code, e.value, now);
      actions.insert(actions.end(), next.begin(), next.end());
    }
    if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
      device.state.down.clear();
      device.state.repeats.clear();
    }
    auto next = device.state.repeat(now);
    actions.insert(actions.end(), next.begin(), next.end());
  }
#endif
  return actions;
}
std::string volumeKeyAction(unsigned type, unsigned code, int value) {
  if (type != 1 || (value != 1 && value != 2))
    return "";
  return code == 115 ? "VOLUME_UP" : code == 114 ? "VOLUME_DOWN" : "";
}
VolumeKeys::VolumeKeys() {
#ifdef __linux__
  std::error_code error;
  fs::path root = "/dev/input";
  if (!fs::is_directory(root, error))
    return;
  for (auto &entry :
       fs::directory_iterator(root, fs::directory_options::skip_permission_denied, error)) {
    if (entry.path().filename().string().find("event") != 0)
      continue;
    int fd = open(entry.path().c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
      continue;
    unsigned long bits[(KEY_MAX + 8 * sizeof(unsigned long)) / (8 * sizeof(unsigned long))]{};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0) {
      close(fd);
      continue;
    }
    auto has = [&](unsigned code) {
      return (bits[code / (8 * sizeof(unsigned long))] >> (code % (8 * sizeof(unsigned long)))) & 1;
    };
    if (has(KEY_VOLUMEUP) || has(KEY_VOLUMEDOWN))
      devices.push_back(fd);
    else
      close(fd);
  }
#endif
}
VolumeKeys::~VolumeKeys() {
#ifdef __linux__
  for (int fd : devices)
    close(fd);
#endif
}
std::vector<std::string> VolumeKeys::poll() {
  std::vector<std::string> actions;
#ifdef __linux__
  for (int fd : devices) {
    input_event event;
    while (read(fd, &event, sizeof(event)) == sizeof(event)) {
      auto action = volumeKeyAction(event.type, event.code, event.value);
      if (!action.empty())
        actions.push_back(action);
    }
  }
#endif
  return actions;
}
std::vector<int16_t> mixVolume(const int16_t *data, size_t frames, int percent) {
  std::vector<int16_t> out(frames * 2);
  int gain = std::clamp(percent, 0, 100);
  for (size_t i = 0; i < out.size(); ++i)
    out[i] = int16_t(int32_t(data[i]) * gain / 100);
  return out;
}
} // namespace rondo
#include <chrono>
namespace rondo {
uint64_t monotonicMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
uint64_t VolumeFeedback::begin(uint64_t now) {
  ++sequence;
  phase = "applying";
  deadline = now + 2000;
  return sequence;
}
void VolumeFeedback::result(uint64_t operation, std::optional<int> value, bool ok, uint64_t now) {
  if (operation != sequence)
    return;
  if (!ok) {
    phase = "error";
    deadline = now + 3000;
    return;
  }
  if (!value) {
    phase = "unknown";
    confirmed.reset();
    deadline = now + 3000;
    return;
  }
  confirmed = std::clamp(*value, 0, 100);
  phase = *confirmed == 0 ? "muted" : "confirmed";
  deadline = now + 1500;
}
void VolumeFeedback::timeout(uint64_t now) {
  if (phase == "applying" && now >= deadline) {
    phase = "error";
    deadline = now + 3000;
  }
}
bool VolumeFeedback::visible(uint64_t now) const { return phase != "hidden" && now < deadline; }
Json VolumeFeedback::json() const {
  return {{"phase", phase},
          {"confirmed", confirmed ? Json(*confirmed) : Json()},
          {"sequence", sequence},
          {"deadline", deadline}};
}
void VolumeFeedback::restore(const Json &j) {
  phase = j.value("phase", "hidden");
  confirmed = j.contains("confirmed") && !j["confirmed"].is_null()
                  ? std::optional<int>(j["confirmed"].get<int>())
                  : std::nullopt;
  sequence = j.value("sequence", uint64_t(0));
  deadline = j.value("deadline", uint64_t(0));
}
} // namespace rondo

namespace rondo {
std::string PowerInputState::event(unsigned type, unsigned code, int value) {
  if (type == 0 && code == 3) { held = false; return ""; }
  if (type != 1 || code != 116) return "";
  if (value == 1) held = true;
  if (value == 0) { bool fresh = held; held = false; return fresh ? "SLEEP" : ""; }
  return "";
}
PowerKeys::PowerKeys() {
#ifdef __linux__
  const char *mode = getenv("RONDO_DEVICE_INPUT");
  if (!mode || std::string(mode) != "h700") return;
  std::error_code error;
  if (!fs::is_directory("/dev/input",error)) return;
  for (auto &entry : fs::directory_iterator("/dev/input",fs::directory_options::skip_permission_denied,error)) {
    if (entry.path().filename().string().find("event") != 0) continue;
    int fd=open(entry.path().c_str(),O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if (fd<0) continue;
    char name[128]{};ioctl(fd,EVIOCGNAME(sizeof(name)),name);
    if (std::string(name).find("axp")!=0 || std::string(name).find("-pek")==std::string::npos) { close(fd);continue; }
    unsigned long keys[(KEY_MAX+8*sizeof(unsigned long))/(8*sizeof(unsigned long))]{};
    bool power=ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(keys)),keys)>=0 &&
      ((keys[KEY_POWER/(8*sizeof(unsigned long))]>>(KEY_POWER%(8*sizeof(unsigned long))))&1);
    if (!power) { close(fd); continue; }
    input_event e; while (read(fd,&e,sizeof(e))==sizeof(e)) {}
    devices.push_back({fd,{}});
    break; // One PMIC power key; never duplicate a sleep request across devices.
  }
#endif
}
PowerKeys::~PowerKeys() {
#ifdef __linux__
  for (auto &d:devices) close(d.fd);
#endif
}
std::vector<std::string> PowerKeys::poll() {
  std::vector<std::string> out;
#ifdef __linux__
  for (auto &d:devices) { input_event e; while (read(d.fd,&e,sizeof(e))==sizeof(e)) {
    auto action=d.state.event(e.type,e.code,e.value); if (!action.empty()) out.push_back(action);
  }}
#endif
  return out;
}
#ifdef __linux__
static void sysWrite(const fs::path &path, const std::string &text, bool required=true) {
  int fd=open(path.c_str(),O_WRONLY|O_CLOEXEC);
  if (fd<0) { if (required) throw std::runtime_error("Cannot open "+path.string()); return; }
  ssize_t n=write(fd,text.data(),text.size()); // Never retry a possibly completed suspend.
  int error=errno; close(fd);
  if (n!=ssize_t(text.size()) && required) throw std::runtime_error("Device write failed: "+path.string()+" errno="+std::to_string(error));
}
static bool mixer(const std::string &path, const char *operation) {
  if (access("/usr/sbin/alsactl",X_OK)!=0) return false;
  pid_t child=fork(); if (child<0) return false;
  if (!child) { execl("/usr/sbin/alsactl","alsactl","-f",path.c_str(),operation,nullptr); _exit(127); }
  auto deadline=monotonicMs()+3000; int status=0;
  for (;;) {
    auto result=waitpid(child,&status,WNOHANG);
    if (result==child) break;
    if (result<0) { if(errno==EINTR)continue;return false; }
    if (monotonicMs()>=deadline) { kill(child,SIGKILL); while (waitpid(child,&status,0)<0 && errno==EINTR) {} return false; }
    usleep(10000);
  }
  return WIFEXITED(status) && WEXITSTATUS(status)==0;
}
class H700Device final : public DeviceControl {
  bool supported = false;
  static void brightnessLog(const std::string &message) {
    std::cerr<<"[brightness] t_ms="<<monotonicMs()<<" pid="<<getpid()<<" "<<message<<"\n";
  }
  int rawBrightness() {
    int fd=open("/dev/disp",O_RDWR|O_CLOEXEC);
    if(fd<0) { int error=errno;brightnessLog("op=get open_rc=-1 errno="+std::to_string(error));throw std::runtime_error("Cannot open display for brightness read"); }
    unsigned long args[4]{};errno=0;
    int raw=ioctl(fd,0x103,args),error=raw<0?errno:0;close(fd);
    brightnessLog("op=get actual_raw="+std::to_string(raw)+" errno="+std::to_string(error));
    if(raw<0 || raw>255)throw std::runtime_error("Cannot read display brightness: actual_raw="+std::to_string(raw)+" errno="+std::to_string(error));
    return raw;
  }
  void rawBrightness(int raw) {
    int fd=open("/dev/disp",O_RDWR|O_CLOEXEC);
    if(fd<0) {int error=errno;brightnessLog("op=set requested_raw="+std::to_string(raw)+" open_rc=-1 errno="+std::to_string(error));throw std::runtime_error("Cannot open display for brightness write");}
    unsigned long args[4]={0,static_cast<unsigned long>(raw),0,0};errno=0;
    int result=ioctl(fd,0x102,args),error=result<0?errno:0;close(fd);
    brightnessLog("op=set requested_raw="+std::to_string(raw)+" ioctl_rc="+std::to_string(result)+" errno="+std::to_string(error));
    if(result<0)throw std::runtime_error("Cannot change display brightness: requested_raw="+std::to_string(raw)+" errno="+std::to_string(error));
    int actual=rawBrightness();
    brightnessLog("op=verify requested_raw="+std::to_string(raw)+" actual_raw="+std::to_string(actual)+" ok="+(actual==raw?"1":"0"));
    if(actual!=raw)throw std::runtime_error("Brightness readback mismatch: requested_raw="+std::to_string(raw)+" actual_raw="+std::to_string(actual));
  }
public:
  explicit H700Device(bool enabled) {
    if (!enabled) return;
    std::ifstream f("/etc/baseos-release"),os("/etc/os-release");
    std::string line;bool target=false,baseos=false;
    while(std::getline(f,line)) if(line=="BASEOS_TARGET=rg34xxsp") target=true;
    while(std::getline(os,line)) if(line=="ID=baseos") baseos=true;
    supported=target && baseos;
  }
  std::optional<int> brightness() override {
    if (!supported) return std::nullopt;
    try { return brightnessPercent(rawBrightness()); } catch (...) { return std::nullopt; }
  }
  void brightness(int percent) override {
    if (!supported) throw std::runtime_error("Unsupported device");
    int raw=brightnessRawLevel(percent);
    brightnessLog("op=request percent="+std::to_string(percent)+" requested_raw="+std::to_string(raw));
    rawBrightness(raw);
  }
  bool canSleep() const override { return supported && value("/sys/power/state").find("mem")!=std::string::npos && access("/sys/power/state",W_OK)==0; }
  std::optional<bool> lidOpen() override {
    if (!supported) return std::nullopt;
    auto s=value("/sys/class/power_supply/axp2202-battery/hallkey");return s=="1"?std::optional<bool>(true):s=="0"?std::optional<bool>(false):std::nullopt;
  }
  // The experimental module loaded on hardware, but hall readback was missing.
  // Keep this capability unavailable until the device contract is established.
  void sleep(bool wakeOnOpen, bool sleepWhenClosed) override {
    if (!canSleep()) throw std::runtime_error("Suspend-to-RAM unavailable");
    if (wakeOnOpen && !canWakeOnLid()) throw std::runtime_error("Lid wake adapter unavailable");
    std::cerr<<"[sleep] t_ms="<<monotonicMs()<<" op=prepare wake_on_open="<<wakeOnOpen<<" lid_sleep="<<sleepWhenClosed<<"\n";
    int old=rawBrightness();
    auto state="/tmp/rondo-mixer-"+std::to_string(getpid())+".state";
    bool savedMixer=mixer(state,"store");
    if (access("/usr/sbin/alsactl",X_OK)==0 && !savedMixer) {
      unlink(state.c_str());throw std::runtime_error("Cannot save mixer before suspend");
    }
    std::string failure;
    try {
      rawBrightness(0);
      sysWrite("/sys/class/graphics/fb0/blank","1",false);
      sysWrite("/sys/class/power_supply/axp2202-battery/work_led","0",false);
      sync();
      for (;;) {
        sysWrite("/sys/class/power_supply/axp2202-battery/os_sleep","16",false);
        std::cerr<<"[sleep] op=suspend mode=stock-power-wake\n";
        // This write returns on resume. Never replace a failed suspend with poweroff.
        sysWrite("/sys/power/state","mem");
        auto open=lidOpen();
        if (!open || *open || !sleepWhenClosed) break;
        std::cerr<<"[sleep] op=closed-lid-resuspend\n";
        usleep(250000); // Ignore pocketed POWER while the lid is still closed.
      }
    } catch (const std::exception &e) { failure=e.what(); }
    if (savedMixer && !mixer(state,"restore")) failure="Mixer restore failed";
    unlink(state.c_str());
    sysWrite("/sys/class/graphics/fb0/blank","0",false);
    sysWrite("/sys/class/power_supply/axp2202-battery/work_led","1",false);
    rawBrightness(old);
    if (!failure.empty()) throw std::runtime_error(failure);
  }
};
#endif
std::unique_ptr<DeviceControl> deviceControl(bool enabled) {
#ifdef __linux__
  return std::make_unique<H700Device>(enabled);
#else
  (void)enabled;return std::make_unique<DeviceControl>();
#endif
}
}
