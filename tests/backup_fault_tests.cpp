// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
// Linux syscall adapters: real transaction, private files, no production hooks.
#include "backup.hpp"
#include "progress.hpp"
#include <cassert>
#include <cerrno>
#include <iostream>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace rondo;
struct PrivateData {
 fs::path path;bool owned=false;
 PrivateData(const fs::path &fallback,bool split):path(fallback) {
  if(split){char name[]="/dev/shm/rondo-faults-XXXXXX";assert(mkdtemp(name));path=name;owned=true;}
 }
 ~PrivateData(){if(owned){std::error_code error;fs::remove_all(path,error);}}
};
static std::string mode,base,gameRoot;static bool relocating=false;static bool armed=false,broken=false,commitSeen=false,partial=false;
extern "C" int __real_rename(const char *,const char *);
extern "C" int __wrap_rename(const char *from,const char *to) {
 std::string target=to;
 if(armed&&target==base+"/.restore-transaction/journal.json")commitSeen=true;
 if(armed&&target==(relocating?base+"/config.json":gameRoot+"/games/"+std::string(64,'a')+"/sram.bin")&&!broken) {
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
 for(bool split:{false,true})for(const char *scenario:{"enospc","rename","kill","rollback-blocked","commit-flush"}) {
  mode=scenario;auto target=root/(std::string(split?"split-":"")+scenario);PrivateData privateData(target,split);auto dataRoot=privateData.path;
  StatePaths paths(target);if(split){paths.resolve=[=](const std::string &c,bool){return c=="config"?target:c=="saves"?dataRoot/"games":target/c;};paths.identity=[] {return Json{{"owner","tf1"},{"saves","tf2"}};};}
  Store old(paths);old.commit(old.data);atomicWrite(dataRoot/"games"/id/"sram.bin",{8,9});
  if(split){struct stat a{},b{};assert(!stat(target.c_str(),&a)&&!stat(dataRoot.c_str(),&b)&&a.st_dev!=b.st_dev);}
  auto config=readBytes(target/"config.json"),memory=readBytes(dataRoot/"games"/id/"sram.bin");base=target.string();gameRoot=dataRoot.string();broken=partial=commitSeen=false;
  if(mode=="kill") {
   auto child=fork();assert(child>=0);if(child==0){armed=true;restoreBackup(paths,copy,ctx);_exit(99);}
   int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==77);
   assert(readJson(target/"config.json")["locale"]=="ru"&&fs::exists(target/".restore-transaction"));
   Store recovered(paths);assert(!recovered.warning.empty());
  } else {
   armed=true;bool failed=false;try{restoreBackup(paths,copy,ctx);}catch(...){failed=true;}armed=false;assert(failed);
   if(mode=="rollback-blocked") {
    assert(fs::exists(target/".restore-transaction"));Store recovered(paths);assert(!recovered.warning.empty());
   }
  }
  if(mode=="commit-flush") {
   // A complete new generation may be visible after the final barrier failed.
   assert(readJson(target/"config.json")["locale"]=="ru"&&readBytes(dataRoot/"games"/id/"sram.bin")==std::vector<uint8_t>({1,2,3}));
  } else assert(readBytes(target/"config.json")==config&&readBytes(dataRoot/"games"/id/"sram.bin")==memory);
  assert(!fs::exists(target/".restore-transaction"));
  std::cout<<"PASS: backup fault "<<scenario<<"\n";
 }
 relocating=true;
 for(bool split:{false,true})for(const char *scenario:{"enospc","rename","kill","rollback-blocked","commit-flush"}) {
  mode=scenario;auto folder=root/((split?"split-relink-":"relink-")+mode);auto card=folder/"card";std::vector<Volume> volumes{{"TF1",card}};
  atomicWrite(card/"NES/Old/game.nes",{1,2,3});auto old=scan(volumes).at(0);auto state=folder/"state";PrivateData privateData(state,split);auto dataRoot=privateData.path;StatePaths paths(state);
  if(split){paths.resolve=[=](const std::string &c,bool){return c=="config"?state:c=="saves"?dataRoot/"games":state/c;};paths.identity=[] {return Json{{"owner","tf1"},{"saves","tf2"}};};}
  Store original(paths);
  auto config=original.data;config["favorites"]={old.id};original.commit(config);auto fingerprint=progressFingerprint(old);
  rememberProgress(paths,old,volumes,fingerprint);atomicWrite(dataRoot/"games"/old.id/"sram.bin",{6,7});
  fs::create_directories(card/"NES/New");fs::rename(old.path,card/"NES/New/game.nes");auto library=scan(volumes);auto game=library.at(0);
  auto candidates=findProgress(paths,game,library,fingerprint,hashFile(game.path),std::string(64,'a'));assert(candidates.size()==1);
  auto before=readBytes(state/"config.json");base=state.string();gameRoot=dataRoot.string();broken=partial=commitSeen=false;
  if(mode=="kill") {
   auto child=fork();assert(child>=0);if(child==0){armed=true;relocateProgress(paths,candidates[0],game,volumes,library,fingerprint,hashFile(game.path),std::string(64,'a'));_exit(99);}
   int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==77);
   assert(hasProgress(paths,game.id)&&fs::exists(state/".restore-transaction"));Store recovered(paths);assert(!recovered.warning.empty());
  } else {
   armed=true;bool failed=false;try{relocateProgress(paths,candidates[0],game,volumes,library,fingerprint,hashFile(game.path),std::string(64,'a'));}catch(...){failed=true;}armed=false;assert(failed);
   if(mode=="rollback-blocked"){assert(fs::exists(state/".restore-transaction"));Store recovered(paths);assert(!recovered.warning.empty());}
  }
  if(mode=="commit-flush")assert(hasProgress(paths,game.id)&&Store(paths).data["favorites"]==Json::array({game.id}));
  else assert(!hasProgress(paths,game.id)&&readBytes(state/"config.json")==before);
  assert(readBytes(dataRoot/"games"/old.id/"sram.bin")==std::vector<uint8_t>({6,7})&&!fs::exists(state/".restore-transaction"));
  std::cout<<"PASS: relocation fault "<<scenario<<"\n";
 }

}
