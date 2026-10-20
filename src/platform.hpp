// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <optional>
#include <memory>
namespace rondo {
struct Telemetry {
  std::optional<int> capacity;
  std::optional<long> voltageMicrovolts;
  std::string batteryStatus;
  bool wifiEnabled = false;
  std::string wifiState;
  std::optional<int> wifiSignalDbm;
};
int brightnessRawLevel(int percent);
int brightnessPercent(int raw);
int wifiSignalLevel(std::optional<int> dbm);
Telemetry readTelemetry(const fs::path &sys = "/sys", const fs::path &proc = "/proc");
bool bootLogoSupported();
void applyBootLogo(bool enabled);
std::optional<bool> readBootLogo();
} // namespace rondo
namespace rondo {
struct H700InputState {
  std::set<std::string> down;
  std::map<std::string, uint64_t> repeats;
  std::vector<std::string> event(unsigned type, unsigned code, int value, uint64_t now);
  std::vector<std::string> repeat(uint64_t now);
};
class H700Keys {
  struct Device { int fd; H700InputState state; };
  std::vector<Device> devices;
public:
  H700Keys();
  ~H700Keys();
  bool available() const { return !devices.empty(); }
  bool pressed(const std::string &) const;
  std::vector<std::string> poll();
};
class VolumeKeys {
  std::vector<int> devices;

public:
  VolumeKeys();
  ~VolumeKeys();
  bool available() const { return !devices.empty(); }
  std::vector<std::string> poll();
};
std::string volumeKeyAction(unsigned type, unsigned code, int value);
std::vector<int16_t> mixVolume(const int16_t *stereo, size_t frames, int percent);
} // namespace rondo
namespace rondo {
uint64_t monotonicMs();
struct VolumeFeedback {
  std::string phase = "hidden";
  std::optional<int> confirmed;
  uint64_t sequence = 0, deadline = 0;
  uint64_t begin(uint64_t now);
  void result(uint64_t operation, std::optional<int> value, bool ok, uint64_t now);
  void timeout(uint64_t now);
  bool visible(uint64_t now) const;
  Json json() const;
  void restore(const Json &);
};
} // namespace rondo
namespace rondo {
// Only the active shell/runner owns power input and device policy.
struct PowerInputState {
  bool held = false;
  std::string event(unsigned type, unsigned code, int value);
};
class PowerKeys {
  struct Device { int fd; PowerInputState state; };
  std::vector<Device> devices;
public:
  PowerKeys();
  ~PowerKeys();
  std::vector<std::string> poll();
};
class DeviceControl {
public:
  virtual ~DeviceControl() = default;
  virtual std::optional<int> brightness() { return std::nullopt; }
  virtual void brightness(int) { throw std::runtime_error("Display adapter unavailable"); }
  virtual bool canBlank() const { return false; }
  virtual void blank(bool) { throw std::runtime_error("Screen blanking unavailable"); }
  virtual bool canSleep() const { return false; }
  virtual bool canWakeOnLid() const { return false; }
  virtual std::optional<bool> lidOpen() { return std::nullopt; }
  virtual void sleep(bool = false, bool = true) { throw std::runtime_error("Sleep adapter unavailable"); }
};
std::unique_ptr<DeviceControl> deviceControl(bool enabled);
}
