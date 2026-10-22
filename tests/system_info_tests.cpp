// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <thread>
#include <sys/stat.h>
using namespace rondo;
static void put(const fs::path &p,const std::string &s){fs::create_directories(p.parent_path());std::ofstream(p)<<s;}
static void finish(SystemInfoProcess &p){auto end=monotonicMs()+4000;while(p.busy()&&monotonicMs()<end){p.poll(monotonicMs());std::this_thread::sleep_for(std::chrono::milliseconds(2));}assert(!p.busy());}
int main(int argc,char **argv){
 assert(argc==4);fs::path work=argv[2],root=work/"fixture",app=root/"app";
 put(root/"etc/os-release","ID=baseos\n");put(root/"etc/baseos-release","BASEOS_TARGET=rg34xxsp\nBASEOS_PLATFORM=h700\nBASEOS_VERSION=1.5.2\n");
 put(root/"proc/meminfo","MemTotal: 1024000 kB\nMemAvailable: 512000 kB\n");put(root/"proc/uptime","3661.5 0\n");put(root/"proc/sys/kernel/osrelease","4.9-test\n");
 put(root/"sys/class/block/mmcblk0/size","15749120\n");put(root/"sys/class/graphics/fb0/virtual_size","720,960\n");fs::create_directories(root/"mnt/SDCARD");
 put(root/"proc/self/mounts","/dev/mmcblk0p7 /mnt/SDCARD vfat rw 0 0\n/dev/mmcblk10p1 /bogus ext4 ro 0 0\n");
 put(app/"build-info.json",Json({{"version","0.7.1"},{"source_id",std::string(64,'a')},{"inputs",{{"picodrive",{{"directory","picodrive-"+std::string(40,'b')}}}}}}).dump());put(app/"cores/pico.so","fixture");
 auto supply=root/"sys/class/power_supply";put(supply/"axp2202-battery/capacity","0\n");put(supply/"axp2202-battery/status","Discharging\n");put(supply/"axp2202-battery/charge_full","3300\n");put(supply/"axp2202-battery/voltage_now","3300000\n");put(supply/"axp2202-usb/online","0\n");
 fs::create_directories(supply/"hid-trap");if(!fs::exists(supply/"hid-trap/capacity"))assert(mkfifo((supply/"hid-trap/capacity").c_str(),0600)==0);
 Json cores=Json::array({{{"path",(app/"cores/pico.so").string()},{"name","PicoDrive"},{"input","picodrive"}}});
 auto data=systemInfoSnapshot(root,app,cores);assert(data["profile"]==true);assert(data["fields"]["ram_total"]["value"]==1048576000ull);assert(data["fields"]["uptime"]["value"]==3661);
 assert(data["cards"][0]["physical_bytes"]==8063549440ull);assert(data["cards"][0]["volumes"].size()==1);assert(data["cards"][1]["state"]=="absent");assert(data["cores"][0]["revision"]==std::string(40,'b'));assert(data["battery"]["supplies"]["axp2202-battery"]["capacity"]=="0");
 put(root/"sys/class/block/mmcblk1/size","123junk");auto unmounted=systemInfoSnapshot(root,app,cores);assert(unmounted["cards"][1]["state"]=="unmounted");assert(!unmounted["cards"][1].contains("physical_bytes"));
 put(root/"proc/self/mounts","/dev/mmcblk0p7 /absent vfat ro 0 0\n");auto missing=systemInfoSnapshot(root,app,cores);assert(missing["cards"][0]["volumes"][0]["state"]=="error");
 put(root/"external.so","external");cores[0]["path"]=(root/"external.so").string();assert(systemInfoSnapshot(root,app,cores)["cores"][0]["revision"]=="");
 SystemInfoProcess process;assert(process.start({"/bin/sleep","30"},monotonicMs(),10));assert(!process.start({"/bin/true"},monotonicMs()));finish(process);assert(process.error=="timeout");
 assert(process.start({"/bin/echo","not json"},monotonicMs()));finish(process);assert(process.error=="invalid");
 std::vector<std::string> command{argv[3],"--system-info-probe",Json({{"app",app.string()},{"cores",Json::array()}}).dump()};
#ifdef __linux__
 if(const char *loader=getenv("RONDO_RUNTIME_LOADER"))command.insert(command.begin(),loader);
#endif
 assert(process.start(command,monotonicMs()));finish(process);assert(process.error.empty());assert(process.result.contains("cards"));
 Options o;o.assets=argv[1];o.state=work/"state";o.output=work/"renders";o.headless=true;
 App a(o);auto original=a.store.data;a.action("MENU");a.settingsFocus=7;a.row=7;a.action("A");assert(a.screen==Screen::SystemInfo);assert(a.systemInfoRows().size()==4);
 a.systemInfoData=data;a.systemInfoError.clear();a.systemInfoPage="sensors";auto readings=a.systemInfoRows();assert(readings[2].value=="3300.0 mAh");
 for(const auto *language:{"en","ru"}){a.locale.select(o.assets,language);
  a.systemInfoPage="root";a.systemInfoRow=0;a.systemInfoStack.clear();a.capture(std::string("root-")+language);a.action("A");assert(a.systemInfoPage=="device");
  a.systemInfoRow=6;a.capture(std::string("device-")+language);a.action("B");assert(a.systemInfoPage=="root"&&a.systemInfoRow==0);
  for(auto page:{"system","cores","cards","card0","card0v0","power","sensors"}){a.systemInfoPage=page;a.systemInfoRow=0;a.systemInfoScroll=0;a.capture(std::string(page)+"-"+language);}
  a.systemInfoPage="system";a.systemInfoRow=1;a.action("X");a.capture(std::string("source-")+language);a.action("B");assert(!a.systemInfoSource);
  a.systemInfoPage="card0v0";a.systemInfoData["cards"]=Json::array();assert(a.systemInfoRows()[0].state=="unknown");a.systemInfoData=data;
  a.systemInfoPage="cores";a.systemInfoRow=99;a.action("A");assert(a.systemInfoRow==0);
  a.systemInfoError="timeout";a.systemInfoData=Json::object();a.systemInfoPage="device";a.capture(std::string("unavailable-")+language);a.systemInfoData=data;a.systemInfoError.clear();
 }
 a.systemInfoPage="root";a.systemInfoStack.clear();a.action("B");assert(a.screen==Screen::Settings&&a.row==7);assert(a.store.data==original);
 std::cout<<"System information: bounded helper, named supplies, card states, installed manifest and EN/RU navigation passed\n";
}
