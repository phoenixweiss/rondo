// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "core.hpp"
#include "psx.hpp"
#include "state.hpp"
#include <limits>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include "platform.hpp"
#include <cmath>
#include <cstring>
#include <dlfcn.h>
namespace rondo {
Core *Core::active = nullptr;
template <class T> T Core::sym(const char *name) {
  auto p = dlsym(handle, name);
  if (!p)
    throw std::runtime_error(std::string("Missing core ABI: ") + name);
  return reinterpret_cast<T>(p);
}
Core::Core(const fs::path &path, const Game &game, const fs::path &saves, bool silent, const fs::path &bios, bool allowTestHle)
    : saveRoot(saves), systemRoot(bios.empty()?saves/"bios":bios) {
  identity = hashFile(path);
  psx=game.system=="PSX";testHle=psx&&allowTestHle;
  if(psx)romHash=psxContentHash(game.path,systemRoot,testHle);
  else {rom = readBytes(game.path);romHash = hashBytes(rom);}
  fs::create_directories(saveRoot);
  fs::create_directories(systemRoot);
  handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle)
    throw std::runtime_error(dlerror());
  active = this;
  try {
    if (sym<unsigned (*)()>("retro_api_version")() != RETRO_API_VERSION)
      throw std::runtime_error("Unsupported libretro ABI");
    sym<void (*)(retro_environment_t)>("retro_set_environment")(environment);
    sym<void (*)(retro_video_refresh_t)>("retro_set_video_refresh")(video);
    sym<void (*)(retro_audio_sample_t)>("retro_set_audio_sample")(audioSample);
    sym<void (*)(retro_audio_sample_batch_t)>("retro_set_audio_sample_batch")(audioBatch);
    sym<void (*)(retro_input_poll_t)>("retro_set_input_poll")(inputPoll);
    sym<void (*)(retro_input_state_t)>("retro_set_input_state")(inputState);
    init = sym<void (*)()>("retro_init");
    deinit = sym<void (*)()>("retro_deinit");
    run = sym<void (*)()>("retro_run");
    unload = sym<void (*)()>("retro_unload_game");
    load = sym<bool (*)(const retro_game_info *)>("retro_load_game");
    serializeSize = sym<size_t (*)()>("retro_serialize_size");
    serialize = sym<bool (*)(void *, size_t)>("retro_serialize");
    unserialize = sym<bool (*)(const void *, size_t)>("retro_unserialize");
    memoryData = sym<void *(*)(unsigned)>("retro_get_memory_data");
    memorySize = sym<size_t (*)(unsigned)>("retro_get_memory_size");
    init();
    initialized = true;
    retro_system_info info{};
    sym<void (*)(retro_system_info *)>("retro_get_system_info")(&info);
    if(psx&&!info.need_fullpath)throw std::runtime_error("PSX core must use full-path disc loading");
    if(psx){auto controller=sym<void (*)(unsigned,unsigned)>("retro_set_controller_port_device");controller(0,RETRO_DEVICE_JOYPAD);controller(1,RETRO_DEVICE_NONE);}
    auto launch = game.path.string();
    if(psx&&searchFold(game.path.extension().string())==".m3u") {
      std::string playlist;for(const auto &disc:psxDiscs(game.path))playlist+=disc.string()+"\n";
      auto normalized=saveRoot/"rondo-discs.m3u";atomicWrite(normalized,{playlist.begin(),playlist.end()});launch=normalized.string();
    }
    retro_game_info content{launch.c_str(), info.need_fullpath ? nullptr : rom.data(),
                            info.need_fullpath ? 0 : rom.size(), nullptr};
    if (!load(&content))
      throw std::runtime_error("Core rejected the ROM");
    loaded = true;
    retro_system_av_info av{};
    sym<void (*)(retro_system_av_info *)>("retro_get_system_av_info")(&av);
    geometryWidth=av.geometry.base_width;geometryHeight=av.geometry.base_height;
    if(std::isfinite(av.geometry.aspect_ratio) && av.geometry.aspect_ratio>0 && av.geometry.aspect_ratio<=8)aspectRatio=av.geometry.aspect_ratio;
    fps = av.timing.fps;
    sampleRate = av.timing.sample_rate;
    if (!std::isfinite(fps) || fps < 1 || fps > 240)
      throw std::runtime_error("Invalid core frame rate");
    for(auto memory:{std::pair<unsigned,const char *>{RETRO_MEMORY_SAVE_RAM,"sram.bin"},
                     {RETRO_MEMORY_RTC,"rtc.bin"}}) {
      const auto path = saveRoot / memory.second;
      const auto previous = fs::path(path.string() + ".previous");
      if (!fs::exists(path) && !fs::exists(previous)) continue;
      auto size = memorySize(memory.first);
      if (!size) continue;
      auto readMemory = [&](const fs::path &file) {
        if (fs::file_size(file) != size) throw std::runtime_error("Incompatible persistent memory size");
        auto bytes = readBytes(file);
        if (bytes.size() != size) throw std::runtime_error("Incomplete persistent memory read");
        return bytes;
      };
      std::vector<uint8_t> bytes;
      try { bytes = readMemory(path); }
      catch (...) { bytes = readMemory(previous); memoryRecovered.push_back(memory.second); }
      if (!memoryData(memory.first)) throw std::runtime_error("Persistent core memory inaccessible");
      memcpy(memoryData(memory.first), bytes.data(), size);
      persistedMemory[memory.first] = std::move(bytes);
    }
    audioEnabled = !silent;
    openAudio();
  } catch (...) {
    if (loaded)
      unload();
    if (initialized)
      deinit();
    if (audio)
      SDL_CloseAudioDevice(audio);
    dlclose(handle);
    handle = nullptr;
    active = nullptr;
    throw;
  }
}
Core::~Core() {
  stopRumble();
  rumbleEnabled=false;
  if (audio)
    SDL_CloseAudioDevice(audio);
  if (loaded)
    unload();
  if (initialized)
    deinit();
  if (handle)
    dlclose(handle);
  active = nullptr;
}
static void coreLog(enum retro_log_level level,const char *format,...) {
 if(level<RETRO_LOG_WARN&&!getenv("RONDO_CORE_TRACE"))return;va_list args;va_start(args,format);vfprintf(stderr,format,args);va_end(args);
}
bool Core::environment(unsigned cmd, void *data) {
  auto &c = *active;
  switch (cmd) {
  case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
    if(!data)return false;
    static_cast<retro_rumble_interface *>(data)->set_rumble_state=rumbleState;return true;
  case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    static_cast<retro_log_callback *>(data)->log=coreLog;return true;
  case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION:
    *static_cast<unsigned *>(data)=1;return true;
  case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
    c.disks=*static_cast<retro_disk_control_callback *>(data);return true;
  case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: {
    auto &d=*static_cast<retro_disk_control_ext_callback *>(data);
    c.disks={d.set_eject_state,d.get_eject_state,d.get_image_index,d.set_image_index,d.get_num_images,d.replace_image_index,d.add_image_index};return true;
  }
  case RETRO_ENVIRONMENT_SET_GEOMETRY: {
    auto &g=*static_cast<retro_game_geometry *>(data);
    c.geometryWidth=g.base_width;c.geometryHeight=g.base_height;++c.geometryChanges;
    if(std::isfinite(g.aspect_ratio)&&g.aspect_ratio>0&&g.aspect_ratio<=8)c.aspectRatio=g.aspect_ratio;return true;
  }
  case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    c.format = *static_cast<retro_pixel_format *>(data);
    return c.format == RETRO_PIXEL_FORMAT_XRGB8888 || c.format == RETRO_PIXEL_FORMAT_RGB565 ||
           c.format == RETRO_PIXEL_FORMAT_0RGB1555;
  case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    *static_cast<const char **>(data) = c.systemRoot.c_str();
    return true;
  case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
    *static_cast<const char **>(data) = c.saveRoot.c_str();
    return true;
  case RETRO_ENVIRONMENT_GET_CAN_DUPE:
    *static_cast<bool *>(data) = true;
    return true;
  case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
  case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
  case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    return true;
  case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
    *static_cast<unsigned *>(data) = 0;
    return true;
  case RETRO_ENVIRONMENT_SET_VARIABLES: {
    auto *v = static_cast<retro_variable *>(data);
    for (; v && v->key; ++v) {
      std::string values = v->value ? v->value : "";
      auto p = values.find(';');
      if (p != values.npos) {
        values = values.substr(p + 1);
        while (!values.empty() && values[0] == ' ')
          values.erase(0, 1);
        c.variables[v->key] = values.substr(0, values.find('|'));
        if(std::string(v->key)=="pcsx_rearmed_vibration")c.variables[v->key]="enabled";
        // The handheld GBA policy uses wall time and hardware-rate audio;
        // link-cable sessions are outside this frontend's supported scope.
        if(std::string(v->key)=="gpsp_rtc_time_source")c.variables[v->key]="system";
        if(std::string(v->key)=="gpsp_sound_rate")c.variables[v->key]="32768";
        if(std::string(v->key)=="gpsp_serial")c.variables[v->key]="disabled";
        if(c.psx&&std::string(v->key)=="pcsx_rearmed_memcard2")c.variables[v->key]="none";
        if(c.psx&&std::string(v->key)=="pcsx_rearmed_memcard1")c.variables[v->key]="libretro";
        if(c.psx&&std::string(v->key)=="pcsx_rearmed_bios")c.variables[v->key]=c.testHle?"HLE":"auto";
        if(c.psx&&std::string(v->key)=="pcsx_rearmed_frameskip_type")c.variables[v->key]="disabled";
        if(std::string(v->key)=="picodrive_input1")c.variables[v->key]="6 button pad";
        // Leave thread placement to the OS on desktop, constrained test containers
        // and H700; do not inherit the core's fixed CPU 0/1 affinity defaults.
        if(std::string(v->key)=="supafaust_thread_affinity_emu" || std::string(v->key)=="supafaust_thread_affinity_ppu")c.variables[v->key]="0x0";
      }
    }
    return true;
  }
  case RETRO_ENVIRONMENT_GET_VARIABLE: {
    auto *v = static_cast<retro_variable *>(data);
    auto i = c.variables.find(v->key);
    v->value = i == c.variables.end() ? nullptr : i->second.c_str();
    return i != c.variables.end();
  }
  case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
    *static_cast<bool *>(data) = false;
    return true;
  case RETRO_ENVIRONMENT_GET_LANGUAGE:
    *static_cast<unsigned *>(data) = RETRO_LANGUAGE_ENGLISH;
    return true;
  default:
    return false;
  }
}
void Core::video(const void *data, unsigned w, unsigned h, size_t pitch) {
  auto &c = *active;
  if (!data)
    return;
  if (!w || !h || w > 2048 || h > 2048 || pitch < w*(c.format==RETRO_PIXEL_FORMAT_XRGB8888?4u:2u))
    throw std::runtime_error("Oversized core frame");
  c.videoPitch=pitch;
  c.width = w;
  c.height = h;
  c.frame.resize(w * h);
  for (unsigned y = 0; y < h; y++)
    for (unsigned x = 0; x < w; x++) {
      auto row = static_cast<const uint8_t *>(data) + y * pitch;
      uint32_t pixel;
      if (c.format == RETRO_PIXEL_FORMAT_XRGB8888) {
        memcpy(&pixel, row + x * 4, 4);
        pixel |= 0xff000000;
      } else {
        uint16_t p;
        memcpy(&p, row + x * 2, 2);
        unsigned r, g, b;
        if (c.format == RETRO_PIXEL_FORMAT_RGB565) {
          r = ((p >> 11) & 31) * 255 / 31;
          g = ((p >> 5) & 63) * 255 / 63;
          b = (p & 31) * 255 / 31;
        } else {
          r = ((p >> 10) & 31) * 255 / 31;
          g = ((p >> 5) & 31) * 255 / 31;
          b = (p & 31) * 255 / 31;
        }
        pixel = 0xff000000 | (r << 16) | (g << 8) | b;
      }
      c.frame[y * w + x] = pixel;
    }
}
Json Core::videoGeometry() const {
  Json result={{"width",width},{"height",height},{"pitch_bytes",videoPitch},
    {"aspect_ratio",aspectRatio},{"base_width",geometryWidth},{"base_height",geometryHeight},
    {"geometry_changes",geometryChanges},{"fps",fps}};
  // Diagnostics only, sampled at the journal cadence, never the input hot path.
  // This is an observation of exact black pixels, not an automatic crop policy.
  if(frame.size()==size_t(width)*height) {
    int left=width,top=height,right=-1,bottom=-1;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
      if(frame[y*width+x]&0x00ffffffu){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
    result["nonblack_bounds"]=right<0?Json(nullptr):Json{{"x",left},{"y",top},{"width",right-left+1},{"height",bottom-top+1}};
  }
  return result;
}
bool Core::playScreenshotClick() {
  if(!audio || volume<=0)return false;
  // A short original two-pulse shutter sound, mixed into the existing device.
  // No extra device open, file access, blocking delay or change to core timing.
  screenshotClick=makeScreenshotClick(sampleRate,volume);screenshotClickOffset=0;
  if(paused) {
    bool ok=SDL_QueueAudio(audio,screenshotClick.data(),screenshotClick.size()*sizeof(int16_t))==0;
    screenshotClick.clear();return ok;
  }
  return true;
}
size_t Core::audioBatch(const int16_t *data, size_t n) {
  auto &c = *active;
  c.audioFrames += n;
  auto mixed = mixVolume(data, n, c.volume);
  if(!c.screenshotClick.empty()) {
    for(size_t i=0;i<mixed.size()&&c.screenshotClickOffset<c.screenshotClick.size();++i)
      mixed[i]=int16_t(std::clamp(int(mixed[i])+int(c.screenshotClick[c.screenshotClickOffset++]),-32768,32767));
    if(c.screenshotClickOffset==c.screenshotClick.size()){c.screenshotClick.clear();c.screenshotClickOffset=0;}
  }
  for (auto sample : mixed)
    c.audioPeak = std::max(c.audioPeak, std::abs(int(sample)));
  if (c.audio && SDL_GetQueuedAudioSize(c.audio) < size_t(c.sampleRate * .12 * 4))
    SDL_QueueAudio(c.audio, mixed.data(), n * 4);
  return n;
}
void Core::audioSample(int16_t l, int16_t r) {
  int16_t pair[] = {l, r};
  audioBatch(pair, 1);
}
void Core::inputPoll() {}
int16_t Core::inputState(unsigned port, unsigned device, unsigned index, unsigned id) {
  if(port!=0)return 0;
  device &= RETRO_DEVICE_MASK;
  if(device==RETRO_DEVICE_JOYPAD && index==0 && id<16)return (active->buttons>>id)&1;
  if(device==RETRO_DEVICE_ANALOG && index<2 && id<2)return active->axes[index*2+id];
  return 0;
}
void Core::psxController(bool dualshock) {
  if(!psx)return;
  stopRumble();
  // PCSX ReARMed advertises DualShock as ANALOG subclass 1, not plain ANALOG.
  sym<void (*)(unsigned,unsigned)>("retro_set_controller_port_device")(
      0,dualshock?RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG,1):RETRO_DEVICE_JOYPAD);
  buttons=0;axes={};
}
void Core::tick() {
  if (!paused) {
    try{run();}catch(...){stopRumble();throw;}
    if(rumble) {
      if(rumbleEnabled)rumble->update(rumbleStrength[0],rumbleStrength[1]);
      else {rumbleStrength={};rumble->update(0,0);}
    }
    ++frames;
    if (frames % 600 == 0)
      flushSram();
  } else {rumbleStrength={};if(rumble)rumble->update(0,0);}
}
bool Core::rumbleState(unsigned port,retro_rumble_effect effect,uint16_t strength) {
  if(!active||port!=0||(effect!=RETRO_RUMBLE_STRONG&&effect!=RETRO_RUMBLE_WEAK))return false;
  auto &c=*active;
  if(!c.rumbleEnabled||c.paused||!c.rumble||!c.rumble->available())return false;
  c.rumbleStrength[effect==RETRO_RUMBLE_STRONG?0:1]=strength;
  return true;
}
void Core::stopRumble() {
  rumbleStrength={};
  if(rumble)rumble->stop();
}
void Core::closeAudio() {
  if (audio) { SDL_ClearQueuedAudio(audio); SDL_CloseAudioDevice(audio); audio=0; }
}
void Core::openAudio() {
  if (!audioEnabled || audio) return;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO)) throw std::runtime_error(SDL_GetError());
  SDL_AudioSpec wanted{};wanted.freq=int(sampleRate);wanted.format=AUDIO_S16SYS;
  wanted.channels=2;wanted.samples=1024;
  audio=SDL_OpenAudioDevice(nullptr,0,&wanted,nullptr,0);
  if (!audio) throw std::runtime_error(SDL_GetError());
  SDL_PauseAudioDevice(audio,0);
}
void Core::silence() {
  screenshotClick.clear();screenshotClickOffset=0;
  stopRumble();
  if (audio)
    SDL_ClearQueuedAudio(audio);
}
bool Core::canSave() const { return serializeSize && serializeSize() > 0; }
void Core::save(int slot) {
  if(slot<0 || slot>2)throw std::runtime_error("Invalid manual slot");
  saveTo(saveRoot/stateName(slot),true);
}
void Core::saveAutomatic() {
  flushSram();
  saveTo(saveRoot/stateName(3),false);
}
void Core::saveTo(const fs::path &p, bool flush) {
  auto n = serializeSize();
  if (!n || n > 32 * 1024 * 1024)
    throw std::runtime_error("Save states unavailable");
  std::vector<uint8_t> b(n);
  if (!serialize(b.data(), b.size()))
    throw std::runtime_error("Serialization failed");
  long long sequence=0;
  for(int slot=0;slot<4;++slot)for(const auto &suffix:{"", ".previous"})try {
    sequence=std::max(sequence,saveSequence(readState(saveRoot/(stateName(slot)+suffix),StateRead::Metadata).meta));
  }catch(...){}
  if(sequence>=std::numeric_limits<long long>::max()-1)throw std::runtime_error("Save sequence exhausted");
  StateFile state;state.payload=std::move(b);
  state.meta={{"save_sequence",sequence+1},{"core",identity},{"rom",romHash},{"utc",time(nullptr)},
              {"preview_width",0},{"preview_height",0}};
  if(psx)state.meta["disc"]=discIndex();
  if(width>0&&height>0&&frame.size()==size_t(width)*height) {
    double scale=std::min({1.0,192.0/width,144.0/height});
    int w=psx?192:std::max(1,int(width*scale)),h=psx?144:std::max(1,int(height*scale));
    state.meta["preview_width"]=w;state.meta["preview_height"]=h;state.pixels.resize(size_t(w)*h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)state.pixels[y*w+x]=frame[(size_t(y)*height/h)*width+size_t(x)*width/w];
  }
  if(fs::exists(p)) {
    bool valid=false;try {auto old=readState(p);valid=old.meta.at("core")==identity&&old.meta.at("rom")==romHash;}catch(...){}
    // Never swallow a backup write failure or replace a valid backup with corruption.
    if(valid)atomicWrite(p.string()+".previous",readBytes(p));
  }
  writeState(p,state);
  if(flush)flushSram();
}
void Core::deleteSlot(int slot) {
  if (slot < 0 || slot > 2) throw std::runtime_error("Invalid save slot");
  const auto path = saveRoot / stateName(slot);
  // Remove the overwrite backup first; a failure keeps the active slot intact.
  fs::remove(path.string() + ".previous");
  fs::remove(path);
  syncDirectory(saveRoot);
  if (fs::exists(path)) throw std::runtime_error("Save deletion readback mismatch");
}
void Core::loadState(int slot) {
  loadFrom(saveRoot/stateName(slot));
}
void Core::loadAutomatic() { loadFrom(saveRoot / stateName(3)); }
fs::path Core::automaticPath() const {
  const auto path = saveRoot / stateName(3);
  auto state = readRecoverableState(path, StateRead::Metadata);
  if (state.meta.at("core") != identity || state.meta.at("rom") != romHash)
    throw std::runtime_error("Automatic state belongs to another ROM or core");
  return state.recovered ? fs::path(path.string() + ".previous") : path;
}
bool Core::compatibleAutomatic() const {
  try {automaticPath();return true;}catch(...){return false;}
}
void Core::loadFrom(const fs::path &path) {
  auto state = readRecoverableState(path);
  const auto &j=state.meta;
  if (j.at("core") != identity || j.at("rom") != romHash)
    throw std::runtime_error("State belongs to another ROM or core");
  const auto &b=state.payload;
  if(psx&&j.contains("disc")&&j["disc"].get<unsigned>()!=discIndex())changeDisc(j["disc"].get<unsigned>());
  if (!unserialize(b.data(), b.size()))
    throw std::runtime_error("Core rejected state");
  stateRecovered = state.recovered;
  silence();
}
bool Core::compatible(int slot) const {
  return compatibleFile(saveRoot/stateName(slot));
}
bool Core::compatibleFile(const fs::path &path) const {
  try {
    auto j = readRecoverableState(path,StateRead::Metadata).meta;
    return j.at("core") == identity && j.at("rom") == romHash;
  } catch (...) {
    return false;
  }
}
void Core::flushSram() {
  for(auto memory:{std::pair<unsigned,const char *>{RETRO_MEMORY_SAVE_RAM,"sram.bin"},
                   {RETRO_MEMORY_RTC,"rtc.bin"}}) {
    auto n=memorySize(memory.first);
    if(!n)continue;
    auto *p=static_cast<uint8_t *>(memoryData(memory.first));
    if(!p)throw std::runtime_error("Persistent core memory inaccessible");
    auto path=saveRoot/memory.second;
    auto &previous=persistedMemory[memory.first];std::error_code error;
    if(previous.size()==n&&std::equal(previous.begin(),previous.end(),p)&&fs::is_regular_file(path,error)&&fs::file_size(path,error)==n&&!error)continue;
    std::vector<uint8_t> bytes(p,p+n);
    // Preserve the last successfully loaded/written bytes, not a possibly
    // truncated on-disk file. A failed backup must stop active replacement.
    if (!previous.empty() && previous != bytes)
      atomicWrite(path.string()+".previous",previous);
    atomicWrite(path,bytes);
    previous=std::move(bytes);
  }
}
std::string Core::stateHash() {
  std::vector<uint8_t> b(serializeSize());
  if (!serialize(b.data(), b.size()))
    throw std::runtime_error("Serialization failed");
  return hashBytes(b);
}
unsigned Core::discCount() const {return psx&&disks.get_num_images?disks.get_num_images():0;}
unsigned Core::discIndex() const {return psx&&disks.get_image_index?disks.get_image_index():0;}
void Core::changeDisc(unsigned index) {
  if(index>=discCount()||!disks.set_eject_state||!disks.set_image_index)throw std::runtime_error("Disc unavailable");
  const auto old=discIndex();
  if(!disks.set_eject_state(true))throw std::runtime_error("Disc tray did not open");
  if(!disks.set_image_index(index)||!disks.set_eject_state(false)) {disks.set_image_index(old);disks.set_eject_state(false);throw std::runtime_error("Disc change failed");}
  buttons=0;silence();
}
} // namespace rondo
