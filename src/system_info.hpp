// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
namespace rondo {
// Read-only collector. Called only by an isolated helper, never the UI thread.
Json systemInfoSnapshot(const fs::path &root, const fs::path &app, const Json &cores);
class SystemInfoProcess {
  int pid=-1,fd=-1;
  uint64_t deadline=0;
  bool expired=false;
  std::string buffer;
public:
  Json result;
  std::string error;
  ~SystemInfoProcess();
  bool busy() const { return pid>0; }
  bool start(const std::vector<std::string> &,uint64_t now,int timeout=3000);
  bool poll(uint64_t now);
};
}
