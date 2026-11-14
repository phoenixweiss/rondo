// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
namespace rondo {
struct GalleryFile {
  fs::path root,relative;
  std::string identity;
};
struct GalleryGroup {
  std::string title,system;
  std::vector<GalleryFile> files;
};
struct GalleryImage {
  int width=0,height=0;
  std::vector<uint8_t> rgba;
};
struct GalleryError : std::runtime_error {
  explicit GalleryError(const std::string &code):std::runtime_error(code){}
};
std::vector<GalleryGroup> screenshotGroups(const StatePaths &);
GalleryImage loadGalleryImage(const StatePaths &,const GalleryFile &);
void deleteGalleryImage(const StatePaths &,const GalleryFile &);
}
