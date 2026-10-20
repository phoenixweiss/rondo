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
  fs::path directory, sys, proc, fallback;
  std::string role;
  uint64_t pollAt = 0, writtenAt = 0;
  Json lastKey, passport;
  bool passportWritten = false;
  uint64_t passportAt = 0;
  size_t limit;
public:
  Json lastError = Json::object();
  BatteryLog(fs::path directory, std::string role, fs::path sys = "/sys",
             fs::path proc = "/proc", size_t limit = 2 * 1024 * 1024,
             fs::path fallback = {});
  // Failure is diagnostic only: never interrupt a game or settings write.
  bool sample(uint64_t now, const Json &context, const std::string &event = "");
};
}
