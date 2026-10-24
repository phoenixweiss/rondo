// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "battery_log.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <chrono>
#include <fcntl.h>
#include <thread>
using namespace rondo;
static void put(const fs::path &path, const std::string &value) {
  fs::create_directories(path.parent_path()); std::ofstream(path) << value;
}
static std::vector<Json> rows(const fs::path &path) {
  std::ifstream input(path); std::string line; std::vector<Json> out;
  while (std::getline(input, line)) {
    try { auto tab=line.find('\t');out.push_back(Json::parse(tab==std::string::npos?line:line.substr(tab+1))); } catch (...) {}
  }
  return out;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  fs::path root = argv[1], sys = root / "sys", proc = root / "proc";
  auto battery = sys / "class/power_supply/axp2202-battery";
  auto usb = sys / "class/power_supply/axp2202-usb";
  put(battery / "type", "Battery\n"); put(battery / "capacity", "85\n");
  put(battery / "status", "Full\n"); put(battery / "voltage_now", "4180000\n");
  put(battery / "current_now", "-12000\n"); put(battery / "time_to_full_now", "0\n");
  put(usb / "online", "1\n"); put(usb / "type", "USB\n");
  put(proc / "sys/kernel/random/boot_id", "fixture-boot\n");
  put(proc / "uptime", "120.25 100.00\n");
  Json before = batterySnapshot(sys, proc), fields = before["supplies"]["axp2202-battery"];
  assert(fields["capacity"] == "85" && fields["status"] == "Full");
  assert(fields["voltage_now"] == "4180000" && fields["current_now"] == "-12000");
  assert(fields["time_to_full_now"] == "0" && fields["charge_full"].is_null());
  assert(before["supplies"]["axp2202-usb"]["online"] == "1");
  assert(before["boot_id"] == "fixture-boot");
  {
    // A foreign capacity (or even type) may block inside a HID driver. FIFOs
    // without a writer prove these files are never opened, not just rate-limited.
    auto foreign=sys/"class/power_supply/hid-private-address-battery";
    auto unknown=sys/"class/power_supply/unrecognized-device";
    fs::create_directories(foreign);fs::create_directories(unknown);
    for(const auto &path:{foreign/"capacity",foreign/"type",unknown/"capacity",unknown/"type"})
      assert(mkfifo(path.c_str(),0600)==0);
    pid_t child=fork();assert(child>=0);
    if(child==0) {
      alarm(2);
      auto snapshot=batterySnapshot(sys,proc);
      if(snapshot["supplies"].size()!=2 || snapshot["supplies"]["axp2202-battery"]["capacity"]!="85" || snapshot.dump().find("private-address")!=std::string::npos)_exit(2);
      BatteryLog journal(root/"peripheral-guard","shell",sys,proc);
      if(!journal.sample(1,Json::object(),"action:DOWN"))_exit(3);
      alarm(0);_exit(0);
    }
    int status=0;while(waitpid(child,&status,0)<0 && errno==EINTR){}
    assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
    assert(rows(root/"peripheral-guard/shell.log").size()==1);
    assert(hashFile(battery/"capacity")==hashBytes({'8','5','\n'}));
  }
  {
    // Hold a known PMIC read in the kernel. Producers must still return, with
    // bounded memory even under a flood of events; no timing claim about SD.
    pid_t child=fork();assert(child>=0);
    if(child==0) {
      alarm(10);
      fs::remove(battery/"capacity");assert(mkfifo((battery/"capacity").c_str(),0600)==0);
      BatteryLog journal(root/"async-stall","runner",sys,proc,2*1024*1024,{},true);
      journal.sample(1,Json::object());
      int writer=-1;
      for(int i=0;i<1000 && writer<0;++i) {
        writer=open((battery/"capacity").c_str(),O_WRONLY|O_NONBLOCK);
        if(writer<0)usleep(1000);
      }
      assert(writer>=0); // Worker reached the deliberately blocked read.
      auto before=std::chrono::steady_clock::now();
      for(int i=0;i<10000;++i)journal.sample(i+2,{{"screen","running"}},"test:event");
      auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
      assert(elapsed<500); // Far below one wait per event; generous for slow CI.
      fs::remove(battery/"capacity");put(battery/"capacity","85\n");
      assert(write(writer,"85\n",3)==3);close(writer);
      journal.drain();auto entries=rows(root/"async-stall/runner.log");
      assert(entries.size()<=65 && entries.size()>1);
      assert(entries.back()["context"]["diagnostic_events_dropped"].get<int>()>0);
      std::cout<<"PASS: 10000 diagnostic submissions during blocked PMIC read: "<<elapsed<<" ms, bounded queue.\n"<<std::flush;
      alarm(0);_exit(0);
    }
    int status=0;while(waitpid(child,&status,0)<0 && errno==EINTR){}
    assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
  }
  put(battery / "temp", "broken\n");
  assert(batterySnapshot(sys, proc)["supplies"]["axp2202-battery"]["temp"] == "broken");
  put(battery / "health", std::string(513, 'x'));
  auto invalid = batterySnapshot(sys, proc);
  assert(invalid["supplies"]["axp2202-battery"]["health"].is_null());
  assert(invalid["errors"]["axp2202-battery/health"] == "too_large");
  auto absent = batterySnapshot(root / "absent", root / "absent");
  assert(absent["supplies"].empty() && absent["boot_id"].is_null());
  fs::remove(battery / "health");
  // Runtime DT linkage, big-endian values, binary/NUL preservation and privacy.
  auto tree = sys / "firmware/devicetree/base";
  auto node = tree / "soc/twi/pmu/bat-power-supply";
  auto target = tree / "axp2202-parameter";
  auto be32 = [](uint32_t n) { return std::string{char(n >> 24), char(n >> 16), char(n >> 8), char(n)}; };
  put(tree / "model", std::string("RG34XX SP\0", 10));
  put(tree / "serial-number", "PRIVATE-SERIAL-DO-NOT-LOG");
  put(tree / "chosen/bootargs", "PRIVATE-BOOT-ARGUMENTS");
  put(proc / "cmdline", "PRIVATE-CMDLINE");
  put(proc / "sys/kernel/osrelease", "4.9.170-test\n");
  put(proc / "sys/kernel/version", "#1 test build\n");
  put(node / "compatible", std::string("x-powers,axp2202-bat-power-supply") + char(0));
  put(node / "pmu_battery_cap", be32(3300));
  put(node / "pmu_init_chgvol", be32(4100));
  put(node / "param", be32(77));
  put(target / "phandle", be32(77));
  put(target / "select", std::string("battery-model\0", 14));
  std::string modelBytes; for (int n=0; n<128; ++n) modelBytes += char(n);
  put(target / "battery-model/parameter", modelBytes);
  auto pass = batteryPassport(sys, proc);
  auto selected = pass["battery_nodes"][0];
  assert(pass["read_only"] == true && pass["pmic_model_read"] == false);
  assert(pass["kernel_release"] == "4.9.170-test");
  assert(selected["properties"]["pmu_battery_cap"]["u32"] == 3300);
  assert(selected["properties"]["pmu_init_chgvol"]["u32"] == 4100);
  assert(selected["selected_model"]["node"] == "axp2202-parameter");
  assert(selected["selected_model"]["parameter"]["bytes"] == 128);
  assert(selected["selected_model"]["parameter"]["sha256"] == hashBytes({modelBytes.begin(),modelBytes.end()}));
  assert(selected["selected_model"]["parameter"]["hex"].get<std::string>().substr(0,8) == "00010203");
  assert(pass.dump().find("PRIVATE") == std::string::npos);
  assert(pass["power_kernel"]["available"] == false);
  auto modelHash = hashFile(target / "battery-model/parameter");
  put(target / "select", "../outside");
  auto unsafe = batteryPassport(sys, proc);
  assert(unsafe["errors"]["model_select"] == "unsafe_name_or_symlink");
  assert(unsafe["battery_nodes"][0]["selected_model"]["parameter"].is_null());
  put(target / "select", "battery-model");
  fs::rename(target / "battery-model", target / "real-model");
  fs::create_directory_symlink(target / "real-model", target / "battery-model");
  assert(batteryPassport(sys, proc)["errors"]["model_select"] == "unsafe_name_or_symlink");
  fs::remove(target / "battery-model");fs::rename(target / "real-model", target / "battery-model");
  put(target / "battery-model/parameter", std::string(4097, 'x'));
  assert(batteryPassport(sys, proc)["errors"]["model_parameter"] == "too_large");
  put(target / "battery-model/parameter", modelBytes);
  put(target / "phandle", be32(78));
  assert(batteryPassport(sys, proc)["errors"]["model_reference"] == "missing");
  put(target / "phandle", be32(77));
  put(tree / "duplicate/phandle", be32(77));
  assert(batteryPassport(sys, proc)["errors"]["model_reference"] == "ambiguous");
  fs::remove_all(tree / "duplicate");
  assert(batteryPassport(root / "missing", proc)["errors"]["battery_node"] == "not_found");
  std::string kernel = "wifi: PRIVATE-NETWORK-NAME\naxp2202: model_update ok\n";
  for (int n=0;n<70;++n) kernel += "AXP2202 gauge_soc " + std::to_string(n) + std::string(600,'x') + "\n";
  auto messages = batteryPowerMessages(kernel);
  assert(messages.size() == 64 && messages[0].get<std::string>().find("gauge_soc 6") != std::string::npos);
  assert(messages.dump().find("PRIVATE") == std::string::npos && messages.back().get<std::string>().size() == 512);
  assert(hashFile(target / "battery-model/parameter") == modelHash);
  put(battery / "charge_counter", "2805000\n");
  assert(batterySnapshot(sys, proc)["supplies"]["axp2202-battery"]["charge_counter"] == "2805000");
  auto log = root / "logs";
  Json context = {{"screen", "library"}};
  BatteryLog shell(log,"shell",sys,proc);
  assert(shell.sample(100,context,"start"));
  auto events=rows(log/"shell.log");assert(events.size()==1&&events[0]["event"]=="start");
  assert(!fs::exists(log/"shell.jsonl"));
  assert(shell.sample(101,context));
  assert(rows(log/"shell.jsonl").size()==1);
  assert(!rows(log/"shell.jsonl")[0].contains("battery_passport")&&!rows(log/"shell.jsonl")[0].contains("storage"));
  assert(shell.sample(5101,context));assert(rows(log/"shell.jsonl").size()==1);
  assert(shell.sample(60101,context));assert(rows(log/"shell.jsonl").size()==2);
  assert(shell.sample(60102,context,"battery_passport"));
  assert(rows(log/"shell.jsonl").back()["battery_passport"]["battery_nodes"][0]["selected_model"]["parameter"]["bytes"]==128);
  // Ordinary events never touch even known PMIC paths, DT or mount tables.
  fs::rename(battery/"capacity",battery/"saved-capacity");assert(mkfifo((battery/"capacity").c_str(),0600)==0);
  pid_t quick=fork();assert(quick>=0);
  if(quick==0){alarm(2);BatteryLog eventsOnly(root/"no-pmic","runner",sys,proc);
    if(!eventsOnly.sample(1,context,"start")||!eventsOnly.sample(2,context,"action:A"))_exit(2);_exit(0);}
  int quickStatus=0;while(waitpid(quick,&quickStatus,0)<0&&errno==EINTR){}
  assert(WIFEXITED(quickStatus)&&WEXITSTATUS(quickStatus)==0);
  fs::remove(battery/"capacity");fs::rename(battery/"saved-capacity",battery/"capacity");
  BatteryLog runner(log,"runner",sys,proc);
  assert(runner.sample(1,context,"start"));
  {std::ofstream out(log/"runner.log",std::ios::app);out<<"broken";}
  assert(runner.sample(2,context,"stop"));assert(rows(log/"runner.log").size()==2);
  auto rowSize=fs::file_size(log/"shell.log");
  BatteryLog rotated(root/"rotated","shell",sys,proc,rowSize*2+256);
  for(int i=0;i<10;++i)assert(rotated.sample(i,context,"sample"));
  assert(fs::exists(root/"rotated/shell.previous.log"));
  assert(fs::file_size(root/"rotated/shell.log")<=rowSize*2+256);
  auto blockedDir=root/"blocked-target";put(blockedDir,"file");auto ram=root/"ram";
  BatteryLog fallback(blockedDir,"runner",sys,proc,2*1024*1024,ram);
  assert(!fallback.sample(0,context,"sleep_after")&&!fallback.lastError.empty());
  auto retained=rows(ram/"runner.log");assert(retained.size()==1&&retained[0]["event"]=="sleep_after");
  fs::remove(blockedDir);fs::create_directories(blockedDir);
  BatteryLog recovered(blockedDir,"runner",sys,proc,2*1024*1024,ram);
  assert(recovered.sample(6000,context,"start"));
  assert(rows(blockedDir/"runner.recovered.log").size()==1&&!fs::exists(ram/"runner.log"));
  put(blockedDir/"keep-me","unrelated");
  put(blockedDir/"runner.jsonl","old telemetry\n");
  clearDiagnosticLogs(blockedDir,ram);
  assert(!fs::exists(blockedDir/"runner.log")&&!fs::exists(blockedDir/"runner.jsonl")&&!fs::exists(blockedDir/"runner.recovered.log")&&fs::exists(blockedDir/"keep-me"));
  // The sampled gauge must not overwrite a different value submitted to the UI.
  put(battery / "capacity", "10\n");
  Json display = {{"screen", "game_menu"}, {"header_visible", true},
      {"percentage_visible", true}, {"source_percent", 100},
      {"displayed_percent", 100}, {"label", "100%"},
      {"rendered_monotonic_ms", 10}, {"submitted_monotonic_ms", 20}};
  Json comparedContext = {{"battery_display", display}};
  BatteryLog compared(root / "compared", "runner", sys, proc);
  assert(compared.sample(100, comparedContext));
  auto pair = rows(root / "compared/runner.jsonl").back()["battery_comparison"];
  assert(pair["system_percent"] == 10 && pair["display"]["displayed_percent"] == 100);
  assert(pair["display"]["label"] == "100%" && pair["display_age_ms"] == 80);
  // Rendering 60 FPS changes timestamps, not the journal's meaningful-change key.
  for (uint64_t now = 116; now <= 10100; now += 16) {
    comparedContext["battery_display"]["submitted_monotonic_ms"] = now;
    assert(compared.sample(now, comparedContext));
  }
  assert(rows(root / "compared/runner.jsonl").size() == 1);
  comparedContext["battery_display"]["source_percent"] = 10;
  comparedContext["battery_display"]["displayed_percent"] = 10;
  comparedContext["battery_display"]["label"] = "10%";
  assert(compared.sample(15100, comparedContext));
  assert(rows(root / "compared/runner.jsonl").size() == 2);
  comparedContext["battery_display"]["percentage_visible"] = false;
  comparedContext["battery_display"]["displayed_percent"] = nullptr;
  comparedContext["battery_display"]["label"] = nullptr;
  assert(compared.sample(20100, comparedContext));
  pair = rows(root / "compared/runner.jsonl").back()["battery_comparison"];
  assert(pair["system_percent"] == 10 && pair["display"]["displayed_percent"].is_null());
  put(battery / "capacity", "invalid\n");
  assert(compared.sample(25100, comparedContext));
  assert(rows(root / "compared/runner.jsonl").back()["battery_comparison"]["system_percent"].is_null());
  put(battery / "capacity", "85\n");
  bool rejected = false;
  try { BatteryLog invalidRole(log, "../bad", sys, proc); } catch (...) { rejected = true; }
  assert(rejected);
  // Sampling never rewrites a gauge value or calibrates its percentage.
  assert(batterySnapshot(sys, proc)["supplies"]["axp2202-battery"]["capacity"] == "85");
  std::cout << "PASS: raw 85%-Full contradiction, signed/missing/invalid fields, time gating, USB changes, lifecycle, rotation, truncated lines and non-fatal writes.\n";
}
