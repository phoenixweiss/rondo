// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "gallery.hpp"
#include "backup.hpp"
#include "stb_image.h"
#include <algorithm>
#include <sys/stat.h>
#include <memory>
namespace rondo {
namespace {
std::string fileIdentity(const fs::path &path) {
  struct stat status{};
  if(lstat(path.c_str(),&status)||!S_ISREG(status.st_mode))
    throw GalleryError("gallery.changed");
#ifdef __APPLE__
  auto modified=status.st_mtimespec,changed=status.st_ctimespec;
#else
  auto modified=status.st_mtim,changed=status.st_ctim;
#endif
  return std::to_string(status.st_dev)+":"+std::to_string(status.st_ino)+":"+
         std::to_string(status.st_size)+":"+std::to_string(modified.tv_sec)+":"+
         std::to_string(modified.tv_nsec)+":"+std::to_string(changed.tv_sec)+":"+
         std::to_string(changed.tv_nsec);
}
fs::path checkedFile(const StatePaths &paths,const GalleryFile &file,bool writing) {
  if(paths.category("screenshots",writing)!=file.root||
     searchFold(file.relative.extension().string())!=".png")
    throw GalleryError("gallery.changed");
  auto path=backupSafePath(file.root,file.relative.generic_string());
  if(fileIdentity(path)!=file.identity)throw GalleryError("gallery.changed");
  return path;
}
}
std::vector<GalleryGroup> screenshotGroups(const StatePaths &paths) {
  auto guard=paths.guard("screenshots",false);
  auto root=paths.category("screenshots");
  if(fs::is_symlink(root))throw GalleryError("gallery.changed");
  std::map<std::string,GalleryGroup> groups;
  if(fs::exists(root)) for(auto it=fs::recursive_directory_iterator(root);
                          it!=fs::recursive_directory_iterator();++it) {
    auto name=it->path().filename().string();
    if(it->is_symlink()||(!name.empty()&&name.front()=='.')) {
      it.disable_recursion_pending();continue;
    }
    if(!it->is_regular_file()||searchFold(it->path().extension().string())!=".png")continue;
    auto relative=it->path().lexically_relative(root);
    auto parent=relative.parent_path();
    auto key=parent.generic_string();
    auto &group=groups[key];
    group.title=parent.filename().string();
    if(!parent.empty()&&parent!=parent.filename())group.system=parent.begin()->string();
    group.files.push_back({root,relative,fileIdentity(it->path())});
  }
  guard();
  std::vector<GalleryGroup> result;
  for(auto &entry:groups) {
    auto &group=entry.second;
    std::sort(group.files.begin(),group.files.end(),[](const GalleryFile &a,const GalleryFile &b) {
      return a.relative.filename().string()>b.relative.filename().string();
    });
    result.push_back(std::move(group));
  }
  std::sort(result.begin(),result.end(),[](const GalleryGroup &a,const GalleryGroup &b) {
    return std::make_pair(searchFold(a.title),a.system)<std::make_pair(searchFold(b.title),b.system);
  });
  return result;
}
GalleryImage loadGalleryImage(const StatePaths &paths,const GalleryFile &file) {
  auto guard=paths.guard("screenshots",false);
  auto path=checkedFile(paths,file,false);
  if(fs::file_size(path)>64*1024*1024)throw GalleryError("gallery.image_error");
  int width=0,height=0,channels=0;
  if(!stbi_info(path.c_str(),&width,&height,&channels)||width<1||height<1||
     width>4096||height>4096)throw GalleryError("gallery.image_error");
  auto *pixels=stbi_load(path.c_str(),&width,&height,&channels,4);
  std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> data(pixels,stbi_image_free);
  if(!pixels)throw GalleryError("gallery.image_error");
  guard();checkedFile(paths,file,false);
  return {width,height,{pixels,pixels+size_t(width)*height*4}};
}
void deleteGalleryImage(const StatePaths &paths,const GalleryFile &file) {
  auto guard=paths.guard("screenshots");
  auto path=checkedFile(paths,file,true);
  guard();
  if(!fs::remove(path))throw GalleryError("gallery.changed");
  syncDirectory(path.parent_path());
}
}
