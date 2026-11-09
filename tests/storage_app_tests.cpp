// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include "state.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sys/wait.h>
#include <unistd.h>
using namespace rondo;
static void document(const fs::path &p,const Json &j){auto s=j.dump();atomicWrite(p,{s.begin(),s.end()});}
static void fails(const std::string &code,const std::function<void()> &fn){try{fn();assert(false);}catch(const StorageError &e){assert(e.code==code);}}
int main(int argc,char **argv){
 assert(argc==4);fs::path assets=fs::absolute(argv[1]),root=fs::absolute(argv[2]),core=fs::absolute(argv[3]);fs::create_directories(root);
 auto one=root/"one",two=root/"two";fs::create_directories(one/"RondoState");fs::create_directories(two);
 std::vector<StorageCard> cards{{"TF1","card-one",one,false},{"TF2","card-two",two,false}};
 auto probe=[&]{return cards;};auto layout=std::make_shared<StorageLayout>(one/"RondoState",probe);
 auto paths=storagePaths(layout);Store original(paths);original.commit(original.data);
 std::vector<uint8_t> rom(16+16384+8192,0);rom[0]='N';rom[1]='E';rom[2]='S';rom[3]=0x1a;rom[4]=1;rom[5]=1;rom[6]=2;
 rom[16]=0x4c;rom[17]=0;rom[18]=0x80;for(int n=16+16384-6;n<16+16384;n+=2)rom[n+1]=0x80;
 atomicWrite(one/"RondoGames/NES/Test/game.nes",rom);std::vector<Volume> volumes{{"TF1",one/"RondoGames"}};auto game=scan(volumes).at(0);
 const std::string fakeCore(64,'c');auto fingerprint=progressFingerprint(game);
 atomicWrite(original.gameDir(game)/"sram.bin",std::vector<uint8_t>(8192,7));rememberProgress(paths,game,volumes,fingerprint);
 StateFile state;state.meta={{"core",fakeCore},{"rom",hashFile(game.path)},{"utc",10},{"save_sequence",1},{"preview_width",0},{"preview_height",0}};state.payload={1,2};writeState(original.gameDir(game)/"slot-0.state",state);
 atomicWrite(paths.category("bios")/"PSX/user.bin",{2});
 for(const auto *category:{"saves","bios","backups","logs","cache"})layout->copyCategory(category,"TF2",true);
 auto pristine=readBytes(two/"RondoState/games"/game.id/"sram.bin");
 // Retained TF1 originals are deliberately different: they must not be selected.
 atomicWrite(one/"RondoState/games"/game.id/"sram.bin",{99});
 Store routed(paths);assert(routed.gameDir(game)==two/"RondoState/games"/game.id);assert(hasProgress(paths,game.id));
 BackupContext context{"0.14.1",{fakeCore}};auto backup=createBackup(paths,context);assert(backup.parent_path()==two/"RondoState/Backups");
 assert(readBytes(backup/"data/games"/game.id/"sram.bin")==pristine);
 atomicWrite(routed.gameDir(game)/"sram.bin",{4});auto next=routed.data;next["locale"]="ru";routed.commit(next);
 restoreBackup(paths,backup,context);assert(readBytes(routed.gameDir(game)/"sram.bin")==pristine);assert(Store(paths).data["locale"]=="en");
 assert(readBytes(one/"RondoState/games"/game.id/"sram.bin")==std::vector<uint8_t>{99});
 assert(backupFolders(paths).size()==1);deleteBackup(paths,backup);assert(backupFolders(paths).empty());
 // Relinking updates TF1 settings and TF2 progress in a single rollback transaction.
 auto moved=game.path.parent_path()/"renamed.nes";fs::rename(game.path,moved);auto library=scan(volumes);auto target=library.at(0);
 auto candidates=findProgress(paths,target,library,fingerprint,hashFile(moved),fakeCore);assert(candidates.size()==1);
 relocateProgress(paths,candidates[0],target,volumes,library,fingerprint,hashFile(moved),fakeCore);
 assert(hasProgress(paths,target.id)&&hasProgress(paths,game.id));assert(!fs::exists(one/"RondoState/games"/target.id));
 Options opts;opts.assets=assets;opts.state=one/"RondoState";opts.storage=layout;opts.volumes=volumes;opts.headless=true;opts.core=core;opts.output=root/"output";
 {App shell(opts);shell.takeScreenshot();for(int i=0;i<400&&shell.screenshotWriter.busy();++i){SDL_Delay(5);shell.screenshotTick();}
  assert(fs::is_directory(one/"RondoState/Screenshots/Interface"));assert(!fs::exists(two/"RondoState/Screenshots"));
  auto data=shell.store.data;data["debug_enabled"]=true;shell.persist(data);shell.configureDiagnostics();assert(shell.batteryLog);
 }
 assert(fs::is_directory(two/"RondoState/Logs"));
 // Real NES serialization, SRAM, load and automatic continuation use TF2.
 {App runner(opts,true,target);runner.batteryLog.reset();assert(runner.core);assert(runner.store.gameDir(target)==two/"RondoState/games"/target.id);
  runner.ticks(3);runner.core->save(0);auto hash=runner.core->stateHash();runner.ticks(2);runner.core->loadState(0);assert(runner.core->stateHash()==hash);
  runner.core->saveAutomatic();assert(runner.core->compatibleAutomatic());assert(fs::is_regular_file(two/"RondoState/games"/target.id/"autosave.state"));
  assert(fs::file_size(two/"RondoState/games"/target.id/"sram.bin")==8192);
  runner.startStatistics();runner.statisticsTick(true);runner.statistics.reset();assert(fs::is_regular_file(two/"RondoState/games"/target.id/"statistics.json"));
  auto present=cards.back();cards.pop_back();fails("storage.missing",[&]{runner.core->flushSram();});cards.push_back(present);}
 auto card=cards.back();cards.pop_back();
 {App shell(opts);assert(!shell.ui->artworkCache->statistics().is_null());shell.prepareGame(target);assert(!shell.launch&&!shell.notice.empty());
  shell.takeScreenshot();for(int i=0;i<400&&shell.screenshotWriter.busy();++i){SDL_Delay(5);shell.screenshotTick();}}
 fails("storage.missing",[&]{Store(paths).gameDir(target,true);});fails("storage.missing",[&]{createBackup(paths,context);});
 assert(!fs::exists(one/"RondoState/games"/target.id));cards.push_back(card);cards.back().readOnly=true;
 fails("storage.readonly",[&]{Store(paths).gameDir(target,true);});cards.back().readOnly=false;cards.back().id="replacement";
 fails("storage.changed",[&]{hasProgress(paths,target.id);});cards.back()=card;
 // An interrupted restore cannot roll back config while its game card is absent.
 auto transaction=one/"RondoState/.restore-transaction";auto config=readBytes(one/"RondoState/config.json");
 auto memory=readBytes(two/"RondoState/games"/target.id/"sram.bin");
 auto logical="games/"+target.id+"/sram.bin";
 atomicWrite(transaction/"originals/config.json",config);atomicWrite(transaction/"originals"/logical,memory);
 Json body={{"format",1},{"phase","prepared"},{"storage",paths.binding()},{"files",Json::array({
   {{"path","config.json"},{"existed",true},{"sha256",hashBytes(config)}},
   {{"path",logical},{"existed",true},{"sha256",hashBytes(memory)}}})}};
 auto text=body.dump();document(transaction/"journal.json",{{"payload",body},{"sha256",hashBytes({text.begin(),text.end()})}});
 auto child=fork();assert(child>=0);if(!child){auto changed=readJson(one/"RondoState/config.json");changed["locale"]="ru";document(one/"RondoState/config.json",changed);atomicWrite(two/"RondoState"/logical,{55});_exit(77);}
 int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==77);
 auto changedConfig=readBytes(one/"RondoState/config.json");cards.pop_back();
 fails("storage.missing",[&]{recoverPendingRestore(paths);});assert(readBytes(one/"RondoState/config.json")==changedConfig&&fs::exists(transaction));
 cards.push_back(card);assert(recoverPendingRestore(paths));assert(readBytes(one/"RondoState/config.json")==config&&readBytes(two/"RondoState"/logical)==memory);
 // New child rejects a stale profile instead of resolving a different destination.
 auto stale=opts;stale.storageBinding=layout->profile();layout->copyCategory("screenshots","TF2",true);
 fails("storage.changed",[&]{App runner(stale,true,target);});
 // Even the first statistics write is guarded in its background job.
 {auto file=root/"blocked-statistics/stats.json";GameStatistics stats(file,target.id,0,[]{throw StorageError("storage.missing");});assert(!stats.flush(1));assert(!fs::exists(file));}
 // File-backed boot sectors exercise the production probe without real devices.
 auto sys=root/"sys",proc=root/"proc",dev=root/"dev";
 auto first=root/"physical-one",second=root/"physical-two";
 fs::create_directories(first/"RondoState");
 fs::create_directories(second);
 const auto cid1=sys/"class/block/mmcblk0/device/cid",cid2=sys/"class/block/mmcblk1/device/cid";
 auto setCid=[](const fs::path &path,char c) { atomicWrite(path,std::vector<uint8_t>(32,c)); };
 setCid(cid1,'A');
 setCid(cid2,'b');
 auto mountCards=[&](const std::string &type,const std::string &options) {
   const auto mounts="/dev/mmcblk0p7 "+first.string()+" vfat rw 0 0\n/dev/mmcblk1p1 "+
       second.string()+" "+type+" "+options+" 0 0\n";
   atomicWrite(proc/"self/mounts",{mounts.begin(),mounts.end()});
 };
 auto sector=[](const std::string &type,uint8_t serial) {
   std::vector<uint8_t> boot(512);
   boot[510]=0x55;boot[511]=0xaa;
   size_t offset=100;
   if(type=="exfat") {
     const std::string magic="EXFAT   ";
     std::copy(magic.begin(),magic.end(),boot.begin()+3);
     boot[105]=1;boot[108]=9;
   } else {
     boot[12]=2;boot[13]=8;boot[14]=32;boot[16]=2;
     if(type=="fat32"){offset=67;boot[36]=4;}
     else {offset=39;boot[18]=2;boot[22]=4;}
     boot[offset-1]=0x29;
   }
   boot[offset]=serial;
   return boot;
 };
 const auto boot1=sector("fat32",0x12),boot2=sector("exfat",0x34);
 atomicWrite(dev/"mmcblk0p7",boot1);
 atomicWrite(dev/"mmcblk1p1",boot2);
 mountCards("exfat","ro");
 auto physicalProbe=[&]{return physicalStorageCards(proc,sys,dev);};
 const std::string hardware1="mmc-cid:"+std::string(32,'a');
 const std::string hardware2="mmc-cid:"+std::string(32,'b');
 auto physical=physicalProbe();
 assert(physical.size()==2 && physical[0].id==hardware1+":vfat:00000012");
 assert(physical[1].readOnly && physical[1].id==hardware2+":exfat:00000034");
 fails("storage.mounts",[&]{physicalStorageCards(proc,sys);});

 mountCards("exfat","rw");
 StorageLayout uncommitted(first/"RondoState",physicalProbe);
 atomicWrite(dev/"mmcblk0p7",sector("fat32",0x13));
 fails("storage.changed",[&]{uncommitted.reopen();});
 fails("storage.changed",[&]{uncommitted.configPath(true);});
 atomicWrite(dev/"mmcblk0p7",boot1);
 atomicWrite(uncommitted.path("saves")/"game/sram.bin",{7});
 uncommitted.copyCategory("saves","TF2",true);
 const auto profile=readBytes(first/"RondoState/storage.json");
 // A bit-for-bit cloned TF1 filesystem on a different card cannot own the profile.
 setCid(cid1,'c');
 fails("storage.changed",[&]{uncommitted.reopen();});
 assert(readBytes(first/"RondoState/storage.json")==profile);
 setCid(cid1,'a');
 // Same CID, same mountpoint and a new serial must block the old SRAM binding.
 fs::rename(second/"RondoState",root/"previous-tf2-data");
 atomicWrite(dev/"mmcblk1p1",sector("exfat",0x35));
 auto afterFormat=uncommitted.reopen();
 fails("storage.changed",[&]{afterFormat->path("saves",true);});
 assert(afterFormat->path("logs",true)==first/"RondoState/Logs");
 assert(!fs::exists(second/"RondoState") && readBytes(first/"RondoState/storage.json")==profile);
 // A corrupt or unreadable TF2 sector must not disable independent TF1 categories.
 atomicWrite(dev/"mmcblk1p1",std::vector<uint8_t>(512,0));
 fails("storage.identity",[&]{afterFormat->path("saves");});
 assert(afterFormat->path("logs",true)==first/"RondoState/Logs");
 assert(afterFormat->cards().at("TF2").at("error")=="storage.identity");
 fs::remove(cid2);
 fails("storage.identity",[&]{afterFormat->destination("TF2");});
 setCid(cid2,'b');

 // FAT16 and exFAT with the same serial are still different filesystem identities.
 mountCards("vfat","rw");
 atomicWrite(dev/"mmcblk1p1",sector("fat16",0x34));
 assert(physicalProbe()[1].id==hardware2+":vfat:00000034");
 fails("storage.changed",[&]{afterFormat->path("saves");});
 mountCards("exfat","rw");
 atomicWrite(dev/"mmcblk1p1",boot2);
 fs::rename(root/"previous-tf2-data",second/"RondoState");
 assert(readBytes(afterFormat->path("saves")/"game/sram.bin")==std::vector<uint8_t>{7});

 // An interrupted operation stays pinned to the original volume on recovery.
 atomicWrite(afterFormat->path("screenshots")/"Interface/test.png",{8});
 StorageTransferHooks interrupt;
 interrupt.checkpoint=[](const std::string &phase){if(phase=="published")throw StorageError("interrupted");};
 fails("interrupted",[&]{afterFormat->copyCategory("screenshots","TF2",true,interrupt);});
 atomicWrite(dev/"mmcblk1p1",sector("exfat",0x36));
 auto recovery=afterFormat->reopen();
 fails("storage.changed",[&]{recovery->finishPending();});
 assert(!recovery->pending().is_null() && readBytes(first/"RondoState/storage.json")==profile);
 atomicWrite(dev/"mmcblk1p1",boot2);
 recovery->finishPending();
 assert(recovery->path("screenshots")==second/"RondoState/Screenshots");
 auto legacy=recovery->profile();
 legacy["owner"]=hardware1;
 document(first/"RondoState/storage.json",legacy);
 const auto legacyBytes=readBytes(first/"RondoState/storage.json");
 fails("storage.legacy_identity",[&]{recovery->reopen();});
 assert(readBytes(first/"RondoState/storage.json")==legacyBytes);
 std::cout<<"PASS: split-card app/runner routing, screenshots, logs, backup/restore, relink, missing/replaced/read-only storage, child binding and physical CID + FAT/exFAT volume serial, reformat and clone guards. No media accessed.\n";
}
