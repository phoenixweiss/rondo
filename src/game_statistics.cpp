// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "game_statistics.hpp"
#include <limits>
namespace rondo {
struct StatisticsIdentityError : std::runtime_error { using std::runtime_error::runtime_error; };
void GameStatistics::validateDocument(const Json &data,const std::string &id) {
  if (!data.is_object() || !data.contains("schema") || !data["schema"].is_number_integer() ||
      !data.contains("game_id") || !data["game_id"].is_string())
    throw std::runtime_error("Invalid statistics document");
  if (data["schema"] != 1 || data["game_id"] != id)
    throw StatisticsIdentityError("Statistics identity/schema mismatch");
  for(auto key:{"launches","played_ms","tracking_started_utc"})
    if(!data.contains(key)||!data[key].is_number_integer()||data[key]<0||data[key]>=std::numeric_limits<int64_t>::max())throw std::runtime_error("Invalid statistics value");
}
static Json readStatistics(const fs::path &path,const std::string &id) {
  if(fs::file_size(path)>16384)throw std::runtime_error("Statistics too large");
  auto data=readJson(path);GameStatistics::validateDocument(data,id);return data;
}
Json GameStatistics::readExact(const fs::path &path,const std::string &id) {return readStatistics(path,id);}
Json GameStatistics::read(const fs::path &path,const std::string &id) {
  auto previous = fs::path(path.string()+".previous");
  if (!fs::exists(path) && !fs::exists(previous)) return Json::object();
  try { return readStatistics(path,id); }
  catch (const StatisticsIdentityError &) { throw; }
  catch (...) { return readStatistics(previous,id); }
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
    try {
      bool valid=false;
      try { readStatistics(file,snapshot.at("game_id")); valid=true; }
      catch (const StatisticsIdentityError &) { throw; } catch (...) {}
      if(valid)atomicWrite(file.string()+".previous",readBytes(file));
      auto text=snapshot.dump()+"\n";atomicWrite(file,{text.begin(),text.end()});return std::string();
    }
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
