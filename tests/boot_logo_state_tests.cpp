#include "app.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <thread>
using namespace rondo;
int main(int argc, char **argv) {
  assert(argc == 4);
  setenv("RONDO_BOOTLOGO_TOOL", argv[3], 1);
  Options options;
  options.assets = argv[1];
  options.state = argv[2];
  options.headless = true;
  App app(options);
  auto finish = [&] {
    auto deadline = monotonicMs() + 5000;
    while (app.bootPending() && monotonicMs() < deadline) {
      app.ticks(0);
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(!app.bootPending());
  };
  app.requestBootLogo(true);
  assert(app.bootPending());
  finish();
  assert(app.bootAcknowledged == true);
  app.action("MENU"); app.action("DOWN"); app.action("DOWN");
  assert(app.row == 2);
  app.action("LEFT");
  assert(app.bootPending() && app.store.data["replace_boot_logo"] == true);
  app.action("A"); // A pending request cannot be toggled or duplicated.
  app.action("X"); assert(app.infoOpen);
  app.action("RIGHT"); assert(app.infoOpen);
  app.action("B"); assert(!app.infoOpen && app.row == 2);
  app.action("B"); // Navigation does not cancel an issued operation.
  finish();
  assert(app.screen == Screen::Library && app.bootAcknowledged == false);
  assert(app.store.data["replace_boot_logo"] == false);
  auto fail = fs::path(std::string(argv[3]) + ".fail");
  std::ofstream(fail) << "fail";
  app.action("MENU"); app.action("DOWN"); app.action("DOWN"); app.action("RIGHT");
  finish();
  assert(app.bootAcknowledged == false && app.store.data["replace_boot_logo"] == false);
  assert(!app.notice.empty());
  app.bootMessageUntil = monotonicMs() - 1;
  app.ticks(0);
  assert(app.notice.empty() && !app.bootLastError.empty());
  app.action("X"); assert(app.infoOpen);
  app.action("B"); assert(!app.infoOpen && app.row == 2);
  fs::remove(fail);
  app.action("RIGHT"); finish();
  assert(app.bootAcknowledged == true && app.store.data["replace_boot_logo"] == true);
  app.action("RIGHT");
  assert(!app.bootPending()); // Idempotent, no redundant filesystem mutation.
  std::cout << "PASS: async boot logo, pending navigation, failure readback, retry, persistence, idempotence.\n";
}
