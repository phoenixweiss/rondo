// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "platform.hpp"
namespace rondo {
// Read-only, allowlisted sysfs observations. Raw strings are never calibrated.
Json batterySnapshot(const fs::path &sys = "/sys", const fs::path &proc = "/proc");
// Runtime DT model/configuration; never reads PMIC BROM/registers.
Json batteryPassport(const fs::path &sys = "/sys", const fs::path &proc = "/proc");
Json batteryPowerMessages(const std::string &text);
void clearDiagnosticLogs(const fs::path &directory, const fs::path &fallback = {});
class BatteryLog {
  struct Worker;
  std::unique_ptr<Worker> worker;
  uint64_t enqueuePollAt = 0;
  bool sampleSync(uint64_t now, const Json &context, const std::string &event);
  fs::path directory, sys, proc, fallback;
  std::string role;
  uint64_t pollAt = 0, writtenAt = 0;
  Json lastKey, passport;
  size_t limit;
public:
  Json lastError = Json::object();
  BatteryLog(fs::path directory, std::string role, fs::path sys = "/sys",
             fs::path proc = "/proc", size_t limit = 2 * 1024 * 1024,
             fs::path fallback = {}, bool background = false);
  ~BatteryLog();
  // Only call while paused or shutting down, never from the frame/input loop.
  void drain();
  // Failure is diagnostic only: never interrupt a game or settings write.
  bool sample(uint64_t now, const Json &context, const std::string &event = "");
};
}
