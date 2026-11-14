// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "removal.hpp"
#include "backup.hpp"
#include "psx.hpp"
#include "state.hpp"
#include "storage.hpp"
#include <algorithm>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
namespace rondo {
namespace {
void flushParent(int descriptor) {
  int result;do{result=fsync(descriptor);}while(result&&errno==EINTR);
  int error=result?errno:0;
#ifdef __linux__
  // Match the existing FAT barrier while retaining the verified directory FD.
  int flushed;do{flushed=syncfs(descriptor);}while(flushed&&errno==EINTR);
  if(flushed)throw std::runtime_error("Cannot flush game filesystem");
#endif
  if(error&&error!=EINVAL&&error!=ENOTSUP)throw std::runtime_error("Cannot sync game directory");
}
std::string identityOf(const struct stat &status,bool metadata) {
  auto value=std::to_string(status.st_dev)+":"+std::to_string(status.st_ino)+":"+std::to_string(status.st_mode);
  if(!metadata)return value;
#ifdef __APPLE__
  auto modified=status.st_mtimespec,changed=status.st_ctimespec;
#else
  auto modified=status.st_mtim,changed=status.st_ctim;
#endif
  return value+":"+std::to_string(status.st_size)+":"+std::to_string(modified.tv_sec)+":"+
    std::to_string(modified.tv_nsec)+":"+std::to_string(changed.tv_sec)+":"+std::to_string(changed.tv_nsec);
}
std::string pathIdentity(const fs::path &path,bool metadata) {
  struct stat status{};
  if(lstat(path.c_str(),&status)||S_ISLNK(status.st_mode))throw RemovalError("removal.changed");
  return identityOf(status,metadata);
}
}
RemovalPreview previewRemoval(const Game &game,const std::vector<Game> &games,
                             const std::vector<Volume> &volumes,const StatePaths &paths,const Json &preferences) {
  RemovalPreview result;result.gameId=game.id;result.preferences=preferences;result.binding=paths.binding();
  auto rememberRoot=[&](const fs::path &root){if(fs::exists(root))result.roots[root]=pathIdentity(root,false);};
  rememberRoot(paths.settings());
  auto configFile=backupSafePath(paths.settings(),"config.json");
  if(fs::exists(configFile))result.configIdentity=pathIdentity(configFile,true);
  std::set<fs::path> added,shared;
  auto warning=[&](const std::string &key){if(std::find(result.warnings.begin(),result.warnings.end(),key)==result.warnings.end())result.warnings.push_back(key);};
  auto add=[&](const fs::path &root,const fs::path &path,RemovalCategory category,const std::string &location,const std::string &reason="") {
    try {
      auto checked=backupSafePath(root,path.lexically_relative(root).generic_string());
      if(!fs::is_regular_file(checked))return;
      if(!added.insert(checked).second)return;
      result.entries.push_back({category,checked,location,reason,fs::file_size(checked),false,pathIdentity(checked,true),pathIdentity(checked.parent_path(),false)});
    } catch(...) {warning("removal.unsafe");}
  };
  auto volume=std::find_if(volumes.begin(),volumes.end(),[&](const Volume &v){return v.id==game.volume;});
  try {
    if(!game.available||volume==volumes.end())throw std::runtime_error("Unavailable game");
    auto root=fs::canonical(volume->root);rememberRoot(root);
    auto rom=backupSafePath(root,game.path.lexically_relative(root).generic_string());
    if(!fs::is_regular_file(rom))throw std::runtime_error("Missing game");
    auto files=game.system=="PSX"?psxDiscs(rom):std::vector<fs::path>{rom};
    files.push_back(rom);
    bool uncertain=false;
    // Include playlists suppressed by library scanning; an unreadable playlist
    // cannot establish exclusive ownership of any disc on this volume.
    if(game.system=="PSX")for(auto it=fs::recursive_directory_iterator(root);it!=fs::recursive_directory_iterator();++it) {
      if(it->is_symlink()){it.disable_recursion_pending();continue;}
      if(!it->is_regular_file()||searchFold(it->path().extension().string())!=".m3u"||it->path()==rom)continue;
      try {for(const auto &disc:psxDiscs(it->path()))shared.insert(disc);}
      catch(...){uncertain=true;}
    }
    bool sharedMedia=false;
    for(const auto &other:games)if(other.id!=game.id&&other.available&&other.path.parent_path()==rom.parent_path())sharedMedia=true;
    const std::set<std::string> contentExtensions{".nes",".gb",".gbc",".gba",".sfc",".smc",".md",".gen",".chd",".m3u",".cue",".iso",".bin"};
    for(const auto &entry:fs::directory_iterator(rom.parent_path()))
      if(entry.is_regular_file()&&contentExtensions.count(searchFold(entry.path().extension().string()))&&
         std::find(files.begin(),files.end(),entry.path())==files.end())sharedMedia=true;
    for(const auto &file:files) {
      bool keep=shared.count(file)||(uncertain&&searchFold(file.extension().string())==".chd");
      add(root,file,keep?RemovalCategory::Kept:RemovalCategory::Rom,game.volume,
          keep?(shared.count(file)?"removal.shared":"removal.uncertain"):"");
    }
    if(uncertain&&game.system=="PSX")warning("removal.uncertain");
    // Never turn a game folder into a recursive deletion target. Only known
    // artwork filenames belong to the media choice; all other files stay.
    for(const auto &entry:fs::directory_iterator(rom.parent_path())) {
      auto name=searchFold(entry.path().filename().string());
      if(entry.is_symlink()){warning("removal.unsafe");continue;}
      if(!entry.is_regular_file()||added.count(entry.path()))continue;
      bool media=name=="cover.png"||name=="title.png"||name=="screenshot.png";
      auto reason=media&&sharedMedia?"removal.shared":"removal.unowned";
      add(root,entry.path(),media&&!sharedMedia?RemovalCategory::Media:RemovalCategory::Kept,game.volume,media&&!sharedMedia?"":reason);
    }
  } catch(...) {warning("removal.rom_unavailable");}
  try {
    auto guard=paths.guard("saves",false);auto root=paths.category("saves");rememberRoot(root);
    auto folder=backupSafePath(root,game.id);
    if(fs::exists(folder))for(const auto &entry:fs::recursive_directory_iterator(folder)) {
      if(entry.is_symlink()){warning("removal.unsafe");continue;}
      if(!entry.is_regular_file())continue;
      auto name=entry.path().filename().string();if(name.size()>9&&name.substr(name.size()-9)==".previous")name.resize(name.size()-9);
      auto category=RemovalCategory::Kept;
      if(entry.path().parent_path()==folder) {
        if(name=="sram.bin"||name=="rtc.bin")category=RemovalCategory::Memory;
        for(int slot=0;slot<4;++slot)if(name==stateName(slot))category=RemovalCategory::Slots;
        if(name=="statistics.json")category=RemovalCategory::Statistics;
      }
      add(root,entry.path(),category,"saves",category==RemovalCategory::Kept?"removal.unowned":"");
    }
    guard();
  } catch(...) {warning("removal.saves_unavailable");}
  try {
    auto guard=paths.guard("screenshots",false);auto root=paths.category("screenshots");rememberRoot(root);
    if(fs::is_symlink(root))throw std::runtime_error("Linked screenshots");
    auto owner=hashBytes({game.id.begin(),game.id.end()})+"\n";
    if(fs::exists(root))for(auto it=fs::recursive_directory_iterator(root);it!=fs::recursive_directory_iterator();++it) {
      if(it->is_symlink()){it.disable_recursion_pending();continue;}
      if(it->path().filename()!=".game-id"||!it->is_regular_file())continue;
      auto marker=backupSafePath(root,it->path().lexically_relative(root).generic_string());
      if(fs::file_size(marker)!=owner.size())continue;
      std::ifstream stream(marker);std::string value(owner.size(),' ');stream.read(value.data(),value.size());
      if(value!=owner)continue;
      for(const auto &entry:fs::directory_iterator(marker.parent_path()))
        if(!entry.is_symlink()&&entry.is_regular_file()&&searchFold(entry.path().extension().string())==".png")
          add(root,entry.path(),RemovalCategory::Screenshots,"screenshots");
    }
    guard();
  } catch(...) {warning("removal.screenshots_unavailable");}
  // These are individual JSON records, never deletion of the shared config file.
  if(preferences.contains("favorites"))for(const auto &id:preferences["favorites"])if(id==game.id)
    result.entries.push_back({RemovalCategory::Library,paths.settings()/"config.json","favorites","",0,true});
  if(preferences.contains("recent_games"))for(const auto &item:preferences["recent_games"])if(item.is_object()&&item.value("id",std::string())==game.id)
    result.entries.push_back({RemovalCategory::Library,paths.settings()/"config.json","recent_games","",0,true});
  if(preferences.contains("game_options")&&preferences["game_options"].contains(game.id))
    result.entries.push_back({RemovalCategory::Library,paths.settings()/"config.json","game_options","",0,true});
  if(preferences.value("resume_game",std::string())==game.id)
    result.entries.push_back({RemovalCategory::Library,paths.settings()/"config.json","resume_game","",0,true});
  for(const auto &key:result.warnings)result.entries.push_back({RemovalCategory::Kept,{},"",key,0,true});
  std::sort(result.entries.begin(),result.entries.end(),[](const auto &a,const auto &b){return std::make_pair(a.category,a.path)<std::make_pair(b.category,b.path);});
  for(const auto &root:result.roots)if(pathIdentity(root.first,false)!=root.second)throw std::runtime_error("Changed storage during inventory");
  return result;
}
RemovalResult executeRemoval(const Game &game,const std::vector<Volume> &volumes,const StatePaths &paths,
                             const RemovalPreview &reviewed,const RemovalChoices &choices,
                             std::function<void(size_t)> checkpoint) {
  RemovalResult result;
  for(const auto &entry:reviewed.entries)if(choices[size_t(entry.category)])++result.total;
  int journal=-1;
  auto append=[&](const Json &event) {
    auto line=event.dump()+"\n";size_t done=0;
    while(done<line.size()) {
      auto count=write(journal,line.data()+done,line.size()-done);
      if(count<0&&errno==EINTR)continue;
      if(count<=0)throw std::runtime_error("Removal receipt write failed");
      done+=size_t(count);
    }
    int synced;do{synced=fsync(journal);}while(synced&&errno==EINTR);
    if(synced)throw std::runtime_error("Removal receipt sync failed");
  };
  auto changed=[] {throw RemovalError("removal.changed");};
  try {
    if(reviewed.gameId!=game.id||!choices[0]||choices[size_t(RemovalCategory::Kept)])changed();
    auto checkRoots=[&] {
      if(paths.binding()!=reviewed.binding)changed();
      for(const auto &root:reviewed.roots)if(pathIdentity(root.first,false)!=root.second)changed();
    };
    checkRoots();
    auto configFile=backupSafePath(paths.settings(),"config.json");
    if((fs::exists(configFile)?pathIdentity(configFile,true):std::string())!=reviewed.configIdentity)changed();
    Store settings(paths);
    if(settings.data!=reviewed.preferences||settings.readOnly)changed();
    auto library=scan(volumes);
    auto targetGame=std::find_if(library.begin(),library.end(),[&](const auto &entry){return entry.id==game.id&&entry.path==game.path&&entry.system==game.system&&entry.volume==game.volume;});
    if(targetGame==library.end())changed();
    auto live=previewRemoval(*targetGame,library,volumes,paths,settings.data);
    auto signature=[](const RemovalPreview &preview) {
      Json entries=Json::array();
      for(const auto &entry:preview.entries)entries.push_back(Json::array({int(entry.category),entry.path.generic_string(),
        entry.location,entry.reason,entry.bytes,entry.record,entry.identity,entry.parentIdentity}));
      return entries;
    };
    if(live.roots!=reviewed.roots||live.warnings!=reviewed.warnings||signature(live)!=signature(reviewed))changed();
    std::vector<RemovalEntry> selected;
    bool target=false;
    for(const auto &entry:live.entries) {
      if(entry.category==RemovalCategory::Rom&&entry.path==game.path)target=true;
      if(choices[size_t(entry.category)])selected.push_back(entry);
    }
    if(!target||selected.empty())changed();
    // The launch target goes first. If a later deletion fails, no remaining
    // playlist will reference an already removed private disc.
    std::stable_sort(selected.begin(),selected.end(),[&](const auto &a,const auto &b){return a.path==game.path&&b.path!=game.path;});
    result.total=selected.size();
    auto expectedConfig=paths.settings(true);
    auto configGuard=[&]{if(paths.settings(true)!=expectedConfig)changed();};configGuard();
    std::vector<std::function<void()>> guards;
    for(const auto &entry:selected)if(!entry.record) {
      if(entry.location=="saves")guards.push_back(paths.guard("saves"));
      if(entry.location=="screenshots")guards.push_back(paths.guard("screenshots"));
    }
    auto guard=[&] {checkRoots();configGuard();for(const auto &check:guards)check();};
    guard();
    auto operations=backupSafePath(paths.settings(true),"Operations");
    fs::create_directories(operations);syncDirectory(operations.parent_path());
    for(int suffix=0;suffix<1000;++suffix) {
      auto name="removal-"+std::to_string(time(nullptr))+"-"+game.id.substr(0,12)+"-"+std::to_string(suffix)+".jsonl";
      auto receipt=backupSafePath(operations,name);
      journal=open(receipt.c_str(),O_WRONLY|O_APPEND|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
      if(journal>=0){result.receipt=receipt;break;}
      if(errno!=EEXIST)throw std::runtime_error("Cannot create removal receipt");
    }
    if(journal<0)throw std::runtime_error("Cannot reserve removal receipt");
    Json planned=Json::array();for(const auto &entry:selected)planned.push_back({{"path",entry.path.generic_string()},{"record",entry.record?entry.location:""},{"bytes",entry.bytes}});
    append({{"format",1},{"event","started"},{"game_id",game.id},{"title",game.title},{"volume",game.volume},{"utc",time(nullptr)},{"planned",planned}});
    syncDirectory(operations);
    for(const auto &entry:selected) {
      if(entry.record)continue;
      result.failed=entry.path;
      if(checkpoint)checkpoint(result.removed);
      guard();
      auto parent=open(entry.path.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
      if(parent<0)changed();
      struct Parent {int fd;~Parent(){close(fd);}} held{parent};
      struct stat directory{},file{};
      if(fstat(parent,&directory)||identityOf(directory,false)!=entry.parentIdentity||
         fstatat(parent,entry.path.filename().c_str(),&file,AT_SYMLINK_NOFOLLOW)||
         !S_ISREG(file.st_mode)||identityOf(file,true)!=entry.identity)changed();
      if(unlinkat(parent,entry.path.filename().c_str(),0))throw std::runtime_error("Cannot remove game file");
      ++result.removed;result.bytes+=entry.bytes;result.removedPaths.push_back(entry.path);
      flushParent(parent);
      append({{"event","removed"},{"path",entry.path.generic_string()},{"bytes",entry.bytes}});
    }
    auto records=std::count_if(selected.begin(),selected.end(),[](const auto &entry){return entry.record;});
    if(records) {
      result.failed=paths.settings()/"config.json";
      if(checkpoint)checkpoint(result.removed);
      guard();Store current(paths);
      if(current.data!=reviewed.preferences)changed();
      auto next=current.data;
      auto &favorites=next["favorites"];
      favorites.erase(std::remove(favorites.begin(),favorites.end(),Json(game.id)),favorites.end());
      auto &recent=next["recent_games"];
      recent.erase(std::remove_if(recent.begin(),recent.end(),[&](const Json &item){return item.value("id",std::string())==game.id;}),recent.end());
      next["game_options"].erase(game.id);
      if(next.value("resume_game",std::string())==game.id)next["resume_game"]="";
      auto expected=next;expected["schema"]=1;expected["revision"]=current.data.value("revision",0LL)+1;
      try {current.commit(next);}
      catch(...) {
        try {if(readJson(current.root/"config.json")==expected){result.recordsApplied=true;result.removed+=size_t(records);}}catch(...){}
        throw;
      }
      result.recordsApplied=true;result.removed+=size_t(records);
      append({{"event","records_removed"},{"game_id",game.id},{"count",records}});
    }
    result.failed.clear();guard();
    append({{"event","completed"},{"removed",result.removed},{"bytes",result.bytes}});
    syncDirectory(result.receipt.parent_path());result.completed=true;
  } catch(const RemovalError &error) {result.error=error.what();}
    catch(const StorageError &error) {result.error=error.code;}
    catch(...) {result.error="removal.io";}
  if(!result.completed&&journal>=0)try {append({{"event","stopped"},{"removed",result.removed},{"error",result.error},{"failed",result.failed.generic_string()}});syncDirectory(result.receipt.parent_path());}catch(...){}
  if(journal>=0)close(journal);
  return result;
}
}
