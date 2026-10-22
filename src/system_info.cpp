// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "system_info.hpp"
#include "battery_log.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
namespace rondo {
static std::string textFile(const fs::path &p,size_t limit=65536) {
  std::ifstream f(p,std::ios::binary);if(!f)return {};
  std::string text(limit+1,'\0');f.read(text.data(),text.size());text.resize(size_t(f.gcount()));
  if(text.size()>limit || f.bad())return {};
  while(!text.empty() && (text.back()=='\n'||text.back()=='\r'||text.back()=='\0'))text.pop_back();
  return text;
}
static std::map<std::string,std::string> releaseFile(const fs::path &p) {
  std::map<std::string,std::string> out;std::istringstream in(textFile(p));std::string line;
  while(std::getline(in,line)) {auto n=line.find('=');if(n==std::string::npos)continue;auto value=line.substr(n+1);if(value.size()>=2&&value.front()=='"'&&value.back()=='"')value=value.substr(1,value.size()-2);out[line.substr(0,n)]=value;}
  return out;
}
static std::string unescapeMount(std::string s) {
 for(auto pair:{std::pair<const char *,const char *>{"\\040"," "},{"\\011","\t"},{"\\134","\\"}}) {size_t at=0;while((at=s.find(pair.first,at))!=s.npos){s.replace(at,4,pair.second);++at;}}return s;
}
Json systemInfoSnapshot(const fs::path &root,const fs::path &app,const Json &cores) {
 Json out={{"fields",Json::object()},{"cards",Json::array()},{"cores",Json::array()},{"sampled_at",time(nullptr)}};
 auto put=[&](const std::string &key,const Json &value,const std::string &source,const std::string &state="ready") {
  out["fields"][key]={{"value",value},{"source",source},{"state",state},{"sampled_at",out["sampled_at"]}};
 };
 auto os=releaseFile(root/"etc/os-release"),base=releaseFile(root/"etc/baseos-release");
 bool profile=os["ID"]=="baseos"&&base["BASEOS_TARGET"]=="rg34xxsp"&&base["BASEOS_PLATFORM"]=="h700";out["profile"]=profile;
 struct utsname u{};uname(&u);
 put("architecture",u.machine,"uname.machine");put("kernel",textFile(root/"proc/sys/kernel/osrelease"),"/proc/sys/kernel/osrelease");
 if(out["fields"]["kernel"]["value"]==""&&root=="/")put("kernel",u.release,"uname.release");
 if(profile) {
  const std::string source="ANBERNIC RG34XXSP · https://win.anbernic.com/product/589.html";
  put("model","ANBERNIC RG34XXSP",source);put("cpu","H700 · 4× Cortex-A53",source);put("cpu_max","1.5 GHz",source);
  put("panel","IPS · 3.4″ · 720×480",source);put("rated_capacity","3300 mAh",source);
 } else {auto model=textFile(root/"sys/firmware/devicetree/base/model",512);put("model",model,"device-tree/model",model.empty()?"unknown":"ready");}
 for(auto key:{"BASEOS_VERSION"})if(!base[key].empty())put("baseos",base[key],"/etc/baseos-release");
 auto mem=textFile(root/"proc/meminfo");std::istringstream min(mem);std::string line;
 while(std::getline(min,line)) {std::istringstream row(line);std::string key,unit;unsigned long long value;if(row>>key>>value>>unit && unit=="kB" && value<1ull<<50) {
  if(key=="MemTotal:")put("ram_total",value*1024,"/proc/meminfo MemTotal (OS memory, not rated RAM)");
  if(key=="MemAvailable:")put("ram_available",value*1024,"/proc/meminfo MemAvailable");}}
 auto uptime=textFile(root/"proc/uptime");try{double seconds=std::stod(uptime);if(std::isfinite(seconds)&&seconds>=0)put("uptime",static_cast<long long>(seconds),"/proc/uptime (OS uptime)");}catch(...){}
 auto mode=textFile(root/"sys/class/graphics/fb0/virtual_size",128);if(!mode.empty())put("mode",mode,"fb0/virtual_size (buffer geometry; panel refresh is not inferred)");
 Json info=Json::object();try{info=readJson(app/"build-info.json");}catch(...){}
 if(!info.is_object())info=Json::object();
 auto version=info.value("version",textFile(app/"VERSION",64));if(!version.empty())put("rondo",version,"installed build-info.json / VERSION");
 if(info.contains("source_id")&&info["source_id"].is_string())put("build",info["source_id"],"installed build-info.json source_id (not a release version)");
 const auto inputs=info.value("inputs",Json::object());
 if(cores.is_array())for(const auto &core:cores) {
  if(!core.is_object()||!core.contains("path")||!core["path"].is_string())continue;
  const auto path=fs::path(core["path"].get<std::string>());std::error_code error;if(!fs::is_regular_file(path,error))continue;
  auto record=fs::weakly_canonical(path.parent_path())==fs::weakly_canonical(app/"cores")?inputs.value(core.value("input",""),Json::object()):Json::object();std::string directory=record.value("directory","");
  std::string revision=directory.size()>=40?directory.substr(directory.size()-40):"";
  if(!std::all_of(revision.begin(),revision.end(),[](unsigned char c){return std::isxdigit(c);}))revision.clear();
  out["cores"].push_back({{"name",core.value("name",path.filename().string())},{"revision",revision},{"source","installed core + build-info.json inputs"}});
 }
 std::vector<std::vector<std::string>> mounts;std::istringstream mountFile(textFile(root/"proc/self/mounts"));
 while(std::getline(mountFile,line)){std::istringstream in(line);std::string dev,path,fs,flags;if(in>>dev>>path>>fs>>flags)mounts.push_back({unescapeMount(dev),unescapeMount(path),fs,flags});}
 for(int slot=0;slot<2;++slot) {
  std::string name="mmcblk"+std::to_string(slot),dev="/dev/"+name;
  Json card={{"slot",slot+1},{"state","unknown"},{"volumes",Json::array()},{"source","block metadata + /proc/self/mounts"}};
  auto size=textFile(root/"sys/class/block"/name/"size",64);bool present=!size.empty()||fs::exists(root/"sys/class/block"/name);
  if(profile)card["state"]=present?"unmounted":"absent";
  try{unsigned long long sectors=std::stoull(size);if(!size.empty()&&std::all_of(size.begin(),size.end(),[](unsigned char c){return std::isdigit(c);})&&sectors<(1ull<<54))card["physical_bytes"]=sectors*512;}catch(...){}
  for(const auto &m:mounts) {
   if(m[0]!=dev && !(m[0].rfind(dev+"p",0)==0))continue;
   // Slot matching is exact; mmcblk10 and arbitrary ROM folder names are not TF2.
   if(m[0]!=dev && (m[0].size()<=dev.size()+1 || !std::all_of(m[0].begin()+dev.size()+1,m[0].end(),[](unsigned char c){return std::isdigit(c);})))continue;
   Json volume={{"device",m[0]},{"mount",m[1]},{"filesystem",m[2]},{"readonly",(","+m[3]+",").find(",ro,")!=std::string::npos}};
   struct statvfs space{};auto path=root=="/"?fs::path(m[1]):root/fs::path(m[1]).relative_path();
   if(statvfs(path.c_str(),&space)==0) {
    volume["state"]="ready";volume["total"]=uint64_t(space.f_blocks)*space.f_frsize;volume["free"]=uint64_t(space.f_bavail)*space.f_frsize;
    volume["used"]=uint64_t(space.f_blocks-space.f_bfree)*space.f_frsize;
   }else volume["state"]="error";
   card["volumes"].push_back(volume);card["state"]="ready";
  }
  out["cards"].push_back(card);
 }
 // Only the two named console PMIC supplies; never enumerate Bluetooth/HID.
 auto battery=batterySnapshot(root/"sys",root/"proc");out["battery"]=battery;
 return out;
}
SystemInfoProcess::~SystemInfoProcess() {if(fd>=0)close(fd);if(pid>0){kill(pid,SIGKILL);waitpid(pid,nullptr,WNOHANG);}}
bool SystemInfoProcess::start(const std::vector<std::string> &command,uint64_t now,int timeout) {
 if(busy()||command.empty())return false;int pipefd[2];if(pipe(pipefd))return false;
 fcntl(pipefd[0],F_SETFD,FD_CLOEXEC);fcntl(pipefd[1],F_SETFD,FD_CLOEXEC);
 posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
 posix_spawn_file_actions_adddup2(&actions,pipefd[1],STDOUT_FILENO);posix_spawn_file_actions_addclose(&actions,pipefd[0]);posix_spawn_file_actions_addclose(&actions,pipefd[1]);
 posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
 std::vector<std::string> args=command;std::vector<char *> argv;for(auto &s:args)argv.push_back(s.data());argv.push_back(nullptr);
 pid_t child;int code=posix_spawn(&child,argv[0],&actions,nullptr,argv.data(),environ);posix_spawn_file_actions_destroy(&actions);close(pipefd[1]);
 if(code){close(pipefd[0]);error="start";return false;}
 fd=pipefd[0];fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK);pid=child;deadline=now+timeout;expired=false;buffer.clear();error.clear();result=Json();return true;
}
bool SystemInfoProcess::poll(uint64_t now) {
 if(pid<0)return false;char chunk[4096];ssize_t count;
 while((count=read(fd,chunk,sizeof(chunk)))>0) {buffer.append(chunk,size_t(count));if(buffer.size()>131072){expired=true;error="oversized";kill(pid,SIGKILL);break;}}
 if(!expired&&now>=deadline){expired=true;error="timeout";kill(pid,SIGKILL);}
 int status=0;auto done=waitpid(pid,&status,WNOHANG);if(done==0)return false;
 while(done>0 && (count=read(fd,chunk,sizeof(chunk)))>0)buffer.append(chunk,size_t(count));
 if(buffer.size()>131072){expired=true;error="oversized";}
 if(done<0){error="wait";}else if(!expired&&WIFEXITED(status)&&WEXITSTATUS(status)==0) {
  try{result=Json::parse(buffer);if(!result.is_object()||!result.contains("fields"))throw std::runtime_error("shape");}catch(...){error="invalid";}
 }else if(error.empty())error="failed";
 close(fd);fd=-1;pid=-1;return true;
}
}
