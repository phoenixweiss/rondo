// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include "platform.hpp"
#include "artwork.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <memory>
#include <array>
namespace rondo {
class UI {
  SDL_Window *window = nullptr;
  SDL_Renderer *display = nullptr;
  SDL_Texture *output = nullptr;
  Timezones frameZones;
  std::string frameZone, frameClock;
  time_t frameSecond = -1;
  std::map<std::pair<int, int>, TTF_Font *> fonts;
  std::map<std::string, SDL_Texture *> textures;
  struct ImageTexture {SDL_Texture *texture;size_t bytes;uint64_t used;};
  std::map<std::string,ImageTexture> imageTextures;
  size_t imageTextureBytes=0;
  uint64_t imageClock=0;
  SDL_Texture *rememberImage(const std::string &,const uint8_t *,int,int,bool);

public:
  std::unique_ptr<ArtworkCache> artworkCache;
  bool artworkPending=false;
  bool artworkImage(const std::vector<ArtworkRequest> &,int,int,int,int);
  SDL_Surface *surface = nullptr;
  SDL_Renderer *renderer = nullptr;
  Json tokens, themes;
  std::string currentTheme;
  static SDL_Color parseColor(const std::string &);
  void setTheme(const std::string &);
  std::unique_ptr<VolumeKeys> volumeKeys;
  std::unique_ptr<PowerKeys> powerKeys;
  std::unique_ptr<H700Keys> handheldKeys;
  fs::path assets;
  SDL_Color bg{}, cream{}, secondary{}, accent{}, focus{}, separator{};
  bool headless;
  bool keyboardHints = false;
  std::optional<Telemetry> wifiOverride;
  void wifiIcon(int x, int y, const Telemetry &);
  bool showBatteryPercent = true;
  bool bluetoothEnabled = false;
  std::map<SDL_JoystickID,SDL_GameController *> controllers;
  std::map<SDL_JoystickID,SDL_Joystick *> unmappedControllers;
  struct ControllerActivity {uint64_t buttons=0,axes=0,hats=0,mappedButtons=0,mappedAxes=0;};
  std::map<SDL_JoystickID,ControllerActivity> controllerActivity;
  Json inputDiagnostic=Json::object();
  uint64_t inputDiagnosticAt=0;
  int controllerMappingsLoaded=0;
  void controllerEvent(const SDL_Event &);
  const Json &controllerDiagnostics();
  std::map<SDL_JoystickID,std::pair<std::string,uint64_t>> axisRepeats;
  void refreshControllers();
  uint16_t controllerButtons(bool leftToDpad = true) const;
  std::array<int16_t,4> controllerAxes() const;
  std::vector<std::string> controllerNavigation();
  // Values submitted to the renderer, not a claim about physical panel pixels.
  Json batteryDisplay = Json::object();
  void battery(const Telemetry &, bool showPercent, int x = 599);
  int status(const std::string &clock, const Telemetry &);
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
  void frame(const std::string &, const std::string &, const std::string &timezone, const std::string &titleIcon = "");
  void row(const std::string &, int, bool, int = 672, const std::string &value = "",
           bool enabled = true, int h = 52, const std::string &endIcon = "", const std::string &startIcon = "");
  void circle(int, int, int, SDL_Color, bool filled = false);
  void keycap(int x, int y, int width, bool round, SDL_Color, int height = 32, bool filled = false);
  void keyArrows(int x, int y, int width, bool vertical, SDL_Color);
  void keyLabel(const std::string &, int x, int y, int width, int size, SDL_Color, int height = 32);
  void notice(const std::string &, const std::string &, bool library = false);
  void hints(const std::vector<std::pair<std::string, std::string>> &, bool disabledActions = false);
  int info(const std::string &, const std::string &, int scroll, const Json &sections = Json());
  void binaryRow(const std::string &, int, bool, std::optional<bool>, const Locale &, bool supported = true);
  bool image(const fs::path &, int, int, int, int, bool smooth);
  void pixels(const std::vector<uint32_t> &, int, int, int, int, int, int, bool fitRawAspect = true);
  void scrollbar(double, double, double, bool library, int top = -1, int bottom = -1);
  void volumeOverlay(VolumeFeedback &, const Locale &, bool systemNotice = false);
  void present();
  void save(const fs::path &);
};
} // namespace rondo
