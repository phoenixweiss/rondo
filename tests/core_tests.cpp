#include "state.hpp"
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
    c.save(1);c.save(1);assert(fs::exists(state/"slot-1.state.previous"));
    c.deleteSlot(1);assert(!fs::exists(state/"slot-1.state")&&!fs::exists(state/"slot-1.state.previous"));
    c.deleteSlot(1); // Repeating an acknowledged deletion remains harmless.
    auto manual=readBytes(state/"slot-0.state");
    c.saveAutomatic();assert(c.compatibleAutomatic());auto automaticHash=c.stateHash();
    for(int i=0;i<12;++i)c.tick();c.loadAutomatic();assert(c.stateHash()==automaticHash);
    for(int i=0;i<12;++i)c.tick();c.saveAutomatic();
    assert(readBytes(state/"slot-0.state")==manual && fs::exists(state/"autosave.state.previous"));
    atomicWrite(state/"autosave.state",{'b','a','d'});assert(c.compatibleAutomatic());
    c.loadAutomatic();assert(c.stateHash()==automaticHash);
    // Valid metadata with a damaged payload must fall back before unserialize.
    atomicWrite(state/"autosave.state",readBytes(state/"autosave.state.previous"));
    auto corrupt=readBytes(state/"autosave.state");corrupt.back()^=1;atomicWrite(state/"autosave.state",corrupt);
    c.loadAutomatic();assert(c.stateHash()==automaticHash);
    c.saveAutomatic();assert(c.compatibleAutomatic());
    c.flushSram();
    auto stamp=fs::last_write_time(state/"sram.bin");c.flushSram();
    assert(fs::last_write_time(state/"sram.bin")==stamp);
    auto savedRam=readBytes(state/"sram.bin");fs::remove(state/"sram.bin");c.flushSram();assert(readBytes(state/"sram.bin")==savedRam);
    persisted = readBytes(state / "sram.bin")[0];
    assert(persisted != 0);
    auto stateFile = readState(state / "slot-0.state");
    stateFile.meta["rom"] = "other-rom";
    writeState(state / "slot-0.state",stateFile);
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
