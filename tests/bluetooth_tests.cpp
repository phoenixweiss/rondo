// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <cassert>
#include <fstream>
#include <thread>
#include <iostream>
#include <unistd.h>
#include <sys/resource.h>
using namespace rondo;
struct FakeBluez : BluezBluetooth {
  bool on=false, reject=false, acknowledge=true;
  int starts=0, scans=0;
  std::map<std::string,BluetoothDevice> entries;
  std::vector<std::vector<std::string>> calls;
  void start() override {++starts;}
  std::string command(const std::vector<std::string> &args,int=8) override {
    calls.push_back(args);
    if(reject)throw BluetoothError("command");
    auto op=args.at(0);
    if(op=="show")return std::string("Controller 00:11:22:33:44:55 Rondo\n\tPowered: ")+(on?"yes\n":"no\n");
    if(op=="power"){if(acknowledge)on=args.at(1)=="on";return "Changing power succeeded\n";}
    if(op=="devices") {std::string out;for(const auto &e:entries)out+="Device "+e.first+" "+e.second.name+"\n";return out;}
    if(op=="scan"){++scans;return "Discovery started\n";}
    if(op=="remove"){if(acknowledge)entries.erase(args.at(1));return "Device removed\n";}
    auto &device=entries.at(args.at(1));
    if(op=="info")return "Device "+device.address+"\n\tAlias: "+device.name+"\n\tPaired: "+(device.paired?"yes":"no")+"\n\tTrusted: "+(device.trusted?"yes":"no")+"\n\tConnected: "+(device.connected?"yes":"no")+"\n";
    if(acknowledge) {
      if(op=="pair")device.paired=true;
      if(op=="trust")device.trusted=true;
      if(op=="connect")device.connected=true;
      if(op=="disconnect")device.connected=false;
    }
    return "success\n";
  }
};
static void wait(App &app) {
  const auto deadline=monotonicMs()+3000;
  while(app.btBusy() && monotonicMs()<deadline){app.btTick();SDL_Delay(1);}
  assert(!app.btBusy());
}
int main(int argc,char **argv) {
  if(argc==2 && std::string(argv[1])=="--transport") {
    std::string input;
    while(std::getline(std::cin,input)) {
      if(input=="close") {close(0);close(1);close(2);SDL_Delay(5000);return 0;}
      if(input=="timeout") {SDL_Delay(5000);return 0;}
      if(input=="flood") {for(int i=0;i<80;++i)std::cout<<std::string(4096,'x')<<std::flush;continue;}
      std::cout<<"[CHG] asynchronous notification\nreply:"<<input<<":"<<getpid()<<"\n"<<std::flush;
    }
    std::cout<<"finished\n"<<std::flush;return 0;
  }
  assert(argc==3);
  {
    std::atomic<bool> cancelled{false};auto fixture=fs::absolute(argv[0]).string();
    BluetoothProcess process({fixture,"--transport"},cancelled);
    auto one=process.request("one",{"reply:one:"},1000);
    auto two=process.request("two",{"reply:two:"},1000);
    auto firstPid=one.substr(one.find("reply:one:")+10);
    auto secondPid=two.substr(two.find("reply:two:")+10);assert(firstPid==secondPid);
    assert(process.finish(1000).find("finished")!=std::string::npos);
    for(const auto &mode:{"close","timeout","flood"}) {
      BluetoothProcess client({fixture,"--transport"},cancelled);bool failed=false;auto start=monotonicMs();
      try {client.request(mode,{"never"},100);}catch(const BluetoothError &e) {failed=true;
        assert(std::string(e.what())==(std::string(mode)=="close"?"closed":std::string(mode)=="flood"?"response":"timeout"));}
      assert(failed && monotonicMs()-start<1000);
    }
    BluetoothProcess client({fixture,"--transport"},cancelled);
    std::thread stop([&]{SDL_Delay(30);cancelled=true;});bool failed=false;
    try {client.request("timeout",{"never"},3000);}catch(const BluetoothError &e) {failed=std::string(e.what())=="cancelled";}
    stop.join();assert(failed);
  }
  {
    const auto objects=R"DBUS(method return
 array [
  dict entry(
   object path "/org/bluez/hci0"
   array [ dict entry( string "org.bluez.Adapter1"
    array [
      dict entry( string "Powered"
        variant boolean true )
      dict entry( string "Discovering"
        variant boolean true )
    ] ) ] )
  dict entry(
   object path "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"
   array [ dict entry( string "org.bluez.Device1"
    array [
      dict entry( string "Name"
        variant string "8BitDo Micro gamepad" )
      dict entry( string "Alias"
        variant string "8BitDo Micro gamepad" )
      dict entry( string "Icon"
        variant string "input-gaming" )
      dict entry( string "Paired"
        variant boolean true )
      dict entry( string "Trusted"
        variant boolean true )
      dict entry( string "Blocked"
        variant boolean false )
      dict entry( string "Connected"
        variant boolean false )
      dict entry( string "Adapter"
        variant object path "/org/bluez/hci0" )
    ] ) ] )
  dict entry(
   object path "/org/bluez/hci0/dev_10_20_30_40_50_60"
   array [ dict entry( string "org.bluez.Device1"
     array [dict entry( string "Alias"
       variant string "10-20-30-40-50-60" )] ) ] )
  dict entry(
   object path "/org/bluez/hci0/dev_10_20_30_40_50_61"
   array [ dict entry( string "org.bluez.Device1"
     array [dict entry( string "UUIDs"
       variant array [
        string "00001812-0000-1000-8000-00805f9b34fb"
       ] )] ) ] )
 ])DBUS";
    auto parsed=bluetoothObjects(objects);
    assert(parsed.available && parsed.enabled && parsed.discovering && parsed.devices.size()==2);
    assert(parsed.devices[0].name=="8BitDo Micro gamepad" && parsed.devices[0].paired && parsed.devices[0].trusted && !parsed.devices[0].connected && parsed.devices[0].input);
    assert(!parsed.devices[1].named && parsed.devices[1].input);
    bool failed=false;try{bluetoothObjects("method return array []");}catch(const BluetoothError &){failed=true;}assert(failed);
  }
const std::string address="AA:BB:CC:DD:EE:FF";
  assert(bluetoothAddress(address) && !bluetoothAddress("AA:BB;rm -rf /") && !bluetoothAddress(address+"\n"));
  auto rows=bluetoothDevices("\033[0;92mDevice AA:BB:CC:DD:EE:FF Controller\033[0m\nDevice invalid injected\nDevice AA:BB:CC:DD:EE:FF Gamepad\n");
  assert(rows.size()==1 && rows[0].name=="Gamepad");
  FakeBluez protocol;protocol.entries[address]={address,"Controller",false,false,false};
  auto state=protocol.enable(true);assert(state.enabled && protocol.starts==1);
  protocol.scan();assert(protocol.scans==1);
  state=protocol.connect(address);assert(state.devices[0].paired && state.devices[0].trusted && state.devices[0].connected);
  state=protocol.disconnect(address);assert(!state.devices[0].connected);
  auto calls=protocol.calls.size();bool rejected=false;
  try{protocol.connect(address+";echo");}catch(const BluetoothError &){rejected=true;}
  assert(rejected && protocol.calls.size()==calls);
  protocol.acknowledge=false;rejected=false;
  try{protocol.enable(false);}catch(const BluetoothError &){rejected=true;}
  assert(rejected && protocol.on);
  rejected=false;try{protocol.forget(address);}catch(const BluetoothError &){rejected=true;}assert(rejected);
  protocol.acknowledge=true;assert(protocol.forget(address).devices.empty());
  Options options;options.assets=fs::absolute(argv[1]);options.state=argv[2];options.output=options.state/"renders";options.headless=true;
  App app(options);assert(!app.bt.available && !app.store.data.value("bluetooth_enabled",true));
  auto fake=std::make_shared<FakeBluez>();fake->entries[address]={address,"Очень длинное имя беспроводного контроллера",false,false,false};
  for(int i=0;i<7;++i) {auto id="10:20:30:40:50:0"+std::to_string(i);fake->entries[id]={id,"Controller "+std::to_string(i),true,true,false};}
  app.btDevice=fake;app.bt.available=true;app.btPollAt=monotonicMs()+15000;
  app.action("MENU");if(app.screen==Screen::LibraryMenu)app.action("A");for(int i=0;i<4;++i)app.action("DOWN");app.action("A");app.action("DOWN");app.action("A");
  assert(app.screen==Screen::Bluetooth);
  app.requestSettings=[](Json){return false;};app.action("RIGHT");wait(app);assert(!fake->on && !app.store.data["bluetooth_enabled"].get<bool>());
  app.requestSettings={};app.action("RIGHT");wait(app);assert(fake->on && app.bt.enabled && app.store.data["bluetooth_enabled"]==true);
  for(auto language:{"en","ru"}) {app.locale.select(options.assets,language);app.capture(std::string("radio-")+language);}
  app.action("DOWN");app.action("DOWN");app.action("A");wait(app);assert(fake->scans==1 && app.screen==Screen::BluetoothDevices);
  for(int i=0;i<10;++i) {app.action("DOWN");}
  app.capture("devices-bottom-ru");assert(app.btScroll>0);
  app.action("A");assert(app.btSelected.address==address);app.action("A");wait(app);
  assert(app.btSelected.connected && app.store.data["bluetooth_device"]==address);
  app.capture("connected-ru");app.btSleep();fake->entries[address].connected=false;app.btResume();wait(app);assert(fake->entries[address].connected);
  app.action("DOWN");app.action("DOWN");app.action("A");assert(app.screen==Screen::BluetoothForget);app.capture("forget-ru");app.action("B");assert(fake->entries.count(address));
  app.action("A");app.action("DOWN");app.action("A");wait(app);assert(!fake->entries.count(address) && app.store.data["bluetooth_device"]=="");
  app.action("B");app.row=0;app.action("LEFT");wait(app);assert(!app.bt.enabled && !fake->on);
  app.action("B");assert(app.screen==Screen::SettingsSection && app.category==5 && app.row==1);
  Store restart(options.state);assert(!restart.data.value("bluetooth_enabled",true));
  fake->on=true;app.btResume();assert(fake->on&&!app.btJob.valid());
  app.btResume(true);wait(app);assert(!fake->on&&!app.bt.enabled);
  std::cout<<"PASS: BlueZ parser, validated argv targets, readback failures, bounded discovery model, EN/RU UI, preference rollback, pairing/trust/connect/disconnect/forget, scrolling and reconnect after sleep; no host Bluetooth accessed.\n";
}
