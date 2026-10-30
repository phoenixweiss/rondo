// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
// Linux syscall adapters: real transaction, private files, no production hooks.
#include "backup.hpp"
#include "progress.hpp"
#include <cassert>
#include <cerrno>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
using namespace rondo;
static std::string mode,base;static bool relocating=false;static bool armed=false,broken=false,commitSeen=false,partial=false;
extern "C" int __real_rename(const char *,const char *);
extern "C" int __wrap_rename(const char *from,const char *to) {
 std::string target=to;
 if(armed&&target==base+"/.restore-transaction/journal.json")commitSeen=true;
 if(armed&&target==(relocating?base+"/config.json":base+"/games/"+std::string(64,'a')+"/sram.bin")&&!broken) {
  if(mode=="kill")_exit(77);
  if(mode=="rename"||mode=="rollback-blocked"){broken=true;errno=EIO;return -1;}
 }
 if(armed&&broken&&mode=="rollback-blocked"&&target==base+"/config.json"){errno=EIO;return -1;}
 return __real_rename(from,to);
}
static std::string pathOf(int fd){char path[4096];auto name="/proc/self/fd/"+std::to_string(fd);auto n=readlink(name.c_str(),path,sizeof(path));return n>0?std::string(path,size_t(n)):std::string();}
extern "C" ssize_t __real_write(int,const void *,size_t);
extern "C" ssize_t __wrap_write(int fd,const void *bytes,size_t count) {
 if(armed&&mode=="enospc"&&(pathOf(fd).find(base+"/.restore-staging-")==0||pathOf(fd).find(base+"/.relink-staging-")==0)) {
  if(partial){errno=ENOSPC;return -1;}partial=true;return __real_write(fd,bytes,std::min(count,size_t(17)));
 }
 return __real_write(fd,bytes,count);
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
 if(armed&&mode=="commit-flush"&&commitSeen&&pathOf(fd)==base+"/.restore-transaction") {armed=false;errno=EIO;return -1;}
 return __real_fsync(fd);
}
int main(int argc,char **argv) {
 assert(argc==2);fs::path root=argv[1];const std::string id(64,'a');BackupContext ctx{"0.12.0",{}};
 Store source(root/"source");auto updated=source.data;updated["locale"]="ru";source.commit(updated);atomicWrite(source.root/"games"/id/"sram.bin",{1,2,3});auto copy=createBackup(source.root,ctx);
 for(const char *scenario:{"enospc","rename","kill","rollback-blocked","commit-flush"}) {
  mode=scenario;auto target=root/scenario;Store old(target);old.commit(old.data);atomicWrite(target/"games"/id/"sram.bin",{8,9});
  auto config=readBytes(target/"config.json"),memory=readBytes(target/"games"/id/"sram.bin");base=target.string();broken=partial=commitSeen=false;
  if(mode=="kill") {
   auto child=fork();assert(child>=0);if(child==0){armed=true;restoreBackup(target,copy,ctx);_exit(99);}
   int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==77);
   assert(readJson(target/"config.json")["locale"]=="ru"&&fs::exists(target/".restore-transaction"));
   Store recovered(target);assert(!recovered.warning.empty());
  } else {
   armed=true;bool failed=false;try{restoreBackup(target,copy,ctx);}catch(...){failed=true;}armed=false;assert(failed);
   if(mode=="rollback-blocked") {
    assert(fs::exists(target/".restore-transaction"));Store recovered(target);assert(!recovered.warning.empty());
   }
  }
  if(mode=="commit-flush") {
   // A complete new generation may be visible after the final barrier failed.
   assert(readJson(target/"config.json")["locale"]=="ru"&&readBytes(target/"games"/id/"sram.bin")==std::vector<uint8_t>({1,2,3}));
  } else assert(readBytes(target/"config.json")==config&&readBytes(target/"games"/id/"sram.bin")==memory);
  assert(!fs::exists(target/".restore-transaction"));
  std::cout<<"PASS: backup fault "<<scenario<<"\n";
 }
 relocating=true;
 for(const char *scenario:{"enospc","rename","kill","rollback-blocked","commit-flush"}) {
  mode=scenario;auto folder=root/("relink-"+mode);auto card=folder/"card";std::vector<Volume> volumes{{"TF1",card}};
  atomicWrite(card/"NES/Old/game.nes",{1,2,3});auto old=scan(volumes).at(0);auto state=folder/"state";Store original(state);
  auto config=original.data;config["favorites"]={old.id};original.commit(config);auto fingerprint=progressFingerprint(old);
  rememberProgress(state,old,volumes,fingerprint);atomicWrite(state/"games"/old.id/"sram.bin",{6,7});
  fs::create_directories(card/"NES/New");fs::rename(old.path,card/"NES/New/game.nes");auto library=scan(volumes);auto game=library.at(0);
  auto candidates=findProgress(state,game,library,fingerprint,hashFile(game.path),std::string(64,'a'));assert(candidates.size()==1);
  auto before=readBytes(state/"config.json");base=state.string();broken=partial=commitSeen=false;
  if(mode=="kill") {
   auto child=fork();assert(child>=0);if(child==0){armed=true;relocateProgress(state,candidates[0],game,volumes,library,fingerprint,hashFile(game.path),std::string(64,'a'));_exit(99);}
   int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==77);
   assert(hasProgress(state,game.id)&&fs::exists(state/".restore-transaction"));Store recovered(state);assert(!recovered.warning.empty());
  } else {
   armed=true;bool failed=false;try{relocateProgress(state,candidates[0],game,volumes,library,fingerprint,hashFile(game.path),std::string(64,'a'));}catch(...){failed=true;}armed=false;assert(failed);
   if(mode=="rollback-blocked"){assert(fs::exists(state/".restore-transaction"));Store recovered(state);assert(!recovered.warning.empty());}
  }
  if(mode=="commit-flush")assert(hasProgress(state,game.id)&&Store(state).data["favorites"]==Json::array({game.id}));
  else assert(!hasProgress(state,game.id)&&readBytes(state/"config.json")==before);
  assert(readBytes(state/"games"/old.id/"sram.bin")==std::vector<uint8_t>({6,7})&&!fs::exists(state/".restore-transaction"));
  std::cout<<"PASS: relocation fault "<<scenario<<"\n";
 }

}
