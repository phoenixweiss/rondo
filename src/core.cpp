// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "core.hpp"
#include <limits>
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
static Json envelope(const fs::path &p) {
  auto j = readJson(p);
  auto payload = j.at("payload").get<std::vector<uint8_t>>();
  if (j.at("sha256") != hashBytes(payload))
    throw std::runtime_error("State checksum mismatch");
  return j;
}
Core::Core(const fs::path &path, const Game &game, const fs::path &saves, bool silent)
    : saveRoot(saves), systemRoot(saves / "bios") {
  identity = hashFile(path);
  rom = readBytes(game.path);
  romHash = hashBytes(rom);
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
    auto launch = game.path.string();
    retro_game_info content{launch.c_str(), info.need_fullpath ? nullptr : rom.data(),
                            info.need_fullpath ? 0 : rom.size(), nullptr};
    if (!load(&content))
      throw std::runtime_error("Core rejected the ROM");
    loaded = true;
    retro_system_av_info av{};
    sym<void (*)(retro_system_av_info *)>("retro_get_system_av_info")(&av);
    if(std::isfinite(av.geometry.aspect_ratio) && av.geometry.aspect_ratio>0 && av.geometry.aspect_ratio<=8)aspectRatio=av.geometry.aspect_ratio;
    fps = av.timing.fps;
    sampleRate = av.timing.sample_rate;
    if (!std::isfinite(fps) || fps < 1 || fps > 240)
      throw std::runtime_error("Invalid core frame rate");
    for(auto memory:{std::pair<unsigned,const char *>{RETRO_MEMORY_SAVE_RAM,"sram.bin"},
                     {RETRO_MEMORY_RTC,"rtc.bin"}}) {
      if(!fs::exists(saveRoot/memory.second))continue;
      auto bytes=readBytes(saveRoot/memory.second);auto size=memorySize(memory.first);
      if(size && bytes.size()==size && memoryData(memory.first))memcpy(memoryData(memory.first),bytes.data(),size);
      else if(size)throw std::runtime_error("Incompatible persistent memory size");
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
bool Core::environment(unsigned cmd, void *data) {
  auto &c = *active;
  switch (cmd) {
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
        // The handheld GBA policy uses wall time and hardware-rate audio;
        // link-cable sessions are outside this frontend's supported scope.
        if(std::string(v->key)=="gpsp_rtc_time_source")c.variables[v->key]="system";
        if(std::string(v->key)=="gpsp_sound_rate")c.variables[v->key]="32768";
        if(std::string(v->key)=="gpsp_serial")c.variables[v->key]="disabled";
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
  if (w > 2048 || h > 2048)
    throw std::runtime_error("Oversized core frame");
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
size_t Core::audioBatch(const int16_t *data, size_t n) {
  auto &c = *active;
  c.audioFrames += n;
  auto mixed = mixVolume(data, n, c.volume);
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
int16_t Core::inputState(unsigned port, unsigned device, unsigned, unsigned id) {
  return port == 0 && device == RETRO_DEVICE_JOYPAD && id < 16 ? ((active->buttons >> id) & 1) : 0;
}
void Core::tick() {
  if (!paused) {
    run();
    ++frames;
    if (frames % 600 == 0)
      flushSram();
  }
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
  if (audio)
    SDL_ClearQueuedAudio(audio);
}
bool Core::canSave() const { return serializeSize && serializeSize() > 0; }
void Core::save(int slot) {
  if(slot<0 || slot>2)throw std::runtime_error("Invalid manual slot");
  saveTo(saveRoot/("slot-"+std::to_string(slot)+".json"),true);
}
void Core::saveAutomatic() {
  flushSram();
  saveTo(saveRoot/"autosave.json",false);
}
void Core::saveTo(const fs::path &p, bool flush) {
  auto n = serializeSize();
  if (!n || n > 32 * 1024 * 1024)
    throw std::runtime_error("Save states unavailable");
  std::vector<uint8_t> b(n);
  if (!serialize(b.data(), b.size()))
    throw std::runtime_error("Serialization failed");
  long long sequence=0;
  for(int i=0;i<3;++i)try {sequence=std::max(sequence,saveSequence(readJson(saveRoot/("slot-"+std::to_string(i)+".json"))));}catch(...){}
  for(const auto *name:{"autosave.json","autosave.json.previous"})
    try {sequence=std::max(sequence,saveSequence(readJson(saveRoot/name)));}catch(...){}
  if(sequence>=std::numeric_limits<long long>::max()-1)throw std::runtime_error("Save sequence exhausted");
  Json j = {{"save_sequence",sequence+1},{"schema", 1},  {"core", identity},       {"rom", romHash},
            {"payload", b}, {"sha256", hashBytes(b)}, {"utc", time(nullptr)}};
  if (width>0 && height>0 && frame.size()==size_t(width)*height) {
    auto bytes=reinterpret_cast<const uint8_t *>(frame.data());
    j["preview"]={{"width",width},{"height",height},{"pixels",frame},
                  {"sha256",hashBytes({bytes,bytes+frame.size()*sizeof(uint32_t)})}};
  }
  if (fs::exists(p) && (p.filename()!="autosave.json" || compatibleFile(p)))
    atomicWrite(p.string() + ".previous", readBytes(p));
  auto s = j.dump();
  atomicWrite(p, {s.begin(), s.end()});
  auto actual = envelope(p);
  if (actual["sha256"] != j["sha256"] || actual["save_sequence"] != j["save_sequence"])
    throw std::runtime_error("Save readback mismatch");
  if (j.contains("preview") && actual.at("preview")!=j.at("preview"))
    throw std::runtime_error("Save preview readback mismatch");
  if(flush)flushSram();
}
void Core::deleteSlot(int slot) {
  if (slot < 0 || slot > 2) throw std::runtime_error("Invalid save slot");
  const auto path = saveRoot / ("slot-" + std::to_string(slot) + ".json");
  // Remove the overwrite backup first; a failure keeps the active slot intact.
  fs::remove(path.string() + ".previous");
  fs::remove(path);
  if (fs::exists(path)) throw std::runtime_error("Save deletion readback mismatch");
}
void Core::loadState(int slot) {
  loadFrom(saveRoot/("slot-"+std::to_string(slot)+".json"));
}
void Core::loadAutomatic() { loadFrom(automaticPath()); }
fs::path Core::automaticPath() const {
  for(const auto *name:{"autosave.json","autosave.json.previous"})
    if(compatibleFile(saveRoot/name))return saveRoot/name;
  throw std::runtime_error("Automatic state unavailable");
}
bool Core::compatibleAutomatic() const {
  try {automaticPath();return true;}catch(...){return false;}
}
void Core::loadFrom(const fs::path &path) {
  auto j = envelope(path);
  if (j.at("core") != identity || j.at("rom") != romHash)
    throw std::runtime_error("State belongs to another ROM or core");
  auto b = j.at("payload").get<std::vector<uint8_t>>();
  if (!unserialize(b.data(), b.size()))
    throw std::runtime_error("Core rejected state");
  silence();
}
bool Core::compatible(int slot) const {
  return compatibleFile(saveRoot/("slot-"+std::to_string(slot)+".json"));
}
bool Core::compatibleFile(const fs::path &path) const {
  try {
    auto j = envelope(path);
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
    atomicWrite(saveRoot/memory.second,{p,p+n});
  }
}
std::string Core::stateHash() {
  std::vector<uint8_t> b(serializeSize());
  if (!serialize(b.data(), b.size()))
    throw std::runtime_error("Serialization failed");
  return hashBytes(b);
}
} // namespace rondo
