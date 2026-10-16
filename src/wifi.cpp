// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "wifi.hpp"
#include "platform.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
namespace rondo {
void clearSecret(std::string &s) {
  for (size_t i = 0; i < s.size(); ++i)
    static_cast<volatile char *>(s.data())[i] = 0;
  s.clear();
}
static std::string hex(const std::string &s) {
  static const char *digits = "0123456789abcdef";
  std::string out;
  for (unsigned char c : s) {
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out;
}
std::string wifiDecode(const std::string &s) {
  std::string out;
  auto digit = [](char c) {
    return std::isdigit(static_cast<unsigned char>(c))
               ? c - '0'
               : std::tolower(static_cast<unsigned char>(c)) - 'a' + 10;
  };
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char c = s[++i];
      if (c == 'x' && i + 2 < s.size() && std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
          std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
        out += char(digit(s[i + 1]) * 16 + digit(s[i + 2]));
        i += 2;
      } else
        out += c == 'n' ? '\n' : c == 'r' ? '\r' : c == 't' ? '\t' : c == 'e' ? char(27) : c;
    } else
      out += s[i];
  }
  return out;
}
std::string wifiLabel(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size();) {
    auto c = static_cast<unsigned char>(s[i]);
    size_t n = c < 0x80                 ? 1
               : c >= 0xc2 && c <= 0xdf ? 2
               : c >= 0xe0 && c <= 0xef ? 3
               : c >= 0xf0 && c <= 0xf4 ? 4
                                        : 0;
    bool valid = n && i + n <= s.size();
    for (size_t j = 1; valid && j < n; ++j)
      valid = (static_cast<unsigned char>(s[i + j]) & 0xc0) == 0x80;
    if (valid && n >= 3) {
      auto b = static_cast<unsigned char>(s[i + 1]);
      valid = !(c == 0xe0 && b < 0xa0) && !(c == 0xed && b >= 0xa0) && !(c == 0xf0 && b < 0x90) &&
              !(c == 0xf4 && b >= 0x90);
    }
    if (!valid) {
      out += "�";
      ++i;
    } else {
      out += n == 1 && (c < 32 || c == 127) ? " " : s.substr(i, n);
      i += n;
    }
  }
  return out.empty() ? "—" : out;
}
static std::vector<std::string> split(const std::string &s, char delimiter, int maximum = 100) {
  std::vector<std::string> out;
  size_t start = 0;
  while (out.size() + 1 < size_t(maximum)) {
    auto at = s.find(delimiter, start);
    if (at == s.npos)
      break;
    out.push_back(s.substr(start, at - start));
    start = at + 1;
  }
  out.push_back(s.substr(start));
  return out;
}
std::vector<WifiNetwork> wifiNetworks(const std::string &scan, const std::string &saved) {
  std::map<std::pair<std::string, std::string>, WifiNetwork> found;
  std::istringstream input(scan);
  std::string line;
  std::getline(input, line);
  while (std::getline(input, line)) {
    auto f = split(line, '\t', 5);
    if (f.size() != 5)
      continue;
    WifiNetwork n;
    n.ssid = wifiDecode(f[4]);
    if (n.ssid.empty() || n.ssid.size() > 32)
      continue;
    n.label = wifiLabel(n.ssid);
    try {
      size_t end = 0;
      n.signal = std::stoi(f[2], &end);
      if (end != f[2].size())
        continue;
    } catch (...) {
      continue;
    }
    const auto &flags = f[3];
    n.security = flags.find("PSK") != flags.npos ? "psk"
                 : flags.find("WPA") != flags.npos || flags.find("RSN") != flags.npos ||
                         flags.find("WEP") != flags.npos || flags.find("EAP") != flags.npos ||
                         flags.find("SAE") != flags.npos || flags.find("OWE") != flags.npos
                     ? "unsupported"
                     : "open";
    n.supported = n.security == "open" || n.security == "psk";
    auto key = std::make_pair(n.ssid, n.security);
    if (!found.count(key) || found[key].signal < n.signal)
      found[key] = n;
  }
  input.clear();
  input.str(saved);
  std::getline(input, line);
  while (std::getline(input, line)) {
    auto f = split(line, '\t', 4);
    if (f.size() != 4)
      continue;
    int id = -1;
    try {
      size_t end = 0;
      id = std::stoi(f[0], &end);
      if (end != f[0].size() || id < 0)
        continue;
    } catch (...) {
      continue;
    }
    auto ssid = wifiDecode(f[1]);
    for (auto &pair : found)
      if (pair.second.ssid == ssid)
        pair.second.savedId = id;
    // Remember saved networks even when their access point is not visible.
    bool seen = false;
    for (auto &pair : found)
      if (pair.second.ssid == ssid)
        seen = true;
    if (!seen && !ssid.empty() && ssid.size() <= 32)
      found[{ssid, "saved"}] = {ssid, wifiLabel(ssid), "saved", -100, id, true};
  }
  std::vector<WifiNetwork> out;
  for (auto &pair : found)
    out.push_back(pair.second);
  std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
    if ((a.savedId >= 0) != (b.savedId >= 0))
      return a.savedId >= 0;
    if (a.signal != b.signal)
      return a.signal > b.signal;
    return a.label < b.label;
  });
  return out;
}
bool wifiPasswordValid(const std::string &s) {
  if (s.size() == 64)
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isxdigit(c); });
  return s.size() >= 8 && s.size() <= 63 &&
         std::all_of(s.begin(), s.end(), [](unsigned char c) { return c >= 32 && c <= 126; });
}
WpaSocket::WpaSocket(const fs::path &server, const fs::path &directory) {
  static std::atomic<unsigned> serial{0};
  local = directory / ("client-" + std::to_string(getpid()) + "-" + std::to_string(++serial));
  sockaddr_un own{}, peer{};
  own.sun_family = peer.sun_family = AF_UNIX;
  if (local.string().size() >= sizeof(own.sun_path) ||
      server.string().size() >= sizeof(peer.sun_path))
    throw WifiError("unavailable");
  fd = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (fd < 0)
    throw WifiError("unavailable");
  if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
    close(fd);
    fd = -1;
    throw WifiError("unavailable");
  }
  std::strcpy(own.sun_path, local.c_str());
  std::strcpy(peer.sun_path, server.c_str());
  if (bind(fd, reinterpret_cast<sockaddr *>(&own), sizeof(own)) ||
      ::connect(fd, reinterpret_cast<sockaddr *>(&peer), sizeof(peer))) {
    close(fd);
    fd = -1;
    unlink(local.c_str());
    throw WifiError("unavailable");
  }
}
WpaSocket::~WpaSocket() {
  if (fd >= 0)
    close(fd);
  unlink(local.c_str());
}
std::string WpaSocket::request(const std::string &text) {
  if (broken)
    throw WifiError("transport");
  if (text.size() > 2048 || send(fd, text.data(), text.size(), 0) != ssize_t(text.size()))
    throw WifiError("transport");
  pollfd p{fd, POLLIN, 0};
  int ready;
  do {
    ready = poll(&p, 1, 900);
  } while (ready < 0 && errno == EINTR);
  if (ready <= 0) {
    broken = true;
    throw WifiError("timeout");
  }
  char data[65536];
  auto n = recv(fd, data, sizeof(data), 0);
  if (n < 0 || n == ssize_t(sizeof(data))) {
    broken = true;
    throw WifiError("transport");
  }
  return std::string(data, size_t(n));
}
std::string WpaSocket::event() {
  pollfd p{fd, POLLIN, 0};
  if (poll(&p, 1, 0) <= 0)
    return {};
  char b[4096];
  auto n = recv(fd, b, sizeof(b), 0);
  return n > 0 ? std::string(b, size_t(n)) : std::string();
}
void WpaWifi::attach(const fs::path &socket, const fs::path &directory) {
  command = std::make_unique<WpaSocket>(socket, directory);
  monitor = std::make_unique<WpaSocket>(socket, directory);
  if (command->request("PING") != "PONG\n" || monitor->request("ATTACH") != "OK\n")
    throw WifiError("unavailable");
}
void WpaWifi::ok(const std::string &text) {
  if (!command || command->request(text) != "OK\n")
    throw WifiError("command");
}
void WpaWifi::abandon() {
  if (pendingId >= 0 && pendingNew)
    ok("REMOVE_NETWORK " + std::to_string(pendingId));
  pendingId = -1;
  pendingNew = false;
  connectDeadline = 0;
}
WifiState WpaWifi::enable(bool enabled) {
  state.available = true;
  state.error.clear();
  for (int i = 0; !radioReady(); ++i) {
    if (cancelled || i == 100)
      throw WifiError(cancelled ? "cancelled" : "initializing");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
  if (cancelled)
    throw WifiError("cancelled");
  const bool previous = radioEnabled();
  try {
    if (!enabled) {
      if (command)
        abandon();
      stop();
      command.reset();
      monitor.reset();
      radio(false);
    } else {
      radio(true);
      start();
    }
  } catch (...) {
    try {
      stop();
      command.reset();
      monitor.reset();
      radio(previous);
      if (previous)
        start();
    } catch (...) {
    }
    throw;
  }
  state.enabled = radioEnabled();
  state.ready = true;
  if (state.enabled != enabled)
    throw WifiError("radio");
  state.phase = enabled ? "disconnected" : "off";
  state.networks.clear();
  state.connectedSsid.clear();
  state.address.clear();
  return status();
}
WifiState WpaWifi::status() {
  state.available = true;
  state.ready = radioReady();
  if (!state.ready) {
    state.phase = "initializing";
    return state;
  }
  state.enabled = radioEnabled();
  if (!state.enabled) {
    state.signalDbm.reset();
    state.phase = "off";
    state.connectedSsid.clear();
    state.address.clear();
    return state;
  }
  if (!command) {
    state.phase = "idle";
    return state;
  }
  bool badKey = false, scanDone = false;
  for (int i = 0; i < 64; ++i) {
    auto event = monitor->event();
    if (event.empty())
      break;
    if (event.find("WRONG_KEY") != event.npos && pendingId >= 0) {
      auto token = event.find(" id=");
      if (token != event.npos) {
        auto id = event.substr(token + 4);
        auto end = id.find_first_of(" \r\n");
        id = id.substr(0, end);
        badKey = badKey || id == std::to_string(pendingId);
      }
    }
    if (event.find("CTRL-EVENT-SCAN-RESULTS") != event.npos)
      scanDone = true;
  }
  auto now = monotonicMs();
  if (badKey && pendingId >= 0) {
    abandon();
    dhcp(false);
    state.error = "password";
  }
  if (connectDeadline && now > connectDeadline) {
    abandon();
    ok("DISCONNECT");
    dhcp(false);
    state.error = "connect_timeout";
  }
  std::map<std::string, std::string> status;
  for (const auto &line : split(command->request("STATUS"), '\n')) {
    auto at = line.find('=');
    if (at != line.npos)
      status[line.substr(0, at)] = line.substr(at + 1);
  }
  if (status["wpa_state"] == "COMPLETED" &&
      (pendingId < 0 || status["id"] == std::to_string(pendingId))) {
    state.connectedSsid = wifiDecode(status["ssid"]);
    dhcp(true);
    state.address = ipAddress();
    state.signalDbm.reset();
    for (const auto &line : split(command->request("SIGNAL_POLL"), '\n')) {
      if (line.compare(0, 5, "RSSI=") != 0)
        continue;
      try {
        size_t end = 0;
        auto value = line.substr(5);
        int dbm = std::stoi(value, &end);
        if (end == value.size() && dbm >= -127 && dbm < 0)
          state.signalDbm = dbm;
      } catch (...) {
      }
    }
    state.phase = state.address.empty() ? "address" : "connected";
    if (!state.address.empty() && pendingId >= 0 && status["id"] == std::to_string(pendingId)) {
      try {
        ok("SAVE_CONFIG");
        state.error.clear();
        state.networks =
            wifiNetworks(command->request("SCAN_RESULTS"), command->request("LIST_NETWORKS"));
      } catch (...) {
        state.error = "save";
      }
      pendingId = -1;
      pendingNew = false;
      connectDeadline = 0;
    }
  } else {
    dhcp(false);
    state.address.clear();
    state.signalDbm.reset();
    state.connectedSsid.clear();
    state.phase = pendingId >= 0 ? "connecting" : "disconnected";
  }
  if (scanning && (scanDone || now >= scanDeadline)) {
    auto rows = command->request("SCAN_RESULTS");
    auto saved = command->request("LIST_NETWORKS");
    state.networks = wifiNetworks(rows, saved);
    scanning = false;
  }
  if (scanning)
    state.phase = "scanning";
  return state;
}
WifiState WpaWifi::scan() {
  if (!state.enabled || !command)
    throw WifiError("off");
  ok("SCAN");
  scanning = true;
  scanDeadline = monotonicMs() + 6000;
  state.error.clear();
  state.phase = "scanning";
  return state;
}
WifiState WpaWifi::connect(const WifiNetwork &network, const std::string &password) {
  if (!state.enabled || !command)
    throw WifiError("off");
  if (!network.supported || network.ssid.empty() || network.ssid.size() > 32)
    throw WifiError("unsupported");
  if (network.savedId < 0 && network.security == "psk" && !wifiPasswordValid(password))
    throw WifiError("password_format");
  if (network.savedId >= 0) {
    const auto management =
        command->request("GET_NETWORK " + std::to_string(network.savedId) + " key_mgmt");
    if (management != "NONE\n" && management.find("WPA-PSK") == management.npos)
      throw WifiError("unsupported");
  }
  abandon();
  dhcp(false);
  state.error.clear();
  state.connectedSsid.clear();
  state.address.clear();
  state.signalDbm.reset();
  int id = network.savedId;
  if (id < 0) {
    auto reply = command->request("ADD_NETWORK");
    try {
      size_t end = 0;
      id = std::stoi(reply, &end);
      if (id < 0 || (end < reply.size() && reply[end] != '\n'))
        throw WifiError("command");
    } catch (...) {
      throw WifiError("command");
    }
    pendingId = id;
    pendingNew = true;
    try {
      ok("SET_NETWORK " + std::to_string(id) + " ssid " + hex(network.ssid));
      ok("SET_NETWORK " + std::to_string(id) + " key_mgmt " +
         (network.security == "open" ? "NONE" : "WPA-PSK"));
      if (network.security == "psk") {
        std::string value = password;
        if (password.size() != 64) {
          value = "\"";
          for (char c : password) {
            if (c == '\\' || c == '\"')
              value += '\\';
            value += c;
          }
          value += '\"';
        }
        auto text = "SET_NETWORK " + std::to_string(id) + " psk " + value;
        try {
          ok(text);
        } catch (...) {
          clearSecret(text);
          clearSecret(value);
          throw;
        }
        clearSecret(text);
        clearSecret(value);
      }
    } catch (...) {
      abandon();
      throw;
    }
  } else {
    pendingId = id;
    pendingNew = false;
  }
  try {
    ok("SELECT_NETWORK " + std::to_string(id));
  } catch (...) {
    abandon();
    throw;
  }
  connectDeadline = monotonicMs() + 35000;
  state.phase = "connecting";
  return state;
}
WifiState WpaWifi::disconnect() {
  if (command) {
    abandon();
    ok("DISCONNECT");
    dhcp(false);
  }
  state.error.clear();
  state.signalDbm.reset();
  state.phase = state.enabled ? "disconnected" : "off";
  state.connectedSsid.clear();
  state.address.clear();
  return state;
}
WifiState WpaWifi::forget(const WifiNetwork &network) {
  if (network.savedId < 0)
    throw WifiError("command");
  disconnect();
  ok("REMOVE_NETWORK " + std::to_string(network.savedId));
  ok("SAVE_CONFIG");
  state.networks =
      wifiNetworks(command->request("SCAN_RESULTS"), command->request("LIST_NETWORKS"));
  return state;
}
} // namespace rondo
#ifdef __linux__
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
namespace rondo {
static std::string readWifi(const fs::path &p) {
  std::ifstream f(p);
  std::string s;
  std::getline(f, s);
  return s;
}
static void writeWifi(const fs::path &p, const std::string &text) {
  int fd = open(p.c_str(), O_WRONLY | O_CLOEXEC);
  if (fd < 0)
    throw WifiError("radio");
  auto count = write(fd, text.data(), text.size());
  int result = close(fd);
  if (count != ssize_t(text.size()) || result)
    throw WifiError("radio");
}
static pid_t spawnWifi(std::vector<std::string> args) {
  std::vector<char *> argv;
  for (auto &s : args)
    argv.push_back(s.data());
  argv.push_back(nullptr);
  posix_spawn_file_actions_t files;
  posix_spawn_file_actions_init(&files);
  // Daemons must not keep SDL audio, evdev or runner IPC descriptors alive.
  std::vector<int> descriptors;
  {
    std::error_code error;
    for (const auto &entry : fs::directory_iterator("/proc/self/fd", error)) {
      try {
        int fd = std::stoi(entry.path().filename().string());
        if (fd > 2)
          descriptors.push_back(fd);
      } catch (...) {
      }
    }
  }
  for (int fd : descriptors)
    if (fcntl(fd, F_GETFD) >= 0)
      posix_spawn_file_actions_addclose(&files, fd);
  for (int fd = 0; fd < 3; ++fd)
    posix_spawn_file_actions_addopen(&files, fd, "/dev/null", fd == 0 ? O_RDONLY : O_WRONLY, 0);
  pid_t pid = -1;
  int result = posix_spawn(&pid, argv[0], &files, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&files);
  if (result)
    throw WifiError("unavailable");
  return pid;
}
static bool running(pid_t &pid) {
  if (pid <= 0)
    return false;
  int status = 0;
  auto result = waitpid(pid, &status, WNOHANG);
  if (result == pid || (result < 0 && errno == ECHILD)) {
    pid = -1;
    return false;
  }
  return true;
}
static void stopWifi(pid_t &pid) {
  if (!running(pid))
    return;
  kill(pid, SIGTERM);
  for (int n = 0; n < 40 && running(pid); ++n)
    usleep(25000);
  if (running(pid)) {
    kill(pid, SIGKILL);
    while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {
    }
    pid = -1;
  }
}
static void runWifi(std::vector<std::string> args) {
  auto pid = spawnWifi(std::move(args));
  int status = 0;
  for (int n = 0; n < 80; ++n) {
    auto result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      if (!WIFEXITED(status) || WEXITSTATUS(status))
        throw WifiError("radio");
      return;
    }
    if (result < 0 && errno != EINTR)
      throw WifiError("radio");
    usleep(25000);
  }
  stopWifi(pid);
  throw WifiError("timeout");
}
class H700Wifi final : public WpaWifi {
  fs::path config, runtime;
  pid_t supplicant = -1, lease = -1;
  bool wanted = false;
  int readiness = -1;
  fs::path rfkill() {
    std::error_code error;
    const fs::path root = "/sys/class/rfkill";
    if (!fs::is_directory(root, error))
      return {};
    for (const auto &entry : fs::directory_iterator(root, error))
      if (readWifi(entry.path() / "type") == "wlan")
        return entry.path();
    return {};
  }
  void foreignOwner() {
    std::error_code error;
    for (const auto &entry : fs::directory_iterator("/proc", error)) {
      auto name = entry.path().filename().string();
      if (name.empty() ||
          !std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isdigit(c); }))
        continue;
      int pid = -1;
      try {
        pid = std::stoi(name);
      } catch (...) {
        continue;
      }
      if (pid == supplicant || pid == lease)
        continue;
      std::ifstream f(entry.path() / "cmdline", std::ios::binary);
      std::string text((std::istreambuf_iterator<char>(f)), {});
      auto args = split(text, '\0');
      if (args.empty())
        continue;
      auto program = fs::path(args[0]).filename().string();
      if (program != "wpa_supplicant" && program != "udhcpc")
        continue;
      if (std::find(args.begin(), args.end(), "wlan0") != args.end() ||
          std::find(args.begin(), args.end(), "-iwlan0") != args.end())
        throw WifiError("busy");
    }
  }
  bool radioReady() override {
    const bool interface = fs::is_directory("/sys/class/net/wlan0"), control = !rfkill().empty();
    const int bits = int(interface) + 2 * int(control);
    if (bits != readiness) {
      readiness = bits;
      std::cerr << "[wifi] t_ms=" << monotonicMs()
                << " op=ready interface_present=" << interface << " wlan_rfkill_present=" << control
                << "\n";
    }
    return interface && control;
  }
  bool radioEnabled() override {
    auto path = rfkill();
    return !path.empty() && readWifi(path / "soft") == "0" && readWifi(path / "hard") == "0";
  }
  void radio(bool enabled) override {
    foreignOwner();
    auto path = rfkill();
    if (path.empty())
      throw WifiError("unavailable");
    if (enabled && readWifi(path / "hard") != "0")
      throw WifiError("blocked");
    writeWifi(path / "soft", enabled ? "0" : "1");
    wanted = enabled;
    runWifi({"/sbin/ip", "link", "set", "wlan0", enabled ? "up" : "down"});
    if (radioEnabled() != enabled)
      throw WifiError("radio");
  }
  void start() override {
    if (command && running(supplicant))
      return;
    stopWifi(supplicant);
    foreignOwner();
    fs::create_directories(config.parent_path());
    fs::create_directories(runtime);
    chmod(config.parent_path().c_str(), 0700);
    chmod(runtime.c_str(), 0700);
    if (!fs::exists(config)) {
      int fd = open(config.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
      if (fd < 0)
        throw WifiError("save");
      std::string defaults = "update_config=1\ndisable_scan_offload=1\n";
      bool failed = write(fd, defaults.data(), defaults.size()) != ssize_t(defaults.size());
      if (fsync(fd))
        failed = true;
      if (close(fd))
        failed = true;
      if (failed)
        throw WifiError("save");
    }
    supplicant = spawnWifi(
        {"/usr/sbin/wpa_supplicant", "-i", "wlan0", "-c", config.string(), "-C", runtime.string()});
    for (int n = 0; n < 80; ++n) {
      if (cancelled) {
        stop();
        throw WifiError("cancelled");
      }
      if (!running(supplicant))
        throw WifiError("unavailable");
      if (fs::exists(runtime / "wlan0")) {
        try {
          attach(runtime / "wlan0", runtime);
          return;
        } catch (const WifiError &) {
          command.reset();
          monitor.reset();
        }
      }
      usleep(50000);
    }
    stop();
    throw WifiError("timeout");
  }
  void stop() override {
    std::exception_ptr failure;
    try {
      dhcp(false);
    } catch (...) {
      failure = std::current_exception();
    }
    stopWifi(supplicant);
    std::error_code error;
    fs::remove(runtime / "wlan0", error);
    if (failure)
      std::rethrow_exception(failure);
  }
  std::string ipAddress() override {
    ifaddrs *interfaces = nullptr;
    if (getifaddrs(&interfaces))
      return {};
    std::string address;
    for (auto *p = interfaces; p; p = p->ifa_next)
      if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET && std::string(p->ifa_name) == "wlan0") {
        char data[INET_ADDRSTRLEN];
        auto *a = reinterpret_cast<sockaddr_in *>(p->ifa_addr);
        if (inet_ntop(AF_INET, &a->sin_addr, data, sizeof(data)) && std::string(data) != "0.0.0.0")
          address = data;
      }
    freeifaddrs(interfaces);
    return address;
  }
  void dhcp(bool enabled) override {
    if (!enabled) {
      bool owned = lease > 0;
      stopWifi(lease);
      if (owned)
        runWifi({"/sbin/ip", "addr", "flush", "dev", "wlan0"});
      return;
    }
    if (running(lease))
      return;
    lease = spawnWifi({"/sbin/udhcpc", "-i", "wlan0", "-f", "-t", "5", "-T", "3", "-p",
                       (runtime / "dhcp.pid").string()});
  }

public:
  explicit H700Wifi(const fs::path &root) : config(root / "WiFi/wpa_supplicant.conf"), runtime() {
    char directory[] = "/tmp/rondo-wifi-XXXXXX";
    if (!mkdtemp(directory))
      throw WifiError("unavailable");
    runtime = directory;
    state.available = true;
  }
  WifiState status() override {
    if (command && !running(supplicant)) {
      command.reset();
      monitor.reset();
      pendingId = -1;
      pendingNew = false;
      connectDeadline = 0;
    }
    try {
      if (wanted && radioReady() && radioEnabled() && !command)
        start();
      return WpaWifi::status();
    } catch (...) {
      command.reset();
      monitor.reset();
      pendingId = -1;
      pendingNew = false;
      connectDeadline = 0;
      stop();
      throw;
    }
  }
  ~H700Wifi() override {
    try {
      stop();
    } catch (...) {
    }
    command.reset();
    monitor.reset();
    std::error_code error;
    fs::remove_all(runtime, error);
  }
};
} // namespace rondo
#endif
namespace rondo {
std::shared_ptr<WifiAdapter> wifiAdapter(const fs::path &state, bool enabled) {
#ifdef __linux__
  if (enabled) {
    std::ifstream f("/etc/baseos-release"), os("/etc/os-release");
    std::string line;
    bool target = false, baseos = false, wifi = false;
    while (std::getline(f, line)) {
      if (line == "BASEOS_TARGET=rg34xxsp")
        target = true;
      if (line == "BASEOS_WIFI=1")
        wifi = true;
    }
    while (std::getline(os, line))
      if (line == "ID=baseos")
        baseos = true;
    if (target && baseos && wifi) {
      try {
        return std::make_shared<H700Wifi>(state);
      } catch (...) {
        return std::make_shared<WifiAdapter>();
      }
    }
  }
#else
  (void)state;
  (void)enabled;
#endif
  return std::make_shared<WifiAdapter>();
}
} // namespace rondo
