// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <atomic>
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
using namespace rondo;
struct Server {
  int fd = -1, selected = -1, sequence = 0, saves = 0;
  fs::path path;
  std::thread thread;
  std::atomic<bool> quit{false}, drop{false}, rejectSave{false}, complete{false}, address{false};
  std::atomic<int> delay{0}, rssi{-45};
  std::mutex mutex;
  std::map<int, std::string> profiles;
  std::vector<sockaddr_un> monitors;
  std::string scanRows = "bssid / frequency / signal level / flags / "
                         "ssid\n00:00:00:00:00:01\t2412\t-75\t[WPA2-PSK-CCMP][ESS]\tTest "
                         "Network\n00:00:00:00:00:02\t2412\t-45\t[WPA2-PSK-CCMP][ESS]\tTest "
                         "Network\n00:00:00:00:00:03\t2412\t-60\t[ESS]\tОткрытая\n00:00:00:00:00:"
                         "04\t2412\t-80\t[WPA2-EAP-CCMP][ESS]\tOffice\n";
  explicit Server(fs::path root) : path(root / "wpa") {
    fs::create_directories(root);
    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    assert(fd >= 0);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    sockaddr_un own{};
    own.sun_family = AF_UNIX;
    assert(path.string().size() < sizeof(own.sun_path));
    strcpy(own.sun_path, path.c_str());
    assert(!bind(fd, reinterpret_cast<sockaddr *>(&own), sizeof(own)));
    thread = std::thread([this] {
      while (!quit) {
        pollfd p{fd, POLLIN, 0};
        if (poll(&p, 1, 20) <= 0)
          continue;
        char b[4096];
        sockaddr_un peer{};
        socklen_t length = sizeof(peer);
        auto n = recvfrom(fd, b, sizeof(b), 0, reinterpret_cast<sockaddr *>(&peer), &length);
        if (n <= 0)
          continue;
        if (drop.exchange(false))
          continue;
        if (auto milliseconds = delay.exchange(0))
          std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        std::string request(b, size_t(n)), reply = "OK\n";
        std::lock_guard<std::mutex> lock(mutex);
        if (request == "PING")
          reply = "PONG\n";
        else if (request == "ATTACH")
          monitors.push_back(peer);
        else if (request == "STATUS")
          reply = std::string("wpa_state=") + (complete ? "COMPLETED" : "DISCONNECTED") +
                  "\nid=" + std::to_string(selected) + "\nssid=Test Network\n";
        else if (request.find("GET_NETWORK ") == 0)
          reply = "WPA-PSK\n";
        else if (request == "SIGNAL_POLL")
          reply = "RSSI=" + std::to_string(rssi.load()) + "\n";
        else if (request == "SCAN_RESULTS")
          reply = scanRows;
        else if (request == "LIST_NETWORKS") {
          reply = "network id / ssid / bssid / flags\n";
          for (const auto &p : profiles)
            reply += std::to_string(p.first) + "\t" + p.second + "\tany\t\n";
        } else if (request == "ADD_NETWORK") {
          int id = sequence++;
          profiles[id] = "Test Network";
          reply = std::to_string(id) + "\n";
        } else if (request.find("SELECT_NETWORK ") == 0) {
          selected = std::stoi(request.substr(15));
          complete = false;
          address = false;
        } else if (request.find("REMOVE_NETWORK ") == 0) {
          profiles.erase(std::stoi(request.substr(15)));
        } else if (request == "SAVE_CONFIG") {
          if (rejectSave)
            reply = "FAIL\n";
          else
            ++saves;
        } else if (request == "DISCONNECT") {
          complete = false;
          address = false;
        }
        sendto(fd, reply.data(), reply.size(), 0, reinterpret_cast<sockaddr *>(&peer), length);
        if (request == "SCAN")
          sendEvent("<3>CTRL-EVENT-SCAN-RESULTS\n");
      }
    });
  }
  void sendEvent(const std::string &text) {
    for (auto &m : monitors)
      sendto(fd, text.data(), text.size(), 0, reinterpret_cast<sockaddr *>(&m), sizeof(m));
  }
  void wrongPassword() {
    std::lock_guard<std::mutex> lock(mutex);
    sendEvent("<3>CTRL-EVENT-SSID-TEMP-DISABLED id=" + std::to_string(selected) +
              " reason=WRONG_KEY\n");
  }
  int savedCount() {
    std::lock_guard<std::mutex> lock(mutex);
    return saves;
  }
  int profileCount() {
    std::lock_guard<std::mutex> lock(mutex);
    return profiles.size();
  }
  ~Server() {
    quit = true;
    thread.join();
    close(fd);
    unlink(path.c_str());
  }
};
struct Adapter : WpaWifi {
  Server &server;
  fs::path directory;
  std::atomic<bool> ready{true};
  bool on = false, rejectRadio = false, dhcpStarted = false;
  Adapter(Server &s, fs::path d) : server(s), directory(std::move(d)) {}
  bool radioReady() override { return ready; }
  bool radioEnabled() override { return on; }
  void radio(bool enabled) override {
    if (rejectRadio) {
      rejectRadio = false;
      throw WifiError("radio");
    }
    on = enabled;
  }
  void start() override {
    if (!command)
      attach(server.path, directory);
  }
  void stop() override { dhcp(false); }
  std::string ipAddress() override { return server.address ? "192.0.2.2" : ""; }
  void dhcp(bool enabled) override { dhcpStarted = enabled; }
  void expire() { connectDeadline = monotonicMs() - 1; }
};
static void wait(App &app, const std::function<bool()> &done) {
  auto deadline = monotonicMs() + 5000;
  while (monotonicMs() < deadline) {
    app.wifiTick();
    if (done() && !app.wifiJob.valid())
      return;
    usleep(2000);
  }
  assert(false && "Wi-Fi test timed out");
}
int main(int argc, char **argv) {
  assert(argc == 3);
  auto root = fs::absolute(argv[2]);
  fs::create_directories(root);
  assert(wifiDecode("a\\x20b\\\\c\\\"d") == "a b\\c\"d");
  assert(wifiLabel(std::string("bad\xff", 4)) == "bad�");
  assert(wifiLabel("line\nname") == "line name");
  assert(!wifiPasswordValid("short"));
  assert(wifiPasswordValid("synthetic-pass-123"));
  assert(!wifiPasswordValid(std::string(64, 'z')));
  assert(wifiPasswordValid(std::string(64, 'a')));
  Server server(root / "socket");
  auto adapter = std::make_shared<Adapter>(server, root / "socket");
  adapter->ready = false;
  auto startup = std::async(std::launch::async, [adapter] { return adapter->enable(true); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  adapter->cancelled = true;
  bool cancelled = false;
  try {
    (void)startup.get();
  } catch (const WifiError &e) {
    cancelled = std::string(e.what()) == "cancelled";
  }
  assert(cancelled && !adapter->on);
  adapter->cancelled = false;
  startup = std::async(std::launch::async, [adapter] { return adapter->enable(true); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  adapter->ready = true;
  assert(startup.get().enabled);
  auto state = adapter->enable(true);
  assert(state.enabled && state.phase == "disconnected");
  adapter->scan();
  const auto scanDeadline = monotonicMs() + 3000;
  do {
    state = adapter->status();
    if (state.phase != "scanning")
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  } while (monotonicMs() < scanDeadline);
  assert(state.networks.size() == 3 && state.networks[0].signal == -45 &&
         state.networks[0].label == "Test Network");
  assert(!state.networks[2].supported);
  auto network = state.networks[0];
  adapter->connect(network, "synthetic-pass-123");
  assert(server.savedCount() == 0);
  server.wrongPassword();
  {
    std::lock_guard<std::mutex> lock(server.mutex);
    server.sendEvent("<3>CTRL-EVENT-SSID-TEMP-DISABLED id=999 reason=WRONG_KEY\n");
  }
  state = adapter->status();
  assert(state.error == "password" && server.profileCount() == 0);
  adapter->connect(network, "synthetic-pass-123");
  server.complete = true;
  state = adapter->status();
  assert(state.phase == "address" && adapter->dhcpStarted && server.savedCount() == 0);
  server.address = true;
  state = adapter->status();
  assert(state.phase == "connected" && state.address == "192.0.2.2" && server.savedCount() == 1 &&
         state.networks[0].savedId >= 0);
  assert(state.signalDbm == -45);
  server.rssi = -75;
  assert(adapter->status().signalDbm == -75);
  server.rssi = 0;
  assert(!adapter->status().signalDbm);
  server.rssi = -45;
  auto saved = state.networks[0];
  adapter->disconnect();
  state = adapter->connect(saved, "");
  assert(state.phase == "connecting");
  adapter->expire();
  state = adapter->status();
  assert(state.error == "connect_timeout" && server.profileCount() == 1);
  adapter->connect(network, "synthetic-pass-123");
  server.complete = true;
  server.address = true;
  server.rejectSave = true;
  state = adapter->status();
  assert(state.phase == "connected" && state.error == "save");
  server.rejectSave = false;
  adapter->forget(saved);
  assert(server.savedCount() == 2);
  adapter->enable(false);
  assert(!adapter->status().enabled);
  adapter->rejectRadio = true;
  bool failed = false;
  try {
    adapter->enable(true);
  } catch (const WifiError &) {
    failed = true;
  }
  assert(failed && !adapter->status().enabled);
  {
    WpaSocket socket(server.path, root / "socket");
    server.drop = true;
    failed = false;
    try {
      socket.request("PING");
    } catch (const WifiError &e) {
      failed = std::string(e.what()) == "timeout";
    }
    assert(failed);
    failed = false;
    try {
      socket.request("PING");
    } catch (const WifiError &) {
      failed = true;
    }
    assert(failed);
  }
  {
    std::ofstream persisted(root / "config-old.json");
    persisted << "{}";
  }
  Options opts;
  opts.assets = fs::absolute(argv[1]);
  opts.state = root / "ui-state";
  opts.output = root / "renders";
  opts.headless = true;
  App app(opts);
  assert(!app.wifi.available && !app.store.data["wifi_enabled"].get<bool>());
  app.action("MENU");if(app.screen==Screen::LibraryMenu)app.action("A");
  for (int i = 0; i < 4; ++i)
    app.action("DOWN");
  app.action("A");app.action("A");
  assert(app.screen == Screen::Wifi);
  app.capture("unavailable");
  app.wifiDevice = adapter;
  app.wifi = adapter->status();
  app.wifiPollAt = monotonicMs() + 10000;
  app.row = 0;
  app.action("A");
  wait(app, [&] { return app.wifi.enabled; });
  assert(app.store.data["wifi_enabled"] == true);
  app.row = 1;
  app.action("A");
  assert(app.screen == Screen::WifiNetworks);
  wait(app, [&] { return app.wifi.phase == "scanning"; });
  app.action("A");
  assert(app.screen == Screen::WifiNetworks);
  wait(app, [&] { return !app.wifi.networks.empty() && app.wifi.phase != "scanning"; });
  for (auto language : {"en", "ru"}) {
    app.locale.select(opts.assets, language);
    app.ui->inputHints(false);
    app.capture(std::string("list-") + language);
  }
  app.action("B");
  assert(app.screen == Screen::Wifi && app.row == 1);
  auto current = app.wifi;
  app.wifi.connectedSsid = "Test Network";
  app.wifi.address = "192.0.2.2";
  app.wifi.signalDbm = -62;
  app.wifi.phase = "connected";
  for (auto language : {"en", "ru"}) {
    app.locale.select(opts.assets, language);
    app.capture(std::string("main-connected-") + language);
    assert(app.ui->wifiOverride && app.ui->wifiOverride->wifiEnabled &&
           app.ui->wifiOverride->wifiSignalDbm == -62);
  }
  app.wifi = current;
  // All printable ASCII characters are reachable with the handheld keyboard.
  std::string reachable = " ";
  for (bool symbols : {false, true})
    for (bool upper : {false, true}) {
      app.wifiSymbols = symbols;
      app.wifiUpper = upper;
      for (auto &r : app.wifiKeys())
        for (auto &k : r)
          if (k.size() == 1)
            reachable += k;
    }
  for (char c = 32; c < 127; ++c)
    assert(reachable.find(c) != reachable.npos);
  // A distinct unsaved protected network exercises password entry and cancellation.
  app.wifiSelected = network;
  app.screen = Screen::WifiNetwork;
  app.row = 0;
  app.action("A");
  assert(app.screen == Screen::WifiPassword);
  app.wifiText("synthetic-pass-123");
  assert(app.wifiPassword == "synthetic-pass-123");
  for (auto language : {"en", "ru"}) {
    app.locale.select(opts.assets, language);
    app.ui->inputHints(false);
    app.capture(std::string("password-") + language);
  }
  app.ui->inputHints(true);
  app.capture("password-keyboard");
  assert(app.trace.dump().find("synthetic-pass-123") == std::string::npos);
  app.action("B");
  assert(app.wifiPassword.empty());
  app.action("A");
  app.wifiText("bad");
  app.action("WIFI_SUBMIT");
  assert(app.screen == Screen::WifiPassword && app.wifi.error == "password_format");
  app.action("B");
  app.screen = Screen::Wifi;
  app.row = 0;
  app.requestSettings = [](Json) { return false; };
  app.action("LEFT");
  wait(app, [&] { return app.wifi.enabled && app.wifi.error == "save"; });
  assert(app.store.data["wifi_enabled"] == true);
  app.requestSettings = {};
  for (auto language : {"en", "ru"}) {
    app.locale.select(opts.assets, language);
    app.wifi.error.clear();
    app.capture(std::string("networks-") + language);
    app.action("X");
    app.capture(std::string("info-") + language);
    app.action("B");
  }
  server.delay = 150;
  app.wifiRun("poll", [adapter] { return adapter->status(); });
  app.action("LEFT");
  assert(app.store.data["wifi_enabled"] == true);
  wait(app, [&] { return !app.wifi.enabled; });
  assert(!Store(opts.state).data["wifi_enabled"].get<bool>());
  app.capture("main-off");
  assert(app.ui->wifiOverride && !app.ui->wifiOverride->wifiEnabled);
  std::cout
      << "PASS: isolated WPA socket, UTF-8/escaped SSIDs, deduplication, unsupported security, IP "
         "acknowledgement, wrong key, timeout, save refusal, radio rollback, reconnect, forget, "
         "secret-free trace, all ASCII keyboard characters and EN/RU UI. No host radio accessed.\n";
}
