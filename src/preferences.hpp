// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <array>
#include <optional>
#include <string_view>
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
struct GamepadSwatch {std::string_view id; uint32_t rgb;};
inline constexpr std::array<GamepadSwatch,9> GamepadSwatches{{
  {"blue",0x29adff},{"green",0x00e436},{"purple",0x83769c},{"yellow",0xffec27},
  {"orange",0xffa300},{"red",0xff004d},{"peach",0xffccaa},{"pink",0xff77a8},{"gray",0xc2c3c7}
}};
int gamepadColor(const Json &,const std::string &key);
int chooseGamepadColor(const std::string &key,const std::array<bool,GamepadSwatches.size()> &used);
inline constexpr int AutomaticGamepadPlayer=-1;
int gamepadPlayer(const Json &,const std::string &key);
std::string bluetoothGamepadKey(std::string address);
void validatePreferences(const Json &);
} // namespace rondo
