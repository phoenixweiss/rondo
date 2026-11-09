// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "backup.hpp"
namespace rondo {
std::string progressFingerprint(const Game &,const std::string &loadedRomHash={});
Json progressRecord(const Game &,const std::vector<Volume> &,const std::string &fingerprint);
Json validateProgressRecord(const Json &,const std::string &gameId);
void rememberProgress(const StatePaths &,const Game &,const std::vector<Volume> &,const std::string &fingerprint);
bool hasProgress(const StatePaths &,const std::string &gameId);
bool hasMissingProgress(const StatePaths &,const std::vector<Game> &);
Json findProgress(const StatePaths &,const Game &,const std::vector<Game> &,const std::string &fingerprint,const std::string &legacyRom,const std::string &legacyCore);
void relocateProgress(const StatePaths &,const Json &candidate,const Game &,const std::vector<Volume> &,const std::vector<Game> &,const std::string &fingerprint,const std::string &legacyRom,const std::string &legacyCore,BackupProgress * = nullptr);
}
