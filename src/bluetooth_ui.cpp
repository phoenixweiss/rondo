// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
namespace rondo {
void App::btRun(const std::string &task,std::function<BluetoothState()> operation) {
  if(btJob.valid()) {
    if(btTask=="poll" && !btQueued) {btQueuedTask=task;btQueued=std::move(operation);}
    return;
  }
  btStarted=monotonicMs();btTask=task;btJob=std::async(std::launch::async,std::move(operation));
}
void App::btSleep() {
  if(!btDevice)return;
  btQueued={};btQueuedTask.clear();btDiscoveryUntil=0;
  btDevice->cancelled=true;
  if(btJob.valid()) {btJob.wait();btTick();}
  btDevice->cancelled=false;
  try {btDevice->stopScan();}catch(...) {}
  btPollAt=0;
}
void App::btResume() {
  if(!btDevice || !btDevice->supported() || !store.data.value("bluetooth_enabled",false))return;
  auto device=btDevice;auto address=store.data.value("bluetooth_device","");
  btRun("resume",[device,address]{
    auto state=device->enable(true);
    for(const auto &entry:state.devices)
      if(entry.address==address && entry.paired && !entry.connected){auto result=device->connect(address);device->stopScan();return result;}
    return state;
  });
}
void App::btTick() {
  if(!btDevice)return;
  if(btJob.valid()) {
    if(btJob.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
    auto task=btTask;
    std::string operationError;
    try {
      std::string focus;
      if(screen==Screen::BluetoothDevices && row<int(bt.devices.size()))focus=bt.devices[row].address;
      bt=btJob.get();
      if(task=="scan")btDiscoveryUntil=monotonicMs()+30000;
      if(task=="stop_scan" || task=="connect" || (task=="toggle" && !bt.enabled))btDiscoveryUntil=0;
      if(!focus.empty())for(size_t i=0;i<bt.devices.size();++i)if(bt.devices[i].address==focus)row=int(i);
      if(task!="poll" && task!="rollback") {btError.clear();notice.clear();}
      if(task=="toggle" && btToggle) {
        bool old=store.data.value("bluetooth_enabled",false);
        try {auto next=store.data;next["bluetooth_enabled"]=*btToggle;persist(next);}
        catch(...) {btError="save";auto device=btDevice;btRun("rollback",[device,old]{return device->enable(old);});}
        btToggle.reset();
      }
      if(task=="connect" || task=="forget" || task=="disconnect") {
        auto next=store.data;
        next["bluetooth_device"]=task=="connect"?btSelected.address:"";
        try {persist(next);}catch(...){btError="save";}
      }
      row=screen==Screen::BluetoothDevices?std::clamp(row,0,std::max(0,int(bt.devices.size())-1)):row;
      for(const auto &entry:bt.devices)if(entry.address==btSelected.address)btSelected=entry;
    } catch(const BluetoothError &error) {operationError=btError=error.what();if(task=="toggle")btToggle.reset();}
      catch(...) {operationError=btError="command";if(task=="toggle")btToggle.reset();}
    btDiagnostic={{"operation",task},{"duration_ms",monotonicMs()-btStarted},{"error",operationError},
      {"enabled",bt.enabled},{"discovering",bt.discovering},{"devices",bt.devices.size()}};
    if(task!="poll" || !operationError.empty())batterySample(operationError.empty()?"bluetooth_result":"bluetooth_failed");
    if(task=="scan" && !operationError.empty())btDiscoveryUntil=0;
    btPollAt=monotonicMs()+(btDiscoveryUntil?1000:15000);
    if(btQueued && !sleeping && !btJob.valid()) {
      auto next=std::move(btQueued);btQueued={};auto name=btQueuedTask;btQueuedTask.clear();btRun(name,std::move(next));
    }
  }
  const bool discoveryVisible=screen==Screen::BluetoothDevices || screen==Screen::BluetoothDevice || screen==Screen::BluetoothForget;
  if(!btJob.valid() && !sleeping && btDiscoveryUntil && (!discoveryVisible || monotonicMs()>=btDiscoveryUntil)) {
    auto device=btDevice;btDiscoveryUntil=0;
    btRun("stop_scan",[device]{device->stopScan();return device->status();});return;
  }
  // A disabled radio needs no repeating service queries, including in its menu.
  if(ui && !sleeping && bt.available && !btJob.valid() && monotonicMs()>=btPollAt && bt.enabled) {
    auto device=btDevice;btRun("poll",[device]{return device->status();});
  }
}
void App::btAction(const std::string &action) {
  if(action=="B") {
    if(screen==Screen::Bluetooth){screen=Screen::SettingsSection;category=5;row=1;}
    else if(screen==Screen::BluetoothForget){screen=Screen::BluetoothDevice;row=2;}
    else if(screen==Screen::BluetoothDevice){screen=Screen::BluetoothDevices;row=btFocus;}
    else {screen=Screen::Bluetooth;row=1;}
    return;
  }
  const int maximum=screen==Screen::Bluetooth?2:screen==Screen::BluetoothDevices?std::max(0,int(bt.devices.size())-1):screen==Screen::BluetoothForget?1:2;
  if(action=="UP")row=std::max(0,row-1);
  if(action=="DOWN")row=std::min(maximum,row+1);
  if(btBusy())return;
  auto device=btDevice;
  if(screen==Screen::Bluetooth) {
    if(row==0 && (action=="A" || action=="LEFT" || action=="RIGHT") && bt.available) {
      bool wanted=action=="A"?!bt.enabled:action=="RIGHT";
      if(wanted!=bt.enabled) {btToggle=wanted;btRun("toggle",[device,wanted]{return device->enable(wanted);});}
    } else if(action=="A" && row==1 && bt.enabled) {screen=Screen::BluetoothDevices;row=0;btScroll=0;}
    else if(action=="A" && row==2 && bt.enabled) {screen=Screen::BluetoothDevices;row=0;btScroll=0;btRun("scan",[device]{return device->scan();});}
  } else if(screen==Screen::BluetoothDevices && action=="A" && row<int(bt.devices.size())) {
    btSelected=bt.devices[row];btFocus=row;screen=Screen::BluetoothDevice;row=0;
  } else if(screen==Screen::BluetoothDevice && action=="A") {
    auto address=btSelected.address;
    if(row==0 && bt.enabled && !btSelected.connected) btRun("connect",[device,address]{auto result=device->connect(address);device->stopScan();return result;});
    if(row==1 && btSelected.connected)btRun("disconnect",[device,address]{return device->disconnect(address);});
    if(row==2) {screen=Screen::BluetoothForget;row=0;}
  } else if(screen==Screen::BluetoothForget && action=="A") {
    if(row==0) {screen=Screen::BluetoothDevice;row=2;}
    else {auto address=btSelected.address;btRun("forget",[device,address]{return device->forget(address);});screen=Screen::BluetoothDevices;row=0;}
  }
}
void App::btDraw() {
  auto tr=[&](const std::string &key){return locale.tr(key);};
  const auto zone=store.data.value("timezone","Etc/UTC");
  std::string phase=btBusy()?tr("bt.busy"):btDiscoveryUntil?tr("bt.searching"):!bt.available?tr("bt.error.unavailable"):tr(bt.enabled?"bt.on":"bt.off");
  ui->frame(screen==Screen::Bluetooth?tr("bt.title"):screen==Screen::BluetoothForget?tr("bt.confirm"):screen==Screen::BluetoothDevice?btSelected.name:tr("bt.devices"),phase,zone);
  if(screen==Screen::Bluetooth) {
    ui->binaryRow("Bluetooth",128,row==0,bt.enabled,locale,bt.available);
    ui->row(tr("bt.devices"),182,row==1,672,"",bt.enabled,52,"chevron-right");
    ui->row(tr("bt.search"),236,row==2,672,"",bt.enabled,52,"chevron-right");
  } else if(screen==Screen::BluetoothDevices) {
    if(bt.devices.empty())ui->text(tr("bt.empty"),32,150,25,700);
    constexpr double viewport=270;const double content=bt.devices.size()*54;
    if(row*54<btScroll)btScroll=row*54;
    if(row*54+54>btScroll+viewport)btScroll=row*54+54-viewport;
    btScroll=std::clamp(btScroll,0.0,std::max(0.0,content-viewport));
    SDL_Rect clip{16,128,672,270};SDL_RenderSetClipRect(ui->renderer,&clip);
    for(size_t i=0;i<bt.devices.size();++i) {
      int y=128+int(i)*54-int(btScroll);if(y+52<=128 || y>=398)continue;
      const auto &entry=bt.devices[i];
      auto label=entry.name==entry.address?locale.tr("bt.unnamed",{{"suffix",entry.address.substr(12)}}):entry.name;
      ui->row(label,y,int(i)==row,672,tr(entry.connected?"bt.connected":entry.paired?"bt.paired":"bt.available"),true,52,"chevron-right");
    }
    SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar(content,viewport,btScroll,false);
  } else if(screen==Screen::BluetoothDevice) {
    ui->row(tr("bt.connect"),128,row==0,672,"",bt.enabled && !btSelected.connected);
    ui->row(tr("bt.disconnect"),182,row==1,672,"",btSelected.connected);
    ui->row(tr("bt.forget"),236,row==2);
  } else {
    ui->row(tr("action.cancel"),128,row==0);
    ui->row(tr("bt.forget"),182,row==1);
  }
  if(screen==Screen::Bluetooth && row==0)
    ui->hints({{"LEFT/RIGHT",tr("footer.value")},{"A",tr("footer.toggle")},{"B",tr("action.back")},{"X",tr("footer.info")}},btBusy()||!bt.available);
  else ui->hints({{"A",tr("action.select")},{"B",tr("action.back")},{"X",tr("footer.info")}},btBusy());
  if(!btError.empty() && btError!="cancelled")ui->notice(tr("bt.error."+btError),"settings");
}
}
