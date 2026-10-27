// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "game_statistics.hpp"
#include <limits>
namespace rondo {
Json GameStatistics::read(const fs::path &path,const std::string &id) {
  if(!fs::exists(path))return Json::object();
  if(fs::file_size(path)>16384)throw std::runtime_error("Statistics too large");
  auto data=readJson(path);
  if(!data.is_object()||data.value("schema",0)!=1||data.value("game_id",std::string())!=id)throw std::runtime_error("Statistics identity/schema mismatch");
  for(auto key:{"launches","played_ms","tracking_started_utc"})
    if(!data.contains(key)||!data[key].is_number_integer()||data[key]<0||data[key]>=std::numeric_limits<int64_t>::max())throw std::runtime_error("Invalid statistics value");
  return data;
}
GameStatistics::GameStatistics(const fs::path &file,const std::string &id,uint64_t now):path(file),last(now),nextCheckpoint(now+60000) {
  data=read(path,id);
  if(data.empty())data={{"schema",1},{"game_id",id},{"launches",0},{"played_ms",0},{"tracking_started_utc",time(nullptr)},{"historical_time_unknown",true}};
  if(data["launches"]>=std::numeric_limits<int64_t>::max()-1)throw std::runtime_error("Statistics counter exhausted");
  data["launches"]=data["launches"].get<int64_t>()+1;
  submit();
}
void GameStatistics::poll() {
  if(!job.valid()||job.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
  error=job.get();if(error.empty())persisted=writing;
}
void GameStatistics::submit() {
  auto file=path;auto snapshot=data;writing=generation;++writesStarted;
  job=std::async(std::launch::async,[file,snapshot] {
    try {auto text=snapshot.dump()+"\n";atomicWrite(file,{text.begin(),text.end()});return std::string();}
    catch(const std::exception &failure){return std::string(failure.what());}
  });
}
void GameStatistics::tick(uint64_t now,bool running) {
  if(active&&now>=last) {
    auto old=data["played_ms"].get<int64_t>();auto elapsed=std::min<uint64_t>(now-last,uint64_t(std::numeric_limits<int64_t>::max()-1-old));
    if(elapsed){data["played_ms"]=old+int64_t(elapsed);++generation;}
  }
  last=now;active=running;poll();
  if(now>=nextCheckpoint){nextCheckpoint=now+60000;if(!job.valid()&&generation>persisted)submit();}
}
bool GameStatistics::flush(uint64_t now,bool running) {
  tick(now,running);if(job.valid()){job.wait();poll();}
  if(generation>persisted){submit();job.wait();poll();}
  return error.empty();
}
GameStatistics::~GameStatistics(){try{flush(last,false);}catch(...) {}}
}
