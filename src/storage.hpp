// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <functional>
#include <memory>
namespace rondo {
struct StorageError : std::runtime_error {
  std::string code,slot;
  StorageError(std::string c,std::string s={}):std::runtime_error(c),code(std::move(c)),slot(std::move(s)){}
};
// An empty ID describes a mounted card whose identity could not be verified.
struct StorageCard {std::string slot,id;fs::path root;bool readOnly=false;};
// A fresh probe must identify physical cards, not volume labels. No mounting,
// repair or provisioning is performed by the storage policy itself.
using StorageProbe=std::function<std::vector<StorageCard>()>;
struct CardMount {std::string slot,device,filesystem;fs::path root;bool readOnly;};
std::vector<CardMount> storageMounts(const std::string &procMounts);
// Read-only probe of the MMC CID and the mounted filesystem's volume serial.
std::vector<StorageCard> physicalStorageCards(const fs::path &proc="/proc",
                                             const fs::path &sys="/sys",
                                             const fs::path &dev="/dev");
struct Tf2FormatTarget {
  std::string id;
  fs::path root;
  uintmax_t bytes=0,device=0;
  bool operator==(const Tf2FormatTarget &other) const {
    return id==other.id && root==other.root && bytes==other.bytes && device==other.device;
  }
};
Tf2FormatTarget physicalTf2FormatTarget();
void formatPhysicalTf2(const Tf2FormatTarget &,const fs::path &tf1,
                       const std::function<void(int)> &stage);
void setFat32VolumeSerial(int fd,uint32_t serial);
std::string storageBytes(uintmax_t,const std::string &locale);
const std::map<std::string,std::string> &storageCategories();
struct StorageTransferHooks {
  std::function<bool()> cancelled;
  std::function<void(const fs::path &,const fs::path &)> copy;
  std::function<uintmax_t(const fs::path &)> available;
  std::function<void(const std::string &)> checkpoint;
  std::function<void(uintmax_t,uintmax_t)> progress;
};
class StorageLayout {
  fs::path config;
  StorageProbe probe;
  Json document;
  StorageCard card(const Json &,bool) const;
  void validateResolved(const Json &) const;
public:
  StorageLayout(fs::path configRoot,StorageProbe);
  const Json &profile() const {return document;}
  fs::path configPath(bool writing=false) const;
  fs::path path(const std::string &category,bool writing=false) const;
  Json destination(const std::string &slot) const;
  Json pending() const;
  void formatTf2(const Tf2FormatTarget &,const std::function<void(const fs::path &)> &) const;
  std::shared_ptr<StorageLayout> reopen() const;
  Json cards() const;
  Json usage(const std::function<bool()> &cancelled={}) const;
  Json review(const std::string &,const std::string &,const std::string &sourceSlot={}) const;
  void resolveCategory(const Json &review,const Json &choices,bool move,bool quiesced,const StorageTransferHooks & = {});
  Json resolvedStatus() const;
  void finishResolved();
  void discardResolved();
  Json pendingStatus() const;
  void discardPending();
  // Finish a fully published copy or acknowledge an already committed profile.
  // An incomplete stage stays inactive for inspection; no files are deleted.
  void finishPending();
  // Caller must stop writers/readers of this category before transfer.
  // Copies and verifies; copy mode keeps the source. Explicit move removes only
  // verified originals after commit. Conflicts are not merged.
  // Profile is switched only after the complete destination is durable.
  void copyCategory(const std::string &,const std::string &slot,bool quiesced,
                    const StorageTransferHooks & = {},bool move=false);
};
StatePaths storagePaths(const std::shared_ptr<StorageLayout> &);
}
