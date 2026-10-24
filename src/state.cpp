// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "state.hpp"
#include <fstream>
#include <algorithm>
#include <stdexcept>
namespace rondo {
namespace {
constexpr uint8_t magic[]={'R','O','N','D','O','S','T',2};
constexpr size_t headerSize=84, maxMeta=16384, maxPayload=32*1024*1024;
void fail(){throw std::runtime_error("Invalid state container");}
void put32(std::vector<uint8_t> &out,uint32_t n){for(int i=0;i<4;++i)out.push_back(uint8_t(n>>(i*8)));}
uint32_t get32(const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
std::vector<uint8_t> take(std::ifstream &in,size_t n){std::vector<uint8_t>b(n);if(n&&!in.read(reinterpret_cast<char *>(b.data()),n))fail();return b;}
std::vector<uint8_t> pixelBytes(const std::vector<uint32_t> &pixels){std::vector<uint8_t>b;b.reserve(pixels.size()*4);for(auto p:pixels)put32(b,p);return b;}
}
std::string stateName(int slot){if(slot<0||slot>3)fail();return slot==3?"autosave.state":"slot-"+std::to_string(slot)+".state";}
std::vector<uint8_t> encodeState(const StateFile &s){
  if(s.payload.empty()||s.payload.size()>maxPayload)fail();
  Json meta=s.meta;meta["schema"]=2;meta["sha256"]=hashBytes(s.payload);
  auto preview=pixelBytes(s.pixels);
  int w=meta.value("preview_width",0),h=meta.value("preview_height",0);
  if(w<0||h<0||w>192||h>144||s.pixels.size()!=size_t(w)*h||(w==0)!=(h==0))fail();
  meta["preview_sha256"]=hashBytes(preview);
  auto text=meta.dump();if(text.size()>maxMeta)fail();
  std::vector<uint8_t> json(text.begin(),text.end());auto digest=hashBytes(json);
  std::vector<uint8_t> out;out.reserve(headerSize+json.size()+preview.size()+s.payload.size());
  out.insert(out.end(),std::begin(magic),std::end(magic));put32(out,json.size());put32(out,preview.size());put32(out,s.payload.size());
  out.insert(out.end(),digest.begin(),digest.end());out.insert(out.end(),json.begin(),json.end());
  out.insert(out.end(),preview.begin(),preview.end());out.insert(out.end(),s.payload.begin(),s.payload.end());return out;
}
StateFile readState(const fs::path &path,StateRead mode){
  std::ifstream in(path,std::ios::binary);if(!in)fail();auto header=take(in,headerSize);
  if(!std::equal(std::begin(magic),std::end(magic),header.begin()))fail();
  auto m=get32(header.data()+8),p=get32(header.data()+12),n=get32(header.data()+16);
  if(!m||m>maxMeta||p>192*144*4||!n||n>maxPayload||fs::file_size(path)!=headerSize+uint64_t(m)+p+n)fail();
  auto json=take(in,m);if(hashBytes(json)!=std::string(header.begin()+20,header.end()))fail();
  StateFile state;state.meta=Json::parse(json);
  auto &j=state.meta;
  if(j.at("schema")!=2||!j.at("core").is_string()||!j.at("rom").is_string()||
     !j.at("sha256").is_string()||j.at("sha256").get<std::string>().size()!=64||
     !j.at("utc").is_number_integer()||!saveSequence(j))fail();
  int w=j.at("preview_width"),h=j.at("preview_height");
  if(w<0||h<0||w>192||h>144||(w==0)!=(h==0)||uint64_t(w)*h*4!=p)fail();
  if(mode==StateRead::Metadata)return state;
  auto preview=take(in,p);if(hashBytes(preview)!=j.at("preview_sha256"))fail();
  state.pixels.reserve(p/4);for(size_t i=0;i<p;i+=4)state.pixels.push_back(get32(preview.data()+i));
  if(mode==StateRead::Preview)return state;
  state.payload=take(in,n);if(hashBytes(state.payload)!=j.at("sha256"))throw std::runtime_error("State checksum mismatch");
  return state;
}
void writeState(const fs::path &path,const StateFile &state){
  auto bytes=encodeState(state);atomicWrite(path,bytes);
  // One bounded binary readback, including preview and core payload checksums.
  auto actual=readState(path);
  if(actual.payload!=state.payload||actual.pixels!=state.pixels||saveSequence(actual.meta)!=saveSequence(state.meta))
    throw std::runtime_error("State readback mismatch");
  for(auto it=state.meta.begin();it!=state.meta.end();++it)
    if(it.key()!="schema"&&it.key()!="sha256"&&it.key()!="preview_sha256"&&actual.meta.at(it.key())!=it.value())
      throw std::runtime_error("State metadata readback mismatch");
}
}
