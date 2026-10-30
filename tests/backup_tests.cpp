// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "backup.hpp"
#include "state.hpp"
#include "game_statistics.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace rondo;
static void document(const fs::path &p,const Json &data){auto text=data.dump();atomicWrite(p,{text.begin(),text.end()});}
static void manifest(const fs::path &folder,Json data){auto text=data.dump();atomicWrite(folder/"manifest.json",{text.begin(),text.end()});auto hash=hashBytes({text.begin(),text.end()});atomicWrite(folder/"manifest.sha256",{hash.begin(),hash.end()});}
static Json journal(const Json &body){auto text=body.dump();return {{"payload",body},{"sha256",hashBytes({text.begin(),text.end()})}};}
int main(int argc,char **argv) {
 assert(argc==2);fs::path root=argv[1];fs::create_directories(root);
 const std::string id(64,'a'),other(64,'b'),core(64,'c');BackupContext ctx{"0.12.0",{core}};
 ctx.games=Json::array({{{"id",id},{"title","Battle City"},{"system","NES"}},{{"id",other},{"title","Not included"},{"system","MD"}}});
 auto source=root/"source";Store store(source);auto data=store.data;data["locale"]="ru";store.commit(data);
 auto game=source/"games"/id;
 StateFile state;state.meta={{"core",core},{"rom",std::string(64,'d')},{"utc",12},{"save_sequence",1},{"preview_width",0},{"preview_height",0}};state.payload={1,2,3,4};writeState(game/"slot-0.state",state);
 atomicWrite(game/"sram.bin",{1,2,3});atomicWrite(game/"rtc.bin",{4,5});
 {GameStatistics stats(game/"statistics.json",id,0);assert(stats.flush(0));}
 atomicWrite(source/"Logs/private.log",{9});atomicWrite(source/"Screenshots/private.png",{8});atomicWrite(source/"BIOS/private.bin",{7});atomicWrite(game/"rondo-discs.m3u",{6});
 BackupProgress progress;auto copy=createBackup(source,ctx,&progress);auto meta=backupManifest(copy);
 assert(meta["format"]==1&&meta["files"].size()==5&&meta["total_bytes"]>0);
 assert(meta["games"].size()==1&&backupGames(copy)[0]["title"]=="Battle City");
 auto legacy=meta;legacy.erase("games");manifest(copy,legacy);
 assert(backupGames(copy)[0]["title"]=="");
 assert(backupGames(copy,{{id,"TF1","NES","Legacy title",{}}})[0]["title"]=="Legacy title");manifest(copy,meta);
 assert(!fs::exists(copy/"data/Logs")&&!fs::exists(copy/"data/BIOS"));
 auto target=root/"target";Store current(target);current.commit(current.data);
 atomicWrite(target/"games"/other/"sram.bin",{7});atomicWrite(target/"games"/id/"slot-2.state",{8});
 atomicWrite(target/"Screenshots/keep.png",{9});
 restoreBackup(target,copy,ctx,&progress);
 assert(Store(target).data["locale"]=="ru"&&readState(target/"games"/id/"slot-0.state").payload==state.payload);
 assert(!fs::exists(target/"games"/id/"slot-2.state"));
 assert(readBytes(target/"games"/other/"sram.bin")==std::vector<uint8_t>{7});
 assert(fs::exists(target/"Screenshots/keep.png")&&!fs::exists(target/".restore-transaction"));
 auto good=readBytes(target/"config.json");auto reject=[&](const BackupContext &inputs){bool failed=false;try{restoreBackup(target,copy,inputs);}catch(...){failed=true;}assert(failed&&readBytes(target/"config.json")==good);};
 std::string frozenHash;backupManifest(copy,&frozenHash);auto frozen=ctx;frozen.expectedManifest=frozenHash;
 auto changed=meta;changed["created_utc"]=changed["created_utc"].get<long long>()+1;manifest(copy,changed);reject(frozen);manifest(copy,meta);
 reject({"0.11.0",{core}});reject({"0.12.0",{"wrong"}});
 auto raw=readBytes(copy/"data"/"games"/id/"sram.bin");atomicWrite(copy/"data"/"games"/id/"sram.bin",{9});reject(ctx);atomicWrite(copy/"data"/"games"/id/"sram.bin",raw);
 for(const auto &name:{"../escape","/absolute","games/../config.json","games/a/sram.bin","Logs/private.log"}) {
  auto bad=meta;bad["files"][0]["path"]=name;manifest(copy,bad);reject(ctx);
 }
 auto bad=meta;bad["files"].push_back(bad["files"][0]);manifest(copy,bad);reject(ctx);manifest(copy,meta);
 atomicWrite(copy/"data/extra.bin",{0});reject(ctx);fs::remove(copy/"data/extra.bin");
 auto link=copy/"data"/"games"/id/"sram.bin";fs::remove(link);fs::create_symlink(target/"config.json",link);reject(ctx);fs::remove(link);atomicWrite(link,raw);
 // Validation and source copying happen before replacing the active generation.
 auto blocked=root/"blocked";fs::create_directories(blocked/"games"/id);Store blockedConfig(blocked);blockedConfig.commit(blockedConfig.data);
 auto before=readBytes(blocked/"config.json");fs::create_directory(blocked/"games"/id/"sram.bin");
 bool refused=false;try{restoreBackup(blocked,copy,ctx);}catch(...){refused=true;}assert(refused&&readBytes(blocked/"config.json")==before);
 // Simulate a durable prepared transaction interrupted after one active rename.
 auto interrupted=root/"interrupted";Store old(interrupted);old.commit(old.data);auto oldConfig=readBytes(interrupted/"config.json");
 auto tx=interrupted/".restore-transaction";atomicWrite(tx/"originals/config.json",oldConfig);
 atomicWrite(interrupted/"config.json",good);atomicWrite(interrupted/"games"/id/"sram.bin",{9});
 Json body={{"format",1},{"phase","prepared"},{"files",Json::array({{{"path","config.json"},{"existed",true},{"sha256",hashBytes(oldConfig)}},{{"path","games/"+id+"/sram.bin"},{"existed",false}}})}};
 document(tx/"journal.json",journal(body));Store recovered(interrupted);
 assert(!recovered.warning.empty()&&readBytes(interrupted/"config.json")==oldConfig&&!fs::exists(interrupted/"games"/id/"sram.bin")&&!fs::exists(tx));
 // A torn primary journal can recover from its separately flushed prepared copy.
 atomicWrite(tx/"originals/config.json",oldConfig);document(tx/"journal.previous.json",journal(body));
 atomicWrite(tx/"journal.json",{0});atomicWrite(interrupted/"config.json",good);
 Store tornJournal(interrupted);assert(readBytes(interrupted/"config.json")==oldConfig&&!fs::exists(tx));
 // A corrupt rollback image must stop startup before *any* active replacement.
 atomicWrite(tx/"originals/config.json",{0});document(tx/"journal.json",journal(body));
 refused=false;try{Store fail(interrupted);}catch(...){refused=true;}assert(refused&&readBytes(interrupted/"config.json")==oldConfig&&fs::exists(tx));
 atomicWrite(tx/"originals/config.json",oldConfig);body["phase"]="committed";document(tx/"journal.json",journal(body));
 assert(!recoverPendingRestore(interrupted)&&!fs::exists(tx));
 // Unrecognized and linked journals are not interpreted as instructions.
 fs::create_directories(tx);document(tx/"journal.json",{{"payload",body},{"sha256","wrong"}});
 refused=false;try{Store fail(interrupted);}catch(...){refused=true;}assert(refused&&readBytes(interrupted/"config.json")==oldConfig);
 fs::remove_all(tx);
 // Last valid state is exported if its active generation is damaged.
 atomicWrite(game/"slot-0.state.previous",readBytes(game/"slot-0.state"));atomicWrite(game/"slot-0.state",{0});
 auto recoveredCopy=createBackup(source,ctx);assert(readState(recoveredCopy/"data"/"games"/id/"slot-0.state").payload==state.payload);
 // Deletion is confined to one direct backup child; active state is untouched.
 auto active=readBytes(source/"config.json");bool deleteRefused=false;
 try{deleteBackup(source,source/"games");}catch(...){deleteRefused=true;}assert(deleteRefused);
 std::string oldDigest;backupManifest(recoveredCopy,&oldDigest);auto altered=backupManifest(recoveredCopy);altered["created_utc"]=0;manifest(recoveredCopy,altered);
 deleteRefused=false;try{deleteBackup(source,recoveredCopy,oldDigest);}catch(...){deleteRefused=true;}assert(deleteRefused&&fs::exists(recoveredCopy));
 auto linked=source/"Backups/Rondo-link";fs::create_directory_symlink(recoveredCopy,linked);
 deleteRefused=false;try{deleteBackup(source,linked);}catch(...){deleteRefused=true;}assert(deleteRefused&&fs::exists(recoveredCopy));fs::remove(linked);
 auto outside=root/"keep.bin";atomicWrite(outside,{1});fs::create_symlink(outside,recoveredCopy/"link.bin");
 std::string digest;backupManifest(recoveredCopy,&digest);deleteBackup(source,recoveredCopy,digest);
 assert(fs::exists(outside)&&!fs::exists(recoveredCopy)&&fs::exists(copy)&&readBytes(source/"config.json")==active);
 auto brokenCopy=source/"Backups/Rondo-broken";atomicWrite(brokenCopy/"manifest.json",{0});deleteBackup(source,brokenCopy);assert(!fs::exists(brokenCopy));
 std::cout<<"PASS: archived/legacy game names, isolated deletion, path/link/frozen-manifest guards; portable snapshot, checksum/version/core/path/symlink rejection, matching-game isolation, rollback journal recovery and damaged-journal refusal.\n";
}
