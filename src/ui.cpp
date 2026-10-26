// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "ui.hpp"
#include "platform.hpp"
#include "libretro.h"
#include <algorithm>
#include <cmath>
#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 4096
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#pragma GCC diagnostic pop
namespace rondo {
static SDL_Color color(const std::string &s) {
  return {uint8_t(std::stoi(s.substr(1, 2), 0, 16)), uint8_t(std::stoi(s.substr(3, 2), 0, 16)),
          uint8_t(std::stoi(s.substr(5, 2), 0, 16)), 255};
}
UI::UI(fs::path a, bool hidden) : assets(std::move(a)), headless(hidden) {
  volumeKeys = std::make_unique<VolumeKeys>();
  powerKeys = std::make_unique<PowerKeys>();
  handheldKeys = std::make_unique<H700Keys>();
  tokens = readJson(assets / "tokens.json");
  auto c = tokens["color"];
  bg = color(c["background"]);
  cream = color(c["text"]);
  secondary = color(c["secondary"]);
  accent = color(c["accent"]);
  focus = color(c["focus_surface"]);
  separator = color(c["separator"]);
  if (headless)
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS))
    throw std::runtime_error(SDL_GetError());
  const auto mappingFile=assets/"ControllerMappings/micro.txt";
  if(fs::is_regular_file(mappingFile))
    controllerMappingsLoaded=SDL_GameControllerAddMappingsFromFile(mappingFile.c_str());
  refreshControllers();
  const char *hintMode = SDL_getenv("RONDO_INPUT_HINTS");
  keyboardHints = hintMode ? std::string(hintMode) == "keyboard"
                          : !headless && std::string(SDL_GetCurrentVideoDriver()) != "mali";
  if (TTF_Init())
    throw std::runtime_error(TTF_GetError());
  surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
  renderer = SDL_CreateSoftwareRenderer(surface);
  if (!renderer)
    throw std::runtime_error(SDL_GetError());
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  if (!headless) {
    window = SDL_CreateWindow("Rondo — local prototype", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_RESIZABLE);
    if (!window)
      throw std::runtime_error(SDL_GetError());
    display = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!display)
      display = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!display)
      throw std::runtime_error(SDL_GetError());
    SDL_RenderSetLogicalSize(display, width, height);
    output = SDL_CreateTexture(display, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                               width, height);
  }
}
UI::~UI() {
  artworkCache.reset();
  for(auto &entry:imageTextures)SDL_DestroyTexture(entry.second.texture);
  for (auto &entry : controllers) SDL_GameControllerClose(entry.second);
  for (auto &entry : unmappedControllers) SDL_JoystickClose(entry.second);
  for (auto &t : textures)
    SDL_DestroyTexture(t.second);
  for (auto &f : fonts)
    TTF_CloseFont(f.second);
  SDL_DestroyTexture(output);
  SDL_DestroyRenderer(display);
  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_FreeSurface(surface);
  TTF_Quit();
  SDL_Quit();
}
TTF_Font *UI::font(int size, int weight) {
  auto key = std::make_pair(size, weight);
  if (!fonts.count(key)) {
    auto path = assets / "Fonts" /
                (weight == 700   ? "Ubuntu-Bold.ttf"
                 : weight == 400 ? "Ubuntu-Regular.ttf"
                                 : "Ubuntu-Medium.ttf");
    auto *f = TTF_OpenFont(path.c_str(), size);
    if (!f)
      throw std::runtime_error(TTF_GetError());
    fonts[key] = f;
  }
  return fonts.at(key);
}
int UI::measure(const std::string &s, int size, int weight) {
  int w = 0;
  TTF_SizeUTF8(font(size, weight), s.c_str(), &w, nullptr);
  return w;
}
std::string UI::fit(std::string s, int max, int size, int weight) {
  if (measure(s, size, weight) <= max)
    return s;
  while (!s.empty() && measure(s + "…", size, weight) > max)
    s = utf8Prefix(s, s.size() - 1);
  return s + "…";
}
void UI::text(std::string s, int x, int y, int size, int weight, SDL_Color c, int max) {
  if (s.empty())
    return;
  if (!c.a)
    c = cream;
  if (max)
    s = fit(s, max, size, weight);
  auto *sf = TTF_RenderUTF8_Blended(font(size, weight), s.c_str(), c);
  if (!sf)
    return;
  auto *t = SDL_CreateTextureFromSurface(renderer, sf);
  SDL_Rect dst{x, y, sf->w, sf->h};
  SDL_RenderCopy(renderer, t, nullptr, &dst);
  SDL_DestroyTexture(t);
  SDL_FreeSurface(sf);
}
void UI::rect(int x, int y, int w, int h, SDL_Color c, int radius) {
  SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
  if (!radius) {
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
    return;
  }
  for (int i = 0; i < h; i++) {
    int inset = 0;
    if (i < radius)
      inset = radius - int(std::sqrt(radius * radius - (radius - i) * (radius - i)));
    else if (i >= h - radius)
      inset = radius -
              int(std::sqrt(radius * radius - (i - (h - radius - 1)) * (i - (h - radius - 1))));
    SDL_RenderDrawLine(renderer, x + inset, y + i, x + w - inset - 1, y + i);
  }
}
void UI::line(int x, int y, int xx, int yy, SDL_Color c) {
  SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
  SDL_RenderDrawLine(renderer, x, y, xx, yy);
}
void UI::refreshControllers() {
  for(auto it=controllers.begin();it!=controllers.end();) {
    if(!SDL_GameControllerGetAttached(it->second)) {
      controllerActivity.erase(it->first);axisRepeats.erase(it->first);
      SDL_GameControllerClose(it->second);it=controllers.erase(it);
    } else ++it;
  }
  for(auto it=unmappedControllers.begin();it!=unmappedControllers.end();) {
    if(!SDL_JoystickGetAttached(it->second)) {
      controllerActivity.erase(it->first);SDL_JoystickClose(it->second);it=unmappedControllers.erase(it);
    } else ++it;
  }
  for(int i=0;i<SDL_NumJoysticks();++i) {
    auto id=SDL_JoystickGetDeviceInstanceID(i);
    if(controllers.count(id))continue;
    const auto *name=SDL_JoystickNameForIndex(i);
    if(name && std::string(name)=="ANBERNIC-keys")continue;
    if(controllers.size()+unmappedControllers.size()>=8 && !unmappedControllers.count(id))continue;
    if(SDL_IsGameController(i)) {
      if(auto *controller=SDL_GameControllerOpen(i)) {
        controllers[id]=controller;
        auto raw=unmappedControllers.find(id);
        if(raw!=unmappedControllers.end()) {SDL_JoystickClose(raw->second);unmappedControllers.erase(raw);}
      }
    } else if(!unmappedControllers.count(id)) {
      // Keep unknown joysticks observable without guessing their button layout.
      if(auto *raw=SDL_JoystickOpen(i))unmappedControllers[id]=raw;
    }
  }
  inputDiagnosticAt=0;
}
void UI::controllerEvent(const SDL_Event &event) {
  SDL_JoystickID id=-1;
  switch(event.type) {
  case SDL_JOYBUTTONDOWN:case SDL_JOYBUTTONUP:id=event.jbutton.which;break;
  case SDL_JOYAXISMOTION:id=event.jaxis.which;break;
  case SDL_JOYHATMOTION:id=event.jhat.which;break;
  case SDL_CONTROLLERBUTTONDOWN:case SDL_CONTROLLERBUTTONUP:id=event.cbutton.which;break;
  case SDL_CONTROLLERAXISMOTION:id=event.caxis.which;break;
  default:return;
  }
  if(!controllers.count(id) && !unmappedControllers.count(id))return;
  auto &counts=controllerActivity[id];
  if(event.type==SDL_JOYBUTTONDOWN || event.type==SDL_JOYBUTTONUP)++counts.buttons;
  if(event.type==SDL_JOYAXISMOTION)++counts.axes;
  if(event.type==SDL_JOYHATMOTION)++counts.hats;
  if(event.type==SDL_CONTROLLERBUTTONDOWN || event.type==SDL_CONTROLLERBUTTONUP)++counts.mappedButtons;
  if(event.type==SDL_CONTROLLERAXISMOTION)++counts.mappedAxes;
}
const Json &UI::controllerDiagnostics() {
  const auto now=monotonicMs();
  if(inputDiagnosticAt && now<inputDiagnosticAt)return inputDiagnostic;
  inputDiagnosticAt=now+1000;
  inputDiagnostic={{"api","SDL2 joystick"},{"platform",SDL_GetPlatform()},
    {"shipped_mappings_loaded",controllerMappingsLoaded},{"devices",Json::array()}};
  auto append=[&](SDL_JoystickID id,SDL_Joystick *joystick,bool mapped,SDL_GameController *controller) {
    char guid[33];SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick),guid,sizeof(guid));
    const auto found=controllerActivity.find(id);
    const auto counts=found==controllerActivity.end()?ControllerActivity{}:found->second;
    Json device={{"guid",guid},{"vendor",SDL_JoystickGetVendor(joystick)},
      {"product",SDL_JoystickGetProduct(joystick)},{"version",SDL_JoystickGetProductVersion(joystick)},
      {"mapped",mapped},{"buttons",SDL_JoystickNumButtons(joystick)},
      {"axes",SDL_JoystickNumAxes(joystick)},{"hats",SDL_JoystickNumHats(joystick)},
      {"activity",{{"raw_buttons",counts.buttons},{"raw_axes",counts.axes},{"raw_hats",counts.hats},
        {"mapped_buttons",counts.mappedButtons},{"mapped_axes",counts.mappedAxes}}}};
    // Binding indices establish the active layout without logging arbitrary
    // device names, serials, Bluetooth addresses or individual button presses.
    if(controller) {
      auto bindings=Json::object();
      for(auto b:{SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_BACK,SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_GUIDE,
        SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT}) {
        auto binding=SDL_GameControllerGetBindForButton(controller,b);std::string target="none";
        if(binding.bindType==SDL_CONTROLLER_BINDTYPE_BUTTON)target="button:"+std::to_string(binding.value.button);
        if(binding.bindType==SDL_CONTROLLER_BINDTYPE_AXIS)target="axis:"+std::to_string(binding.value.axis);
        if(binding.bindType==SDL_CONTROLLER_BINDTYPE_HAT)target="hat:"+std::to_string(binding.value.hat.hat)+":"+std::to_string(binding.value.hat.hat_mask);
        bindings[SDL_GameControllerGetStringForButton(b)]=target;
      }
      device["bindings"]=std::move(bindings);
    }
    inputDiagnostic["devices"].push_back(std::move(device));
  };
  for(const auto &entry:controllers)append(entry.first,SDL_GameControllerGetJoystick(entry.second),true,entry.second);
  for(const auto &entry:unmappedControllers)append(entry.first,entry.second,false,nullptr);
  return inputDiagnostic;
}
uint16_t UI::controllerButtons(bool leftToDpad) const {
  uint16_t mask=0;
  if(leftToDpad) {
    auto axes=handheldKeys->axes();
    if(axes[0]<-12000)mask|=1<<RETRO_DEVICE_ID_JOYPAD_LEFT;
    if(axes[0]>12000)mask|=1<<RETRO_DEVICE_ID_JOYPAD_RIGHT;
    if(axes[1]<-12000)mask|=1<<RETRO_DEVICE_ID_JOYPAD_UP;
    if(axes[1]>12000)mask|=1<<RETRO_DEVICE_ID_JOYPAD_DOWN;
  }
  for(const auto &entry:controllers) {
    auto *controller=entry.second;
    if(!SDL_GameControllerGetAttached(controller))continue;
    auto put=[&](int button,bool pressed){if(pressed)mask|=uint16_t(1<<button);};
    put(RETRO_DEVICE_ID_JOYPAD_A,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_A));
    put(RETRO_DEVICE_ID_JOYPAD_B,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_B));
    put(RETRO_DEVICE_ID_JOYPAD_X,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_X));
    put(RETRO_DEVICE_ID_JOYPAD_Y,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_Y));
    put(RETRO_DEVICE_ID_JOYPAD_L,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
    put(RETRO_DEVICE_ID_JOYPAD_R,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
    put(RETRO_DEVICE_ID_JOYPAD_START,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_START));
    put(RETRO_DEVICE_ID_JOYPAD_SELECT,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_BACK));
    for(int d=0;d<4;++d)put(RETRO_DEVICE_ID_JOYPAD_UP+d,SDL_GameControllerGetButton(controller,SDL_GameControllerButton(SDL_CONTROLLER_BUTTON_DPAD_UP+d)));
    put(RETRO_DEVICE_ID_JOYPAD_L2,SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>12000);
    put(RETRO_DEVICE_ID_JOYPAD_R2,SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>12000);
    put(RETRO_DEVICE_ID_JOYPAD_L3,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSTICK));
    put(RETRO_DEVICE_ID_JOYPAD_R3,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSTICK));
    const int x=leftToDpad?SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX):0;
    const int y=leftToDpad?SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY):0;
    put(RETRO_DEVICE_ID_JOYPAD_LEFT,x < -12000);put(RETRO_DEVICE_ID_JOYPAD_RIGHT,x > 12000);
    put(RETRO_DEVICE_ID_JOYPAD_UP,y < -12000);put(RETRO_DEVICE_ID_JOYPAD_DOWN,y > 12000);
  }
  return mask;
}
std::array<int16_t,4> UI::controllerAxes() const {
  auto result=handheldKeys->axes();
  int strongest=0;for(auto value:result)strongest+=std::abs(int(value));
  // One physical pad owns both sticks in this single-player frontend. Choosing
  // the strongest pad avoids mixing axes from two controllers or idle drift.
  for(const auto &entry:controllers) {
    if(!SDL_GameControllerGetAttached(entry.second))continue;
    std::array<int16_t,4> axes{};int strength=0;
    for(int i=0;i<4;++i) {
      int raw=SDL_GameControllerGetAxis(entry.second,SDL_GameControllerAxis(i));
      axes[i]=stickAxis(raw);strength+=std::abs(int(axes[i]));
    }
    if(strength>strongest){strongest=strength;result=axes;}
  }
  return result;
}
std::vector<std::string> UI::controllerNavigation() {
  std::vector<std::string> result;
  // The built-in sticks share navigation with mapped SDL pads.
  auto builtin=handheldKeys->axes();
  auto &held=axisRepeats[-1];auto now=monotonicMs();
  std::string direction=builtin[1]<-12000?"UP":builtin[1]>12000?"DOWN":builtin[0]<-12000?"LEFT":builtin[0]>12000?"RIGHT":"";
  if(direction.empty())axisRepeats.erase(-1);
  else if(held.first!=direction){held={direction,now+350};result.push_back(direction);}
  else if(now>=held.second){held.second=now+110;result.push_back(direction);}
  for(const auto &entry:controllers) {
    auto *controller=entry.second;
    const int x=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX);
    const int y=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY);
    const std::string direction=y < -12000?"UP":y > 12000?"DOWN":x < -12000?"LEFT":x > 12000?"RIGHT":"";
    auto &repeat=axisRepeats[entry.first];const auto now=monotonicMs();
    if(direction.empty()){repeat={"",0};continue;}
    if(repeat.first!=direction){repeat={direction,now+350};result.push_back(direction);}
    else if(now>=repeat.second){repeat.second=now+110;result.push_back(direction);}
  }
  return result;
}
void UI::icon(const std::string &name, int x, int y, int size) {
  if (name == "check") {
    // Draw the selection mark independently of the text font's glyph coverage.
    for (int offset = 0; offset < 2; ++offset) {
      line(x + size * 4 / 24, y + size * 12 / 24 + offset,
           x + size * 9 / 24, y + size * 17 / 24 + offset, cream);
      line(x + size * 9 / 24, y + size * 17 / 24 + offset,
           x + size * 20 / 24, y + size * 6 / 24 + offset, cream);
    }
    return;
  }
  image(assets / "IconsPNG" / (name + ".png"), x, y, size, size, true);
}
void UI::battery(const Telemetry &telemetry, bool showPercent, int x) {
  const int y = 34, w = 27, h = 14;
  const auto level = telemetry.capacity;
  const bool known = level && *level >= 0 && *level <= 100;
  const std::string percentage = known ? std::to_string(*level) + "%" : "—";
  batteryDisplay["header_visible"] = true;
  batteryDisplay["percentage_visible"] = showPercent;
  batteryDisplay["source_percent"] = level ? Json(*level) : Json(nullptr);
  batteryDisplay["displayed_percent"] = showPercent && known ? Json(*level) : Json(nullptr);
  batteryDisplay["label"] = showPercent ? Json(percentage) : Json(nullptr);
  batteryDisplay["rendered_monotonic_ms"] = monotonicMs();
  batteryDisplay["submitted_monotonic_ms"] = nullptr;
  const auto color = known && *level <= 15 ? accent : cream;
  rect(x, y, w, h, color, 3);
  rect(x + 2, y + 2, w - 4, h - 4, bg, 1);
  rect(x + w + 1, y + 4, 3, h - 8, color, 1);
  if (known && *level > 0)
    rect(x + 3, y + 3, std::max(1, (w - 6) * *level / 100), h - 6, color, 1);
  if (!known) {
    line(x + 6, y + 10, x + 20, y + 3, cream);
    line(x + 6, y + 11, x + 20, y + 4, cream);
  }
  if (telemetry.batteryStatus == "Charging") {
    // Cut the large charging mark out of the fill and outline for constant contrast.
    const SDL_Point bolt[] = {{x + 18, y - 5}, {x + 7, y + 8}, {x + 14, y + 8},
                              {x + 9, y + 19}, {x + 22, y + 4}, {x + 15, y + 4}};
    std::vector<SDL_Rect> spans;
    for (int row = y - 5; row <= y + 19; ++row) {
      std::vector<int> edges;
      for (int i = 0; i < 6; ++i) {
        const auto &a = bolt[i], &b = bolt[(i + 1) % 6];
        if ((a.y <= row && b.y > row) || (b.y <= row && a.y > row))
          edges.push_back(a.x + (row - a.y) * (b.x - a.x) / (b.y - a.y));
      }
      std::sort(edges.begin(), edges.end());
      for (size_t i = 1; i < edges.size(); i += 2)
        spans.push_back({edges[i - 1], row, edges[i] - edges[i - 1] + 1, 1});
    }
    for (const auto &span : spans)
      rect(span.x - 2, span.y - 2, span.w + 4, 5, bg);
    for (const auto &span : spans)
      rect(span.x, span.y, span.w, span.h, cream);
  }
  if (showPercent)
    text(percentage, x + 40, 29, 21);
}
void UI::wifiIcon(int x, int y, const Telemetry &telemetry) {
  const int level = wifiSignalLevel(telemetry.wifiSignalDbm);
  const bool connected = telemetry.wifiState == "connected";
  for (int arc=1;arc<=3;++arc) {
    const double radius = 1 + arc*5;
    const auto color = connected ? (level>=arc ? cream : level<0 ? secondary : separator) : secondary;
    for (int n=0;n<24;++n) {
      const double a=(-134.0+88.0*n/24)*3.141592653589793/180;
      const double b=(-134.0+88.0*(n+1)/24)*3.141592653589793/180;
      for(int stroke=0;stroke<2;++stroke)
        line(x+11+int(std::lround(radius*std::cos(a))),y+20+int(std::lround(radius*std::sin(a)))+stroke,
             x+11+int(std::lround(radius*std::cos(b))),y+20+int(std::lround(radius*std::sin(b)))+stroke,color);
    }
  }
  circle(x+11,y+20,1,connected?cream:secondary,true);
  if (telemetry.wifiState=="disconnected") {
    line(x+1,y+22,x+21,y+2,bg);line(x+1,y+23,x+21,y+3,cream);
  } else if(telemetry.wifiState=="connecting") rect(x+18,y+19,4,4,accent,2);
}
int UI::status(const std::string &clock, const Telemetry &telemetry) {
  const bool known = telemetry.capacity && *telemetry.capacity >= 0 && *telemetry.capacity <= 100;
  const auto percentage = known ? std::to_string(*telemetry.capacity) + "%" : "—";
  int x = width - 24 - measure(clock, 21);
  text(clock, x, 29, 21);
  const int batteryWidth = showBatteryPercent ? 40 + measure(percentage, 21) : 31;
  x -= 12 + batteryWidth;
  battery(telemetry, showBatteryPercent, x);
  if (telemetry.wifiEnabled) {
    x -= 12 + 22;
    wifiIcon(x, 29, telemetry);
  }
  if (bluetoothEnabled) {
    x -= 12 + 22;
    const auto c=cream;
    for(int stroke=0;stroke<2;++stroke) {
      const int dx=x+stroke;
      line(dx+10,29,dx+10,51,c);
      line(dx+10,29,dx+18,36,c);line(dx+18,36,dx+3,47,c);
      line(dx+10,51,dx+18,44,c);line(dx+18,44,dx+3,33,c);
    }
  }
  return x;
}
void UI::frame(const std::string &title, const std::string &context, const std::string &zone, const std::string &titleIcon) {
  rect(0, 0, width, height, bg);
  // Only the clock is available on the host; unknown battery, no invented
  // Wi-Fi.
  auto now = time(nullptr);
  if(now!=frameSecond || zone!=frameZone) {
    std::string old = getenv("TZ") ? getenv("TZ") : "";
    bool had = getenv("TZ");
    if (frameZones.valid(zone))frameZones.applyHost(zone);
    tm local{};localtime_r(&now,&local);char clock[8];
    strftime(clock,sizeof(clock),"%H:%M",&local);frameClock=clock;
    if(had)setenv("TZ",old.c_str(),1);else unsetenv("TZ");
    tzset();frameSecond=now;frameZone=zone;
  }
  auto telemetry = readTelemetry();
  if (wifiOverride) {
    telemetry.wifiEnabled=wifiOverride->wifiEnabled;
    telemetry.wifiState=wifiOverride->wifiState;
    telemetry.wifiSignalDbm=wifiOverride->wifiSignalDbm;
  }
  const int statusLeft = status(frameClock, telemetry);
  const int titleX = titleIcon.empty() ? 24 : 62;
  if (!titleIcon.empty()) icon(titleIcon,24,28,26);
  text(title, titleX, 22, 29, 700, cream, statusLeft - titleX - 24);

  line(24, 68, 696, 68, separator);
  line(24, 432, 696, 432, separator);
  text(context, 24, 85, 19, 500, secondary, 672);
}
void UI::row(const std::string &label, int y, bool selected, int width, const std::string &value,
             bool enabled, int h, const std::string &endIcon, const std::string &startIcon) {
  if (selected) {
    rect(16, y, width, h, focus, 9);
    rect(16, y + 7, 4, h - 14, accent, 2);
  }
  auto c = enabled ? cream : secondary;
  int available = width - 38;
  int labelX = startIcon.empty() ? 32 : 66;
  if (!startIcon.empty()) {
    icon(startIcon, 32, y + (h - 22) / 2, 22);
    available -= 34;
  }
  int valueW = measure(value, 21);
  if (!value.empty())
    available -= valueW + 38;
  if (!endIcon.empty())
    available -= 38;
  // Focused long game titles use a measured two-line row, never byte
  // truncation.
  if (h == 78) {
    std::string first = label, second;
    while (!first.empty() && measure(first, 23, 700) > available) {
      auto split = first.find_last_of(' ');
      if (split == first.npos) {
        auto cut = utf8Prefix(first, first.size() - 1);
        second = first.substr(cut.size()) + second;
        first = cut;
      } else {
        second = first.substr(split + 1) + (second.empty() ? "" : " ") + second;
        first = first.substr(0, split);
      }
    }
    text(first, labelX, y + 10, 23, 700, c, available);
    text(second, labelX, y + 38, 23, 700, c, available);
  } else
    text(label, labelX, y + 12, 23, selected ? 700 : 500, c, available);
  if (!value.empty())
    text(value, width - 8 - valueW - (endIcon.empty() ? 0 : 38), y + 14, 21, 500, c);
  if (!endIcon.empty())
    icon(endIcon, width - 30, y + (h - 22) / 2, 22);
}
// Coverage-sampled reusable outlines remain smooth on the native 720x480 panel.
void UI::circle(int x, int y, int size, SDL_Color c, bool filled) {
  auto key = "circle:" + std::to_string(size) + ":" + std::to_string(filled) + ":" +
             std::to_string(c.r) + ":" + std::to_string(c.g) + ":" + std::to_string(c.b);
  if (!textures.count(key)) {
    auto *sf = SDL_CreateRGBSurfaceWithFormat(0,size,size,32,SDL_PIXELFORMAT_RGBA32);
    double outer = size / 2.0 - 0.5, inner = filled ? 0 : outer - 1.25;
    for (int yy=0; yy<size; ++yy) for (int xx=0; xx<size; ++xx) {
      int coverage=0;
      for (int sy=0; sy<4; ++sy) for (int sx=0; sx<4; ++sx) {
        double dx=xx+(sx+0.5)/4-size/2.0, dy=yy+(sy+0.5)/4-size/2.0;
        double r=std::sqrt(dx*dx+dy*dy);
        coverage += r<=outer && (filled || r>=inner);
      }
      auto *pixels = reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(sf->pixels)+yy*sf->pitch);
      pixels[xx]=SDL_MapRGBA(sf->format,c.r,c.g,c.b,Uint8(coverage*255/16));
    }
    auto *texture=SDL_CreateTextureFromSurface(renderer,sf); SDL_FreeSurface(sf);
    SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND); textures[key]=texture;
  }
  SDL_Rect dst{x,y,size,size}; SDL_RenderCopy(renderer,textures.at(key),nullptr,&dst);
}
void UI::notice(const std::string &message, const std::string &symbol, bool library) {
  int w=std::clamp(measure(message,22,400)+86,400,640), x=(720-w)/2;
  int h=library?48:64;
  rect(x,76,w,h,separator,9); rect(x+1,77,w-2,h-2,focus,8);
  icon(symbol,x+24,76+(h-24)/2,24);
  text(message,x+62,76+(h-TTF_FontHeight(font(22,400)))/2,22,400,cream,w-78);
}
void UI::inputHints(bool keyboard) {
  keyboardHints = keyboard;
  // The separately spawned runner inherits the active input mode.
  SDL_setenv("RONDO_INPUT_HINTS", keyboard ? "keyboard" : "controller", 1);
}
void UI::keycap(int x, int y, int width, bool round, SDL_Color c, int height, bool filled) {
  // One antialiased 1.25px outline for circular and rectangular footer keys.
  auto key="keycap:"+std::to_string(width)+":"+std::to_string(height)+":"+std::to_string(round)+":"+std::to_string(filled)+":"+
           std::to_string(c.r)+":"+std::to_string(c.g)+":"+std::to_string(c.b)+":"+std::to_string(c.a);
  if (!textures.count(key)) {
    auto *surface=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
    const double radius=round?(height-1)/2.0:5.5;
    for(int yy=0;yy<height;++yy) for(int xx=0;xx<width;++xx) {
      int coverage=0;
      for(int sy=0;sy<4;++sy) for(int sx=0;sx<4;++sx) {
        double qx=std::abs(xx+(sx+0.5)/4-width/2.0)-(width/2.0-radius-0.5);
        double qy=std::abs(yy+(sy+0.5)/4-height/2.0)-(height/2.0-radius-0.5);
        double distance=std::min(std::max(qx,qy),0.0)+
                        std::hypot(std::max(qx,0.0),std::max(qy,0.0))-radius;
        coverage+=distance<=0 && (filled || distance>=-1.25);
      }
      auto *pixels=reinterpret_cast<Uint32*>(static_cast<Uint8*>(surface->pixels)+yy*surface->pitch);
      pixels[xx]=SDL_MapRGBA(surface->format,c.r,c.g,c.b,Uint8(coverage*c.a/16));
    }
    auto *texture=SDL_CreateTextureFromSurface(renderer,surface);
    SDL_FreeSurface(surface);
    SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
    textures[key]=texture;
  }
  SDL_Rect dst{x,y,width,height};
  SDL_RenderCopy(renderer,textures.at(key),nullptr,&dst);
}
void UI::binaryRow(const std::string &label, int y, bool selected, std::optional<bool> value,
                   const Locale &locale, bool supported) {
  row(fit(label, 496, 23, selected ? 700 : 500), y, selected);
  if (!supported || !value) {
    text("—", 644, y + 12, 23, 500, secondary);
    return;
  }
  rect(552, y + 9, 128, 34, separator, 6);
  rect(553, y + 10, 126, 32, bg, 5);
  rect(*value ? 616 : 552, y + 9, 64, 34, cream, 6);
  for (int i = 0; i < 2; ++i) {
    auto label = locale.tr(i ? "value.on" : "value.off");
    text(label, 552 + i * 64 + (64 - measure(label, 19)) / 2, y + 15, 19, 500,
         *value == bool(i) ? bg : secondary);
  }
}
void UI::keyArrows(int x, int y, int width, bool vertical, SDL_Color c) {
  auto key="keyarrows:"+std::to_string(width)+":"+std::to_string(vertical)+":"+
           std::to_string(c.r)+":"+std::to_string(c.g)+":"+std::to_string(c.b);
  if (!textures.count(key)) {
    auto *surface=SDL_CreateRGBSurfaceWithFormat(0,width,32,32,SDL_PIXELFORMAT_RGBA32);
    // Symmetric geometry around each key's centre; supersampling avoids a
    // one-sided second stroke shifting the visible bounds right/down.
    struct Segment {double ax,ay,bx,by;};
    std::vector<Segment> lines;
    for(int i=0;i<2;++i) {
      double cx=width/2.0+(i?9:-9), direction=i?1:-1;
      if(vertical) {
        lines.push_back({cx-6,16-direction*3,cx,16+direction*3});
        lines.push_back({cx,16+direction*3,cx+6,16-direction*3});
      } else {
        lines.push_back({cx-direction*3,10,cx+direction*3,16});
        lines.push_back({cx+direction*3,16,cx-direction*3,22});
      }
    }
    for(int yy=0;yy<32;++yy) for(int xx=0;xx<width;++xx) {
      int coverage=0;
      for(int sy=0;sy<4;++sy) for(int sx=0;sx<4;++sx) {
        double px=xx+(sx+0.5)/4,py=yy+(sy+0.5)/4,distance=100;
        for(auto segment:lines) {
          double dx=segment.bx-segment.ax,dy=segment.by-segment.ay;
          double t=std::clamp(((px-segment.ax)*dx+(py-segment.ay)*dy)/(dx*dx+dy*dy),0.0,1.0);
          distance=std::min(distance,std::hypot(px-segment.ax-t*dx,py-segment.ay-t*dy));
        }
        coverage+=distance<=0.85;
      }
      auto *pixels=reinterpret_cast<Uint32*>(static_cast<Uint8*>(surface->pixels)+yy*surface->pitch);
      pixels[xx]=SDL_MapRGBA(surface->format,c.r,c.g,c.b,Uint8(coverage*c.a/16));
    }
    auto *texture=SDL_CreateTextureFromSurface(renderer,surface);SDL_FreeSurface(surface);
    SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);textures[key]=texture;
  }
  SDL_Rect dst{x,y,width,32};SDL_RenderCopy(renderer,textures.at(key),nullptr,&dst);
}
void UI::keyLabel(const std::string &label, int x, int y, int width, int size, SDL_Color c, int height) {
  auto *surface=TTF_RenderUTF8_Blended(font(size,700),label.c_str(),c);
  if(!surface)return;
  int left=surface->w,top=surface->h,right=-1,bottom=-1;
  for(int yy=0;yy<surface->h;++yy) for(int xx=0;xx<surface->w;++xx) {
    auto *pixels=reinterpret_cast<Uint32*>(static_cast<Uint8*>(surface->pixels)+yy*surface->pitch);
    Uint8 r,g,b,a;SDL_GetRGBA(pixels[xx],surface->format,&r,&g,&b,&a);
    if(a>16) {left=std::min(left,xx);right=std::max(right,xx);top=std::min(top,yy);bottom=std::max(bottom,yy);}
  }
  if(right>=left) {
    auto *texture=SDL_CreateTextureFromSurface(renderer,surface);
    SDL_Rect ink{left,top,right-left+1,bottom-top+1};
    SDL_Rect dst{x+(width-ink.w)/2,y+(height-ink.h)/2,ink.w,ink.h};
    SDL_RenderCopy(renderer,texture,&ink,&dst);SDL_DestroyTexture(texture);
  }
  SDL_FreeSurface(surface);
}
std::string UI::buttonLabel(const std::string &action) const {
  if (action == "LEFT/RIGHT") return "‹ / ›";
  if (!keyboardHints)
    return action;
  static const std::map<std::string, std::string> labels = {
      {"A", "Enter"}, {"B", "Esc"}, {"X", "C"}, {"Y", "V"},
      {"L1", "Q"}, {"R1", "E"}, {"L1/R1", "Q/E"}, {"MENU", "Tab"}};
  auto it = labels.find(action);
  return it == labels.end() ? action : it->second;
}
void UI::hints(const std::vector<std::pair<std::string, std::string>> &items, bool disabledActions) {
  int x = 24;
  for (auto &[action, label] : items) {
    bool vertical = action == "UP/DOWN", horizontal = action == "LEFT/RIGHT";
    auto button = buttonLabel(action);
    auto c = disabledActions && (action == "A" || action == "LEFT/RIGHT") ? secondary : cream;
    bool round = !keyboardHints && button.size() == 1;
    int keySize = button == "L1/R1" ? 14 : 16;
    int bw = vertical || horizontal ? 44 : round ? 32 : std::max(36, measure(button, keySize, 700) + 20);
    int lw = measure(label, 19);
    if (x + bw + 8 + lw > 696)
      break;
    auto outline=c;
    outline.a=200;
    keycap(x,440,bw,round,outline);
    if (vertical || horizontal) {
      keyArrows(x,440,bw,vertical,c);
    } else
      keyLabel(button,x,440,bw,keySize,c);
    text(label, x + bw + 8, 444, 19, 500, c);
    x += bw + 8 + lw + 16;
  }
}
int UI::info(const std::string &title, const std::string &body, int scroll, const Json &sections) {
  auto wrap = [&](const std::string &input, int width, int size) {
    std::vector<std::string> lines;
    std::string current;
    for (size_t i = 0; i < input.size();) {
      unsigned char lead = input[i];
      size_t bytes = lead < 128 ? 1 : lead < 224 ? 2 : lead < 240 ? 3 : 4;
      auto glyph = input.substr(i, bytes); i += bytes;
      if (glyph == "\n") { lines.push_back(current); current.clear(); continue; }
      if (!current.empty() && measure(current + glyph, size, 400) > width) {
        auto split = current.find_last_of(' ');
        if (split != std::string::npos) {
          lines.push_back(current.substr(0, split)); current = current.substr(split + 1);
        } else { lines.push_back(current); current.clear(); }
      }
      current += glyph;
    }
    lines.push_back(current); return lines;
  };
  struct Part { std::string text; int x, y, size, weight; bool muted = false; };
  std::vector<Part> parts;
  int y = 0;
  auto append = [&](const std::string &text, int x, int width, int size, bool muted = false) {
    for (auto &line : wrap(text, width, size)) { parts.push_back({line,x,y,size,400,muted}); y += 31; }
  };
  if (!sections.is_null()) {
    append(sections.at("intro"),64,592,22); y += 17;
    parts.push_back({sections.at("on_label"),64,y,22,700});
    append(sections.at("on"),140,516,22);
    parts.push_back({sections.at("off_label"),64,y,22,700});
    append(sections.at("off"),140,516,22); y += 20;
    append(sections.at("timing"),64,592,22);
    append(sections.at("note"),64,592,21,true);
    if (!sections.value("status", "").empty()) append(sections.at("status"),64,592,21);
  } else append(body,64,592,22);
  int content = y;
  int maximum = std::max(0, (content - 217 + 30) / 31);
  scroll = std::clamp(scroll, 0, maximum);
  rect(0,0,720,480,SDL_Color{0,0,0,153});
  rect(40,96,640,296,separator,9); rect(41,97,638,294,focus,8);
  text(title,64,114,25,700,cream,592); line(64,153,656,153,separator);
  SDL_Rect clip{64,166,592,219}; SDL_RenderSetClipRect(renderer,&clip);
  for (auto &part : parts) text(part.text,part.x,168+part.y-scroll*31,part.size,part.weight,part.muted?secondary:cream,part.x==140?516:592);
  SDL_RenderSetClipRect(renderer,nullptr);
  if (maximum) {
    auto g=scrollGeometry(content,217,scroll*31,217);
    rect(664,168,3,217,separator); rect(664,168+int(g.position),3,int(g.thumb),cream);
  }
  rect(0,433,720,47,bg); line(24,432,696,432,separator);
  return scroll;
}

bool UI::image(const fs::path &p, int x, int y, int w, int h, bool smooth) {
  std::error_code fileError;
  auto stamp = fs::last_write_time(p, fileError);
  if (fileError)
    return false;
  auto size = fs::file_size(p, fileError);
  if (fileError)
    return false;
  std::string key = p.string() + "#" + std::to_string(smooth) + "#" +
                    std::to_string(static_cast<long long>(stamp.time_since_epoch().count())) + "#" +
                    std::to_string(size);
  auto it = imageTextures.find(key);
  SDL_Texture *t = nullptr;
  bool temporary=false;
  if (it == imageTextures.end()) {
    int iw = 0, ih = 0, channels = 0;
    auto *bytes = stbi_load(p.c_str(), &iw, &ih, &channels, 4);
    if (!bytes)
      return false;
    t=rememberImage(key,bytes,iw,ih,smooth);
    temporary=size_t(iw)*ih*4>4*1024*1024;
    stbi_image_free(bytes);
    if (!t)
      return false;
  } else {it->second.used=++imageClock;t = it->second.texture;}
  int iw, ih;
  SDL_QueryTexture(t, nullptr, nullptr, &iw, &ih);
  double scale=std::min(double(w)/iw,double(h)/ih);
  SDL_Rect r{x+(w-int(iw*scale))/2,y+(h-int(ih*scale))/2,int(iw*scale),int(ih*scale)};
  SDL_RenderCopy(renderer, t, nullptr, &r);
  if(temporary)SDL_DestroyTexture(t);
  return true;
}
SDL_Texture *UI::rememberImage(const std::string &key,const uint8_t *pixels,int width,int height,bool smooth) {
  size_t bytes=size_t(width)*height*4;
  while(imageTextureBytes+bytes>4*1024*1024&&!imageTextures.empty()) {
    auto oldest=std::min_element(imageTextures.begin(),imageTextures.end(),[](const auto &a,const auto &b){return a.second.used<b.second.used;});
    imageTextureBytes-=oldest->second.bytes;SDL_DestroyTexture(oldest->second.texture);imageTextures.erase(oldest);
  }
  auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(const_cast<uint8_t *>(pixels),width,height,32,width*4,SDL_PIXELFORMAT_RGBA32);
  if(!surface)return nullptr;auto *texture=SDL_CreateTextureFromSurface(renderer,surface);SDL_FreeSurface(surface);
  if(!texture)return nullptr;
  SDL_SetTextureScaleMode(texture,smooth?SDL_ScaleModeLinear:SDL_ScaleModeNearest);
  if(bytes<=4*1024*1024){imageTextures[key]={texture,bytes,++imageClock};imageTextureBytes+=bytes;}
  return texture;
}
bool UI::artworkImage(const std::vector<ArtworkRequest> &requests,int x,int y,int w,int h) {
  artworkPending=false;
  if(!artworkCache||requests.empty())return false;
  artworkCache->select(requests);auto image=artworkCache->get(requests.front());
  if(!image){artworkPending=!artworkCache->missing(requests.front());return false;}
  auto id="art:"+image->identity;auto found=imageTextures.find(id);SDL_Texture *texture=nullptr;
  if(found!=imageTextures.end()){found->second.used=++imageClock;texture=found->second.texture;}
  else texture=rememberImage(id,image->rgba.data(),image->width,image->height,requests.front().smooth);
  if(!texture)return false;
  double scale=std::min(double(w)/image->width,double(h)/image->height);
  SDL_Rect target{x+(w-int(image->width*scale))/2,y+(h-int(image->height*scale))/2,int(image->width*scale),int(image->height*scale)};
  SDL_RenderCopy(renderer,texture,nullptr,&target);return true;
}
void UI::pixels(const std::vector<uint32_t> &p, int iw, int ih, int x, int y, int w, int h, bool fitRawAspect) {
  if (p.empty())
    return;
  auto *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, iw, ih);
  SDL_UpdateTexture(t, nullptr, p.data(), iw * 4);
  SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
  SDL_Rect r{x,y,w,h};
  if(fitRawAspect) {
    double scale=std::min(double(w)/iw,double(h)/ih);
    r={x+(w-int(iw*scale))/2,y+(h-int(ih*scale))/2,int(iw*scale),int(ih*scale)};
  }
  // Gameplay may supply a display-aspect-corrected rectangle. Fitting the raw
  // 512x240 PSX pixels again would incorrectly turn that 4:3 view into 32:15.

  SDL_RenderCopy(renderer, t, nullptr, &r);
  SDL_DestroyTexture(t);
}
void UI::scrollbar(double content, double viewport, double offset, bool library, int top, int bottom) {
  auto token = tokens["layout"][library ? "game_scrollbar" : "scrollbar"];
  int x = token["x"];
  if (top < 0) top = token["top"];
  if (bottom < 0) bottom = token["bottom"];
  auto s = scrollGeometry(content, viewport, offset, bottom - top);
  if (!s.visible)
    return;
  rect(x, top, 3, bottom - top, separator, 1);
  rect(x, top + int(s.position), 3, int(s.thumb), secondary, 1);
}
void UI::volumeOverlay(VolumeFeedback &feedback, const Locale &locale, bool /*systemNotice*/) {
  feedback.timeout(monotonicMs());
  if (!feedback.visible(monotonicMs()))
    return;
  auto spec = readJson(assets / "volume-component.json");
  auto panel = spec["panel"], ico = spec["icon"], track = spec["track"];
  int x = panel["x"], y = panel["y"], w = panel["width"], h = panel["height"],
      radius = panel["radius"];
  int dy = 0; // Corner overlay stays above system notices.
  y += dy;
  rect(x, y, w, h, focus, radius);
  bool error = feedback.phase == "error", unavailable = feedback.phase == "unavailable";
  bool known = feedback.confirmed.has_value() && !error && !unavailable && feedback.phase != "unknown";
  bool muted = known && *feedback.confirmed == 0;
  icon(muted ? "volume-x" : "volume-2", ico["x"], ico["y"].get<int>() + dy, ico["size"]);
  if (!known) {
    auto status = spec.at("state_word");
    auto key = error ? "volume.error" : unavailable ? "volume.unavailable" : "volume.unknown";
    int size = status["size"], weight = status["weight"];
    text(locale.tr(key), status["x"],
         status["center_y"].get<int>() + dy - TTF_FontHeight(font(size, weight)) / 2,
         size, weight, cream, status["max_width"]);
  } else {
    int tx = track["x"], ty = track["y"].get<int>() + dy, tw = track["width"], th = track["height"],
        tr = track["radius"];
    // Zero keeps an empty track; unavailable readings use the state word.
    rect(tx, ty, tw, th, separator, tr);
    int fill = tw * std::clamp(*feedback.confirmed, 0, 100) / 100;
    if (fill > 0)
      rect(tx, ty, fill, th, cream, std::min(tr, fill / 2));
  }
}
void UI::present() {
  SDL_RenderPresent(renderer);
  if (display) {
    SDL_UpdateTexture(output, nullptr, surface->pixels, surface->pitch);
    SDL_SetRenderDrawColor(display, bg.r, bg.g, bg.b, 255);
    SDL_RenderClear(display);
    SDL_RenderCopy(display, output, nullptr, nullptr);
    SDL_RenderPresent(display);
  }
  batteryDisplay["submitted_monotonic_ms"] = monotonicMs();
}
void UI::save(const fs::path &p) {
  present();
  fs::create_directories(p.parent_path());
  auto *converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
  if (!converted)
    throw std::runtime_error(SDL_GetError());
  int ok = stbi_write_png(p.c_str(), width, height, 4, converted->pixels, converted->pitch);
  SDL_FreeSurface(converted);
  if (!ok)
    throw std::runtime_error("Cannot write preview PNG");
}
} // namespace rondo
