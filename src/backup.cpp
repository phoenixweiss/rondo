// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "backup.hpp"
#include "state.hpp"
#include "progress.hpp"
#include "game_statistics.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <tuple>
namespace rondo {
namespace {
constexpr size_t maxFile=34*1024*1024,maxFiles=10000;
std::array<unsigned,3> version(const std::string &text) {
  std::array<unsigned,3> result{};size_t at=0;
  for(size_t n=0;n<3;++n) {
    auto end=text.find('.',at);auto part=text.substr(at,end==std::string::npos?end:end-at);
    if(part.empty()||part.size()>6||part.find_first_not_of("0123456789")!=std::string::npos||(n<2)==(end==std::string::npos))throw BackupError("backup.version");
    result[n]=std::stoul(part);at=end+1;
  }
  return result;
}
std::vector<uint8_t> bounded(const fs::path &file,size_t limit=maxFile) {
  if(!fs::is_regular_file(file)||fs::is_symlink(fs::symlink_status(file))||fs::file_size(file)>limit)throw BackupError("backup.invalid");
  auto bytes=readBytes(file);if(bytes.size()>limit)throw BackupError("backup.invalid");return bytes;
}
fs::path temporary(const fs::path &parent,const char *prefix) {
  std::string text=(parent/(std::string(prefix)+"XXXXXX")).string();std::vector<char> name(text.begin(),text.end());name.push_back(0);
  if(!mkdtemp(name.data()))throw BackupError("backup.io");
  return fs::path(name.data());
}
void validateContent(const std::vector<uint8_t> &bytes,const std::string &name,const Store &validator,const BackupContext *context) try {
  if(name=="config.json"||name=="config.previous.json") {
    if(bytes.size()>4*1024*1024)throw BackupError("backup.invalid");
    validator.validateDocument(Json::parse(bytes));return;
  }
  auto leaf=fs::path(name).filename().string();if(leaf.size()>9&&leaf.substr(leaf.size()-9)==".previous")leaf.resize(leaf.size()-9);
  if(leaf=="identity.json") {
    if(bytes.size()>16384)throw BackupError("backup.invalid");
    validateProgressRecord(Json::parse(bytes),fs::path(name).parent_path().filename().string());
  } else if(leaf=="statistics.json") {
    if(bytes.size()>16384)throw BackupError("backup.invalid");
    GameStatistics::validateDocument(Json::parse(bytes),fs::path(name).parent_path().filename().string());
  } else if(leaf.find(".state")!=std::string::npos) {
    auto state=decodeState(bytes);
    if(context&&!context->cores.count(state.meta.at("core")))throw BackupError("backup.core");
  } else if(bytes.empty()||bytes.size()>32*1024*1024)throw BackupError("backup.invalid");
} catch(const BackupError &){throw;} catch(const std::exception &){throw BackupError("backup.invalid");}
void checkContext(const Json &manifest,const BackupContext &context) {
  if(version(manifest.at("rondo_version"))>version(context.version))throw BackupError("backup.version");
}
void validateData(const fs::path &folder,const Json &manifest,const Store &validator,const BackupContext &context,BackupProgress *progress) {
  checkContext(manifest,context);if(progress){progress->phase=2;progress->done=0;progress->total=manifest.at("files").size();}
  auto data=backupSafePath(folder,"data");std::set<std::string> expected;
  for(const auto &file:manifest.at("files"))expected.insert(file.at("path").get<std::string>());
  for(const auto &file:manifest.at("files")) {
    auto name=file.at("path").get<std::string>();expected.insert(name);
    auto path=backupSafePath(data,name);
    auto bytes=bounded(path);
    if(bytes.size()!=file.at("bytes")||hashBytes(bytes)!=file.at("sha256"))throw BackupError("backup.invalid");
    bool historical=name.size()>9&&name.substr(name.size()-9)==".previous"&&expected.count(name.substr(0,name.size()-9));
    validateContent(bytes,name,validator,historical?nullptr:&context);if(progress)++progress->done;
  }
  // Unknown data files are never silently imported or ignored.
  for(const auto &item:fs::recursive_directory_iterator(data)) {
    if(item.is_symlink()||(!item.is_directory()&&!item.is_regular_file()))throw BackupError("backup.invalid");
    if(item.is_regular_file()&&!expected.count(item.path().lexically_relative(data).generic_string()))throw BackupError("backup.invalid");
  }
}
}
Json backupManifest(const fs::path &folder,std::string *manifestDigest) try {
  auto file=backupSafePath(folder,"manifest.json");auto bytes=bounded(file,8*1024*1024);
  auto digest=bounded(backupSafePath(folder,"manifest.sha256"),64);
  if(std::string(digest.begin(),digest.end())!=hashBytes(bytes))throw BackupError("backup.invalid");
  auto manifest=Json::parse(bytes);
  if(!manifest.is_object()||manifest.at("format")!=1||!manifest.at("created_utc").is_number_integer()||
     !manifest.at("rondo_version").is_string()||!manifest.at("files").is_array()||manifest.at("files").empty()||manifest.at("files").size()>maxFiles)
    throw BackupError("backup.invalid");
  version(manifest.at("rondo_version"));
  std::set<std::string> seen;uint64_t total=0;
  for(const auto &file:manifest.at("files")) {
    auto name=file.at("path").get<std::string>();
    if(!backupDataPath(name)||!seen.insert(name).second||!file.at("bytes").is_number_unsigned()||file.at("bytes")>maxFile||
       !file.at("sha256").is_string()||file.at("sha256").get<std::string>().size()!=64)throw BackupError("backup.invalid");
    total+=file.at("bytes").get<uint64_t>();
  }
  if(!seen.count("config.json")||!manifest.at("total_bytes").is_number_unsigned()||manifest.at("total_bytes")!=total)throw BackupError("backup.invalid");
  if(manifestDigest)*manifestDigest=hashBytes(bytes);
  return manifest;
} catch(const BackupError &){throw;} catch(const std::exception &){throw BackupError("backup.invalid");}
Json backupGames(const fs::path &folder,const std::vector<Game> &known) {
  auto manifest=backupManifest(folder);std::map<std::string,Json> entries;
  for(const auto &file:manifest["files"]) {
    auto name=file["path"].get<std::string>();
    if(name.find("games/")==0){auto id=fs::path(name).parent_path().filename().string();entries[id]={{"id",id},{"title",""},{"system",""}};}
  }
  auto merge=[&](const Json &item) {
    if(!item.is_object()||!item.contains("id")||!item["id"].is_string()||!item.contains("title")||!item["title"].is_string()||!item.contains("system")||!item["system"].is_string())return;
    auto found=entries.find(item["id"].get<std::string>());if(found==entries.end())return;
    auto title=item["title"].get<std::string>(),system=item["system"].get<std::string>();
    if(title.size()>1024||system.size()>16)return;
    found->second={{"id",item["id"]},{"title",title},{"system",system}};
  };
  for(const auto &game:known)merge({{"id",game.id},{"title",game.title},{"system",game.system}});
  try {
    for(const auto &file:manifest["files"])if(file["path"]=="config.json") {
      auto bytes=bounded(backupSafePath(folder,"data/config.json"),4*1024*1024);
      if(hashBytes(bytes)!=file["sha256"])break;
      auto config=Json::parse(bytes);
      if(config.contains("recent_games")&&config["recent_games"].is_array())for(const auto &item:config["recent_games"])merge(item);
    }
  }catch(...){} // Older copies may still have names in the installed library.
  if(manifest.contains("games")&&manifest["games"].is_array())for(const auto &item:manifest["games"])merge(item);
  Json result=Json::array();for(const auto &entry:entries)result.push_back(entry.second);
  std::sort(result.begin(),result.end(),[](const Json &a,const Json &b){return std::make_tuple(searchFold(a["title"]),a["system"],a["id"])<std::make_tuple(searchFold(b["title"]),b["system"],b["id"]);});
  return result;
}
void deleteBackup(const fs::path &root,const fs::path &folder,const std::string &expectedManifest) {
  auto parent=backupSafePath(root,"Backups");auto name=folder.filename().string();
  if(name.find("Rondo-")!=0||folder.lexically_normal()!=parent/name||!fs::is_directory(backupSafePath(parent,name)))throw BackupError("backup.invalid");
  auto lockPath=backupSafePath(root,".backup.lock");int fd=open(lockPath.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
  if(fd<0)throw BackupError("backup.io");
  struct Close{int fd;~Close(){close(fd);}} closeLock{fd};
  if(flock(fd,LOCK_EX|LOCK_NB))throw BackupError("backup.busy");
  if(!expectedManifest.empty()){std::string digest;backupManifest(folder,&digest);if(digest!=expectedManifest)throw BackupError("backup.changed");}
  // remove_all removes links themselves, never the linked destination. Only the
  // selected immediate Backups child is eligible, including a damaged copy.
  fs::remove_all(folder);syncDirectory(parent);
}
std::vector<fs::path> backupFolders(const fs::path &root) {
  std::vector<fs::path> result;auto folder=backupSafePath(root,"Backups");if(!fs::exists(folder))return result;
  for(const auto &item:fs::directory_iterator(folder))if(!item.is_symlink()&&item.is_directory()&&item.path().filename().string().find("Rondo-")==0)result.push_back(item.path());
  std::sort(result.rbegin(),result.rend());return result;
}
fs::path createBackup(const fs::path &root,const BackupContext &context,BackupProgress *progress) {
  version(context.version);
  if(fs::exists(backupSafePath(root,".restore-transaction")))throw BackupError("backup.unfinished");
  Store validator(root);if(validator.readOnly)throw BackupError("backup.version");
  if(!validator.fresh) {
    bool valid=false;
    for(const auto *name:{"config.json","config.previous.json"})try{validator.validateDocument(readJson(backupSafePath(root,name)));valid=true;break;}catch(...){}
    if(!valid)throw BackupError("backup.invalid");
  }
  auto folder=backupSafePath(root,"Backups");fs::create_directories(folder);
  auto staging=temporary(folder,".creating-");
  try {
    std::vector<std::string> names{"config.json"};
    try {validator.validateDocument(readJson(backupSafePath(root,"config.previous.json")));names.push_back("config.previous.json");}catch(...){}
    auto games=backupSafePath(root,"games");
    if(fs::exists(games))for(auto it=fs::recursive_directory_iterator(games);it!=fs::recursive_directory_iterator();++it) {
      auto name=it->path().lexically_relative(root).generic_string();
      if(it->is_directory()) {
        auto id=it->path().filename().string();
        if(it.depth()>0||id.size()!=64||id.find_first_not_of("0123456789abcdef")!=std::string::npos)it.disable_recursion_pending();
      }
      if(backupDataPath(name)) {
        if(it->is_symlink()||!it->is_regular_file())throw BackupError("backup.invalid");
        names.push_back(name);
      }
    }
    if(names.size()>maxFiles)throw BackupError("backup.invalid");
    uint64_t bytes=1024*1024+validator.data.dump(2).size();for(const auto &name:names)if(name!="config.json")bytes+=fs::file_size(backupSafePath(root,name));
    if(fs::space(root).available<bytes)throw BackupError("backup.space");
    if(progress){progress->phase=1;progress->done=0;progress->total=names.size();}
    Json manifest={{"format",1},{"rondo_version",context.version},{"created_utc",time(nullptr)},{"total_bytes",uint64_t(0)},{"files",Json::array()}};
    for(const auto &name:names) {
      auto target=staging/"data"/name;std::vector<uint8_t> bytes;
      if(name=="config.json") {
        auto text=validator.data.dump(2)+"\n";bytes={text.begin(),text.end()};
        validateContent(bytes,name,validator,nullptr);
      } else {
        bool valid=true;
        try{bytes=bounded(backupSafePath(root,name));validateContent(bytes,name,validator,nullptr);}catch(...){valid=false;}
        bool previous=name.size()>9&&name.substr(name.size()-9)==".previous";
        if(!valid&&previous){if(progress)++progress->done;continue;}
        if(!valid) {bytes=bounded(backupSafePath(root,name+".previous"));validateContent(bytes,name,validator,nullptr);}
      }
      atomicWrite(target,bytes);
      Json item={{"path",name},{"bytes",bytes.size()},{"sha256",hashBytes(bytes)}};
      manifest["total_bytes"]=manifest["total_bytes"].get<uint64_t>()+bytes.size();manifest["files"].push_back(item);
      if(progress)++progress->done;
    }
    manifest["games"]=Json::array();std::set<std::string> savedGames;
    for(const auto &file:manifest["files"]){auto name=file["path"].get<std::string>();if(name.find("games/")==0)savedGames.insert(fs::path(name).parent_path().filename().string());}
    if(context.games.is_array())for(const auto &game:context.games)if(game.is_object()&&game.contains("id")&&game["id"].is_string()&&savedGames.count(game["id"].get<std::string>()))manifest["games"].push_back(game);
    // Export preserves valid older-core states; compatibility is checked when
    // restoring, not used to discard historical saves while making a copy.
    auto text=manifest.dump(2)+"\n";auto hash=hashBytes({text.begin(),text.end()});
    atomicWrite(staging/"manifest.json",{text.begin(),text.end()});atomicWrite(staging/"manifest.sha256",{hash.begin(),hash.end()});
    backupManifest(staging);
    if(progress){progress->phase=2;progress->done=0;progress->total=manifest["files"].size();}
    for(const auto &file:manifest["files"]) {
      if(hashFile(staging/"data"/file["path"].get<std::string>())!=file["sha256"])throw BackupError("backup.io");
      if(progress)++progress->done;
    }
    tm date{};auto now=time(nullptr);gmtime_r(&now,&date);char stamp[40];strftime(stamp,sizeof(stamp),"Rondo-%Y-%m-%d_%H-%M-%S",&date);
    auto target=folder/stamp;for(int n=2;fs::exists(target);++n)target=folder/(std::string(stamp)+"-"+std::to_string(n));
    fs::rename(staging,target);syncDirectory(folder);return target;
  } catch (...) {std::error_code error;fs::remove_all(staging,error);throw;}
}
void restoreBackup(const fs::path &root,const fs::path &folder,const BackupContext &context,BackupProgress *progress) {
  // All source checks precede both the rollback journal and active replacements.
  std::string digest;auto manifest=backupManifest(folder,&digest);
  if(!context.expectedManifest.empty()&&context.expectedManifest!=digest)throw BackupError("backup.changed");
  checkContext(manifest,context);
  if(fs::exists(backupSafePath(root,".restore-transaction")))throw BackupError("backup.unfinished");
  Store validator(root);if(validator.readOnly)throw BackupError("backup.version");
  validateData(folder,manifest,validator,context,progress);
  fs::create_directories(root);
  if(fs::space(root).available<manifest.at("total_bytes").get<uint64_t>()+1024*1024)throw BackupError("backup.space");
  auto staging=temporary(root,".restore-staging-");
  try {
    if(progress){progress->phase=1;progress->done=0;progress->total=manifest.at("files").size();}
    for(const auto &file:manifest.at("files")) {
      auto name=file.at("path").get<std::string>();auto bytes=bounded(backupSafePath(folder/"data",name));
      if(hashBytes(bytes)!=file.at("sha256"))throw BackupError("backup.changed");
      atomicWrite(staging/"data"/name,bytes);if(progress)++progress->done;
    }
    auto changes=manifest.at("files");std::set<std::string> included,games;
    for(const auto &file:changes) {
      auto name=file.at("path").get<std::string>();included.insert(name);
      if(name.find("games/")==0)games.insert(fs::path(name).parent_path().filename().string());
    }
    auto removeAbsent=[&](const std::string &name) {
      if(!included.count(name)&&fs::exists(backupSafePath(root,name)))changes.push_back({{"path",name},{"remove",true}});
    };
    removeAbsent("config.previous.json");
    for(const auto &game:games) {
      auto folder=backupSafePath(root,"games/"+game);
      if(fs::is_directory(folder))for(const auto &file:fs::directory_iterator(folder)) {
        auto name=file.path().lexically_relative(root).generic_string();if(backupDataPath(name))removeAbsent(name);
      }
    }
    applyBackupFiles(root,staging,changes,progress);
  }catch(...){std::error_code error;fs::remove_all(staging,error);throw;}
}
}
