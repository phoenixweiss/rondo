// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "libretro.h"
#include "model.hpp"
#include <SDL.h>
#include <array>
namespace rondo {
class Core {
  void *handle = nullptr;
  bool loaded = false;
  bool psx=false, testHle=false;
  retro_disk_control_callback disks{};
  bool initialized = false;
  bool audioEnabled = false;
  SDL_AudioDeviceID audio = 0;
  fs::path saveRoot, systemRoot;
  std::vector<uint8_t> rom;
  std::map<unsigned,std::vector<uint8_t>> persistedMemory;
  std::map<std::string, std::string> variables;
  retro_pixel_format format = RETRO_PIXEL_FORMAT_0RGB1555;
  static Core *active;
  static bool environment(unsigned, void *);
  static void video(const void *, unsigned, unsigned, size_t);
  static size_t audioBatch(const int16_t *, size_t);
  static void audioSample(int16_t, int16_t);
  static void inputPoll();
  static int16_t inputState(unsigned, unsigned, unsigned, unsigned);
  void (*init)() = nullptr;
  void (*deinit)() = nullptr;
  void (*run)() = nullptr;
  void (*unload)() = nullptr;
  bool (*load)(const retro_game_info *) = nullptr;
  size_t (*serializeSize)() = nullptr;
  bool (*serialize)(void *, size_t) = nullptr;
  bool (*unserialize)(const void *, size_t) = nullptr;
  void *(*memoryData)(unsigned) = nullptr;
  size_t (*memorySize)(unsigned) = nullptr;
  template <class T> T sym(const char *name);

public:
  std::vector<uint32_t> frame;
  int width = 256, height = 240;
  uint16_t buttons = 0;
  std::array<int16_t,4> axes{}; // left X/Y, right X/Y, libretro signed range
  void psxController(bool dualshock);
  double fps = 60, sampleRate = 48000, aspectRatio = 4.0/3.0;
  std::string identity, romHash;
  uint64_t frames = 0, audioFrames = 0;
  size_t videoPitch=0;
  unsigned geometryWidth=0,geometryHeight=0,geometryChanges=0;
  Json videoGeometry() const;
  int volume = 80, audioPeak = 0;
  bool paused = false;
  Core(const fs::path &core, const Game &, const fs::path &saves, bool silent, const fs::path &bios = {}, bool allowTestHle = false);
  bool usesHle() const {return psx&&testHle;}
  unsigned discCount() const;
  unsigned discIndex() const;
  void changeDisc(unsigned);
  ~Core();
  void tick();
  void silence();
  void closeAudio();
  void openAudio();
  bool canSave() const;
  void saveAutomatic();
  void loadAutomatic();
  bool compatibleAutomatic() const;
private:
  void saveTo(const fs::path &, bool flush);
  void loadFrom(const fs::path &);
  bool compatibleFile(const fs::path &) const;
  fs::path automaticPath() const;
public:
  void save(int slot);
  void deleteSlot(int slot);
  void loadState(int slot);
  bool compatible(int slot) const;
  void flushSram();
  std::string stateHash();
};
} // namespace rondo
