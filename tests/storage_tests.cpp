// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "storage.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
using namespace rondo;
template<class F>void fails(const std::string &code,F fn){bool caught=false;try{fn();}catch(const StorageError &e){caught=true;assert(e.code==code);}assert(caught);}
int main(int argc,char **argv) {
 assert(argc==2);fs::path work=fs::absolute(argv[1]);fs::create_directories(work);
 int test=0;
 auto run=[&](auto body){auto root=work/std::to_string(test++);fs::create_directories(root/"one/RondoState");fs::create_directories(root/"two");
  std::vector<StorageCard> cards{{"TF1","card-one",root/"one",false},{"TF2","card-two",root/"two",false}};
  StorageProbe probe=[&]{return cards;};StorageLayout layout(root/"one/RondoState",probe);body(layout,cards,root,probe);};
 auto seed=[](const fs::path &p,const std::string &text="saved"){atomicWrite(p,{text.begin(),text.end()});};
 run([&](auto &l,auto &,auto root,auto){for(const auto &e:storageCategories())assert(l.path(e.first)==root/"one/RondoState"/e.second);
  assert(!fs::exists(root/"one/RondoState/storage.json"));fails("storage.category",[&]{l.path("config");});});
 run([&](auto &l,auto &cards,auto root,auto) {
  Tf2FormatTarget target{cards[1].id,cards[1].root,128000000000,2};
  int called=0;
  auto format=[&](const fs::path &owner){assert(owner==root/"one");++called;};
  l.formatTf2(target,format);
  auto wrong=target;wrong.root=cards[0].root;
  fails("storage.changed",[&]{l.formatTf2(wrong,format);});
  cards[1].readOnly=true;fails("storage.readonly",[&]{l.formatTf2(target,format);});cards[1].readOnly=false;
  seed(l.configPath()/".restore-transaction/journal.json");
  fails("storage.unfinished",[&]{l.formatTf2(target,format);});
  fs::remove_all(l.configPath()/".restore-transaction");
  seed(l.path("saves")/"game/sram.bin");
  l.copyCategory("saves","TF2",true);
  fails("storage.format_in_use",[&]{l.formatTf2(target,format);});
  assert(called==1&&fs::exists(l.path("saves")/"game/sram.bin"));
 });
 run([&](auto &l,auto &cards,auto root,auto probe){seed(l.path("saves")/"abc/slot-0.state");seed(l.path("screenshots")/"Interface/test.png");
  l.copyCategory("saves","TF2",true);assert(l.path("saves")==root/"two/RondoState/games");assert(l.path("screenshots")==root/"one/RondoState/Screenshots");
  assert(readBytes(l.path("saves")/"abc/slot-0.state")==readBytes(root/"one/RondoState/games/abc/slot-0.state"));assert(l.pending().is_null());
  StorageLayout reload(root/"one/RondoState",probe);assert(reload.profile()==l.profile());
  cards.pop_back();fails("storage.missing",[&]{reload.path("saves",true);});assert(reload.path("screenshots")==root/"one/RondoState/Screenshots");
  cards.push_back({"TF2","another-card",root/"two",false});fails("storage.changed",[&]{reload.path("saves");});cards.back().id="card-two";cards.back().readOnly=true;
  assert(reload.path("saves")==root/"two/RondoState/games");fails("storage.readonly",[&]{reload.path("saves",true);});
 });
 for(const auto &entry:storageCategories())run([&](auto &l,auto &,auto root,auto probe){seed(l.path(entry.first)/"nested/data");l.copyCategory(entry.first,"TF2",true);
  StorageLayout reload(root/"one/RondoState",probe);assert(reload.path(entry.first)==root/"two/RondoState"/entry.second);});
 run([&](auto &l,auto &,auto root,auto){seed(l.path("saves")/"game/sram.bin");fs::create_directories(root/"two/RondoState/games");fails("storage.conflict",[&]{l.copyCategory("saves","TF2",true);});assert(l.profile()["revision"]==0);});
 run([&](auto &l,auto &,auto,auto){fails("storage.active",[&]{l.copyCategory("saves","TF2",false);});StorageTransferHooks h;h.available=[](auto){return 1;};fails("storage.space",[&]{l.copyCategory("saves","TF2",true,h);});assert(l.pending().is_null());});
 run([&](auto &l,auto &,auto root,auto){seed(l.path("saves")/"a");StorageTransferHooks h;h.cancelled=[] {return true;};fails("storage.cancelled",[&]{l.copyCategory("saves","TF2",true,h);});assert(!fs::exists(root/"two/RondoState"));});
 run([&](auto &l,auto &,auto root,auto){seed(l.path("saves")/"a");StorageTransferHooks h;h.copy=[&](auto,auto target){seed(target,"partial");throw StorageError("storage.space");};
  fails("storage.space",[&]{l.copyCategory("saves","TF2",true,h);});assert(l.path("saves")==root/"one/RondoState/games");assert(!l.pending().is_null());fails("storage.unfinished",[&]{l.finishPending();});});
 for(auto phase:{"copied","published","committed"})run([&](auto &l,auto &,auto root,auto probe){seed(l.path("saves")/"a");StorageTransferHooks h;h.checkpoint=[&](auto name){if(name==phase)throw StorageError("interrupted");};
  fails("interrupted",[&]{l.copyCategory("saves","TF2",true,h);});StorageLayout reload(root/"one/RondoState",probe);
  if(std::string(phase)=="copied"){fails("storage.unfinished",[&]{reload.finishPending();});assert(reload.path("saves")==root/"one/RondoState/games");}
  else {reload.finishPending();assert(reload.path("saves")==root/"two/RondoState/games");assert(reload.pending().is_null());}
  assert(readBytes(root/"one/RondoState/games/a")==std::vector<uint8_t>({'s','a','v','e','d'}));
 });
 run([&](auto &l,auto &cards,auto root,auto){seed(l.path("saves")/"a");StorageTransferHooks h;h.copy=[&](auto a,auto b){fs::copy_file(a,b);cards.back().id="replacement";};
  fails("storage.changed",[&]{l.copyCategory("saves","TF2",true,h);});assert(!fs::exists(root/"two/RondoState/games"));assert(l.profile()["revision"]==0);});
 run([&](auto &l,auto &,auto,auto){seed(l.path("saves")/"a");StorageTransferHooks h;h.copy=[&](auto a,auto b){fs::copy_file(a,b);seed(a,"changed");};
  fails("storage.changed",[&]{l.copyCategory("saves","TF2",true,h);});assert(l.profile()["revision"]==0);});
 run([&](auto &l,auto &,auto root,auto){fs::create_directory_symlink(root/"two",root/"one/RondoState/games");fails("storage.path",[&]{l.path("saves");});});
 run([&](auto &l,auto &,auto root,auto){fs::create_directory_symlink(root/"one",root/"two/RondoState");fails("storage.path",[&]{l.copyCategory("saves","TF2",true);});});
 run([&](auto &l,auto &,auto root,auto probe){auto profile=l.profile();profile["placements"]["unknown"]={{"slot","TF2"},{"id","card-two"}};auto text=profile.dump();seed(root/"one/RondoState/storage.json",text);fails("storage.profile",[&]{StorageLayout other(root/"one/RondoState",probe);});});
 run([&](auto &l,auto &cards,auto,auto){cards.back().root=cards.front().root;fails("storage.ambiguous",[&]{l.path("saves");});});
 run([&](auto &l,auto &,auto root,auto probe){auto p=l.profile();p["revision"]="bad";seed(root/"one/RondoState/storage.json",p.dump());fails("storage.profile",[&]{StorageLayout bad(root/"one/RondoState",probe);});});
 run([&](auto &,auto &,auto root,auto probe){seed(root/"one/RondoState/storage.json","{");fails("storage.profile",[&]{StorageLayout bad(root/"one/RondoState",probe);});});
 run([&](auto &l,auto &,auto root,auto probe){seed(l.path("saves")/"a");StorageLayout stale(root/"one/RondoState",probe);l.copyCategory("saves","TF2",true);
  fails("storage.stale",[&]{stale.copyCategory("screenshots","TF2",true);});assert(!fs::exists(root/"two/RondoState/Screenshots"));});
 run([&](auto &l,auto &,auto root,auto probe){seed(l.path("saves")/"a");StorageTransferHooks h;h.checkpoint=[&](auto name){if(name=="published")throw StorageError("interrupted");};
  fails("interrupted",[&]{l.copyCategory("saves","TF2",true,h);});seed(root/"two/RondoState/games/a","corrupt");StorageLayout reload(root/"one/RondoState",probe);
  fails("storage.unfinished",[&]{reload.finishPending();});assert(reload.path("saves")==root/"one/RondoState/games");});
 // A real process death releases the lock and leaves an inspectable journal.
 for(auto phase:{"copied","published","committed"})run([&](auto &l,auto &,auto root,auto probe){seed(l.path("saves")/"a");
  auto child=fork();assert(child>=0);if(!child){StorageTransferHooks h;h.checkpoint=[&](auto name){if(name==phase)_exit(44);};l.copyCategory("saves","TF2",true,h);_exit(2);}
  int status=0;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==44);
  StorageLayout reload(root/"one/RondoState",probe);
  if(std::string(phase)=="copied")fails("storage.unfinished",[&]{reload.finishPending();});
  else {reload.finishPending();assert(reload.path("saves")==root/"two/RondoState/games");}
  assert(fs::exists(root/"one/RondoState/games/a"));
 });
 // Move removes only verified originals after the profile commit.
 run([&](auto &l,auto &,auto root,auto){seed(l.path("saves")/"a");l.copyCategory("saves","TF2",true,{},true);assert(!fs::exists(root/"one/RondoState/games/a"));assert(fs::exists(l.path("saves")/"a")&&l.pending().is_null());});
 run([&](auto &l,auto &cards,auto root,auto){seed(l.path("saves")/"a");cards.front().readOnly=true;fails("storage.readonly",[&]{l.copyCategory("saves","TF2",true,{},true);});assert(!fs::exists(root/"two/RondoState"));});
 run([&](auto &l,auto &,auto root,auto){seed(l.path("saves")/"a");StorageTransferHooks h;h.checkpoint=[&](auto phase){if(phase=="committed"){seed(root/"one/RondoState/games/a","changed");throw StorageError("interrupted");}};
  fails("interrupted",[&]{l.copyCategory("saves","TF2",true,h,true);});auto reload=l.reopen();fails("storage.cleanup",[&]{reload->finishPending();});assert(fs::exists(root/"one/RondoState/games/a")&&fs::exists(root/"two/RondoState/games/a"));});
 run([&](auto &l,auto &,auto root,auto){seed(l.path("saves")/"a");StorageTransferHooks h;h.checkpoint=[](auto phase){if(phase=="committed")throw StorageError("interrupted");};
  fails("interrupted",[&]{l.copyCategory("saves","TF2",true,h,true);});auto reload=l.reopen();assert(reload->pendingStatus().at("verified")==true);reload->finishPending();assert(!fs::exists(root/"one/RondoState/games/a"));});
 run([&](auto &l,auto &,auto root,auto){seed(l.path("saves")/"a");StorageTransferHooks h;h.checkpoint=[](auto phase){if(phase=="copied")throw StorageError("interrupted");};
  fails("interrupted",[&]{l.copyCategory("saves","TF2",true,h);});assert(l.pendingStatus().at("verified")==false);l.discardPending();assert(l.pending().is_null()&&fs::exists(root/"one/RondoState/games/a"));});
 run([&](auto &l,auto &,auto,auto){seed(l.path("saves")/"a");StorageTransferHooks h;h.checkpoint=[](auto phase){if(phase=="published")throw StorageError("interrupted");};
  fails("interrupted",[&]{l.copyCategory("saves","TF2",true,h);});fails("storage.unfinished",[&]{l.discardPending();});l.finishPending();assert(fs::exists(l.path("saves")/"a"));});
 run([&](auto &l,auto &cards,auto root,auto){seed(l.path("saves")/"a");StorageTransferHooks h;h.checkpoint=[&](auto phase){if(phase=="committed")cards.back().readOnly=true;};
  fails("storage.readonly",[&]{l.copyCategory("saves","TF2",true,h,true);});assert(fs::exists(root/"one/RondoState/games/a"));cards.back().readOnly=false;l.finishPending();assert(!fs::exists(root/"one/RondoState/games/a"));});
 auto mounts=storageMounts("/dev/mmcblk1p1 /mnt/card\\040two exfat ro,noatime 0 0\n/dev/mmcblk0p7 /mnt/system vfat rw,utf8 0 0\n/dev/sda1 /mnt/ignore ext4 rw 0 0\n");
 assert(mounts.size()==2&&mounts[0].slot=="TF2"&&mounts[0].root=="/mnt/card two"&&mounts[0].readOnly&&!mounts[1].readOnly);
 fails("storage.ambiguous",[]{storageMounts("/dev/mmcblk1p1 /a vfat rw\n/dev/mmcblk1p1 /b vfat rw\n");});
 fails("storage.filesystem",[]{storageMounts("/dev/mmcblk1p1 /a ext4 rw\n");});
 fails("storage.mounts",[]{storageMounts("/dev/mmcblk1p1 /a\\000b exfat rw\n");});
 std::cout<<"PASS: "<<test<<" isolated storage scenarios; all six categories, pinned cards, missing/read-only/replaced cards, copy/readback, conflicts, interruption/recovery, cancellation, ENOSPC injection, copy retention and explicit verified move cleanup. No media accessed.\n";
}
