// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "bluetooth.hpp"
#include "wifi.hpp"
#include "platform.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
namespace rondo {
bool bluetoothAddress(const std::string &s) {
  if(s.size()!=17) return false;
  for(size_t i=0;i<s.size();++i)
    if(i%3==2 ? s[i]!=':' : !std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
  return true;
}
static std::string clean(const std::string &s) {
  std::string result;
  for(size_t i=0;i<s.size();++i) {
    if(s[i]=='\033' && i+1<s.size() && s[i+1]=='[') {i+=2;while(i<s.size() && (s[i]<0x40 || s[i]>0x7e)) ++i;}
    else if(s[i]!='\r') result+=s[i];
  }
  return result;
}
std::vector<BluetoothDevice> bluetoothDevices(const std::string &output) {
  std::map<std::string,BluetoothDevice> devices;
  std::istringstream lines(clean(output));std::string line;
  while(std::getline(lines,line)) {
    if(line.rfind("Device ",0)!=0 || line.size()<24) continue;
    auto address=line.substr(7,17);
    if(!bluetoothAddress(address)) continue;
    std::transform(address.begin(),address.end(),address.begin(),[](unsigned char c){return std::toupper(c);});
    devices[address]={address,wifiLabel(line.size()>25?line.substr(25):address),false,false,false};
    if(devices.size()>=64) break;
  }
  std::vector<BluetoothDevice> result;
  for(auto &entry:devices) result.push_back(entry.second);
  return result;
}
BluetoothDevice bluetoothInfo(const std::string &address,const std::string &output) {
  BluetoothDevice result{address,address,false,false,false};
  auto rows=bluetoothDevices(output);
  if(rows.empty() || rows[0].address!=address) throw BluetoothError("readback");
  result.name=rows[0].name;
  std::istringstream lines(clean(output));std::string line;
  while(std::getline(lines,line)) {
    auto start=line.find_first_not_of(" \t");if(start==line.npos)continue;line=line.substr(start);
    if(line.rfind("Alias: ",0)==0) result.name=wifiLabel(line.substr(7));
    if(line=="Paired: yes")result.paired=true;
    if(line=="Trusted: yes")result.trusted=true;
    if(line=="Connected: yes")result.connected=true;
  }
  return result;
}
BluetoothState BluezBluetooth::status() {
  if(auto state=snapshot())return *state;
  const auto deadline=monotonicMs()+8000;
  BluetoothState state;state.available=true;
  auto output=clean(command({"show"},2));
  if(output.find("Controller ")==output.npos) throw BluetoothError("unavailable");
  state.enabled=output.find("Powered: yes")!=output.npos;
  if(!state.enabled) return state;
  for(const auto &device:bluetoothDevices(command({"devices"},2))) {
    if(cancelled)throw BluetoothError("cancelled");
    if(monotonicMs()>=deadline)throw BluetoothError("timeout");
    state.devices.push_back(bluetoothInfo(device.address,command({"info",device.address},2)));
  }
  std::stable_sort(state.devices.begin(),state.devices.end(),[](const auto &a,const auto &b){
    if(a.connected!=b.connected)return a.connected>b.connected;
    if(a.paired!=b.paired)return a.paired>b.paired;
    return a.name<b.name;
  });
  return state;
}
BluetoothState BluezBluetooth::enable(bool wanted) {
  if(wanted) start();
  command({"power",wanted?"on":"off"});
  auto state=status();if(state.enabled!=wanted)throw BluetoothError("readback");return state;
}
BluetoothState BluezBluetooth::scan() {
  if(!status().enabled)throw BluetoothError("off");
  command({"scan","on"},4);
  return status();
}
BluetoothState BluezBluetooth::connect(const std::string &address) {
  if(!bluetoothAddress(address))throw BluetoothError("address");
  auto before=bluetoothInfo(address,command({"info",address}));
  if(!before.paired) {
    command({"pair",address},25);
    auto paired=bluetoothInfo(address,command({"info",address}));
    if(!paired.paired)throw BluetoothError("pair");
  }
  command({"trust",address});
  command({"connect",address},20);
  auto after=bluetoothInfo(address,command({"info",address}));
  if(!after.paired || !after.trusted || !after.connected)throw BluetoothError("connect");
  return status();
}
BluetoothState BluezBluetooth::disconnect(const std::string &address) {
  if(!bluetoothAddress(address))throw BluetoothError("address");
  command({"disconnect",address});
  if(bluetoothInfo(address,command({"info",address})).connected)throw BluetoothError("readback");
  return status();
}
BluetoothState BluezBluetooth::forget(const std::string &address) {
  if(!bluetoothAddress(address))throw BluetoothError("address");
  command({"remove",address});
  // List all devices, including those currently out of range, for removal readback.
  for(const auto &device:bluetoothDevices(command({"devices"})))
    if(device.address==address)throw BluetoothError("readback");
  return status();
}
}
namespace rondo {
static std::string dbusQuoted(const std::string &line) {
  auto pos=line.find('"');if(pos==line.npos)return {};
  std::istringstream input(line.substr(pos));std::string value;input>>std::quoted(value);return value;
}
BluetoothState bluetoothObjects(const std::string &text) {
  // dbus-send's GetManagedObjects tree: each object path starts a new object.
  BluetoothState state;
  std::string path,block;
  auto flush=[&] {
    if(path=="/org/bluez/hci0") {
      state.available=block.find("org.bluez.Adapter1")!=block.npos;
    }
    if(path.rfind("/org/bluez/hci0/dev_",0)!=0 || path.find('/',20)!=path.npos || block.find("org.bluez.Device1")==block.npos)return;
    BluetoothDevice device;device.address=path.substr(20);
    std::replace(device.address.begin(),device.address.end(),'_',':');
    if(!bluetoothAddress(device.address))return;
    std::string name,alias,property;bool expectingKey=false;
    std::istringstream rows(block);std::string line;
    while(std::getline(rows,line)) {
      if(line.find("dict entry(")!=line.npos)expectingKey=true;
      if(expectingKey && line.find("string ")!=line.npos && line.find("variant")==line.npos) {
        property=dbusQuoted(line);expectingKey=false;
      }
      if(line.find("variant")!=line.npos) {
        if(property=="Name")name=dbusQuoted(line);
        if(property=="Alias")alias=dbusQuoted(line);
        if(property=="Paired")device.paired=line.find("true")!=line.npos;
        if(property=="Trusted")device.trusted=line.find("true")!=line.npos;
        if(property=="Connected")device.connected=line.find("true")!=line.npos;
        if(property=="Icon")device.input=dbusQuoted(line).rfind("input-",0)==0;
        if(property=="Class") {
          auto n=line.find("uint32 ");
          if(n!=line.npos) {try {auto cls=std::stoul(line.substr(n+7),nullptr,0);device.input=device.input || ((cls>>8)&31)==5;}catch(...) {}}
        }
      }
      if(property=="UUIDs" && (line.find("00001124-")!=line.npos || line.find("00001812-")!=line.npos))device.input=true;
    }
    auto dashed=device.address;std::replace(dashed.begin(),dashed.end(),':','-');
    auto meaningful=[&](const std::string &n){return !n.empty() && n!=device.address && n!=dashed;};
    device.named=meaningful(name) || meaningful(alias);
    device.name=wifiLabel(meaningful(alias)?alias:meaningful(name)?name:device.address);
    // Unresolved advertisements are not useful pairing targets. Keep paired
    // peripherals even out of range, and unnamed devices advertising HID.
    if((device.named || device.paired || device.input) && state.devices.size()<64)state.devices.push_back(device);
  };
  std::istringstream lines(text);std::string line,property;
  bool adapter=false;
  while(std::getline(lines,line)) {
    if(line.find_first_not_of(" \t")!=line.npos && line.compare(line.find_first_not_of(" \t"),12,"object path ")==0) {flush();path=dbusQuoted(line);block.clear();adapter=path=="/org/bluez/hci0";property.clear();}
    block+=line+"\n";
    if(adapter) {
      if(line.find("string ")!=line.npos && line.find("variant")==line.npos)property=dbusQuoted(line);
      if(line.find("variant")!=line.npos) {
        if(property=="Powered")state.enabled=line.find("true")!=line.npos;
        if(property=="Discovering")state.discovering=line.find("true")!=line.npos;
      }
    }
  }
  flush();
  if(!state.available)throw BluetoothError("unavailable");
  if(!state.enabled) {state.devices.clear();state.discovering=false;}
  std::stable_sort(state.devices.begin(),state.devices.end(),[](const auto &a,const auto &b){
    if(a.connected!=b.connected)return a.connected>b.connected;
    if(a.paired!=b.paired)return a.paired>b.paired;
    if(a.input!=b.input)return a.input>b.input;
    return a.name<b.name;
  });
  return state;
}
BluetoothProcess::BluetoothProcess(const std::vector<std::string> &arguments,std::atomic<bool> &stop):cancelled(stop) {
  if(arguments.empty())throw BluetoothError("service");
  int channel[2];if(socketpair(AF_UNIX,SOCK_STREAM,0,channel))throw BluetoothError("service");
  for(int descriptor:channel)fcntl(descriptor,F_SETFD,FD_CLOEXEC);
#ifdef __APPLE__
  int yes=1;setsockopt(channel[0],SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
#endif
  auto args=arguments;std::vector<char *> argv;for(auto &arg:args)argv.push_back(arg.data());argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
  for(int target=0;target<3;++target)posix_spawn_file_actions_adddup2(&actions,channel[1],target);
#ifdef __linux__
  std::error_code error;std::vector<int> descriptors;
  for(const auto &entry:fs::directory_iterator("/proc/self/fd",error)) {
    try {int descriptor=std::stoi(entry.path().filename().string());if(descriptor>2)descriptors.push_back(descriptor);}catch(...) {}
  }
  for(int descriptor:descriptors)if(fcntl(descriptor,F_GETFD)>=0)posix_spawn_file_actions_addclose(&actions,descriptor);
#endif
  posix_spawnattr_t attributes;posix_spawnattr_init(&attributes);
#ifdef __APPLE__
  posix_spawnattr_setflags(&attributes,POSIX_SPAWN_CLOEXEC_DEFAULT);
#endif
  pid_t child=-1;int result=posix_spawn(&child,argv[0],&actions,&attributes,argv.data(),environ);
  posix_spawnattr_destroy(&attributes);
  posix_spawn_file_actions_destroy(&actions);close(channel[1]);
  if(result) {close(channel[0]);throw BluetoothError("service");}
  fd=channel[0];pid=child;fcntl(fd,F_SETFL,O_NONBLOCK);
}
BluetoothProcess::~BluetoothProcess() {
  if(fd>=0)close(fd);
  if(pid>0) {
    int result;auto waited=waitpid(pid,&result,WNOHANG);
    if(waited==0) {kill(pid,SIGKILL);while(waitpid(pid,&result,0)<0 && errno==EINTR) {}}
  }
}
std::string BluetoothProcess::readSome(int milliseconds) {
  if(cancelled)throw BluetoothError("cancelled");
  pollfd descriptor{fd,POLLIN,0};int result;
  do {result=poll(&descriptor,1,milliseconds);}while(result<0 && errno==EINTR && !cancelled);
  if(result<0)throw BluetoothError("service");
  if(!result)return {};
  char buffer[4096];auto n=read(fd,buffer,sizeof(buffer));
  if(n==0)throw BluetoothError("closed"); // EOF is terminal, never poll a HUP in a busy loop.
  if(n<0) {if(errno==EAGAIN || errno==EINTR)return {};throw BluetoothError("service");}
  return std::string(buffer,size_t(n));
}
std::string BluetoothProcess::request(const std::string &command,const std::vector<std::string> &success,int milliseconds) {
  // Drain notifications from the previous operation without retaining them.
  for(int i=0;i<64;++i) {pollfd pending{fd,POLLIN,0};if(poll(&pending,1,0)<=0)break;readSome(0);}
  auto input=command+"\n";size_t sent=0;auto deadline=monotonicMs()+milliseconds;
  while(sent<input.size()) {
    if(cancelled)throw BluetoothError("cancelled");
#ifdef MSG_NOSIGNAL
    auto n=send(fd,input.data()+sent,input.size()-sent,MSG_NOSIGNAL);
#else
    auto n=send(fd,input.data()+sent,input.size()-sent,0);
#endif
    if(n>0)sent+=size_t(n);
    else if(errno==EAGAIN || errno==EINTR) {if(monotonicMs()>=deadline)throw BluetoothError("timeout");pollfd d{fd,POLLOUT,0};poll(&d,1,25);}
    else throw BluetoothError("service");
  }
  std::string output;
  while(monotonicMs()<deadline) {
    output+=readSome(25);
    if(output.size()>256*1024)throw BluetoothError("response");
    auto plain=clean(output);
    if(plain.find("Failed to ")!=plain.npos || plain.find("org.bluez.Error.")!=plain.npos || plain.find("No default controller available")!=plain.npos)throw BluetoothError("command");
    for(const auto &token:success)if(plain.find(token)!=plain.npos)return plain;
  }
  throw BluetoothError("timeout");
}
std::string BluetoothProcess::finish(int milliseconds) {
  shutdown(fd,SHUT_WR);std::string output;auto deadline=monotonicMs()+milliseconds;
  bool closed=false;int result=0;
  while(monotonicMs()<deadline) {
    if(cancelled)throw BluetoothError("cancelled");
    if(!closed) {
      try {output+=readSome(25);}catch(const BluetoothError &e) {if(std::string(e.what())!="closed")throw;closed=true;}
    }
    if(output.size()>256*1024)throw BluetoothError("response");
    auto waited=waitpid(pid,&result,WNOHANG);
    if(waited==pid) {
      pid=-1;
      // A service launcher can leave daemon descendants holding stdin open.
      // Completion belongs to the owned child, not those unrelated services.
      while(!closed) {
        pollfd pending{fd,POLLIN,0};if(poll(&pending,1,0)<=0)break;
        try {output+=readSome(0);}catch(const BluetoothError &e) {if(std::string(e.what())!="closed")throw;closed=true;}
        if(output.size()>256*1024)throw BluetoothError("response");
      }
      if(!WIFEXITED(result) || WEXITSTATUS(result))throw BluetoothError("command");
      return output;
    }
    if(waited<0 && errno!=EINTR)throw BluetoothError("service");
    if(closed)poll(nullptr,0,10);
  }
  throw BluetoothError("timeout");
}
}
#ifdef __linux__
namespace rondo {
class H700Bluetooth final : public BluezBluetooth {
  fs::path helper;
  std::unique_ptr<BluetoothProcess> client;
  BluetoothState readState() {
    BluetoothProcess process({"/usr/bin/env","LC_ALL=C","/usr/bin/dbus-send","--system","--print-reply","--reply-timeout=1500","--dest=org.bluez","/","org.freedesktop.DBus.ObjectManager.GetManagedObjects"},cancelled);
    return bluetoothObjects(process.finish(2000));
  }
  std::optional<BluetoothState> snapshot() override {return readState();}
  std::string command(const std::vector<std::string> &args,int timeout) override {
    auto op=args.at(0);
    if(op=="info" || op=="devices") {
      auto state=readState();std::string output;
      for(const auto &device:state.devices) {
        if(op=="info" && device.address!=args.at(1))continue;
        output+="Device "+device.address+" "+device.name+"\n";
        if(op=="info")output+="\tAlias: "+device.name+"\n\tPaired: "+(device.paired?"yes":"no")+"\n\tTrusted: "+(device.trusted?"yes":"no")+"\n\tConnected: "+(device.connected?"yes":"no")+"\n";
      }
      return output;
    }
    try {
      if(!client) {
        client=std::make_unique<BluetoothProcess>(std::vector<std::string>{"/usr/bin/env","LC_ALL=C","/usr/bin/bluetoothctl"},cancelled);
        // The daemon is ready before this session starts. The list reply also
        // confirms that bluetoothctl has populated its adapter proxy.
        client->request("list",{"Controller "},3000);
        client->request("agent NoInputNoOutput",{"Agent registered","Agent is already registered"},3000);
        client->request("default-agent",{"Default agent request successful"},3000);
      }
      std::string input;for(const auto &arg:args) {if(!input.empty())input+=' ';input+=arg;}
      std::vector<std::string> success;
      if(op=="power")success={"Changing power "+args.at(1)+" succeeded"};
      else if(op=="scan")success={args.at(1)=="on"?"Discovery started":"Discovery stopped"};
      else if(op=="pair")success={"Pairing successful","AlreadyExists"};
      else if(op=="trust")success={"trust succeeded"};
      else if(op=="connect")success={"Connection successful","AlreadyConnected"};
      else if(op=="disconnect")success={"Successful disconnected","NotConnected"};
      else if(op=="remove")success={"Device has been removed"};
      else throw BluetoothError("command");
      auto output=client->request(input,success,timeout*1000);
      if(op=="power" && args.at(1)=="off")client.reset();
      return output;
    }catch(...) {client.reset();throw;}
  }
  void start() override {BluetoothProcess process({"/bin/sh",helper.string()},cancelled);process.finish(15000);}
public:
  explicit H700Bluetooth(fs::path script):helper(std::move(script)){}
  void stopScan() override {
    if(!client)return;
    try {if(readState().discovering)command({"scan","off"},3);}
    catch(...) {client.reset();throw;}
  }
};
}
#endif
namespace rondo {
std::shared_ptr<BluetoothAdapter> bluetoothAdapter(const fs::path &helper,bool active) {
#ifdef __linux__
  if(active && fs::is_regular_file("/etc/baseos-release") && fs::is_regular_file("/usr/bin/bluetoothctl") && fs::is_regular_file("/usr/bin/dbus-send") && fs::is_regular_file(helper))
    return std::make_shared<H700Bluetooth>(helper);
#else
  (void)helper;(void)active;
#endif
  return std::make_shared<BluetoothAdapter>();
}
}
