// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
namespace rondo {
std::vector<fs::path> psxDiscs(const fs::path &);
std::string psxIdentity(const std::vector<fs::path> &);
bool psxBiosFilename(const std::string &);
std::vector<fs::path> psxBiosFiles(const fs::path &);
fs::path psxPrepareBios(const fs::path &,const fs::path &);
std::string psxBiosIdentity(const fs::path &);
std::string psxContentHash(const fs::path &,const fs::path &,bool forceHle=false);
bool psxBiosAvailable(const fs::path &);
}
