// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <memory>
namespace rondo {
struct ClockState {
  bool available = false, enabled = false, synchronized = false;
  long long syncUtc = 0;
  long long manualUtc = 0;
  bool rtcSaved = false;
};
struct ClockError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
class ClockAdapter {
public:
  virtual ~ClockAdapter() = default;
  virtual ClockState status() { return {}; }
  virtual ClockState enable(bool) { throw ClockError("unavailable"); }
  virtual ClockState restart() { throw ClockError("unavailable"); }
  virtual ClockState setTime(long long) { throw ClockError("unavailable"); }
  virtual bool supported() const { return false; }
};
std::shared_ptr<ClockAdapter> clockAdapter(const fs::path &notify, bool enabled);
bool clockBaseosPlatformSupported(const std::string &release, const std::string &os);
struct ClockFields {
  int year = 2026, month = 1, day = 1, hour = 0, minute = 0;
};
int clockDays(int year, int month);
// Main/UI thread only: uses the currently selected TZ, just like slotDate.
long long clockUtc(const ClockFields &);
} // namespace rondo
