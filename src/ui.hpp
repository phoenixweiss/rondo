// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include "platform.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <memory>
namespace rondo {
class UI {
  SDL_Window *window = nullptr;
  SDL_Renderer *display = nullptr;
  SDL_Texture *output = nullptr;
  std::map<std::pair<int, int>, TTF_Font *> fonts;
  std::map<std::string, SDL_Texture *> textures;

public:
  SDL_Surface *surface = nullptr;
  SDL_Renderer *renderer = nullptr;
  Json tokens;
  std::unique_ptr<VolumeKeys> volumeKeys;
  std::unique_ptr<PowerKeys> powerKeys;
  std::unique_ptr<H700Keys> handheldKeys;
  fs::path assets;
  SDL_Color bg{}, cream{}, secondary{}, accent{}, focus{}, separator{};
  bool headless;
  bool keyboardHints = false;
  void inputHints(bool keyboard);
  std::string buttonLabel(const std::string &) const;
  int width = 720, height = 480;
  explicit UI(fs::path, bool);
  ~UI();
  UI(const UI &) = delete;
  UI &operator=(const UI &) = delete;
  TTF_Font *font(int, int = 500);
  int measure(const std::string &, int, int = 500);
  std::string fit(std::string, int, int, int = 500);
  void text(std::string, int, int, int = 23, int = 500, SDL_Color = {}, int = 0);
  void rect(int, int, int, int, SDL_Color, int radius = 0);
  void line(int, int, int, int, SDL_Color);
  void icon(const std::string &, int, int, int = 22);
  void frame(const std::string &, const std::string &, const std::string &timezone);
  void row(const std::string &, int, bool, int = 672, const std::string &value = "",
           bool enabled = true, int h = 52, const std::string &endIcon = "", const std::string &startIcon = "");
  void circle(int, int, int, SDL_Color, bool filled = false);
  void notice(const std::string &, const std::string &, bool library = false);
  void hints(const std::vector<std::pair<std::string, std::string>> &, bool disabledActions = false);
  int info(const std::string &, const std::string &, int scroll, const Json &sections = Json());
  void binaryRow(const std::string &, int, bool, std::optional<bool>, const Locale &, bool supported = true);
  bool image(const fs::path &, int, int, int, int, bool smooth);
  void pixels(const std::vector<uint32_t> &, int, int, int, int, int, int);
  void scrollbar(double, double, double, bool library);
  void volumeOverlay(VolumeFeedback &, const Locale &, bool systemNotice = false);
  void present();
  void save(const fs::path &);
};
} // namespace rondo
