// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <fstream>
#include <atomic>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
extern char **environ;
using namespace rondo;
static void message(int fd, const Json &j) {
  auto s = j.dump() + "\n";
  size_t n = 0;
  while (n < s.size()) {
    auto r = write(fd, s.data() + n, s.size() - n);
    if (r < 0 && errno == EINTR)
      continue;
    if (r <= 0)
      throw std::runtime_error("Session channel closed");
    n += r;
  }
}
static Json receive(int fd, int timeout = -1) {
  auto deadline = monotonicMs() + (timeout < 0 ? 0 : timeout);
  std::string s;
  char c;
  while (s.size() < 1024 * 1024) {
    if (timeout >= 0) {
      pollfd ready{fd, POLLIN, 0};
      auto now = monotonicMs();
      if (now >= deadline || poll(&ready, 1, int(deadline - now)) <= 0)
        throw std::runtime_error("Settings authority timed out");
    }
    auto n = read(fd, &c, 1);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      throw std::runtime_error("Session channel closed");
    if (c == '\n')
      return Json::parse(s);
    s += c;
  }
  throw std::runtime_error("Session message too large");
}
static std::string key(SDL_Keycode k) {
  switch (k) {
  case SDLK_VOLUMEUP:
  case SDLK_EQUALS:
    return "VOLUME_UP";
  case SDLK_VOLUMEDOWN:
  case SDLK_MINUS:
    return "VOLUME_DOWN";
  case SDLK_UP:
    return "UP";
  case SDLK_DOWN:
    return "DOWN";
  case SDLK_LEFT:
    return "LEFT";
  case SDLK_RIGHT:
    return "RIGHT";
  case SDLK_z:
  case SDLK_RETURN:
    return "A";
  case SDLK_x:
  case SDLK_ESCAPE:
    return "B";
  case SDLK_c:
    return "X";
  case SDLK_v:
    return "Y";
  case SDLK_q:
    return "L1";
  case SDLK_e:
    return "R1";
  case SDLK_TAB:
    return "MENU";
  default:
    return "";
  }
}
static std::string pad(uint8_t b) {
  switch (b) {
  case SDL_CONTROLLER_BUTTON_A:
    return "A";
  case SDL_CONTROLLER_BUTTON_B:
    return "B";
  case SDL_CONTROLLER_BUTTON_X:
    return "X";
  case SDL_CONTROLLER_BUTTON_Y:
    return "Y";
  case SDL_CONTROLLER_BUTTON_DPAD_UP:
    return "UP";
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
    return "DOWN";
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    return "LEFT";
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
    return "RIGHT";
  case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
    return "L1";
  case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
    return "R1";
  case SDL_CONTROLLER_BUTTON_GUIDE:
  case SDL_CONTROLLER_BUTTON_BACK:
    return "MENU";
  default:
    return "";
  }
}
static void held(App &a) {
  if (!a.core || a.screen != Screen::Running)
    return;
  const auto *k = SDL_GetKeyboardState(nullptr);
  uint16_t mask = 0;
  auto put = [&](int id, bool on) {
    if (on)
      mask |= uint16_t(1 << id);
  };
  put(RETRO_DEVICE_ID_JOYPAD_A, k[SDL_SCANCODE_Z]);
  put(RETRO_DEVICE_ID_JOYPAD_B, k[SDL_SCANCODE_X]);
  put(RETRO_DEVICE_ID_JOYPAD_START, k[SDL_SCANCODE_RETURN]);
  put(RETRO_DEVICE_ID_JOYPAD_SELECT, k[SDL_SCANCODE_RSHIFT]);
  put(RETRO_DEVICE_ID_JOYPAD_UP, k[SDL_SCANCODE_UP]);
  put(RETRO_DEVICE_ID_JOYPAD_DOWN, k[SDL_SCANCODE_DOWN]);
  put(RETRO_DEVICE_ID_JOYPAD_LEFT, k[SDL_SCANCODE_LEFT]);
  put(RETRO_DEVICE_ID_JOYPAD_RIGHT, k[SDL_SCANCODE_RIGHT]);
  put(RETRO_DEVICE_ID_JOYPAD_X, k[SDL_SCANCODE_S]);
  put(RETRO_DEVICE_ID_JOYPAD_Y, k[SDL_SCANCODE_A]);
  put(RETRO_DEVICE_ID_JOYPAD_L, k[SDL_SCANCODE_Q]);
  put(RETRO_DEVICE_ID_JOYPAD_R, k[SDL_SCANCODE_E]);
  auto &raw = *a.ui->handheldKeys;
  put(RETRO_DEVICE_ID_JOYPAD_A, raw.pressed("A"));
  put(RETRO_DEVICE_ID_JOYPAD_B, raw.pressed("B"));
  put(RETRO_DEVICE_ID_JOYPAD_START, raw.pressed("START"));
  put(RETRO_DEVICE_ID_JOYPAD_SELECT, raw.pressed("SELECT"));
  put(RETRO_DEVICE_ID_JOYPAD_UP, raw.pressed("UP"));
  put(RETRO_DEVICE_ID_JOYPAD_DOWN, raw.pressed("DOWN"));
  put(RETRO_DEVICE_ID_JOYPAD_LEFT, raw.pressed("LEFT"));
  put(RETRO_DEVICE_ID_JOYPAD_RIGHT, raw.pressed("RIGHT"));
  put(RETRO_DEVICE_ID_JOYPAD_X, raw.pressed("X"));
  put(RETRO_DEVICE_ID_JOYPAD_Y, raw.pressed("Y"));
  put(RETRO_DEVICE_ID_JOYPAD_L, raw.pressed("L1"));
  put(RETRO_DEVICE_ID_JOYPAD_R, raw.pressed("R1"));
  mask |= a.ui->controllerButtons();
  a.core->buttons = a.gameButtons(mask);
}
static void events(App &a) {
  for (auto &action:a.ui->powerKeys->poll()) a.action(action);
  for (auto &action : a.ui->handheldKeys->poll()) {
    a.ui->inputHints(false);
    a.action(action);
  }
  for (auto &action : a.ui->volumeKeys->poll())
    a.action(action);
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if(a.screenBlank && (e.type==SDL_KEYDOWN || e.type==SDL_TEXTINPUT || e.type==SDL_CONTROLLERBUTTONDOWN || e.type==SDL_JOYBUTTONDOWN)) {
      a.activity();SDL_FlushEvents(SDL_KEYDOWN,SDL_TEXTINPUT);continue;
    }
    if(e.type==SDL_TEXTINPUT && a.activity())continue;
    if(e.type==SDL_CONTROLLERDEVICEADDED || e.type==SDL_CONTROLLERDEVICEREMOVED ||
       e.type==SDL_CONTROLLERDEVICEREMAPPED || e.type==SDL_JOYDEVICEADDED || e.type==SDL_JOYDEVICEREMOVED)
      a.ui->refreshControllers();
    a.ui->controllerEvent(e);
    if (e.type == SDL_QUIT) {
      if (a.core) {
        a.screen = Screen::GameMenu;
        a.core->paused = true;
        a.action("DOWN");
        a.row = 4;
        a.action("A");
      } else
        a.stop = true;
    }
    if(e.type==SDL_TEXTINPUT && a.screen==Screen::Search && !a.ui->handheldKeys->available()) {a.ui->inputHints(true);a.searchText(e.text.text);continue;}
    if(e.type==SDL_KEYDOWN && a.screen==Screen::Search && !a.ui->handheldKeys->available()) {
      a.ui->inputHints(true);
      auto k=e.key.keysym.sym;
      if(k==SDLK_BACKSPACE)a.action("SEARCH_DELETE");
      else if(k==SDLK_RETURN)a.action("SEARCH_SUBMIT");
      else if(k==SDLK_ESCAPE)a.action("B");
      else if(k==SDLK_UP || k==SDLK_DOWN || k==SDLK_LEFT || k==SDLK_RIGHT)a.action(key(k));
      continue;
    }
    if(e.type==SDL_TEXTINPUT && a.screen==Screen::WifiPassword){a.ui->inputHints(true);a.wifiText(e.text.text);continue;}
    if(e.type==SDL_KEYDOWN && a.screen==Screen::WifiPassword && !a.ui->handheldKeys->available()) {
      a.ui->inputHints(true);
      if(e.key.keysym.sym==SDLK_BACKSPACE)a.action("WIFI_DELETE");
      else if(e.key.keysym.sym==SDLK_RETURN)a.action("WIFI_SUBMIT");
      else if(e.key.keysym.sym==SDLK_ESCAPE)a.action("B");
      else if(e.key.keysym.sym==SDLK_UP || e.key.keysym.sym==SDLK_DOWN || e.key.keysym.sym==SDLK_LEFT || e.key.keysym.sym==SDLK_RIGHT)a.action(key(e.key.keysym.sym));
      continue;
    }
    if (e.type == SDL_KEYDOWN) {
      if (a.ui->handheldKeys->available())
        continue; // The handheld's evdev pad must not also generate keyboard actions.
      if (e.key.repeat && e.key.keysym.sym != SDLK_UP && e.key.keysym.sym != SDLK_DOWN &&
          key(e.key.keysym.sym) != "VOLUME_UP" && key(e.key.keysym.sym) != "VOLUME_DOWN")
        continue;
      auto action = key(e.key.keysym.sym);
      if (!action.empty() && action != "VOLUME_UP" && action != "VOLUME_DOWN")
        a.ui->inputHints(true);
      if (a.ui->volumeKeys->available() && (action == "VOLUME_UP" || action == "VOLUME_DOWN"))
        continue;
      if (!action.empty())
        a.action(action);
    }
    if (e.type == SDL_JOYBUTTONDOWN && !a.ui->volumeKeys->available()) {
      if (e.jbutton.button == 16)
        a.action("VOLUME_UP");
      if (e.jbutton.button == 15)
        a.action("VOLUME_DOWN");
    }
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
      auto *controller = SDL_GameControllerFromInstanceID(e.cbutton.which);
      const char *name = controller ? SDL_GameControllerName(controller) : nullptr;
      if (name && std::string(name) == "ANBERNIC-keys") continue;
      auto action = pad(e.cbutton.button);
      // Keep Select usable in NES; Guide or Start+Back opens the game menu.
      if(e.cbutton.button==SDL_CONTROLLER_BUTTON_BACK)
        action=a.screen==Screen::Running?(controller && SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_START)?"MENU":""):"B";
      if(e.cbutton.button==SDL_CONTROLLER_BUTTON_START && controller && SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_BACK))action="MENU";
      if (!action.empty()) {
        a.ui->inputHints(false);
        a.action(action);
      }
    }
  }
  if(a.screen!=Screen::Running)for(const auto &action:a.ui->controllerNavigation()) {
    a.ui->inputHints(false);a.action(action);
  }

}
static void script(App &a, const Json &commands, const std::function<void(const Json &)> &after) {
  for (auto &c : commands) {
    if (c.contains("action"))
      a.action(c.at("action"));
    if (c.contains("text") && a.screen==Screen::Search)
      a.searchText(c.at("text"));
    if (c.contains("assert_visible") && a.visible().size()!=c.at("assert_visible").get<size_t>())
      throw std::runtime_error("Library result count mismatch");
    if (c.contains("frames"))
      a.ticks(c.at("frames"));
    if (c.contains("wait_ms"))
      SDL_Delay(c.at("wait_ms").get<unsigned>());
    if(c.value("flush_volume",false))a.volumeTick(true);
    if (c.contains("capture"))
      a.capture(c.at("capture"));
    if (c.contains("assert_screen") && a.screenName() != c.at("assert_screen"))
      throw std::runtime_error("Unexpected screen: " + a.screenName());
    if (c.value("assert_notice_empty", false) && !a.notice.empty())
      throw std::runtime_error(a.notice);
    if (c.value("assert_notice", false) && a.notice.empty())
      throw std::runtime_error("Expected a visible failure notice");
    after(c);
  }
}
static Json supervise(App &a, const Json &runnerScript) {
  int channel[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, channel))
    throw std::runtime_error("Cannot create runner channel");
  auto &g = a.game;
  Json entry = {{"id", g.id},
                {"volume", g.volume},
                {"system", g.system},
                {"title", g.title},
                {"path", g.path.string()}, {"runtime_volume", a.previewVolume}};
  std::vector<std::string> args = {a.options.executable.string(),
                                   "--runner",
                                   "--assets",
                                   a.options.assets.string(),
                                   "--state",
                                   a.options.state.string(),
                                   "--core",
                                   a.options.core.string(),
                                   "--output",
                                   a.options.output.string(),
                                   "--entry",
                                   entry.dump(),
                                   "--volume-feedback",
                                   a.volumeFeedback.json().dump(),
                                   "--channel",
                                   std::to_string(channel[1])};
  auto gbCore=a.options.coreFor("GB");
  if(!gbCore.empty()) {args.push_back("--gb-core");args.push_back(gbCore.string());}
  auto gbaCore=a.options.coreFor("GBA");
  if(!gbaCore.empty()) {args.push_back("--gba-core");args.push_back(gbaCore.string());}
  auto snesCore=a.options.coreFor("SNES");
  if(!snesCore.empty()) {args.push_back("--snes-core");args.push_back(snesCore.string());}
  if (a.options.headless)
    args.push_back("--headless");
  if (a.resume)
    args.push_back("--resume");
  if (!runnerScript.is_null()) {
    auto p = a.options.state / "runner-script.json";
    auto s = runnerScript.dump();
    atomicWrite(p, {s.begin(), s.end()});
    args.insert(args.end(), {"--script", p.string()});
  }
  // FAT copies can have no executable bit. Match BaseOS's loader-based launch.
  std::string spawnExecutable = a.options.executable.string();
#ifdef __linux__
  if (const char *loader = getenv("RONDO_RUNTIME_LOADER")) {
    spawnExecutable = loader;
    args.insert(args.begin(), spawnExecutable);
  }
#endif
  std::vector<char *> argv;
  for (auto &s : args)
    argv.push_back(s.data());
  argv.push_back(nullptr);
  a.draw();
  a.ui.reset();
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addclose(&actions, channel[0]);
  pid_t pid;
  int error = posix_spawn(&pid, spawnExecutable.c_str(), &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(channel[1]);
  Json result;
  std::string failure, historyWarning;
  bool runnerReady=false;
  if (error) {
    close(channel[0]);
    failure = "Runner spawn failed";
  } else {
    try {
      bool done = false;
      while (!done) {
        // Complete asynchronous boot changes even while the runner owns SDL.
        a.ticks(0);
        pollfd ready{channel[0], POLLIN, 0};
        int available = poll(&ready, 1, 50);
        if (available < 0 && errno == EINTR) continue;
        if (available < 0) throw std::runtime_error("Runner channel poll failed");
        if (!available) continue;
        auto reply = receive(channel[0]);
        auto type = reply.value("type", "");
        if (type == "ready") {
          if (!runnerReady) {
            runnerReady=true;
            try { a.rememberLaunch(); }
            catch (const std::exception &e) {
              historyWarning=a.locale.tr("recent.write_failed");
              std::cerr << "Recent history: " << e.what() << "\n";
            }
          }
          a.record("runner_ready");
        }
        else if (type == "settings" || type == "volume") {
          try {
            auto next = a.store.data;
            if(type=="settings")next["game_options"] = reply.at("data").at("game_options");
            next["volume"] = type=="volume"?reply.at("volume").get<int>():reply.at("data").value("volume", 80);
            if(next["volume"].get<int>()<0 || next["volume"].get<int>()>100)throw std::runtime_error("Invalid volume");
            // Runtime audio is independent from durable settings acknowledgement.
            a.applyVolume(next["volume"].get<int>());
            a.store.commit(next);
            message(channel[0], {{"ok", true},
                                 {"request_id", reply.value("request_id", uint64_t(0))},
                                 {"data", a.store.data}});
          } catch (const std::exception &e) {
            std::cerr << "Settings authority: " << e.what() << "\n";
            a.batterySample("settings_write_failed");
            message(channel[0], {{"ok", false},
                                 {"request_id", reply.value("request_id", uint64_t(0))},
                                 {"error", e.what()}});
          }
        } else if (type == "stopped") {
          result = reply;
          if (reply.contains("feedback"))
            a.volumeFeedback.restore(reply["feedback"]);
          a.applyVolume(reply.value("runtime_volume", a.previewVolume));
          if (reply.contains("keyboard_hints"))
            SDL_setenv("RONDO_INPUT_HINTS", reply["keyboard_hints"].get<bool>() ? "keyboard" : "controller", 1);
          done = true;
        } else if (type == "error")
          throw std::runtime_error(reply.value("error", "Runner failed"));
      }
    } catch (const std::exception &e) {
      failure = e.what();
    }
    close(channel[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
      failure = "Runner failed; unsaved progress may be lost";
  }
  if (failure.empty() && result.value("game_closed",false)) {
    try { a.rememberClosedGame(); }
    catch (const std::exception &e) {
      historyWarning=a.locale.tr("recent.write_failed");
      std::cerr << "Closed-game state: " << e.what() << "\n";
    }
  }
  a.ui = std::make_unique<UI>(a.options.assets, a.options.headless);
  // Time spent in the runner is not idle time in the library.
  a.lastActivity=monotonicMs();
  a.batteryWarningsLoaded=false;
  a.slotCache.clear();
  a.launch = false;
  a.screen = failure.empty() ? Screen::Library : Screen::Recovery;
  a.notice = failure.empty() ? historyWarning : failure;
  a.record(failure.empty() ? "runner_released" : "runner_failure");
  return result;
}
int main(int argc, char **argv) {
  signal(SIGPIPE, SIG_IGN);
  int fd = -1;
  try {
    Options opts;
    opts.executable = fs::absolute(argv[0]);
    opts.assets = opts.executable.parent_path().parent_path() / "assets";
    opts.output = fs::temp_directory_path() / "rondo-preview";
    bool runner = false, resume = false;
    Json entry, initialFeedback;
    for (int i = 1; i < argc; i++) {
      std::string flag = argv[i];
      auto value = [&]() {
        if (++i >= argc)
          throw std::runtime_error("Missing argument for " + flag);
        return std::string(argv[i]);
      };
      if (flag == "--assets")
        opts.assets = fs::absolute(value());
      else if (flag == "--state")
        opts.state = fs::absolute(value());
      else if (flag == "--core")
        opts.core = fs::absolute(value());
      else if (flag == "--gb-core")
        opts.gbCore = fs::absolute(value());
      else if (flag == "--gba-core")
        opts.gbaCore = fs::absolute(value());
      else if (flag == "--snes-core")
        opts.snesCore = fs::absolute(value());
      else if (flag == "--output")
        opts.output = fs::absolute(value());
      else if (flag == "--script")
        opts.script = fs::absolute(value());
      else if (flag == "--volume") {
        auto v = value();
        auto p = v.find('=');
        if (p == v.npos)
          throw std::runtime_error("Use --volume ID=PATH");
        opts.volumes.push_back({v.substr(0, p), fs::absolute(v.substr(p + 1))});
      } else if (flag == "--headless")
        opts.headless = true;
      else if (flag == "--runner")
        runner = true;
      else if (flag == "--resume")
        resume = true;
      else if (flag == "--volume-feedback")
        initialFeedback = Json::parse(value());
      else if (flag == "--entry")
        entry = Json::parse(value());
      else if (flag == "--channel")
        fd = std::stoi(value());
      else if (flag == "--runtime-info") {
        Json j;
        SDL_version linked;
        SDL_GetVersion(&linked);
        j["sdl"] = {linked.major, linked.minor, linked.patch};
        j["ttf"] = {TTF_Linked_Version()->major, TTF_Linked_Version()->minor,
                    TTF_Linked_Version()->patch};
        j["video_drivers"] = Json::array();
        j["audio_drivers"] = Json::array();
        for (int d = 0; d < SDL_GetNumVideoDrivers(); d++)
          j["video_drivers"].push_back(SDL_GetVideoDriver(d));
        for (int d = 0; d < SDL_GetNumAudioDrivers(); d++)
          j["audio_drivers"].push_back(SDL_GetAudioDriver(d));
        std::cout << j.dump(2) << "\n";
        return 0;
      } else if (flag == "--help") {
        std::cout << "rondo --volume CARD_ID=ROMS_PATH --state NEW_STATE_PATH "
                     "--core FCEUMM_LIBRARY [--gb-core GAMBATTE_LIBRARY] [--gba-core GPSP_LIBRARY] [--snes-core SUPAFAUST_LIBRARY] [--headless --script SCRIPT.json "
                     "--output DIR]\n";
        return 0;
      } else
        throw std::runtime_error("Unknown argument: " + flag);
    }
    if (opts.state.empty())
      throw std::runtime_error("An explicit --state directory is required");
    if (!opts.script.empty() && !opts.headless)
      throw std::runtime_error("Scripted checks require --headless");
    for (auto &v : opts.volumes) {
      auto s = fs::weakly_canonical(opts.state), r = fs::weakly_canonical(v.root);
      auto rel = s.lexically_relative(r);
      if (s == r || (!rel.empty() && *rel.begin() != ".."))
        throw std::runtime_error("State must be outside source libraries");
    }
    Game game;
    if (runner)
      game = {entry.at("id"), entry.at("volume"), entry.at("system"), entry.at("title"),
              entry.at("path").get<std::string>()};
    App app(opts, runner, game);
    if (runner) app.applyVolume(entry.value("runtime_volume", app.previewVolume));
    if (!initialFeedback.is_null())
      app.volumeFeedback.restore(initialFeedback);
    Json results = Json::array();
    auto requestCounter = std::make_shared<std::atomic<uint64_t>>(0);
    if (runner) {
      if (fd < 0)
        throw std::runtime_error("Runner requires shell authority");

      app.requestVolume = [fd,requestCounter](int value) {
        auto request=++*requestCounter;
        message(fd,{{"type","volume"},{"request_id",request},{"volume",value}});
        const auto deadline=monotonicMs()+2000;
        for(;;) {
          auto now=monotonicMs();
          if(now>=deadline)throw std::runtime_error("Volume authority timed out");
          auto reply=receive(fd,int(deadline-now));
          if(reply.value("request_id",uint64_t(0))!=request)continue;
          if(!reply.value("ok",false))throw std::runtime_error("Volume authority: "+reply.value("error","write rejected"));
          return reply.at("data");
        }
      };
      app.requestSettings = [&](Json next) {
        auto request = ++*requestCounter;
        message(fd, {{"type", "settings"}, {"request_id", request}, {"data", next}});
        auto deadline = monotonicMs() + 2000;
        Json reply;
        do {
          auto now = monotonicMs();
          if (now >= deadline)
            throw std::runtime_error("Settings authority timed out");
          reply = receive(fd, int(deadline - now));
        } while (reply.value("request_id", uint64_t(0)) != request);

        if (reply.value("ok", false)) {
          app.store.data = reply.at("data");
          return true;
        }
        throw std::runtime_error("Settings authority: " + reply.value("error", "write rejected"));
      };
      if (resume) {
        int latest = app.latestResumeSlot();
        if (latest < 0)
          throw std::runtime_error("Resume state unavailable");
        app.slot = latest;
        if(latest==3)app.core->loadAutomatic();else app.core->loadState(latest);
        app.record(latest==3?"resumed_auto":"resumed_slot:"+std::to_string(latest));
      }
      app.ticks(1);
      message(fd, {{"type", "ready"}});
    }
    auto after = [&](const Json &c) {
      if (c.contains("assert_info") && app.infoOpen != c.at("assert_info").get<bool>())
        throw std::runtime_error("Setting Info focus mismatch");
      if (c.contains("assert_runtime_volume") && (app.previewVolume != c.at("assert_runtime_volume").get<int>() ||
          (app.core && app.core->volume != app.previewVolume)))
        throw std::runtime_error("Live volume mismatch");
      if (c.contains("assert_integer_scaling")) {
        bool scaling = app.store.integerScaling(app.game.id);
        if (scaling != c.at("assert_integer_scaling").get<bool>())
          throw std::runtime_error("Integer scaling acknowledgement mismatch");
      }
      if (app.launch) {
        results.push_back(supervise(app, c.value("runner_script", Json())));
      }
      if (c.contains("assert_config"))
        for (auto it = c["assert_config"].begin(); it != c["assert_config"].end(); ++it)
          if (app.store.data[it.key()] != it.value())
            throw std::runtime_error("Configuration assertion failed: " + it.key());
    };
    if (!opts.script.empty()) {
      script(app, readJson(opts.script), after);
      if (runner && !app.stop)
        throw std::runtime_error("Runner script did not confirm exit");
    } else {
      uint64_t next = SDL_GetPerformanceCounter();
      const double counterMs=1000.0/SDL_GetPerformanceFrequency();
      while (!app.stop) {
        auto inputAt=SDL_GetPerformanceCounter();
        events(app);
        held(app);
        after(Json::object());
        auto updateAt=SDL_GetPerformanceCounter();
        app.ticks(1);
        auto drawAt=SDL_GetPerformanceCounter();
        app.draw();
        auto endAt=SDL_GetPerformanceCounter();
        app.observeFrame((updateAt-inputAt)*counterMs,(drawAt-updateAt)*counterMs,(endAt-drawAt)*counterMs);
        double hz = app.core ? app.core->fps : 60;
        next += uint64_t(SDL_GetPerformanceFrequency() / hz);
        auto now = SDL_GetPerformanceCounter();
        if (next > now)
          SDL_Delay(unsigned((next - now) * 1000 / SDL_GetPerformanceFrequency()));
        else if (now - next > SDL_GetPerformanceFrequency() / 4)
          next = now;
      }
    }
    app.volumeTick(true);
    // The adapter has its own bounded deadline; finish its acknowledgement on exit.
    while (app.bootPending()) { app.ticks(0); SDL_Delay(5); }
    Json report = {{"trace", app.trace},
                   {"runner_sessions", results},
                   {"config", app.store.data},
                   {"headless", opts.headless},
                   {"hardware_verified", false}};
    if (runner) {
      app.core->flushSram();
      report["frames"] = app.core->frames;
      report["audio_frames"] = app.core->audioFrames;
      report["state_hash"] = app.core->stateHash();
      message(fd,
              {{"type", "stopped"}, {"game_closed", app.gameClosed}, {"report", report}, {"feedback", app.volumeFeedback.json()},
               {"runtime_volume", app.previewVolume},
               {"keyboard_hints", app.ui->keyboardHints}});
      close(fd);
    }
    fs::create_directories(opts.output);
    auto reportPath = opts.output / (runner ? "runner-report.json" : "shell-report.json");
    std::ofstream(reportPath) << report.dump(2) << "\n";
    std::cout << "Rondo check complete: " << reportPath << "\n";
    return app.systemExitCode;
  } catch (const std::exception &e) {
    if (fd >= 0) {
      try {
        message(fd, {{"type", "error"}, {"error", e.what()}});
      } catch (...) {
      }
      close(fd);
    }
    std::cerr << "Rondo: " << e.what() << "\n";
    return 1;
  }
}
