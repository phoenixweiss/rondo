// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
namespace rondo {
// Metadata reads never deserialize, allocate or hash the core payload.
enum class StateRead { Metadata, Preview, Full };
struct StateFile {
  Json meta = Json::object();
  std::vector<uint8_t> payload;
  std::vector<uint32_t> pixels;
};
std::string stateName(int slot); // 0..2 manual, 3 automatic
StateFile readState(const fs::path &, StateRead = StateRead::Full);
std::vector<uint8_t> encodeState(const StateFile &);
void writeState(const fs::path &, const StateFile &);
}
