// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <atomic>
#include <memory>
namespace rondo {
struct WifiNetwork {
  std::string ssid, label, security;
  int signal = -100, savedId = -1;
  bool supported = false;
};
struct WifiState {
  bool available = false, ready = false, enabled = false;
  std::optional<int> signalDbm;
  std::string phase = "unavailable", error, connectedSsid, address;
  std::vector<WifiNetwork> networks;
};
struct WifiError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
std::string wifiDecode(const std::string &);
std::string wifiLabel(const std::string &);
std::vector<WifiNetwork> wifiNetworks(const std::string &, const std::string &);
bool wifiPasswordValid(const std::string &);
void clearSecret(std::string &);
// Commands never enter shell argv, diagnostics or the common preferences file.
class WpaSocket {
  int fd = -1;
  bool broken = false;
  fs::path local;

public:
  WpaSocket(const fs::path &server, const fs::path &directory);
  ~WpaSocket();
  WpaSocket(const WpaSocket &) = delete;
  WpaSocket &operator=(const WpaSocket &) = delete;
  std::string request(const std::string &);
  std::string event();
};
class WifiAdapter {
public:
  std::atomic<bool> cancelled{false};
  virtual ~WifiAdapter() = default;
  virtual bool supported() const { return false; }
  virtual WifiState status() { return {}; }
  virtual WifiState enable(bool) { throw WifiError("unavailable"); }
  virtual WifiState scan() { throw WifiError("unavailable"); }
  virtual WifiState connect(const WifiNetwork &, const std::string &) {
    throw WifiError("unavailable");
  }
  virtual WifiState disconnect() { throw WifiError("unavailable"); }
  virtual WifiState forget(const WifiNetwork &) { throw WifiError("unavailable"); }
};
// Protocol layer is shared by H700 and isolated socket tests.
class WpaWifi : public WifiAdapter {
protected:
  std::unique_ptr<WpaSocket> command, monitor;
  WifiState state;
  int pendingId = -1;
  bool pendingNew = false, scanning = false;
  uint64_t connectDeadline = 0, scanDeadline = 0;
  virtual bool radioReady() = 0;
  virtual bool radioEnabled() = 0;
  virtual void radio(bool) = 0;
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual std::string ipAddress() = 0;
  virtual void dhcp(bool) = 0;
  void attach(const fs::path &, const fs::path &);
  void ok(const std::string &);
  void abandon();

public:
  bool supported() const override { return true; }
  WifiState status() override;
  WifiState enable(bool) override;
  WifiState scan() override;
  WifiState connect(const WifiNetwork &, const std::string &) override;
  WifiState disconnect() override;
  WifiState forget(const WifiNetwork &) override;
};
std::shared_ptr<WifiAdapter> wifiAdapter(const fs::path &state, bool enabled);
} // namespace rondo
