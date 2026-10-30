// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "progress.hpp"
#include "state.hpp"
#include "psx.hpp"
#include "game_statistics.hpp"
#include <algorithm>
#include <unistd.h>
namespace rondo {
namespace {
bool hex(const std::string &value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos;}
Json wrapped(const Json &body){auto text=body.dump();return {{"payload",body},{"sha256",hashBytes({text.begin(),text.end()})}};}
std::vector<std::string> files(const fs::path &root,const std::string &id) {
 if(!hex(id))throw BackupError("backup.invalid");
 auto folder=backupSafePath(root,"games/"+id);std::vector<std::string> out;
 if(fs::is_directory(folder))for(const auto &item:fs::directory_iterator(folder)) {
  auto name=item.path().lexically_relative(root).generic_string();
  if(backupDataPath(name)) {backupSafePath(root,name);if(!item.is_regular_file()||item.file_size()>34*1024*1024)throw BackupError("backup.invalid");out.push_back(name);}
 }
 std::sort(out.begin(),out.end());return out;
}
Json snapshot(const fs::path &root,const std::string &id) {
 Json out=Json::array();for(const auto &name:files(root,id))out.push_back({{"path",name},{"bytes",fs::file_size(root/name)},{"sha256",hashFile(root/name)}});return out;
}
std::string digest(const Json &files){auto text=files.dump();return hashBytes({text.begin(),text.end()});}
Json document(const fs::path &root,const std::string &id) {
 auto p=backupSafePath(root,"games/"+id+"/identity.json");if(fs::file_size(p)>16384)throw BackupError("backup.invalid");return validateProgressRecord(readJson(p),id);
}
void stage(const fs::path &root,const std::string &name,const std::vector<uint8_t> &bytes,Json &changes) {
 atomicWrite(root/"data"/name,bytes);changes.push_back({{"path",name},{"sha256",hashBytes(bytes)}});
}
std::vector<uint8_t> jsonBytes(const Json &data){auto text=data.dump(2)+"\n";return {text.begin(),text.end()};}
}
std::string progressFingerprint(const Game &game,const std::string &loadedRomHash) {
 auto identity=game.system=="PSX"?psxIdentity(psxDiscs(game.path)):loadedRomHash.empty()?hashFile(game.path):loadedRomHash;
 if(game.system!="PSX"&&!hex(identity))throw BackupError("backup.invalid");
 auto text=game.system+"\n"+identity;return hashBytes({text.begin(),text.end()});
}
Json progressRecord(const Game &game,const std::vector<Volume> &volumes,const std::string &fingerprint) {
 auto volume=std::find_if(volumes.begin(),volumes.end(),[&](const Volume &v){return v.id==game.volume;});
 if(volume==volumes.end()||!hex(game.id)||!hex(fingerprint))throw BackupError("backup.invalid");
 auto relative=fs::weakly_canonical(game.path).lexically_relative(fs::weakly_canonical(volume->root)).generic_string();
 auto text=game.volume+"\n"+relative;if(relative.empty()||fs::path(relative).is_absolute()||*fs::path(relative).begin()==".."||hashBytes({text.begin(),text.end()})!=game.id)throw BackupError("backup.invalid");
 return wrapped({{"schema",1},{"game_id",game.id},{"system",game.system},{"title",game.title},{"volume",game.volume},{"relative_path",relative},{"fingerprint",fingerprint},{"last_launched_utc",time(nullptr)}});
}
Json validateProgressRecord(const Json &data,const std::string &gameId) {
 auto body=data.at("payload");auto text=body.dump();
 if(data.at("sha256")!=hashBytes({text.begin(),text.end()})||body.at("schema")!=1||body.at("game_id")!=gameId||!hex(gameId)||!hex(body.at("fingerprint")))throw BackupError("backup.invalid");
 for(const auto *key:{"system","title","volume","relative_path"})if(!body.at(key).is_string()||body[key].get<std::string>().size()>4096)throw BackupError("backup.invalid");
 auto relative=body.at("relative_path").get<std::string>(),identity=body.at("volume").get<std::string>()+"\n"+relative;
 if(relative.empty()||fs::path(relative).is_absolute()||hashBytes({identity.begin(),identity.end()})!=gameId)throw BackupError("backup.invalid");
 for(const auto &part:fs::path(relative))if(part==".."||part==".")throw BackupError("backup.invalid");
 return body;
}
void rememberProgress(const fs::path &root,const Game &game,const std::vector<Volume> &volumes,const std::string &fingerprint) {
 auto record=progressRecord(game,volumes,fingerprint);auto path=backupSafePath(root,"games/"+game.id+"/identity.json");
 if(fs::exists(path)) {auto old=document(root,game.id);if(old.at("fingerprint")!=fingerprint)throw BackupError("progress.changed_rom");if(old==record.at("payload"))return;}
 atomicWrite(path,jsonBytes(record));
}
bool hasProgress(const fs::path &root,const std::string &id) {
 for(const auto &name:files(root,id))if(fs::path(name).filename()!="identity.json")return true;return false;
}
bool hasMissingProgress(const fs::path &root,const std::vector<Game> &games) {
 auto folder=backupSafePath(root,"games");if(!fs::is_directory(folder))return false;std::set<std::string> live;
 for(const auto &game:games)if(game.available)live.insert(game.id);
 for(const auto &item:fs::directory_iterator(folder))if(!item.is_symlink()&&item.is_directory()) {
  auto id=item.path().filename().string();if(hex(id)&&!live.count(id)&&fs::is_regular_file(item.path()/"identity.json"))return true;
 }
 return false;
}
Json findProgress(const fs::path &root,const Game &target,const std::vector<Game> &games,const std::string &fingerprint,const std::string &legacyRom,const std::string &legacyCore) {
 if(hasProgress(root,target.id))throw BackupError("progress.target_exists");
 auto folder=backupSafePath(root,"games");Json out=Json::array();if(!fs::is_directory(folder))return out;
 std::set<std::string> live;for(const auto &game:games)if(game.available)live.insert(game.id);
 Json history=Json::array();try{history=Store(root).data.at("recent_games");}catch(...){}
 for(const auto &item:fs::directory_iterator(folder)) {
  auto id=item.path().filename().string();if(!hex(id)||id==target.id||live.count(id)||item.is_symlink()||!item.is_directory())continue;
  try {
   Json old=Json::object();bool matched=false;long long utc=0;
   if(fs::exists(item.path()/"identity.json")){old=document(root,id);matched=old.at("system")==target.system&&old.at("fingerprint")==fingerprint;}
   else {
    for(const auto &recent:history)if(recent.value("id","")==id)old=recent;
    if(old.contains("system")&&old["system"]!=target.system)continue;
    for(int slot=0;slot<4&&!matched;++slot)try{auto meta=readRecoverableState(item.path()/stateName(slot),StateRead::Metadata).meta;matched=meta.at("rom")==legacyRom&&meta.at("core")==legacyCore;}catch(...){}
   }
   if(matched&&hasProgress(root,id)) {for(int slot=0;slot<4;++slot)try{utc=std::max(utc,readRecoverableState(item.path()/stateName(slot),StateRead::Metadata).meta.value("utc",0LL));}catch(...){}
    utc=std::max(utc,old.value("last_launched_utc",0LL));auto list=snapshot(root,id);out.push_back({{"id",id},{"title",old.value("title",std::string())},{"relative_path",old.value("relative_path",std::string())},{"files",list},{"sha256",digest(list)},{"utc",utc}});}
  }catch(...){} // Unverifiable progress remains untouched and is never guessed from a title.
 }
 std::sort(out.begin(),out.end(),[](const Json &a,const Json &b){return a.at("utc")==b.at("utc")?a.at("id")<b.at("id"):a.at("utc")>b.at("utc");});return out;
}
void relocateProgress(const fs::path &root,const Json &candidate,const Game &target,const std::vector<Volume> &volumes,const std::vector<Game> &games,const std::string &fingerprint,const std::string &legacyRom,const std::string &legacyCore,BackupProgress *progress) {
 if(progress)progress->phase=2;
 if(progressFingerprint(target)!=fingerprint)throw BackupError("progress.changed_rom");
 auto matches=findProgress(root,target,games,fingerprint,legacyRom,legacyCore);bool found=false;
 for(const auto &match:matches)if(match.at("id")==candidate.at("id")&&match.at("sha256")==candidate.at("sha256")&&match.at("files")==candidate.at("files"))found=true;
 if(!found)throw BackupError("backup.changed");
 Store settings(root);if(settings.readOnly)throw BackupError("backup.version");auto next=settings.data;auto source=candidate.at("id").get<std::string>();
 auto &options=next["game_options"];if(options.contains(source)){if(options.contains(target.id)&&options[target.id]!=options[source])throw BackupError("progress.target_exists");options[target.id]=options[source];}
 std::set<std::string> seen;Json favorites=Json::array();for(auto favorite:next["favorites"]){if(favorite==source)favorite=target.id;if(seen.insert(favorite.get<std::string>()).second)favorites.push_back(favorite);}next["favorites"]=favorites;
 auto recent=recentAfterLaunch(Json::array(),target,volumes).at(0);for(auto &entry:next["recent_games"])if(entry.at("id")==source)entry=recent;
 if(next.value("focus",std::string())==source)next["focus"]=target.id;
 if(next.value("resume_game",std::string())==source)next["resume_game"]=target.id;
 next["revision"]=settings.data.value("revision",0LL)+1;settings.validateDocument(next);
 std::string pattern=(root/".relink-staging-XXXXXX").string();std::vector<char> temp(pattern.begin(),pattern.end());temp.push_back(0);if(!mkdtemp(temp.data()))throw BackupError("backup.io");fs::path staging(temp.data());
 try {
  Json changes=Json::array();if(progress){progress->phase=1;progress->done=0;progress->total=candidate.at("files").size();}
  for(const auto &file:candidate.at("files")) {
   auto name=file.at("path").get<std::string>(),leaf=fs::path(name).filename().string();if(leaf=="identity.json"||leaf=="identity.json.previous")continue;
   auto path=backupSafePath(root,name);if(fs::file_size(path)!=file.at("bytes"))throw BackupError("backup.changed");
   auto bytes=readBytes(path);if(hashBytes(bytes)!=file.at("sha256"))throw BackupError("backup.changed");
   if((leaf.find("sram.bin")==0||leaf.find("rtc.bin")==0)&&(bytes.empty()||bytes.size()>32*1024*1024))throw BackupError("backup.invalid");
   if(leaf.find(".state")!=std::string::npos)decodeState(bytes);
   if(leaf.find("statistics.json")==0){auto stats=Json::parse(bytes);GameStatistics::validateDocument(stats,source);stats["game_id"]=target.id;bytes=jsonBytes(stats);}
   stage(staging,"games/"+target.id+"/"+leaf,bytes,changes);if(progress)++progress->done;
  }
  stage(staging,"games/"+target.id+"/identity.json",jsonBytes(progressRecord(target,volumes,fingerprint)),changes);
  stage(staging,"config.previous.json",jsonBytes(settings.data),changes);stage(staging,"config.json",jsonBytes(next),changes);
  if(hasProgress(root,target.id)||progressFingerprint(target)!=fingerprint)throw BackupError("progress.target_exists");
  applyBackupFiles(root,staging,changes,progress,target.id);
 }catch(...){std::error_code error;fs::remove_all(staging,error);throw;}
}
}
