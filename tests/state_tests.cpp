// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "state.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace rondo;
int main(int argc,char **argv){
  assert(argc==2);fs::path path=fs::path(argv[1])/"roundtrip.state";
  StateFile s;s.meta={{"core","core"},{"rom","rom"},{"save_sequence",1},{"utc",1234},{"preview_width",2},{"preview_height",1}};
  s.pixels={0xff012345,0xffabcdef};s.payload.resize(0x440000);for(size_t i=0;i<s.payload.size();++i)s.payload[i]=uint8_t(i);
  writeState(path,s);assert(readState(path).payload==s.payload&&readState(path).pixels==s.pixels);
  assert(readState(path,StateRead::Metadata).payload.empty()&&readState(path,StateRead::Metadata).pixels.empty());
  auto original=readBytes(path);auto bad=[&](std::vector<uint8_t> bytes){atomicWrite(path,bytes);bool rejected=false;try{readState(path);}catch(...){rejected=true;}assert(rejected);};
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
  std::cout<<"PASS: bounded state container, endian-safe preview, corruption/truncation/oversize rejection and atomic failure.\n";
}
