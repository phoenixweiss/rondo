// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <cassert>
#include <iostream>
#include <thread>
using namespace rondo;
struct FakeClock final : ClockAdapter {
  ClockState state{true, false, false, 0};
  bool fail = false;
  int starts = 0;
  int writes = 0;
  long long writtenUtc = 0;
  bool rtc = true, setFails = false;
  bool supported() const override { return true; }
  ClockState status() override { return state; }
  ClockState enable(bool value) override {
    if (fail) {
      fail = false;
      throw ClockError("service");
    }
    state.enabled = value;
    state.synchronized = false;
    state.syncUtc = 0;
    return state;
  }
  ClockState restart() override {
    ++starts;
    return enable(true);
  }
  ClockState setTime(long long utc) override {
    ++writes;
    enable(false);
    if (setFails) throw ClockError("set");
    writtenUtc = utc;
    auto result = state;
    result.manualUtc = utc; result.rtcSaved = rtc;
    return result;
  }
};
static void wait(App &app, const std::function<bool()> &done) {
  auto deadline = monotonicMs() + 6000;
  while (monotonicMs() < deadline) {
    app.clockTick();
    if (done() && !app.clockJob.valid())
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  assert(!"Clock operation timed out");
}
int main(int argc, char **argv) {
  assert(argc == 3);
  const std::string platform = "BASEOS_TARGET=rg34xxsp\nBASEOS_PLATFORM=h700\n";
  for (auto version : {"1.3.0", "1.5.2", "9.9.9", ""})
    assert(clockBaseosPlatformSupported(platform + "BASEOS_VERSION=" + version + "\n", "ID=baseos\n"));
  assert(!clockBaseosPlatformSupported(platform, "ID=other\n"));
  assert(!clockBaseosPlatformSupported("BASEOS_PLATFORM=h700\nBASEOS_TARGET=rg35xxsp\n", "ID=baseos\n"));
  assert(!clockBaseosPlatformSupported("BASEOS_TARGET=rg34xxsp\n", "ID=baseos\n"));
  assert(!clockBaseosPlatformSupported("BASEOS_PLATFORM=other\nBASEOS_TARGET=rg34xxsp\n", "ID=baseos\n"));
  assert(!clockBaseosPlatformSupported("", "ID=baseos\n"));
  Options o;
  o.assets = fs::absolute(argv[1]);
  o.state = argv[2];
  o.output = o.state / "renders";
  o.headless = true;
  App a(o);
  assert(!a.clockState.available && a.store.data["ntp_auto"] == true);
  a.action("MENU");
  for (int i = 0; i < 3; ++i)
    a.action("DOWN");
  a.action("A");
  assert(a.screen == Screen::Clock);
  a.capture("unavailable");
  auto device = std::make_shared<FakeClock>();
  a.clockDevice = device;
  a.clockState = device->enable(true);
  a.clockPollAt = monotonicMs() + 10000;
  a.wifi.enabled = true;
  a.wifi.address = "192.0.2.2";
  a.action("LEFT");
  wait(a, [&] { return !a.store.data["ntp_auto"].get<bool>(); });
  assert(!device->state.enabled);
  a.requestSettings = [](Json) { return false; };
  a.action("RIGHT");
  wait(a, [&] { return a.clockError == "save" && !device->state.enabled; });
  assert(a.store.data["ntp_auto"] == false);
  a.requestSettings = {};
  a.row = 1;
  a.action("A");
  wait(a, [&] { return a.clockManual && device->state.enabled; });
  assert(a.store.data["ntp_auto"] == false);
  device->state.synchronized = true;
  device->state.syncUtc = 1760000000;
  a.clockPollAt = 0;
  wait(a, [&] { return a.clockLastSync == 1760000000 && !device->state.enabled; });
  assert(!a.clockManual);
  assert(readJson(o.state / "Time/last-sync.json")["utc"] == 1760000000);
  for (auto language : {"en", "ru"}) {
    a.locale.select(o.assets, language);
    a.capture(std::string("updated-") + language);
    a.action("X");
    a.capture(std::string("info-") + language);
    a.action("B");
  }
  a.row = 1;
  a.wifi.address.clear();
  int starts = device->starts;
  a.action("A");
  assert(a.clockError == "network" && device->starts == starts);
  a.wifi.address = "192.0.2.2";
  a.action("A");
  wait(a, [&] { return a.clockManual; });
  a.clockDeadline = monotonicMs() - 1;
  a.clockPollAt = 0;
  wait(a, [&] { return a.clockError == "timeout" && !device->state.enabled; });
  assert(!a.clockManual);
  a.action("A");
  wait(a, [&] { return a.clockManual; });
  a.sleeping = true;
  a.clockSleep();
  a.sleeping = false;
  assert(!device->state.enabled && !a.clockManual);
  a.row = 0;
  a.action("RIGHT");
  wait(a, [&] { return a.store.data["ntp_auto"].get<bool>(); });
  assert(device->state.enabled);
  a.wifi.address.clear();
  a.clockNetworkWasOnline = false;
  a.clockPollAt = monotonicMs() + 10000;
  for (auto language : {"en", "ru"}) {
    a.locale.select(o.assets, language);
    a.capture(std::string("waiting-") + language);
  }
  assert(a.clockPhase() == "waiting");
  // Date validation is in the selected IANA zone, including leap days and DST.
  a.zones.applyHost("Etc/UTC");
  assert(clockDays(2024,2)==29 && clockDays(2026,2)==28 && clockDays(2100,2)==28);
  auto rejects = [&](ClockFields fields, const std::string &error) {
    try { clockUtc(fields); assert(false); } catch (const ClockError &e) { assert(e.what()==error); }
  };
  rejects({2026,2,29,12,0},"invalid");
  rejects({2019,1,1,0,0},"invalid");
  rejects({2026,1,1,24,0},"invalid");
  assert(clockUtc({2024,2,29,0,0})==1709164800);
  a.zones.applyHost("Europe/London");
  rejects({2026,3,29,1,30},"invalid");
  rejects({2026,10,25,1,30},"ambiguous");
  a.zones.applyHost("Asia/Kathmandu");
  assert(clockUtc({2026,1,1,5,45})==1767225600);

  a.row=2; a.action("A");
  assert(a.screen==Screen::ClockEdit);
  a.clockFields={2024,2,29,23,59};
  a.row=2; a.action("UP"); // Year 2025 clamps February 29 to February 28.
  assert(a.clockFields.year==2025 && a.clockFields.day==28);
  a.row=4; a.action("UP"); assert(a.clockFields.minute==0);
  a.action("DOWN"); assert(a.clockFields.minute==59);
  for (auto language : {"en","ru"}) {
    a.locale.select(o.assets,language);
    a.capture(std::string("manual-edit-")+language);
  }
  a.action("B");
  assert(a.screen==Screen::Clock && a.row==2 && device->writes==0);
  a.action("A"); a.row=4;
  a.requestSettings=[](Json){return false;};
  a.action("A");
  assert(a.clockError=="save" && device->writes==0 && a.store.data["ntp_auto"]==true);
  a.requestSettings={};
  a.clockFields={2026,1,1,12,30};
  auto history=a.clockLastSync;
  a.action("A");
  wait(a,[&]{return a.clockLocalUpdated;});
  assert(a.screen==Screen::Clock && !device->state.enabled && a.clockError.empty());
  assert(a.store.data["ntp_auto"]==false && a.clockLastSync==history);
  assert(a.clockPhase()=="local");
  assert(readJson(o.state/"config.json")["ntp_auto"]==false);
  assert(readJson(o.state/"Time/last-sync.json")["utc"]==history);
  a.zones.applyHost(a.store.data.value("timezone","Etc/UTC"));
  assert(device->writtenUtc==clockUtc({2026,1,1,12,30}));
  a.clockPollAt=0;
  wait(a,[&]{return a.clockPhase()=="local";});
  for (auto language : {"en","ru"}) {
    a.locale.select(o.assets,language);
    a.capture(std::string("manual-success-")+language);
  }
  a.row=2; a.action("A"); a.row=4; device->rtc=false; a.action("A");
  wait(a,[&]{return a.clockError=="rtc";});
  assert(a.clockLocalUpdated && a.screen==Screen::Clock);
  a.capture("manual-rtc-failure-ru");
  a.row=2; a.action("A"); a.row=4; device->setFails=true; a.action("A");
  wait(a,[&]{return a.clockError=="set";});
  assert(!a.clockLocalUpdated && a.screen==Screen::ClockEdit);
  a.capture("manual-set-failure-ru");
  a.action("B");
  a.action("B");
  assert(a.screen == Screen::Settings && a.row == 3);
  assert(saveSequence(Json{{"save_sequence", "bad"}}) == 0 &&
         saveSequence(Json{{"save_sequence", -1}}) == 0);
  std::cout << "PASS: native time settings, one-shot sync without enabling automatic mode, "
               "verified history, preference failure rollback, network absence, timeout, sleep "
               "cleanup, offline date editor, cancellation, leap/DST validation, persistence "
               "before clock writes, UTC conversion, partial RTC failure, EN/RU and no host clock writes.\n";
}
