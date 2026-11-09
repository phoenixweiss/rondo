// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "storage.hpp"
#include "state.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <array>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <sys/stat.h>
#include <random>
#ifdef __linux__
#include <spawn.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif
namespace rondo {
namespace {
void safe(const fs::path &p) {
 if(!p.is_absolute()||p.lexically_normal()!=p||p.string().find('\0')!=std::string::npos)throw StorageError("storage.path");
 fs::path at;
 for(const auto &part:p){at/=part;std::error_code e;auto st=fs::symlink_status(at,e);
  if(e&&e!=std::errc::no_such_file_or_directory)throw StorageError("storage.io");
  if(fs::is_symlink(st))throw StorageError("storage.path");}
}
std::string unescape(const std::string &s) {
 std::string out;
 for(size_t i=0;i<s.size();++i){if(s[i]!='\\'){out+=s[i];continue;}
  if(i+3>=s.size())throw StorageError("storage.mounts");int n=0;
  for(int j=1;j<=3;++j){if(s[i+j]<'0'||s[i+j]>'7')throw StorageError("storage.mounts");n=n*8+s[i+j]-'0';}
  if(!n)throw StorageError("storage.mounts");out+=char(n);i+=3;}
 return out;
}
bool option(const std::string &s,const std::string &v){return (","+s+",").find(","+v+",")!=std::string::npos;}
void saveJson(const fs::path &p,const Json &j){auto s=j.dump(2)+"\n";atomicWrite(p,{s.begin(),s.end()});}
void copyFile(const fs::path &source,const fs::path &target,const std::function<void()> &check) {
 int in=open(source.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(in<0)throw StorageError("storage.io");
 int out=open(target.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
 if(out<0){close(in);throw StorageError(errno==ENOSPC?"storage.space":"storage.io");}
 try{char buf[65536];for(;;){check();ssize_t n=read(in,buf,sizeof(buf));if(n<0){if(errno==EINTR)continue;throw StorageError("storage.io");}if(!n)break;
   for(ssize_t done=0;done<n;){auto w=write(out,buf+done,size_t(n-done));if(w<0&&errno==EINTR)continue;
    if(w<=0)throw StorageError(errno==ENOSPC?"storage.space":"storage.io");done+=w;}}
  if(fsync(out))throw StorageError("storage.io");close(out);close(in);
 }catch(...){close(out);close(in);throw;}
}
Json inventory(const fs::path &root) {
 Json files=Json::array();safe(root);if(!fs::exists(root))return files;
 if(!fs::is_directory(root))throw StorageError("storage.path");
 std::vector<fs::path> paths;
 for(const auto &e:fs::recursive_directory_iterator(root)) {
  if(e.is_symlink())throw StorageError("storage.path");
  if(e.is_regular_file())paths.push_back(e.path());else if(!e.is_directory())throw StorageError("storage.path");
  if(paths.size()>100000)throw StorageError("storage.limit");
 }
 std::sort(paths.begin(),paths.end());
 for(const auto &p:paths)files.push_back({{"path",p.lexically_relative(root).generic_string()},{"bytes",fs::file_size(p)},{"sha256",hashFile(p)}});
 return files;
}
std::string groupFor(const std::string &category,const std::string &name) {
 fs::path path(name);auto first=path.begin();if(first==path.end())throw StorageError("storage.path");
 if(category=="saves"||category=="backups")return first->string();
 if(category=="screenshots"&&std::distance(path.begin(),path.end())>=3&&*first!="Interface"){auto second=std::next(first);return (fs::path(*first)/ *second).generic_string();}
 return name;
}
Json grouped(const Json &files,const std::string &category) {
 Json groups=Json::object();for(const auto &f:files){auto key=groupFor(category,f.at("path"));if(!groups.contains(key))groups[key]=Json::array();groups[key].push_back(f);}return groups;
}
Json saveIdentity(const fs::path &folder,const std::string &id) {
 if(id.size()!=64||id.find_first_not_of("0123456789abcdef")!=std::string::npos)return Json();
 auto path=folder/"identity.json";if(!fs::is_regular_file(path)||fs::file_size(path)>16384)return Json();
 try{auto j=readJson(path),body=j.at("payload");auto text=body.dump();
 if(j.at("sha256")!=hashBytes({text.begin(),text.end()})||body.at("schema")!=1||body.at("game_id")!=id||!body.at("system").is_string()||!body.at("fingerprint").is_string())return Json();
 if(body.contains("title")&&(!body.at("title").is_string()||body.at("title").get<std::string>().size()>4096))return Json();
 auto fingerprint=body.at("fingerprint").get<std::string>();if(fingerprint.size()!=64||fingerprint.find_first_not_of("0123456789abcdef")!=std::string::npos)return Json();return body;
 }catch(...){return Json();}
}
Json bundleMetadata(const fs::path &root,const std::string &key,const Json &files,bool saves) {
 uintmax_t bytes=0;for(const auto &f:files)bytes+=f.at("bytes").get<uintmax_t>();
 Json result{{"bytes",bytes},{"files",files.size()},{"states",0},{"latest_utc",0},{"cores",Json::array()},{"roms",Json::array()}};
 if(!saves)return result;
 auto identity=saveIdentity(root/key,key);if(!identity.is_null())result["identity"]=identity;
 std::set<std::string> cores,roms;long long utc=0;int states=0;
 for(const auto &f:files){auto leaf=fs::path(f.at("path").get<std::string>()).filename().string();if(leaf.size()<6||leaf.substr(leaf.size()-6)!=".state")continue;
  try{auto meta=readState(root/f.at("path").get<std::string>(),StateRead::Metadata).meta;if(identity.is_null())throw StorageError("storage.incompatible");auto rom=meta.at("rom").get<std::string>();if(rom.size()!=64||rom.find_first_not_of("0123456789abcdef")!=std::string::npos)throw StorageError("storage.incompatible");if(identity.at("system")!="PSX"){auto text=identity.at("system").get<std::string>()+"\n"+rom;if(hashBytes({text.begin(),text.end()})!=identity.at("fingerprint"))throw StorageError("storage.incompatible");}roms.insert(rom);auto core=meta.at("core").get<std::string>();if(core.size()!=64||core.find_first_not_of("0123456789abcdef")!=std::string::npos)throw StorageError("storage.incompatible");cores.insert(core);utc=std::max(utc,meta.value("utc",0LL));++states;}catch(...){result["invalid_state"]=true;}}
 result["cores"]=cores;result["roms"]=roms;result["states"]=states;result["latest_utc"]=utc;return result;
}
Json conflictList(const std::string &category,const fs::path &source,const fs::path &target,const Json &sourceFiles,const Json &targetFiles) {
 auto a=grouped(sourceFiles,category),b=grouped(targetFiles,category);Json result=Json::array();
 for(const auto &entry:a.items())if(b.contains(entry.key())&&b.at(entry.key())!=entry.value()) {
  auto left=bundleMetadata(source,entry.key(),entry.value(),category=="saves"),right=bundleMetadata(target,entry.key(),b.at(entry.key()),category=="saves");
  bool compatible=true;std::string title=entry.key();
  if(category=="saves") {
   compatible=left.contains("identity")&&right.contains("identity")&&!left.value("invalid_state",false)&&!right.value("invalid_state",false);
   if(compatible){auto x=left.at("identity"),y=right.at("identity");compatible=x.at("system")==y.at("system")&&x.at("fingerprint")==y.at("fingerprint")&&(left.at("cores").empty()||right.at("cores").empty()||(left.at("cores")==right.at("cores")&&left.at("roms")==right.at("roms")));title=x.value("title",std::string());}
   if(title==entry.key())title.clear();
  }
  result.push_back({{"key",entry.key()},{"title",title},{"compatible",compatible},{"source",left},{"target",right}});
 }
 return result;
}
Json selectedFiles(const Json &plan,const Json &choices) {
 auto category=plan.at("category").get<std::string>();auto a=grouped(plan.at("files"),category),b=grouped(plan.at("target_files"),category);
 std::set<std::string> conflicts;for(const auto &c:plan.at("conflicts")) {
  auto key=c.at("key").get<std::string>();conflicts.insert(key);
  if(!c.at("compatible").get<bool>())throw StorageError("storage.incompatible");
  if(!choices.contains(key)||(choices.at(key)!="source"&&choices.at(key)!="target"))throw StorageError("storage.choose_versions");
 }
 for(const auto &entry:choices.items())if(!conflicts.count(entry.key()))throw StorageError("storage.journal");
 Json files=Json::array();for(const auto &entry:b.items())if(!a.contains(entry.key())||(conflicts.count(entry.key())&&choices.at(entry.key())=="target"))for(const auto &f:entry.value())files.push_back(f);
 for(const auto &entry:a.items())if(!conflicts.count(entry.key())||choices.at(entry.key())=="source")for(const auto &f:entry.value())files.push_back(f);
 std::sort(files.begin(),files.end(),[](const Json &x,const Json &y){return x.at("path")<y.at("path");});return files;
}
void writeMerge(const fs::path &config,const Json &j) {auto body=j.dump();Json wrapped{{"payload",j},{"sha256",hashBytes({body.begin(),body.end()})}};if(wrapped.dump(2).size()+1>32*1024*1024)throw StorageError("storage.limit");saveJson(config/".storage-merge.json",wrapped);}
Json readMerge(const fs::path &config) {
 auto path=config/".storage-merge.json";safe(path);if(fs::file_size(path)>32*1024*1024)throw StorageError("storage.journal");
 try{auto wrapped=readJson(path),j=wrapped.at("payload");auto text=j.dump();if(wrapped.at("sha256")!=hashBytes({text.begin(),text.end()})||j.at("format")!=1)throw StorageError("storage.journal");return j;}catch(const Json::exception &){throw StorageError("storage.journal");}
}
void removeVerified(const fs::path &root,const Json &expected,const std::function<void()> &check) {
 auto actual=inventory(root);for(const auto &f:actual)if(std::find(expected.begin(),expected.end(),f)==expected.end())throw StorageError("storage.cleanup");
 for(const auto &f:actual){check();auto file=root/f.at("path").get<std::string>();safe(file);if(hashFile(file)!=f.at("sha256"))throw StorageError("storage.cleanup");fs::remove(file);syncDirectory(file.parent_path());}
 if(fs::exists(root)){std::vector<fs::path> dirs{root};for(const auto &e:fs::recursive_directory_iterator(root))if(e.is_directory())dirs.push_back(e.path());std::sort(dirs.rbegin(),dirs.rend());for(const auto &d:dirs){std::error_code error;fs::remove(d,error);if(error&&error!=std::errc::directory_not_empty)throw StorageError("storage.cleanup");}syncDirectory(root.parent_path());}
}
struct Lock {int fd=-1;explicit Lock(const fs::path &path) {
 fd=open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
 if(fd<0)throw StorageError("storage.io");
 if(flock(fd,LOCK_EX|LOCK_NB)){close(fd);fd=-1;throw StorageError("storage.busy");}
 }~Lock(){if(fd>=0){flock(fd,LOCK_UN);close(fd);}}};
}
std::string storageBytes(uintmax_t n,const std::string &locale) {
 bool ru=locale.compare(0,2,"ru")==0;static const char *en[]{"B","KB","MB","GB","TB"},*russian[]{"Б","КБ","МБ","ГБ","ТБ"};
 double value=n;int unit=0;while(value>=1000&&unit<4){value/=1000;++unit;}
 std::ostringstream out;out<<std::fixed<<std::setprecision(unit&&value<10?1:0)<<value;auto text=out.str();if(ru)std::replace(text.begin(),text.end(),'.',',');
 return text+" "+(ru?russian[unit]:en[unit]);
}
const std::map<std::string,std::string> &storageCategories(){static const std::map<std::string,std::string> c{
 {"saves","games"},{"screenshots","Screenshots"},{"bios","BIOS"},{"backups","Backups"},{"logs","Logs"},{"cache","Cache"}};return c;}
std::vector<CardMount> storageMounts(const std::string &text) {
 std::vector<CardMount> result;std::set<std::string> seen;std::istringstream lines(text);std::string line;
 while(std::getline(lines,line)){std::istringstream in(line);std::string dev,root,type,opts,unused;
  if(!(in>>dev>>root>>type>>opts))continue;
  auto slot=dev=="/dev/mmcblk0p7"?"TF1":dev=="/dev/mmcblk1p1"?"TF2":"";
  if(!*slot)continue;if(type!="vfat"&&type!="exfat")throw StorageError("storage.filesystem",slot);
  if(!seen.insert(slot).second)throw StorageError("storage.ambiguous",slot);
  fs::path path=unescape(root);if(!path.is_absolute()||path.lexically_normal()!=path)throw StorageError("storage.mounts");
  result.push_back({slot,dev,type,path,!option(opts,"rw")||option(opts,"ro")});
 }
 return result;
}
namespace {
std::string readCid(const fs::path &path) {
  std::ifstream input(path);
  std::string cid, extra;
  if (!(input >> cid) || input >> extra || cid.size() != 32 ||
      cid.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return {};
  std::transform(cid.begin(), cid.end(), cid.begin(), [](unsigned char c) {
    return char(std::tolower(c));
  });
  return cid;
}

std::string volumeSerial(const std::array<uint8_t, 512> &sector, const std::string &type) {
  if (sector[510] != 0x55 || sector[511] != 0xaa) return {};
  size_t offset;
  if (type == "exfat") {
    if (std::memcmp(sector.data()+3, "EXFAT   ", 8) ||
        !std::all_of(sector.begin()+11, sector.begin()+64, [](uint8_t b) { return b == 0; }) ||
        sector[108] < 9 || sector[108] > 12 || sector[105] != 1) return {};
    offset = 100;
  } else {
    // FAT12/16 and FAT32 place the extended BPB in different locations.
    const int sectorSize = sector[11] | (sector[12] << 8);
    const int clusterSize = sector[13];
    if ((sectorSize != 512 && sectorSize != 1024 && sectorSize != 2048 && sectorSize != 4096) ||
        !clusterSize || (clusterSize & (clusterSize-1)) ||
        !(sector[14] || sector[15]) || !sector[16]) return {};
    const bool fat32 = !(sector[22] || sector[23]);
    if (fat32) {
      if (sector[17] || sector[18] || !(sector[36] || sector[37] || sector[38] || sector[39])) return {};
    } else if (!(sector[17] || sector[18])) return {};
    offset = fat32 ? 67 : 39;
    if (sector[offset-1] != 0x28 && sector[offset-1] != 0x29) return {};
  }
  if (std::all_of(sector.begin()+offset, sector.begin()+offset+4, [](uint8_t b) { return b == 0; })) return {};
  constexpr char hex[] = "0123456789abcdef";
  std::string serial;
  for (size_t i = offset+4; i > offset; --i) {
    auto b = sector[i-1];
    serial += hex[b >> 4];
    serial += hex[b & 15];
  }
  return serial;
}

std::string readVolumeSerial(const CardMount &mount, const fs::path &dev, bool physical) {
  const auto node = dev / fs::path(mount.device).filename();
  struct stat mounted{}, device{}, after{};
  if (stat(mount.root.c_str(), &mounted) || !S_ISDIR(mounted.st_mode)) return {};
  const int fd = open(node.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) return {};
  std::array<uint8_t, 512> sector{};
  ssize_t size = -1;
  if (fstat(fd, &device) == 0 &&
      (physical ? S_ISBLK(device.st_mode) && mounted.st_dev == device.st_rdev : S_ISREG(device.st_mode))) {
    do { size = pread(fd, sector.data(), sector.size(), 0); } while (size < 0 && errno == EINTR);
  }
  close(fd);
  if (size != static_cast<ssize_t>(sector.size()) || stat(mount.root.c_str(), &after) ||
      mounted.st_dev != after.st_dev || mounted.st_ino != after.st_ino) return {};
  return volumeSerial(sector, mount.filesystem);
}

bool legacyMmcId(const Json &value) {
  if (!value.is_string()) return false;
  const auto &id = value.get_ref<const std::string &>();
  return id.size() == 40 && id.compare(0, 8, "mmc-cid:") == 0 &&
         id.find_first_not_of("0123456789abcdef", 8) == std::string::npos;
}
}

std::vector<StorageCard> physicalStorageCards(const fs::path &proc, const fs::path &sys, const fs::path &dev) {
  // Fixture probes use regular files. Mixed real and fixture roots are rejected.
  const bool physical = proc == "/proc" && sys == "/sys" && dev == "/dev";
  if (!physical && (proc == "/proc" || sys == "/sys" || dev == "/dev")) throw StorageError("storage.mounts");
  std::ifstream mounts(proc/"self/mounts");
  if (!mounts) throw StorageError("storage.mounts");
  std::string text((std::istreambuf_iterator<char>(mounts)), {});
  std::vector<StorageCard> cards;
  for (const auto &mount : storageMounts(text)) {
    auto block = mount.slot == "TF1" ? "mmcblk0" : "mmcblk1";
    const auto cidPath = sys/"class/block"/block/"device/cid";
    const auto cid = readCid(cidPath);
    std::string id;
    if (!cid.empty()) {
      const auto serial = readVolumeSerial(mount, dev, physical);
      if (!serial.empty() && readCid(cidPath) == cid) id = "mmc-cid:" + cid + ":" + mount.filesystem + ":" + serial;
    }
    cards.push_back({mount.slot, std::move(id), mount.root, mount.readOnly});
  }
  return cards;
}

void setFat32VolumeSerial(int fd,uint32_t serial) {
  std::array<uint8_t,512> boot{},backup{};
  if (!serial || pread(fd,boot.data(),boot.size(),0)!=ssize_t(boot.size()) ||
      boot[22] || boot[23] || volumeSerial(boot,"vfat").empty())
    throw StorageError("storage.format_verify");
  const int sectorSize=boot[11] | (boot[12]<<8);
  const int backupSector=boot[50] | (boot[51]<<8);
  const int reserved=boot[14] | (boot[15]<<8);
  const off_t backupOffset=off_t(sectorSize)*backupSector;
  if (!backupSector || backupSector>=reserved ||
      pread(fd,backup.data(),backup.size(),backupOffset)!=ssize_t(backup.size()) ||
      boot!=backup) throw StorageError("storage.format_verify");
  uint8_t bytes[]{uint8_t(serial),uint8_t(serial>>8),uint8_t(serial>>16),uint8_t(serial>>24)};
  if (pwrite(fd,bytes,sizeof bytes,backupOffset+67)!=ssize_t(sizeof bytes) ||
      pwrite(fd,bytes,sizeof bytes,67)!=ssize_t(sizeof bytes) || fsync(fd) ||
      pread(fd,boot.data(),boot.size(),0)!=ssize_t(boot.size()) ||
      pread(fd,backup.data(),backup.size(),backupOffset)!=ssize_t(backup.size()) ||
      boot!=backup || !std::equal(std::begin(bytes),std::end(bytes),boot.begin()+67))
    throw StorageError("storage.format_verify");
}

#ifdef __linux__
namespace {
constexpr const char *tf2Node="/dev/mmcblk1p1";
const fs::path tf2Disk="/sys/class/block/mmcblk1";
const fs::path tf2Partition=tf2Disk/"mmcblk1p1";
struct FormatFd {
  int fd;
  ~FormatFd(){if(fd>=0)close(fd);}
};
int deviceMounts(dev_t device) {
  std::ifstream in("/proc/self/mountinfo");
  std::string line;
  int count=0;
  const auto id=std::to_string(major(device))+":"+std::to_string(minor(device));
  while(std::getline(in,line)) {
    std::istringstream fields(line);
    std::string index,parent,number;
    if(!(fields>>index>>parent>>number))throw StorageError("storage.mounts");
    if(number==id)++count;
  }
  if(!in.eof())throw StorageError("storage.mounts");
  return count;
}
uintmax_t formatGeometry(dev_t device) {
  struct stat disk{},node{},tf1{};
  if(lstat("/dev/mmcblk1",&disk) || lstat(tf2Node,&node) || lstat("/dev/mmcblk0",&tf1) ||
     !S_ISBLK(disk.st_mode) || !S_ISBLK(node.st_mode) || !S_ISBLK(tf1.st_mode) ||
     node.st_rdev!=device || node.st_rdev==tf1.st_rdev || disk.st_rdev==tf1.st_rdev ||
     disk.st_rdev==device || deviceMounts(disk.st_rdev))
    throw StorageError("storage.format_target");
  for(const auto &entry:fs::directory_iterator(tf2Disk))
    if(entry.path()!=tf2Partition && fs::exists(entry.path()/"partition"))
      throw StorageError("storage.format_target");
  std::ifstream number(tf2Partition/"dev"), parentNumber(tf2Disk/"dev"), size(tf2Partition/"size");
  std::string partitionId,diskId;
  uintmax_t sectors=0;
  if(!(number>>partitionId) || partitionId!=std::to_string(major(device))+":"+std::to_string(minor(device)) ||
     !(parentNumber>>diskId) || diskId!=std::to_string(major(disk.st_rdev))+":"+std::to_string(minor(disk.st_rdev)) ||
     !(size>>sectors) || sectors<131072 || sectors>UINT32_MAX)
    throw StorageError("storage.format_target");
  return sectors*512;
}
void runFatFormatter(int fd) {
  // Keep the open partition pinned across the child call. BusyBox only takes
  // a pathname, so use this process's descriptor instead of reopening the MMC node.
  auto target="/proc/"+std::to_string(getpid())+"/fd/"+std::to_string(fd);
  char *args[]{const_cast<char *>("busybox"),const_cast<char *>("mkfs.vfat"),
               const_cast<char *>("-n"),const_cast<char *>("RONDO"),target.data(),nullptr};
  char *environment[]{nullptr};
  pid_t child;
  if(posix_spawn(&child,"/usr/bin/busybox",nullptr,nullptr,args,environment))
    throw StorageError("storage.format_write");
  int status=0;
  pid_t result;
  do {result=waitpid(child,&status,0);}while(result<0&&errno==EINTR);
  if(result!=child || !WIFEXITED(status) || WEXITSTATUS(status))
    throw StorageError("storage.format_write");
}
}
#endif

Tf2FormatTarget physicalTf2FormatTarget() {
#ifdef __linux__
  if(geteuid()!=0 || access("/usr/bin/busybox",X_OK))throw StorageError("storage.format_unavailable");
  for(const auto &card:physicalStorageCards())if(card.slot=="TF2") {
    if(card.readOnly)throw StorageError("storage.readonly","TF2");
    if(card.id.empty())throw StorageError("storage.identity","TF2");
    if(card.root!="/mnt/sdcard")throw StorageError("storage.format_target");
    struct stat mounted{};
    if(stat(card.root.c_str(),&mounted) || deviceMounts(mounted.st_dev)!=1)
      throw StorageError("storage.format_target");
    return {card.id,card.root,formatGeometry(mounted.st_dev),mounted.st_dev};
  }
  throw StorageError("storage.missing","TF2");
#else
  throw StorageError("storage.format_unavailable");
#endif
}

void formatPhysicalTf2(const Tf2FormatTarget &target,const fs::path &tf1,
                       const std::function<void(int)> &stage) {
#ifdef __linux__
  if(!(physicalTf2FormatTarget()==target) || tf1==target.root || fs::current_path()!=tf1)
    throw StorageError("storage.changed","TF2");
  const auto bridge=tf1/"System/Rondo/tools/tf2-launch.sh";
  safe(bridge);
  if(!fs::is_regular_file(bridge) || fs::file_size(bridge)>65536 ||
     !fs::is_regular_file(tf1/"System/launch_frontend.sh") ||
     !fs::is_regular_file(tf1/"System/Rondo/bin/rondo"))
    throw StorageError("storage.format_unavailable");
  const auto launcher=readBytes(bridge);
  if(launcher.empty())throw StorageError("storage.format_unavailable");
  const auto cid=target.id.substr(8,32);
  std::random_device random;
  uint32_t serial;
  std::string volumeId;
  do {
    serial=random();
    std::ostringstream out;out<<std::hex<<std::setw(8)<<std::setfill('0')<<serial;
    volumeId=out.str();
  } while(!serial || target.id.substr(target.id.size()-8)==volumeId);
  const auto newId="mmc-cid:"+cid+":vfat:"+volumeId;
  FormatFd partition{open(tf2Node,O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK)};
  FormatFd disk{open(tf2Disk.c_str(),O_DIRECTORY|O_RDONLY|O_CLOEXEC)};
  struct stat pinned{},pinnedDisk{};
  uint64_t size=0;
  if(partition.fd<0 || disk.fd<0 || fstat(partition.fd,&pinned) || !S_ISBLK(pinned.st_mode) ||
     uintmax_t(pinned.st_rdev)!=target.device || fstat(disk.fd,&pinnedDisk) ||
     ioctl(partition.fd,BLKGETSIZE64,&size) || size!=target.bytes)
    throw StorageError("storage.format_target");
  auto sameCard=[&] {
    struct stat current{};
    return !stat(tf2Disk.c_str(),&current) && current.st_ino==pinnedDisk.st_ino &&
      current.st_dev==pinnedDisk.st_dev && readCid(tf2Disk/"device/cid")==cid &&
      formatGeometry(pinned.st_rdev)==target.bytes;
  };
  if(!sameCard() || !(physicalTf2FormatTarget()==target))throw StorageError("storage.changed","TF2");
  stage(0);
  if(umount(target.root.c_str()))throw StorageError("storage.format_unmount");
  stage(1);
  if(!sameCard() || deviceMounts(pinned.st_rdev))throw StorageError("storage.changed","TF2");
  runFatFormatter(partition.fd);
  if(!sameCard() || deviceMounts(pinned.st_rdev))throw StorageError("storage.changed","TF2");
  stage(2);
  setFat32VolumeSerial(partition.fd,serial);
  if(!sameCard() || deviceMounts(pinned.st_rdev))throw StorageError("storage.changed","TF2");
  if(mount(tf2Node,target.root.c_str(),"vfat",MS_NOATIME,"utf8,shortname=mixed"))
    throw StorageError("storage.format_mount");
  const auto formatted=physicalTf2FormatTarget();
  if(formatted.id!=newId || formatted.bytes!=target.bytes || formatted.device!=target.device || !sameCard())
    throw StorageError("storage.format_verify");
  stage(3);
  FormatFd mounted{open(target.root.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC)};
  struct stat verified{};
  if(mounted.fd<0 || fstat(mounted.fd,&verified) || verified.st_dev!=pinned.st_rdev || !sameCard())
    throw StorageError("storage.format_verify");
  const fs::path root="/proc/self/fd/"+std::to_string(mounted.fd);
  fs::create_directory(root/"RondoGames");
  atomicWrite(root/"System/launch_frontend.sh",launcher);
  syncDirectory(root);
  if(readBytes(root/"System/launch_frontend.sh")!=launcher || !(physicalTf2FormatTarget()==formatted))
    throw StorageError("storage.format_verify");
#else
  (void)target;(void)tf1;(void)stage;
  throw StorageError("storage.format_unavailable");
#endif
}
StatePaths storagePaths(const std::shared_ptr<StorageLayout> &layout) {
 StatePaths paths(layout->configPath());
 paths.resolve=[layout](const std::string &category,bool writing){return category=="config"?layout->configPath(writing):layout->path(category,writing);};
 paths.identity=[layout]{layout->configPath();return layout->profile();};
 return paths;
}
StorageCard StorageLayout::card(const Json &selected,bool writing) const {
 const auto slot=selected.at("slot").get<std::string>();auto all=probe();std::set<std::string> slots,ids;
 for(const auto &c:all){if((c.slot!="TF1"&&c.slot!="TF2")||c.id.size()>128||c.id.find_first_of("\r\n")!=std::string::npos||
    !slots.insert(c.slot).second||(!c.id.empty()&&!ids.insert(c.id).second))throw StorageError("storage.ambiguous");
  safe(c.root);}
 auto found=std::find_if(all.begin(),all.end(),[&](const auto &c){return c.slot==slot;});
 if(found==all.end()||!fs::is_directory(found->root))throw StorageError("storage.missing",slot);
 if(found->id.empty())throw StorageError("storage.identity",slot);
 if(selected.contains("id")&&selected.at("id")!=found->id) {
  if(legacyMmcId(selected.at("id")))throw StorageError("storage.legacy_identity",slot);
  throw StorageError("storage.changed",slot);
 }
 for(const auto &c:all)if(c.slot!=slot) {
  auto a=found->root.lexically_relative(c.root),b=c.root.lexically_relative(found->root);
  if(found->root==c.root||(!a.empty()&&*a.begin()!="..")||(!b.empty()&&*b.begin()!=".."))throw StorageError("storage.ambiguous");
 }
 if(writing&&found->readOnly)throw StorageError("storage.readonly",slot);
 return *found;
}
StorageLayout::StorageLayout(fs::path p,StorageProbe cb):config(std::move(p)),probe(std::move(cb)) {
 safe(config);auto owner=card({{"slot","TF1"}},false);
 if(config!=owner.root/"RondoState")throw StorageError("storage.owner");
 auto file=config/"storage.json";safe(file);
 try {
 if(fs::exists(file)&&fs::file_size(file)>65536)throw StorageError("storage.profile");
 if(fs::exists(file))document=readJson(file);
 else document={{"format",1},{"revision",0},{"owner",owner.id},{"placements",Json::object()}};
 if(!document.is_object()||document.size()!=4||document.at("format")!=1||!document.at("revision").is_number_integer()||
    document.at("revision").get<int64_t>()<0||!document.at("owner").is_string()||!document.at("placements").is_object())throw StorageError("storage.profile");
 if(document.at("owner")!=owner.id) {
  if(legacyMmcId(document.at("owner")))throw StorageError("storage.legacy_identity","TF1");
  throw StorageError("storage.changed","TF1");
 }
 for(const auto &e:document.at("placements").items()) {
  if(!storageCategories().count(e.key())||!e.value().is_object()||e.value().size()!=2||!e.value().contains("slot")||!e.value().contains("id")||
    (e.value().at("slot")!="TF1"&&e.value().at("slot")!="TF2")||!e.value().at("id").is_string()||(e.value().at("id").get<std::string>().empty()||e.value().at("id").get<std::string>().size()>128))throw StorageError("storage.profile");
 }
 }catch(const Json::exception &){throw StorageError("storage.profile");}
}
fs::path StorageLayout::configPath(bool writing) const {
 auto owner=card({{"slot","TF1"},{"id",document.at("owner")}},writing);
 if(config!=owner.root/"RondoState")throw StorageError("storage.changed","TF1");
 safe(config);
 auto file=config/"storage.json";safe(file);
 if(fs::exists(file)) {if(fs::file_size(file)>65536||readJson(file)!=document)throw StorageError("storage.stale");}
 else if(document.at("revision")!=0)throw StorageError("storage.stale");
 return config;
}
fs::path StorageLayout::path(const std::string &category,bool writing) const {
 auto it=storageCategories().find(category);if(it==storageCategories().end())throw StorageError("storage.category");
 // The owner and the selected card are checked on every operation, not just boot.
 configPath();
 auto selected=document.at("placements").value(category,Json{{"slot","TF1"},{"id",document.at("owner")}});
 auto c=card(selected,writing);auto result=c.root/"RondoState"/it->second;safe(result);return result;
}
Json StorageLayout::destination(const std::string &slot) const {auto c=card({{"slot",slot}},false);return {{"slot",c.slot},{"id",c.id}};}
Json StorageLayout::pending() const {
 if(fs::exists(config/".storage-merge.json")){if(fs::exists(config/".storage-transfer.json"))throw StorageError("storage.journal");return readMerge(config);}
 auto file=config/".storage-transfer.json";safe(file);if(fs::exists(file)&&fs::file_size(file)>32*1024*1024)throw StorageError("storage.journal");return fs::exists(file)?readJson(file):Json();
}
void StorageLayout::formatTf2(const Tf2FormatTarget &target,
                              const std::function<void(const fs::path &)> &format) const {
  configPath(true);
  safe(config/".storage-transfer.lock");
  Lock lock(config/".storage-transfer.lock");
  configPath(true);
  if(!pending().is_null() || fs::exists(config/".restore-transaction"))
    throw StorageError("storage.unfinished");
  for(const auto &entry:document.at("placements").items())
    if(entry.value().at("slot")=="TF2")throw StorageError("storage.format_in_use","TF2");
  const auto current=card({{"slot","TF2"},{"id",target.id}},true);
  if(current.root!=target.root)throw StorageError("storage.changed","TF2");
  format(config.parent_path());
}
void StorageLayout::finishPending() {
 if(fs::exists(config/".storage-merge.json")){finishResolved();return;}
 configPath(true);
 safe(config/".storage-transfer.lock");Lock lock(config/".storage-transfer.lock");
 auto journal=pending();if(journal.is_null())return;
 if(!journal.is_object()||(journal.size()!=8&&journal.size()!=9)||journal.at("format")!=1)throw StorageError("storage.journal");
 auto category=journal.at("category").get<std::string>();
 if(!storageCategories().count(category))throw StorageError("storage.journal");
 const auto &before=journal.at("before"), &after=journal.at("after");
 if(before.at("owner")!=document.at("owner")||before.at("revision").get<int64_t>()==INT64_MAX)throw StorageError("storage.journal");
 auto expected=before;auto selected=after.at("placements").at(category);
 expected["revision"]=before.at("revision").get<int64_t>()+1;expected["placements"][category]=selected;
 if(expected!=after||(document!=before&&document!=after))throw StorageError("storage.stale");
 auto targetCard=card(selected,document==before);auto target=targetCard.root/"RondoState"/storageCategories().at(category);safe(target);
 if(target.string()!=journal.at("target")||!fs::is_directory(target)||inventory(target)!=journal.at("files"))throw StorageError("storage.unfinished");
 if(document==before) {
  auto source=path(category,false);if(source.string()!=journal.at("source")||inventory(source)!=journal.at("files"))throw StorageError("storage.changed");
  if(fs::exists(config/"storage.json")&&readJson(config/"storage.json")!=document)throw StorageError("storage.stale");
  saveJson(config/"storage.json",after);document=after;
 }
 if(journal.value("move",false)) {
  auto sourceSelection=before.at("placements").value(category,Json{{"slot","TF1"},{"id",before.at("owner")}});
  auto sourceCard=card(sourceSelection,true);auto source=sourceCard.root/"RondoState"/storageCategories().at(category);safe(source);
  if(source.string()!=journal.at("source")||source==target)throw StorageError("storage.journal");
  // A changed source is never removed. Recovery accepts only remaining verified originals.
  auto remaining=inventory(source);auto expectedFiles=journal.at("files");
  for(const auto &f:remaining)if(std::find(expectedFiles.begin(),expectedFiles.end(),f)==expectedFiles.end())throw StorageError("storage.cleanup");
  for(const auto &f:remaining){card(sourceSelection,true);card(selected,true);auto path=source/f.at("path").get<std::string>();safe(path);
   if(hashFile(path)!=f.at("sha256"))throw StorageError("storage.cleanup");fs::remove(path);syncDirectory(path.parent_path());}
  std::vector<fs::path> dirs;if(fs::exists(source)){for(const auto &e:fs::recursive_directory_iterator(source))if(e.is_directory())dirs.push_back(e.path());dirs.push_back(source);}
  std::sort(dirs.rbegin(),dirs.rend());for(const auto &dir:dirs){std::error_code error;fs::remove(dir,error);if(error&&error!=std::errc::directory_not_empty)throw StorageError("storage.cleanup");}
  syncDirectory(source.parent_path());
 }
 fs::remove(config/".storage-transfer.json");syncDirectory(config);
}
void StorageLayout::copyCategory(const std::string &category,const std::string &slot,bool quiesced,const StorageTransferHooks &hooks,bool move) {
 if(!quiesced)throw StorageError("storage.active");
 auto source=path(category,move);auto selection=destination(slot);auto targetCard=card(selection,true);
 auto leaf=storageCategories().at(category);auto target=targetCard.root/"RondoState"/leaf;safe(target);
 if(source==target)return;
 configPath(true);
 fs::create_directories(config);safe(config/".storage-transfer.lock");Lock lock(config/".storage-transfer.lock");
 if(!pending().is_null())throw StorageError("storage.unfinished");
 auto current=config/"storage.json";
 if(fs::exists(current)&&readJson(current)!=document)throw StorageError("storage.stale");
 if(fs::exists(target))throw StorageError("storage.conflict",slot);
 auto files=inventory(source);uintmax_t bytes=1048576;
 for(const auto &f:files){auto size=f.at("bytes").get<uintmax_t>();if(size>UINTMAX_MAX-bytes)throw StorageError("storage.limit");bytes+=size;}
 auto free=hooks.available?hooks.available(targetCard.root):fs::space(targetCard.root).available;
 if(bytes>free)throw StorageError("storage.space",slot);
 auto check=[&]{if(hooks.cancelled&&hooks.cancelled())throw StorageError("storage.cancelled");
  if(path(category,false)!=source)throw StorageError("storage.changed");card(selection,true);};
 check();fs::create_directories(target.parent_path());safe(target.parent_path());
 std::string pattern=(target.parent_path()/".storage-incoming-XXXXXX").string();std::vector<char> tmp(pattern.begin(),pattern.end());tmp.push_back(0);
 if(!mkdtemp(tmp.data()))throw StorageError("storage.io");fs::path staging(tmp.data());
 auto next=document;if(document.at("revision").get<int64_t>()==INT64_MAX)throw StorageError("storage.limit");
 next["revision"]=document.at("revision").get<int64_t>()+1;next["placements"][category]=selection;
 auto journal=Json{{"format",1},{"category",category},{"source",source.string()},{"target",target.string()},
  {"staging",staging.string()},{"before",document},{"after",next},{"files",files}};
 if(move)journal["move"]=true;
 saveJson(config/".storage-transfer.json",journal);
 uintmax_t done=0;if(hooks.progress)hooks.progress(0,bytes-1048576);
 for(const auto &f:files){check();auto rel=fs::path(f.at("path").get<std::string>());auto dest=staging/rel;
  fs::create_directories(dest.parent_path());safe(source/rel);safe(dest);
  if(hooks.copy)hooks.copy(source/rel,dest);else copyFile(source/rel,dest,check);
  safe(dest);if(!fs::is_regular_file(dest)||fs::file_size(dest)!=f.at("bytes")||hashFile(dest)!=f.at("sha256"))throw StorageError("storage.verify");
  done+=f.at("bytes").get<uintmax_t>();if(hooks.progress)hooks.progress(done,bytes-1048576);
 }
 check();if(inventory(source)!=files||inventory(staging)!=files)throw StorageError("storage.changed");
 std::vector<fs::path> dirs{staging};for(const auto &e:fs::recursive_directory_iterator(staging))if(e.is_directory())dirs.push_back(e.path());
 std::sort(dirs.rbegin(),dirs.rend());for(const auto &dir:dirs)syncDirectory(dir);
 if(hooks.checkpoint)hooks.checkpoint("copied");check();safe(target);
 if(fs::exists(target))throw StorageError("storage.conflict",slot);
 fs::rename(staging,target);syncDirectory(target.parent_path());
 if(hooks.checkpoint)hooks.checkpoint("published");check();
 if(fs::exists(current)&&readJson(current)!=document)throw StorageError("storage.stale");
 saveJson(current,next);document=next;
 if(hooks.checkpoint)hooks.checkpoint("committed");
 // Release the transfer lock before recovery performs the optional source cleanup.
 if(move){flock(lock.fd,LOCK_UN);finishPending();}
 else {fs::remove(config/".storage-transfer.json");syncDirectory(config);}
}
std::shared_ptr<StorageLayout> StorageLayout::reopen() const {
  auto reopened = std::make_shared<StorageLayout>(config, probe);
  if (reopened->document.at("owner") != document.at("owner")) throw StorageError("storage.changed", "TF1");
  return reopened;
}
Json StorageLayout::cards() const {
 Json result=Json::object();
 for(const auto *slot:{"TF1","TF2"})try{auto c=card({{"slot",slot}},false);auto space=fs::space(c.root);
  result[slot]={{"state","ready"},{"id",c.id},{"root",c.root.string()},{"readonly",c.readOnly},{"total",space.capacity},{"free",space.available}};
 }catch(const StorageError &e){result[slot]={{"state",e.code=="storage.missing"?"missing":"error"},{"error",e.code}};}
 return result;
}
Json StorageLayout::review(const std::string &category,const std::string &slot,const std::string &sourceSlot) const {
 configPath(true);
 auto source=sourceSlot.empty()?path(category):card(destination(sourceSlot),false).root/"RondoState"/storageCategories().at(category);safe(source);auto targetCard=card(destination(slot),true);auto target=targetCard.root/"RondoState"/storageCategories().at(category);safe(target);
 auto files=inventory(source);uintmax_t bytes=0;for(const auto &f:files)bytes+=f.at("bytes").get<uintmax_t>();
 auto selected=sourceSlot.empty()?document.at("placements").value(category,Json{{"slot","TF1"},{"id",document.at("owner")}}):destination(sourceSlot);
 auto from=card(selected,false);if(source==target)throw StorageError("storage.category");auto targetFiles=inventory(target);auto conflicts=conflictList(category,source,target,files,targetFiles);auto free=fs::space(targetCard.root).available;
 return {{"category",category},{"source",selected},{"target",destination(slot)},{"profile",document},
  {"bytes",bytes},{"files",files},{"target_files",targetFiles},{"conflicts",conflicts},{"intent",sourceSlot.empty()?"placement":"collect"},{"free",free},{"conflict",fs::exists(target)},{"readonly_source",from.readOnly}};
}
Json StorageLayout::usage(const std::function<bool()> &cancelled) const {
 auto result=cards();
 for(const auto *slot:{"TF1","TF2"}) {
  auto &out=result[slot];if(out.at("state")!="ready")continue;
  try {
   auto selected=destination(slot);auto c=card(selected,false);auto before=fs::space(c.root);Json sizes=Json::object(),present=Json::object();
   for(const auto *key:{"games","saves","screenshots","bios","backups","logs","cache","system"})sizes[key]=uintmax_t(0);
   std::set<std::pair<uintmax_t,uintmax_t>> inodes;size_t count=0;
   for(const auto &e:fs::recursive_directory_iterator(c.root)) {
    if(cancelled&&cancelled())throw StorageError("storage.cancelled");if(++count>1000000)throw StorageError("storage.limit");
    struct stat st{};if(lstat(e.path().c_str(),&st))throw StorageError("storage.io");
    if(!S_ISDIR(st.st_mode)&&st.st_nlink>1&&!inodes.insert({st.st_dev,st.st_ino}).second)continue;
    auto relative=e.path().lexically_relative(c.root).generic_string();std::string category;
    if(relative=="RondoGames"||relative.find("RondoGames/")==0)category="games";
    else if(relative=="System"||relative.find("System/")==0)category="system";
    else if(relative=="RondoState"||relative.find("RondoState/")==0) {
     category="system";for(const auto &entry:storageCategories()){auto prefix="RondoState/"+entry.second;if(relative==prefix||relative.find(prefix+"/")==0){category=entry.first;break;}}
    }
    if(!category.empty()&&S_ISREG(st.st_mode))present[category]=true;
    if(!category.empty())sizes[category]=sizes.at(category).get<uintmax_t>()+uintmax_t(st.st_blocks)*512;
   }
   card(selected,false);out["present"]=present;auto after=fs::space(c.root);uintmax_t sum=0;for(const auto &v:sizes)sum+=v.get<uintmax_t>();
   auto used=after.capacity-after.available;
   if(before.capacity!=after.capacity||before.available!=after.available||sum>used){out["usage_state"]="stale";continue;}
   sizes["other"]=used-sum;sizes["free"]=after.available;out["sizes"]=sizes;out["usage_state"]="ready";
   out["free"]=after.available;out["total"]=after.capacity;
  }catch(const StorageError &e){out["usage_state"]="error";out["error"]=e.code;}
   catch(...){out["usage_state"]="error";out["error"]="storage.io";}
 }
 return result;
}
Json StorageLayout::pendingStatus() const {
 if(fs::exists(config/".storage-merge.json"))return resolvedStatus();
 auto j=pending();if(j.is_null())return j;
 if(!j.is_object()||!j.contains("before")||!j.contains("after")||!j.contains("files")||j.at("format")!=1)throw StorageError("storage.journal");
 if(j.size()!=8&&j.size()!=9)throw StorageError("storage.journal");
 auto category=j.at("category").get<std::string>();if(!storageCategories().count(category))throw StorageError("storage.journal");
 auto before=j.at("before"),expected=before;
 if(before.at("owner")!=document.at("owner")||before.at("revision").get<int64_t>()==INT64_MAX)throw StorageError("storage.journal");
 expected["revision"]=before.at("revision").get<int64_t>()+1;expected["placements"][category]=j.at("after").at("placements").at(category);
 if(expected!=j.at("after"))throw StorageError("storage.journal");
 auto sourceSelection=before.at("placements").value(category,Json{{"slot","TF1"},{"id",before.at("owner")}});
 auto sourceCard=card(sourceSelection,false);auto source=sourceCard.root/"RondoState"/storageCategories().at(category);safe(source);
 if(source.string()!=j.at("source"))throw StorageError("storage.journal");
 auto selection=j.at("after").at("placements").at(category);
 auto c=card(selection,false);auto target=c.root/"RondoState"/storageCategories().at(category);safe(target);
 if(target.string()!=j.at("target")||(document!=j.at("before")&&document!=j.at("after")))throw StorageError("storage.journal");
 bool verified=fs::is_directory(target)&&inventory(target)==j.at("files");
 return {{"category",category},{"target",selection.at("slot")},{"verified",verified},{"committed",document==j.at("after")},{"move",j.value("move",false)}};
}
void StorageLayout::discardPending() {
 if(fs::exists(config/".storage-merge.json")){discardResolved();return;}
 configPath(true);Lock lock(config/".storage-transfer.lock");auto j=pending();if(j.is_null())return;
 auto status=pendingStatus();if(status.at("verified")==true||document!=j.at("before"))throw StorageError("storage.unfinished");
 auto c=card(j.at("after").at("placements").at(j.at("category").get<std::string>()),true);
 fs::path target=j.at("target").get<std::string>(),stage=j.at("staging").get<std::string>();safe(stage);
 if(target.parent_path()!=c.root/"RondoState"||stage.parent_path()!=target.parent_path()||(stage.filename().string().find(".storage-incoming-")!=0||stage.filename().string().size()!=std::string(".storage-incoming-").size()+6))throw StorageError("storage.journal");
 if(fs::exists(target))throw StorageError("storage.conflict");
 if(fs::exists(stage))fs::remove_all(stage);syncDirectory(stage.parent_path());
 fs::remove(config/".storage-transfer.json");syncDirectory(config);
}

void StorageLayout::validateResolved(const Json &j) const {
 if(!j.is_object()||j.size()!=18||j.at("format")!=1)throw StorageError("storage.journal");
 auto category=j.at("category").get<std::string>();if(!storageCategories().count(category)||!j.at("move").is_boolean()||!j.at("target_existed").is_boolean())throw StorageError("storage.journal");
 auto before=j.at("before"),after=j.at("after"),expected=before;
 if(before.at("owner")!=document.at("owner")||before.at("revision").get<int64_t>()==INT64_MAX)throw StorageError("storage.journal");
 if(j.at("intent")=="placement"){expected["revision"]=before.at("revision").get<int64_t>()+1;expected["placements"][category]=j.at("target");}
 else if(j.at("intent")!="collect")throw StorageError("storage.journal");
 if(expected!=after||(document!=before&&document!=after))throw StorageError("storage.stale");
 auto source=card(j.at("source"),false),target=card(j.at("target"),false);
 auto leaf=storageCategories().at(category);if(source.id==target.id||j.at("source_path")!=(source.root/"RondoState"/leaf).string()||j.at("target_path")!=(target.root/"RondoState"/leaf).string())throw StorageError("storage.journal");
 fs::path stage=j.at("stage").get<std::string>(),old=j.at("old").get<std::string>();safe(stage);safe(old);
 auto prefix=std::string(".storage-merged-");if(stage.parent_path()!=target.root/"RondoState"||stage.filename().string().find(prefix)!=0||stage.filename().string().size()!=prefix.size()+6||old.string()!=stage.string()+".old")throw StorageError("storage.journal");
 if(j.at("phase")!="prepared"&&j.at("phase")!="retired"&&j.at("phase")!="published"&&j.at("phase")!="committed")throw StorageError("storage.journal");
 for(const auto *key:{"source_files","target_files","merged_files","copied_source"}) {
  if(!j.at(key).is_array()||j.at(key).size()>100000)throw StorageError("storage.journal");std::set<std::string> seen;
  for(const auto &f:j.at(key)){auto name=f.at("path").get<std::string>();fs::path p(name);
   if(name.empty()||p.is_absolute()||p.lexically_normal()!=p||name.find('\0')!=std::string::npos||!seen.insert(name).second||!f.at("bytes").is_number_unsigned()||!f.at("sha256").is_string()||f.at("sha256").get<std::string>().size()!=64)throw StorageError("storage.journal");
   for(const auto &part:p)if(part==".."||part==".")throw StorageError("storage.journal");
  }
 }
 for(const auto &f:j.at("copied_source"))if(std::find(j.at("source_files").begin(),j.at("source_files").end(),f)==j.at("source_files").end()||std::find(j.at("merged_files").begin(),j.at("merged_files").end(),f)==j.at("merged_files").end())throw StorageError("storage.journal");
}
void StorageLayout::resolveCategory(const Json &plan,const Json &choices,bool move,bool quiesced,const StorageTransferHooks &hooks) {
 if(!quiesced)throw StorageError("storage.active");configPath(true);fs::create_directories(config);Lock lock(config/".storage-transfer.lock");if(!pending().is_null())throw StorageError("storage.unfinished");
 auto category=plan.at("category").get<std::string>(),slot=plan.at("target").at("slot").get<std::string>();
 auto current=review(category,slot,plan.at("intent")=="collect"?plan.at("source").at("slot").get<std::string>():std::string());
 for(const auto *key:{"category","source","target","profile","files","target_files","conflicts","intent"})if(current.at(key)!=plan.at(key))throw StorageError("storage.changed");
 auto from=card(plan.at("source"),move),to=card(plan.at("target"),true);auto leaf=storageCategories().at(category);auto source=from.root/"RondoState"/leaf,target=to.root/"RondoState"/leaf;
 auto merged=selectedFiles(plan,choices);if(merged.size()>100000)throw StorageError("storage.limit");uintmax_t bytes=1048576;for(const auto &f:merged){auto n=f.at("bytes").get<uintmax_t>();if(n>UINTMAX_MAX-bytes)throw StorageError("storage.limit");bytes+=n;}
 if((hooks.available?hooks.available(to.root):fs::space(to.root).available)<bytes)throw StorageError("storage.space");
 auto check=[&]{if(hooks.cancelled&&hooks.cancelled())throw StorageError("storage.cancelled");card(plan.at("source"),move);card(plan.at("target"),true);configPath(true);};check();
 fs::create_directories(target.parent_path());std::string pattern=(target.parent_path()/".storage-merged-XXXXXX").string();std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);if(!mkdtemp(name.data()))throw StorageError("storage.io");fs::path stage=name.data();
 Json after=document;if(plan.at("intent")=="placement"){after["revision"]=document.at("revision").get<int64_t>()+1;after["placements"][category]=plan.at("target");}
 Json copied=Json::array();for(const auto &f:merged)if(std::find(plan.at("files").begin(),plan.at("files").end(),f)!=plan.at("files").end()){auto key=groupFor(category,f.at("path"));if(!choices.contains(key)||choices.at(key)=="source")copied.push_back(f);}
 Json j={{"format",1},{"phase","prepared"},{"category",category},{"source",plan.at("source")},{"target",plan.at("target")},{"source_path",source.string()},{"target_path",target.string()},{"stage",stage.string()},{"old",stage.string()+".old"},{"before",document},{"after",after},{"source_files",plan.at("files")},{"target_files",plan.at("target_files")},{"merged_files",merged},{"copied_source",copied},{"move",move},{"target_existed",fs::exists(target)},{"intent",plan.at("intent")}};
 try{writeMerge(config,j);}catch(...){if(fs::is_empty(stage)){fs::remove(stage);syncDirectory(stage.parent_path());}throw;}uintmax_t done=0;if(hooks.progress)hooks.progress(0,bytes-1048576);
 for(const auto &f:merged){check();auto rel=f.at("path").get<std::string>();auto origin=std::find(copied.begin(),copied.end(),f)!=copied.end()?source:target;auto destination=stage/rel;safe(origin/rel);safe(destination);fs::create_directories(destination.parent_path());
  if(hooks.copy)hooks.copy(origin/rel,destination);else copyFile(origin/rel,destination,[&]{if(hooks.cancelled&&hooks.cancelled())throw StorageError("storage.cancelled");});
  if(fs::file_size(destination)!=f.at("bytes")||hashFile(destination)!=f.at("sha256"))throw StorageError("storage.verify");done+=f.at("bytes").get<uintmax_t>();if(hooks.progress)hooks.progress(done,bytes-1048576);
 }
 check();if(inventory(source)!=plan.at("files")||inventory(target)!=plan.at("target_files")||inventory(stage)!=merged)throw StorageError("storage.changed");
 std::vector<fs::path> dirs{stage};for(const auto &e:fs::recursive_directory_iterator(stage))if(e.is_directory())dirs.push_back(e.path());std::sort(dirs.rbegin(),dirs.rend());for(const auto &d:dirs)syncDirectory(d);
 if(hooks.checkpoint)hooks.checkpoint("copied");check();
 if(j.at("target_existed")==true){fs::rename(target,j.at("old").get<std::string>());syncDirectory(target.parent_path());}
 j["phase"]="retired";writeMerge(config,j);if(hooks.checkpoint)hooks.checkpoint("retired");
 // Publication and commit are not cancellable. A death here is recoverable from both trees.
 card(plan.at("source"),move);card(plan.at("target"),true);configPath(true);if(inventory(stage)!=merged)throw StorageError("storage.verify");
 fs::rename(stage,target);syncDirectory(target.parent_path());j["phase"]="published";writeMerge(config,j);if(hooks.checkpoint)hooks.checkpoint("published");
 card(plan.at("source"),move);card(plan.at("target"),true);if(inventory(target)!=merged)throw StorageError("storage.verify");if(fs::exists(config/"storage.json")&&readJson(config/"storage.json")!=document)throw StorageError("storage.stale");
 if(after!=document){saveJson(config/"storage.json",after);document=after;}
 j["phase"]="committed";writeMerge(config,j);if(hooks.checkpoint)hooks.checkpoint("committed");
 flock(lock.fd,LOCK_UN);finishResolved();
}
Json StorageLayout::resolvedStatus() const {
 auto j=readMerge(config);validateResolved(j);fs::path target=j.at("target_path").get<std::string>(),old=j.at("old").get<std::string>(),stage=j.at("stage").get<std::string>();
 bool committed=j.at("phase")=="committed"&&document==j.at("after");
 bool published=fs::is_directory(target)&&inventory(target)==j.at("merged_files")&&(committed||!j.at("target_existed").get<bool>()||fs::is_directory(old));
 bool retired=!fs::exists(target)&&fs::is_directory(stage)&&inventory(stage)==j.at("merged_files")&&(!j.at("target_existed").get<bool>()||(fs::is_directory(old)&&inventory(old)==j.at("target_files")));
 return {{"category",j.at("category")},{"target",j.at("target").at("slot")},{"verified",published||retired},{"committed",committed},{"move",j.at("move")},{"merge",true}};
}
void StorageLayout::finishResolved() {
 configPath(true);Lock lock(config/".storage-transfer.lock");auto j=readMerge(config);validateResolved(j);
 fs::path source=j.at("source_path").get<std::string>(),target=j.at("target_path").get<std::string>(),stage=j.at("stage").get<std::string>(),old=j.at("old").get<std::string>();
 auto check=[&]{card(j.at("source"),j.at("move"));card(j.at("target"),true);configPath(true);};check();
 bool committed=j.at("phase")=="committed"&&document==j.at("after");
 if(!committed){
  if(inventory(source)!=j.at("source_files"))throw StorageError("storage.changed");
  if(fs::exists(stage)) {
   if(inventory(stage)!=j.at("merged_files"))throw StorageError("storage.unfinished");
   if(fs::exists(target)){if(fs::exists(old)||!j.at("target_existed").get<bool>()||inventory(target)!=j.at("target_files"))throw StorageError("storage.changed");fs::rename(target,old);syncDirectory(target.parent_path());}
   if(j.at("target_existed")==true&&(!fs::is_directory(old)||inventory(old)!=j.at("target_files")))throw StorageError("storage.unfinished");
   fs::rename(stage,target);syncDirectory(target.parent_path());
  }
  if(!fs::is_directory(target)||inventory(target)!=j.at("merged_files"))throw StorageError("storage.unfinished");
  if(document!=j.at("after")){saveJson(config/"storage.json",j.at("after"));document=j.at("after");}
  j["phase"]="committed";writeMerge(config,j);
 }
 if(inventory(target)!=j.at("merged_files"))throw StorageError("storage.changed");
 if(j.at("move")==true){
  auto remaining=inventory(source);for(const auto &f:remaining)if(std::find(j.at("source_files").begin(),j.at("source_files").end(),f)==j.at("source_files").end())throw StorageError("storage.cleanup");
  for(const auto &f:j.at("copied_source")){check();auto file=source/f.at("path").get<std::string>();safe(file);if(fs::exists(file)){if(hashFile(file)!=f.at("sha256"))throw StorageError("storage.cleanup");fs::remove(file);syncDirectory(file.parent_path());}}
  std::vector<fs::path> dirs;if(fs::is_directory(source)){dirs.push_back(source);for(const auto &e:fs::recursive_directory_iterator(source))if(e.is_directory())dirs.push_back(e.path());}std::sort(dirs.rbegin(),dirs.rend());for(const auto &d:dirs){std::error_code e;fs::remove(d,e);if(e&&e!=std::errc::directory_not_empty)throw StorageError("storage.cleanup");if(!e)syncDirectory(d.parent_path());}
 }
 check();removeVerified(old,j.at("target_files"),check);fs::remove(config/".storage-merge.json");syncDirectory(config);
}
void StorageLayout::discardResolved() {
 configPath(true);Lock lock(config/".storage-transfer.lock");auto j=readMerge(config);validateResolved(j);
 if(j.at("phase")=="committed"||document!=j.at("before"))throw StorageError("storage.unfinished");
 card(j.at("target"),true);fs::path target=j.at("target_path").get<std::string>(),stage=j.at("stage").get<std::string>(),old=j.at("old").get<std::string>();
 if(fs::exists(old)) {
  if(inventory(old)!=j.at("target_files"))throw StorageError("storage.changed");
  if(fs::exists(target)){if(fs::exists(stage)||inventory(target)!=j.at("merged_files"))throw StorageError("storage.changed");fs::rename(target,stage);syncDirectory(target.parent_path());}
  fs::rename(old,target);syncDirectory(target.parent_path());
 }else if(j.at("target_existed")==false&&fs::exists(target)) {
  if(fs::exists(stage)||inventory(target)!=j.at("merged_files"))throw StorageError("storage.changed");fs::rename(target,stage);syncDirectory(target.parent_path());
 }
 if(fs::exists(stage))fs::remove_all(stage);syncDirectory(stage.parent_path());fs::remove(config/".storage-merge.json");syncDirectory(config);
}

}
