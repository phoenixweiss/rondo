// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "backup.hpp"
#include <algorithm>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
namespace rondo {
namespace {
constexpr size_t maxFile=34*1024*1024;
const char *transaction=".restore-transaction";
struct Lock {
  int fd=-1;
  explicit Lock(const fs::path &root) {
    auto path=backupSafePath(root,".backup.lock");
    fd=open(path.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0)throw BackupError("backup.io");
    if(flock(fd,LOCK_EX|LOCK_NB)){close(fd);fd=-1;throw BackupError("backup.busy");}
  }
  ~Lock(){if(fd>=0)close(fd);}
};
Json readJournal(const fs::path &dir,const std::string &name) {
  auto path=backupSafePath(dir,name);
  if(fs::file_size(path)>8*1024*1024)throw BackupError("backup.invalid");
  auto wrapped=readJson(path);auto body=wrapped.at("payload");auto text=body.dump();
  if(wrapped.at("sha256")!=hashBytes({text.begin(),text.end()}))throw BackupError("backup.invalid");
  if(body.at("format")!=1)throw BackupError("backup.version");
  if((body.at("phase")!="prepared"&&body.at("phase")!="committed")||!body.at("files").is_array()||body.at("files").size()>10000)
    throw BackupError("backup.invalid");
  std::set<std::string> seen;
  for(const auto &file:body.at("files")) {
    auto name=file.at("path").get<std::string>();
    if(!backupDataPath(name)||!seen.insert(name).second||!file.at("existed").is_boolean())throw BackupError("backup.invalid");
  }
  return body;
}
Json journal(const fs::path &dir) {
  try{return readJournal(dir,"journal.json");}
  catch(const BackupError &error){if(error.code=="backup.version")throw;}
  catch(...){}
  return readJournal(dir,"journal.previous.json");
}
void writeJournal(const fs::path &dir,const Json &body) {
  auto text=body.dump();Json wrapped={{"payload",body},{"sha256",hashBytes({text.begin(),text.end()})}};
  text=wrapped.dump()+"\n";atomicWrite(dir/"journal.json",{text.begin(),text.end()});
}
void rollback(const fs::path &root,const fs::path &dir,const Json &body,BackupProgress *progress) {
  // Validate the entire rollback generation before touching any active file.
  for(const auto &file:body.at("files")) {
    auto name=file.at("path").get<std::string>();backupSafePath(root,name);
    if(file.at("existed")==true) {
      auto source=backupSafePath(dir/"originals",name);
      if(!fs::is_regular_file(source)||fs::file_size(source)>maxFile||hashFile(source)!=file.at("sha256"))throw BackupError("backup.unfinished");
    }
  }
  if(progress){progress->phase=4;progress->done=0;progress->total=body.at("files").size();}
  // Free newly created files before replaying originals, including an ENOSPC rollback.
  for(bool original:{false,true})for(const auto &file:body.at("files")) {
    if(file.at("existed").get<bool>()!=original)continue;
    auto name=file.at("path").get<std::string>();auto target=backupSafePath(root,name);
    if(file.at("existed")==true)atomicWrite(target,readBytes(backupSafePath(dir/"originals",name)));
    else if(fs::exists(target)){fs::remove(target);syncDirectory(target.parent_path());}
    if(progress)++progress->done;
  }
}
bool recover(const fs::path &root,BackupProgress *progress=nullptr) {
  auto dir=backupSafePath(root,transaction);
  if(!fs::exists(dir))return false;
  auto body=journal(dir);bool reverted=body.at("phase")=="prepared";
  if(reverted)rollback(root,dir,body,progress);
  fs::remove_all(dir);syncDirectory(root);return reverted;
}
}
bool backupDataPath(const std::string &name) {
  if(name=="config.json"||name=="config.previous.json")return true;
  fs::path path(name);if(path.generic_string()!=name||path.is_absolute())return false;
  std::vector<std::string> parts;for(const auto &part:path)parts.push_back(part.string());
  if(parts.size()!=3||parts[0]!="games"||parts[1].size()!=64||parts[1].find_first_not_of("0123456789abcdef")!=std::string::npos)return false;
  const std::set<std::string> leaves{"sram.bin","rtc.bin","statistics.json","identity.json","slot-0.state","slot-1.state","slot-2.state","autosave.state"};
  auto leaf=parts[2];if(leaf.size()>9&&leaf.substr(leaf.size()-9)==".previous")leaf.resize(leaf.size()-9);
  return leaves.count(leaf);
}
fs::path backupSafePath(const fs::path &root,const std::string &name) {
  fs::path relative(name);
  if(relative.empty()||relative.is_absolute()||relative.has_root_name()||name.find('\0')!=std::string::npos||relative.lexically_normal()!=relative)
    throw BackupError("backup.invalid");
  auto path=root;
  if(fs::is_symlink(fs::symlink_status(path)))throw BackupError("backup.invalid");
  for(const auto &part:relative) {
    if(part==".."||part==".")throw BackupError("backup.invalid");
    path/=part;
    if(fs::is_symlink(fs::symlink_status(path)))throw BackupError("backup.invalid");
  }
  return path;
}
bool recoverPendingRestore(const fs::path &root) {
  if(!fs::exists(backupSafePath(root,transaction)))return false;
  Lock lock(root);return recover(root);
}
void applyBackupFiles(const fs::path &root,const fs::path &staging,const Json &files,BackupProgress *progress,const std::string &newGame) {
  Lock lock(root);
  if(!newGame.empty()) {
    if(!backupDataPath("games/"+newGame+"/sram.bin"))throw BackupError("backup.invalid");
    auto folder=backupSafePath(root,"games/"+newGame);
    if(fs::is_directory(folder))for(const auto &file:fs::directory_iterator(folder)) {
      auto name=file.path().lexically_relative(root).generic_string();
      if(backupDataPath(name)&&file.path().filename()!="identity.json")throw BackupError("progress.target_exists");
    }
  }
  if(!files.is_array()||files.empty()||files.size()>10000)throw BackupError("backup.invalid");
  if(fs::exists(backupSafePath(root,transaction)))throw BackupError("backup.unfinished");
  std::set<std::string> seen;
  Json body={{"format",1},{"phase","prepared"},{"files",Json::array()}};
  uint64_t needed=1024*1024,maxSize=0;
  for(const auto &file:files) {
    auto name=file.at("path").get<std::string>();if(!backupDataPath(name)||!seen.insert(name).second)throw BackupError("backup.invalid");
    auto target=backupSafePath(root,name);
    uint64_t size=0;
    if(!file.value("remove",false))size=fs::file_size(backupSafePath(staging/"data",name));
    if(size>maxFile)throw BackupError("backup.invalid");
    maxSize=std::max<uint64_t>(maxSize,size);
    Json entry={{"path",name},{"existed",fs::exists(target)}};
    if(entry["existed"]==true) {
      if(!fs::is_regular_file(target))throw BackupError("backup.invalid");
      auto old=fs::file_size(target);if(old>maxFile)throw BackupError("backup.space");
      needed+=old+(size>old?size-old:0);maxSize=std::max<uint64_t>(maxSize,old);entry["sha256"]=hashFile(target);
    }
    if(entry["existed"]==false)needed+=size;
    body["files"].push_back(entry);
  }
  // Staged new data already occupies disk space. Keep enough additional space
  // for original copies, replacement growth and a complete rollback temporary.
  if(fs::space(root).available<needed+maxSize)throw BackupError("backup.space");
  for(const auto &file:body["files"])if(file["existed"]==true) {
    auto name=file["path"].get<std::string>();auto bytes=readBytes(backupSafePath(root,name));
    if(hashBytes(bytes)!=file["sha256"])throw BackupError("backup.changed");
    atomicWrite(staging/"originals"/name,bytes);
  }
  writeJournal(staging,body);
  atomicWrite(staging/"journal.previous.json",readBytes(staging/"journal.json"));
  auto dir=root/transaction;fs::rename(staging,dir);syncDirectory(root);
  try {
    if(progress){progress->phase=3;progress->done=0;progress->total=files.size();}
    for(const auto &file:files) {
      auto name=file["path"].get<std::string>();
      if(file.value("remove",false)) {
        auto target=backupSafePath(root,name);
        if(fs::remove(target))syncDirectory(target.parent_path());
        if(progress)++progress->done;
        continue;
      }
      auto bytes=readBytes(backupSafePath(dir/"data",name));
      if(hashBytes(bytes)!=file["sha256"])throw BackupError("backup.changed");
      auto target=backupSafePath(root,name);atomicWrite(target,bytes);
      if(hashFile(target)!=file["sha256"])throw BackupError("backup.io");
      if(progress)++progress->done;
    }
    body["phase"]="committed";writeJournal(dir,body);
  } catch (...) {recover(root,progress);throw;}
  fs::remove_all(dir);syncDirectory(root);
}
}
