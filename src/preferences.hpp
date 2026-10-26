// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <array>
#include <optional>
namespace rondo {
std::string themeId(std::string);
std::string coreId(const std::string &system);
struct CoreEntry {
  std::string id, name, systems;
  fs::path path;
  bool experimental=false;
  mutable std::optional<bool> present;
  bool available() const;
  bool enabled(const Json &) const;
};
std::vector<std::pair<int,std::string>> coreControls(const std::string &);
std::string physicalButton(int);
int mappedSource(const Json &, const std::string &,int);
uint16_t mappedButtons(const Json &,const std::string &,uint16_t);
void validatePreferences(const Json &);
} // namespace rondo
