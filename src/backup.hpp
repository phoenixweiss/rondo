// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <atomic>
#include <memory>
#include <stdexcept>
namespace rondo {
struct BackupContext { std::string version; std::set<std::string> cores; std::string expectedManifest{}; Json games=Json::array(); };
struct BackupProgress { std::atomic<size_t> done{0},total{0}; std::atomic<int> phase{0}; };
struct BackupError : std::runtime_error {
  std::string code;
  explicit BackupError(const std::string &key):std::runtime_error(key),code(key){}
};
bool backupDataPath(const std::string &);
fs::path backupSafePath(const fs::path &,const std::string &);
fs::path backupSafePath(const StatePaths &,const std::string &,bool writing=false);
// Returns true after rolling back an interrupted multi-file restore. Unknown or
// damaged journals fail closed before settings/core initialization can write.
bool recoverPendingRestore(const StatePaths &);
void applyBackupFiles(const StatePaths &,const fs::path &,const Json &,BackupProgress * = nullptr,const std::string &newGame = {});
Json backupManifest(const fs::path &,std::string *digest = nullptr);
Json backupGames(const fs::path &,const std::vector<Game> & = {});
void deleteBackup(const StatePaths &,const fs::path &,const std::string &expectedManifest = {});
std::vector<fs::path> backupFolders(const StatePaths &);
fs::path createBackup(const StatePaths &,const BackupContext &,BackupProgress * = nullptr);
void restoreBackup(const StatePaths &,const fs::path &,const BackupContext &,BackupProgress * = nullptr);
}
