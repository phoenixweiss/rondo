// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <future>
namespace rondo {
// One writer for the active session; library views only read bounded snapshots.
class GameStatistics {
  fs::path path;
  Json data;
  uint64_t last=0,nextCheckpoint=0,generation=1,persisted=0,writing=0;
  bool active=false;
  std::future<std::string> job;
  void poll();
  void submit();
public:
  std::string error;
  size_t writesStarted=0;
  GameStatistics(const fs::path &,const std::string &,uint64_t now);
  ~GameStatistics();
  void tick(uint64_t now,bool running);
  bool flush(uint64_t now,bool running=false);
  const Json &snapshot() const {return data;}
  static Json read(const fs::path &,const std::string &);
};
}
