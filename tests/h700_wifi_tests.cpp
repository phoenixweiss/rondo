// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
// Run only inside the synthetic BaseOS chroot. Never operate a real radio.
#include "platform.hpp"
#include "wifi.hpp"
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace rondo;
static std::string executable;
static int inherited = -1;
static bool rejectStart = false;
extern char **environ;
extern "C" int __real_posix_spawn(pid_t *, const char *, const posix_spawn_file_actions_t *,
                                  const posix_spawnattr_t *, char *const[], char *const[]);
extern "C" int __wrap_posix_spawn(pid_t *pid, const char *path,
                                  const posix_spawn_file_actions_t *files,
                                  const posix_spawnattr_t *attributes, char *const argv[],
                                  char *const env[]) {
  assert(std::string(path) == "/sbin/ip" || std::string(path) == "/usr/sbin/wpa_supplicant");
  std::vector<std::string> args = {executable,
                                   std::string(path) == "/sbin/ip" ? "--fake-ip"
                                   : rejectStart                   ? "--reject-start"
                                                                   : "--fake-wpa",
                                   std::to_string(inherited)};
  for (int i = 1; argv[i]; ++i)
    args.emplace_back(argv[i]);
  std::vector<char *> values;
  for (auto &s : args)
    values.push_back(s.data());
  values.push_back(nullptr);
  return __real_posix_spawn(pid, executable.c_str(), files, attributes, values.data(), env);
}
static std::string get(const char *path) {
  std::ifstream f(path);
  std::string s;
  std::getline(f, s);
  return s;
}
int main(int argc, char **argv) {
  if (argc > 1) {
    if (std::string(argv[1]) == "--foreign") {
      for (;;)
        pause();
    }
    assert(argc > 2 && fcntl(std::stoi(argv[2]), F_GETFD) == -1 && errno == EBADF);
    if (std::string(argv[1]) == "--fake-ip")
      return 0;
    if (std::string(argv[1]) == "--reject-start")
      return 1;
    fs::path runtime;
    for (int i = 3; i + 1 < argc; ++i)
      if (std::string(argv[i]) == "-C")
        runtime = argv[i + 1];
    assert(!runtime.empty());
    std::ofstream("/tmp/rondo-test-wpa.pid") << getpid();
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    assert(fd >= 0);
    sockaddr_un own{};
    own.sun_family = AF_UNIX;
    strcpy(own.sun_path, (runtime / "wlan0").c_str());
    assert(!bind(fd, reinterpret_cast<sockaddr *>(&own), sizeof(own)));
    std::vector<sockaddr_un> listeners;
    for (;;) {
      char data[2048];
      sockaddr_un peer{};
      socklen_t length = sizeof(peer);
      auto n = recvfrom(fd, data, sizeof(data), 0, reinterpret_cast<sockaddr *>(&peer), &length);
      if (n <= 0)
        continue;
      std::string request(data, size_t(n)), reply = "OK\n";
      if (request == "PING")
        reply = "PONG\n";
      else if (request == "ATTACH")
        listeners.push_back(peer);
      else if (request == "STATUS")
        reply = "wpa_state=DISCONNECTED\n";
      else if (request == "SCAN_RESULTS")
        reply = "bssid / frequency / signal level / flags / "
                "ssid\n00:00:00:00:00:01\t2412\t-50\t[WPA2-PSK-CCMP][ESS]\tFixture WLAN\n";
      else if (request == "LIST_NETWORKS")
        reply = "network id / ssid / bssid / flags\n";
      sendto(fd, reply.data(), reply.size(), 0, reinterpret_cast<sockaddr *>(&peer), length);
      if (request == "SCAN") {
        std::string event = "<3>CTRL-EVENT-SCAN-RESULTS\n";
        for (auto &listener : listeners)
          sendto(fd, event.data(), event.size(), 0, reinterpret_cast<sockaddr *>(&listener),
                 sizeof(listener));
      }
    }
  }
  assert(get("/tmp/rondo-synthetic-hardware") == "test-only");
  executable = fs::absolute(argv[0]).string();
  auto wifi = wifiAdapter("/tmp/h700-wifi-state", true);
  assert(wifi->supported());
  inherited = open("/tmp/rondo-audio-fd-fixture", O_CREAT | O_RDWR, 0600);
  assert(inherited > 2);
  fs::create_directories("/proc/self/fd");
  std::ofstream("/proc/self/fd/" + std::to_string(inherited)) << "fixture";
  assert(!wifi->enable(false).enabled);
  auto state = wifi->enable(true);
  assert(state.enabled && state.phase == "disconnected");
  assert(get("/sys/class/rfkill/rfkill0/soft") == "0");
  auto pid = std::stoi(get("/tmp/rondo-test-wpa.pid"));
  assert(kill(pid, 0) == 0);
  wifi->scan();
  const auto scanDeadline = monotonicMs() + 3000;
  do {
    state = wifi->status();
    if (state.phase != "scanning")
      break;
    usleep(20000);
  } while (monotonicMs() < scanDeadline);
  assert(state.networks.size() == 1 && state.networks[0].label == "Fixture WLAN");
  state = wifi->enable(false);
  assert(!state.enabled && get("/sys/class/rfkill/rfkill0/soft") == "1");
  assert(kill(pid, 0) == -1 && errno == ESRCH);
  assert(fcntl(inherited, F_GETFD) >= 0);
  rejectStart = true;
  bool failed = false;
  try {
    wifi->enable(true);
  } catch (const WifiError &) {
    failed = true;
  }
  assert(failed && get("/sys/class/rfkill/rfkill0/soft") == "1");
  rejectStart = false;
  // A separate Wi-Fi owner must survive refusal; no killall or global service stop.
  pid_t foreign = -1;
  char program[] = "wpa_supplicant", mode[] = "--foreign", interfaceFlag[] = "-i",
       interface[] = "wlan0";
  char *args[] = {program, mode, interfaceFlag, interface, nullptr};
  assert(__real_posix_spawn(&foreign, executable.c_str(), nullptr, nullptr, args, environ) == 0);
  fs::create_directories("/proc/" + std::to_string(foreign));
  {
    std::ofstream process("/proc/" + std::to_string(foreign) + "/cmdline", std::ios::binary);
    const char text[] = "wpa_supplicant\0-i\0wlan0\0";
    process.write(text, sizeof(text) - 1);
  }
  usleep(100000);
  failed = false;
  try {
    wifi->enable(true);
  } catch (const WifiError &e) {
    failed = std::string(e.what()) == "busy";
  }
  assert(failed && kill(foreign, 0) == 0 && get("/sys/class/rfkill/rfkill0/soft") == "1");
  kill(foreign, SIGTERM);
  assert(waitpid(foreign, nullptr, 0) == foreign);
  close(inherited);
  std::cout << "PASS: H700 synthetic rfkill readback, private WPA child/socket, descriptor "
               "isolation, scan, owned-process stop, rejected-start rollback and foreign-owner "
               "preservation. No hardware accessed.\n";
}
