// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <future>
namespace rondo {
uint16_t screenshotChord(const Json &);
std::string screenshotTimestamp(const Timezones &,const std::string &,time_t);
inline bool validScreenshotChord(int mask) {
  if(mask<1 || mask>65535)return false;
  int count=0;for(int bit=0;bit<16;++bit)count+=(mask>>bit)&1;
  // D-pad and Start+Select are reserved for navigation/the game menu.
  return count>=2 && count<=4 && !(mask&0xf0) && (mask&12)!=12;
}

// Reserve only the first 70 ms for a chord. Ordinary single presses still work.
class ScreenshotChord {
  uint16_t candidate=0, swallowed=0;
  uint64_t since=0;
  bool passed=false;
public:
  void reset();
  uint16_t sample(uint16_t held,uint16_t chord,uint64_t now,bool &trigger);
};
class ScreenshotWriter {
  std::future<fs::path> job;
public:
  bool busy() const {return job.valid();}
  bool submit(const fs::path &,const Game &,const std::vector<uint32_t> &,int,int,const std::string &timestamp="",double displayAspect=0);
  bool submitInterface(const fs::path &,const std::vector<uint32_t> &,int,int,const std::string &timestamp="");
  // Empty until finished; errors propagate only when the UI collects the result.
  std::optional<fs::path> poll();
};
} // namespace rondo
