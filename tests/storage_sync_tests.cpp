// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
// Linux linker adapters exercise the real writer without production fault hooks.
#include "model.hpp"
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using namespace rondo;
static std::string fault;
static bool armed=false,interrupted=false,partial=false;
static std::vector<std::string> calls;
static bool fail(const std::string &operation) {
  if(!armed)return false;
  calls.push_back(operation);
  if(fault==operation+"-eintr"&&!interrupted){interrupted=true;errno=EINTR;return true;}
  if(fault==operation){errno=EIO;return true;}
  return false;
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat info{};assert(!fstat(fd,&info));
  auto operation=S_ISDIR(info.st_mode)?"directory":"file";
  if(armed&&fault=="unsupported-directory"&&S_ISDIR(info.st_mode)){calls.push_back(operation);errno=EINVAL;return -1;}
  if(fail(operation))return -1;
  return __real_fsync(fd);
}
extern "C" int __real_syncfs(int);
extern "C" int __wrap_syncfs(int fd) {if(fail("filesystem"))return -1;return __real_syncfs(fd);}
extern "C" int __real_rename(const char *,const char *);
extern "C" int __wrap_rename(const char *from,const char *to) {if(fail("rename"))return -1;return __real_rename(from,to);}
extern "C" ssize_t __real_write(int,const void *,size_t);
extern "C" ssize_t __wrap_write(int fd,const void *bytes,size_t count) {
  if(armed&&fault=="partial") {
    if(partial){errno=ENOSPC;return -1;}
    partial=true;return __real_write(fd,bytes,std::min(count,size_t(17)));
  }
  if(fail("write"))return -1;
  return __real_write(fd,bytes,count);
}
int main(int argc,char **argv) {
  assert(argc==2);fs::path root=argv[1];fs::create_directories(root);
  auto target=root/"value";
  std::vector<uint8_t> previous(4096,0x12),next(8192,0x34);
  for(const char *scenario:{"none","write","partial","file","rename","directory","filesystem",
                           "write-eintr","file-eintr","directory-eintr","filesystem-eintr","unsupported-directory"}) {
    armed=false;atomicWrite(target,previous);
    fault=scenario;interrupted=false;partial=false;calls.clear();armed=true;
    bool failed=false;try {atomicWrite(target,next);}catch(...){failed=true;}armed=false;
    bool pre=fault=="write"||fault=="partial"||fault=="file"||fault=="rename";
    bool post=fault=="directory"||fault=="filesystem";
    assert(failed==(pre||post));
    assert(readBytes(target)==(pre?previous:next));
    for(const auto &file:fs::directory_iterator(root))assert(file.path().filename()=="value");
    if(fault=="none")assert((calls==std::vector<std::string>{"write","file","rename","directory","filesystem"}));
    if(fault=="unsupported-directory")assert(calls.back()=="filesystem");
    if(fault.find("eintr")!=std::string::npos)assert(interrupted);
    std::cout<<"PASS: storage fault "<<scenario<<"\n";
  }
}
