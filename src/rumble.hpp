// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
namespace rondo {
// Binary H700 motor. Only the worker touches the driver; a heartbeat watchdog
// stops it if emulation stops making progress. Strong/weak channels are combined.
class Rumble {
  using Clock = std::chrono::steady_clock;
  std::function<bool(bool)> write;
  mutable std::mutex mutex;
  std::condition_variable changed;
  Clock::time_point deadline{};
  bool requested=false, quitting=false, healthy=true, forceOff=false;
  uint64_t sequence=0, completed=0;
  std::thread worker;
  void loop();
public:
  explicit Rumble(std::function<bool(bool)> writer);
  ~Rumble();
  bool available() const;
  void update(uint16_t strong, uint16_t weak);
  void stop();
};
std::shared_ptr<Rumble> h700Rumble(bool enabled);
}
