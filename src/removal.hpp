// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <array>
namespace rondo {
enum class RemovalCategory { Rom, Media, Memory, Slots, Screenshots, Statistics, Library, Kept, Count };
struct RemovalEntry {
  RemovalCategory category=RemovalCategory::Kept;
  fs::path path;
  std::string location,reason;
  uintmax_t bytes=0;
  bool record=false;
  std::string identity{},parentIdentity{};
};
struct RemovalPreview {
  std::vector<RemovalEntry> entries;
  std::vector<std::string> warnings;
  std::string gameId,configIdentity;
  Json preferences,binding;
  std::map<fs::path,std::string> roots;
};
struct RemovalError : std::runtime_error {explicit RemovalError(const std::string &code):std::runtime_error(code){}};
using RemovalChoices=std::array<bool,size_t(RemovalCategory::Count)>;
struct RemovalResult {
  bool completed=false,recordsApplied=false;
  size_t total=0,removed=0;
  uintmax_t bytes=0;
  std::string error;
  fs::path failed,receipt;
  std::vector<fs::path> removedPaths;
};
RemovalPreview previewRemoval(const Game &,const std::vector<Game> &,const std::vector<Volume> &,
                              const StatePaths &,const Json &preferences);
RemovalResult executeRemoval(const Game &,const std::vector<Volume> &,const StatePaths &,
                             const RemovalPreview &,const RemovalChoices &,
                             std::function<void(size_t)> checkpoint={});
}
