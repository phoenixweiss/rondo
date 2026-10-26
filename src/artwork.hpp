// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
namespace rondo {
struct ArtworkRequest {
  fs::path path;
  int width=204,height=204;
  bool smooth=true;
  std::string key() const;
};
struct ArtworkImage {
  std::string identity;
  int width=0,height=0;
  std::vector<uint8_t> rgba;
};
class ArtworkCache {
  struct Record {std::shared_ptr<const ArtworkImage> image;uint64_t checked=0,used=0;};
  struct DiskRecord {uintmax_t size=0;uint64_t used=0;};
  fs::path directory;
  size_t memoryBudget,diskBudget;
  mutable std::mutex mutex;
  std::condition_variable changed;
  std::map<std::string,Record> records;
  std::map<std::string,DiskRecord> disk;
  std::map<std::string,std::string> failedSources;
  std::set<std::string> wanted;
  std::deque<ArtworkRequest> queue;
  std::string running;
  bool quitting=false,writable=false;
  uint64_t clock=0,diskClock=0;
  size_t resident=0;
  uint64_t decodes=0,hits=0,cancelled=0;
  std::thread worker;
  void loop();
  void openDisk();
  void trimDisk(size_t extra=0);
  void touchDisk(const std::string &,size_t);
  std::shared_ptr<ArtworkImage> load(const ArtworkRequest &);
  bool stillWanted(const std::string &);
public:
  explicit ArtworkCache(fs::path directory,size_t memoryBytes=4*1024*1024,size_t diskBytes=32*1024*1024);
  ~ArtworkCache();
  // Current image first, followed by at most four neighbours. Queued stale jobs
  // are discarded; an in-flight decode never publishes to a different selection.
  void select(const std::vector<ArtworkRequest> &);
  std::shared_ptr<const ArtworkImage> get(const ArtworkRequest &);
  bool missing(const ArtworkRequest &) const;
  Json statistics() const;
};
}
