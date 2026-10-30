// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "state.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace rondo;

// Deterministic short writes on a private directory, without filling a host disk.
// Process death is not a simulated SD power cut: kernel caches remain alive.
static void interruptedWrites(const fs::path &root) {
  fs::create_directories(root);
  const auto target=root/"active.bin";
  std::vector<uint8_t> previous(4096,0x35),next(65536,0x72);
  atomicWrite(target,previous);
  auto pending=[&] {
    std::vector<fs::path> result;
    for(const auto &file:fs::directory_iterator(root))
      if(file.path().filename().string().find("active.bin.tmp.")==0)result.push_back(file.path());
    return result;
  };
  for(bool abrupt:{false,true}) {
    pid_t child=fork();assert(child>=0);
    if(child==0) {
      struct rlimit limit{1024,1024};
      if(setrlimit(RLIMIT_FSIZE,&limit))_exit(90);
      if(abrupt)signal(SIGXFSZ,[](int){_exit(84);});else signal(SIGXFSZ,SIG_IGN);
      try {atomicWrite(target,next);_exit(91);}catch(...){_exit(83);}
    }
    int status=0;while(waitpid(child,&status,0)<0&&errno==EINTR){}
    assert(WIFEXITED(status)&&WEXITSTATUS(status)==(abrupt?84:83));
    assert(readBytes(target)==previous);
    auto leftovers=pending();
    assert(leftovers.size()==(abrupt?1u:0u));
    if(abrupt) {assert(fs::file_size(leftovers[0])<=1024);fs::remove(leftovers[0]);}
  }
  atomicWrite(target,next);assert(readBytes(target)==next&&pending().empty());
  // Backup succeeds, replacement fails midway: reload the original config and
  // retain a valid recovery generation instead of acknowledging the new value.
  Store store(root/"config");store.commit(store.data);
  auto old=readBytes(root/"config/config.json");
  pid_t child=fork();assert(child>=0);
  if(child==0) {
    struct rlimit limit{rlim_t(old.size()+128),rlim_t(old.size()+128)};
    if(setrlimit(RLIMIT_FSIZE,&limit))_exit(90);
    signal(SIGXFSZ,SIG_IGN);
    auto changed=store.data;changed["private_test_padding"]=std::string(old.size()+1024,'x');
    try {store.commit(changed);_exit(91);}catch(...){_exit(83);}
  }
  int status=0;while(waitpid(child,&status,0)<0&&errno==EINTR){}
  assert(WIFEXITED(status)&&WEXITSTATUS(status)==83);
  Store recovered(root/"config");assert(recovered.warning.empty());
  assert(readBytes(root/"config/config.json")==old);
  assert(readBytes(root/"config/config.previous.json")==old);
  assert(!recovered.data.contains("private_test_padding"));
  for(const auto &file:fs::directory_iterator(root/"config"))
    assert(file.path().filename().string().find(".tmp.")==std::string::npos);
  bool rejected=false;try{syncDirectory(target);}catch(const fs::filesystem_error &){rejected=true;}assert(rejected);
  syncDirectory(root);
  std::cout<<"PASS: short-write cleanup, process interruption, old-generation preservation, failed config replacement and retry.\n";
}
int main(int argc,char **argv){
  assert(argc==2);fs::path path=fs::path(argv[1])/"roundtrip.state";
  StateFile s;s.meta={{"core","core"},{"rom","rom"},{"save_sequence",1},{"utc",1234},{"preview_width",2},{"preview_height",1}};
  s.pixels={0xff012345,0xffabcdef};s.payload.resize(0x440000);for(size_t i=0;i<s.payload.size();++i)s.payload[i]=uint8_t(i);
  writeState(path,s);assert(readState(path).payload==s.payload&&readState(path).pixels==s.pixels);
  assert(readState(path,StateRead::Metadata).payload.empty()&&readState(path,StateRead::Metadata).pixels.empty());
  auto original=readBytes(path);assert(decodeState(original).payload==s.payload);auto bad=[&](std::vector<uint8_t> bytes){atomicWrite(path,bytes);bool rejected=false;try{readState(path);}catch(...){rejected=true;}assert(rejected);};
  for(auto offset:{size_t(0),size_t(8),size_t(16),size_t(20),size_t(90),original.size()-1}){auto bytes=original;bytes[offset]^=0xff;bad(std::move(bytes));}
  for(auto length:{size_t(0),size_t(7),size_t(83),original.size()-1})bad({original.begin(),original.begin()+length});
  auto bytes=original;bytes.push_back(0);bad(bytes);
  // A damaged payload does not get read for a menu, but MUST fail before load.
  bytes=original;bytes.back()^=1;atomicWrite(path,bytes);
  assert(readState(path,StateRead::Preview).pixels==s.pixels);bad(bytes);
  atomicWrite(path,original);auto compact=s;compact.pixels.clear();compact.meta["preview_width"]=0;compact.meta["preview_height"]=0;
  writeState(path,compact);assert(readState(path).pixels.empty());
  // Failed writes must leave the prior active generation readable.
  auto target=path.parent_path()/"blocked";fs::create_directory(target);std::ofstream(target/"keep")<<"keep";
  bool failed=false;try{writeState(target,s);}catch(...){failed=true;}assert(failed&&readState(path).payload==s.payload);
  auto recover=path.parent_path()/"recover.state";
  writeState(recover,s);atomicWrite(recover.string()+".previous",readBytes(recover));
  fs::remove(recover);
  auto recovered=readRecoverableState(recover);assert(recovered.recovered&&recovered.payload==s.payload);
  atomicWrite(recover,{'b','a','d'});
  assert(readRecoverableState(recover,StateRead::Preview).recovered);
  auto damaged=readBytes(recover.string()+".previous");damaged.back()^=1;
  atomicWrite(recover,damaged);assert(readRecoverableState(recover).recovered);
  // A preview read remains independent of the heavy core payload.
  assert(!readRecoverableState(recover,StateRead::Preview).recovered);
  atomicWrite(recover.string()+".previous",{'b','a','d'});
  failed=false;try{readRecoverableState(recover);}catch(...){failed=true;}assert(failed);
  interruptedWrites(path.parent_path()/"interrupted-writes");
  std::cout<<"PASS: bounded state container, endian-safe preview, corruption/truncation/oversize rejection and atomic failure.\n";
}
