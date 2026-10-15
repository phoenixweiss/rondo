#include "core.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace rondo;
int main(int argc, char **argv) {
  assert(argc == 4);
  Game g{"core-test", "fixture", "NES", "Original NROM", argv[2]};
  fs::path state = argv[3];
  fs::create_directories(state);
  uint8_t persisted = 0;
  {
    Core c(argv[1], g, state, true);
    for (int i = 0; i < 180; i++)
      c.tick();
    assert(c.frame.size() > 0 && c.audioFrames > 0 && c.audioPeak > 0);
    c.volume = 0;
    c.audioPeak = 0;
    for (int i = 0; i < 10; i++)
      c.tick();
    assert(c.audioPeak == 0);
    c.volume = 100;
    assert(std::adjacent_find(c.frame.begin(), c.frame.end(), std::not_equal_to<uint32_t>()) !=
           c.frame.end());
    auto hash = c.stateHash();
    c.save(0);
    assert(c.compatible(0));
    for (int i = 0; i < 60; i++)
      c.tick();
    assert(c.stateHash() != hash);
    c.loadState(0);
    assert(c.stateHash() == hash);
    c.flushSram();
    persisted = readBytes(state / "sram.bin")[0];
    assert(persisted != 0);
    auto j = readJson(state / "slot-0.json");
    j["rom"] = "other-rom";
    auto s = j.dump();
    atomicWrite(state / "slot-0.json", {s.begin(), s.end()});
    assert(!c.compatible(0));
    bool rejected = false;
    try {
      c.loadState(0);
    } catch (...) {
      rejected = true;
    }
    assert(rejected);
  }
  {
    Core c(argv[1], g, state, true);
    c.flushSram();
    assert(readBytes(state / "sram.bin")[0] == persisted);
  }
  std::cout << "Real FCEUmm: video/audio callbacks, state roundtrip, wrong-ROM "
               "rejection and SRAM restart passed.\n";
}
