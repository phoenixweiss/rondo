// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "psx.hpp"
#include <fstream>
#include <algorithm>
namespace rondo {
std::vector<fs::path> psxDiscs(const fs::path &path) {
 if(searchFold(path.extension().string())==".chd")return {fs::canonical(path)};
 if(searchFold(path.extension().string())!=".m3u")throw std::runtime_error("PlayStation accepts CHD discs or CHD-only M3U playlists");
 if(fs::file_size(path)>65536)throw std::runtime_error("Oversized PSX playlist");
 std::ifstream f(path);if(!f)throw std::runtime_error("Cannot read PSX playlist");std::vector<fs::path> out;std::string line;auto root=fs::canonical(path.parent_path());
 while(std::getline(f,line)) {
  if(out.empty()&&line.rfind("\xef\xbb\xbf",0)==0)line.erase(0,3);
  while(!line.empty()&&(line.back()=='\r'||line.back()==' '||line.back()=='\t'))line.pop_back();
  auto at=line.find_first_not_of(" \t");if(at==line.npos||line[at]=='#')continue;line.erase(0,at);
  fs::path relative(line);if(relative.is_absolute()||searchFold(relative.extension().string())!=".chd")throw std::runtime_error("Playlist must contain relative CHD paths only");
  auto candidate=root/relative;if(fs::is_symlink(candidate)||!fs::is_regular_file(candidate))throw std::runtime_error("Playlist disc missing or symbolic link");
  auto disc=fs::canonical(candidate),rel=disc.lexically_relative(root);
  if(rel.empty()||*rel.begin()=="..")throw std::runtime_error("Playlist disc escapes its directory");
  if(std::find(out.begin(),out.end(),disc)!=out.end())throw std::runtime_error("Duplicate disc in playlist");
  if(disc.string().find_first_of("\r\n")!=std::string::npos)throw std::runtime_error("Invalid disc filename");
  out.push_back(disc);if(out.size()>8)throw std::runtime_error("Too many playlist discs");
 }
 if(f.bad()||out.empty())throw std::runtime_error("Empty or unreadable PSX playlist");return out;
}
std::string psxIdentity(const std::vector<fs::path> &images) {
 Json ids=Json::array();for(const auto &p:images) {
  std::ifstream f(p,std::ios::binary);std::vector<uint8_t> header(16);f.read(reinterpret_cast<char *>(header.data()),16);
  if(f.gcount()!=16||std::string(header.begin(),header.begin()+8)!="MComprHD")throw std::runtime_error("Invalid CHD header");
  auto be=[&](int n){return (uint32_t(header[n])<<24)|(uint32_t(header[n+1])<<16)|(uint32_t(header[n+2])<<8)|header[n+3];};auto length=be(8),version=be(12);
  if(version<3||version>5||length<76||length>512||fs::file_size(p)<length)throw std::runtime_error("Unsupported or truncated CHD");
  header.resize(length);f.read(reinterpret_cast<char *>(header.data()+16),length-16);if(f.gcount()!=length-16)throw std::runtime_error("Unreadable CHD header");
  // CHD headers carry logical disc SHA-1. No multi-gigabyte image allocation or
  // full SD reread at every launch. Full file integrity is checked by the kit.
  ids.push_back({{"header",hashBytes(header)},{"bytes",fs::file_size(p)}});
 }
 return ids.dump();
}
bool psxBiosFilename(const std::string &name) {
 return !name.empty()&&name.size()<=255&&name!="."&&name!=".."&&
  std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127||c=='/'||c==92;});
}
static bool validBios(const fs::path &p) {
 if(fs::is_symlink(p)||!fs::is_regular_file(p)||fs::file_size(p)!=512*1024)return false;
 auto name=searchFold(p.filename().string());
 if(name!="psxonpsp660.bin" && !(name.rfind("scph",0)==0&&searchFold(p.extension().string())==".bin"))return false;
 auto bytes=readBytes(p);if(bytes.size()!=512*1024)return false;
 return (bytes[1]==0&&bytes[2]==8&&bytes[3]==0x3c&&bytes[4]==0x3f)||std::string(bytes.begin()+0x12c,bytes.begin()+0x139)=="PS compatible";
}
std::vector<fs::path> psxBiosFiles(const fs::path &root) {
 std::vector<fs::path> paths;
 if(fs::is_directory(root) && !fs::is_symlink(root))for(const auto &e:fs::directory_iterator(root))
  if(validBios(e.path()))paths.push_back(e.path());
 std::sort(paths.begin(),paths.end());return paths;
}
std::string psxBiosIdentity(const fs::path &root) {
 Json hashes=Json::array();
 if(validBios(root))hashes.push_back({searchFold(root.filename().string()),hashFile(root)});
 else for(const auto &p:psxBiosFiles(root))hashes.push_back({p.filename().string(),hashFile(p)});
 if(hashes.empty())throw std::runtime_error("Selected PSX BIOS is unavailable or invalid");
 return hashes.dump();
}
fs::path psxPrepareBios(const fs::path &selected,const fs::path &cache) {
 if(!validBios(selected))throw std::runtime_error("Selected PSX BIOS is unavailable or invalid");
 auto name=searchFold(selected.filename().string());auto bytes=readBytes(selected);auto digest=hashBytes(bytes);
 auto identity=name+"\n"+digest;auto directory=cache/hashBytes({identity.begin(),identity.end()});
 // The core has no filename selection API. Give it a directory containing
 // exactly the selected image, so its automatic priority cannot override it.
 if(fs::is_symlink(cache)||fs::is_symlink(directory))throw std::runtime_error("Unsafe BIOS cache");
 fs::create_directories(directory);
 for(const auto &e:fs::directory_iterator(directory))
  if(e.path().filename()!=name || e.is_symlink() || !e.is_regular_file())throw std::runtime_error("Unexpected BIOS cache contents");
 auto target=directory/name;
 if(!fs::is_regular_file(target)||hashFile(target)!=digest)atomicWrite(target,bytes);
 if(hashFile(target)!=digest)throw std::runtime_error("Selected BIOS copy verification failed");
 return directory;
}
std::string psxContentHash(const fs::path &path,const fs::path &bios,bool hle) {auto text=psxIdentity(psxDiscs(path));if(!hle)text+=psxBiosIdentity(bios);else text+="HLE";return hashBytes({text.begin(),text.end()});}
bool psxBiosAvailable(const fs::path &path) {try{psxBiosIdentity(path);return true;}catch(...){return false;}}

}
