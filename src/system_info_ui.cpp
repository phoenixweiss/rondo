// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <ctime>
namespace rondo {
static std::string scalar(const Json &v) {return v.is_string()?v.get<std::string>():v.is_null()?"":v.dump();}
static std::string amount(double v,const std::string &unit) {std::ostringstream out;out<<std::fixed<<std::setprecision(v<10?2:1)<<v<<" "<<unit;return out.str();}
std::vector<App::SystemInfoRow> App::systemInfoRows() const {
 std::vector<SystemInfoRow> rows;
 auto label=[&](const std::string &key){return locale.tr("sysinfo."+key);};
 auto add=[&](std::string key,std::string value,std::string source="",std::string state="ready",std::string page="") {
  rows.push_back({label(key),std::move(value),std::move(source),std::move(state),std::move(page)});
 };
 auto field=[&](std::string key,std::string name="") {
  if(name.empty())name=key;Json fields=systemInfoData.value("fields",Json::object());
  auto entry=fields.value(name,Json::object());std::string state=entry.value("state",systemInfoError.empty()?"unknown":"error");
  auto value=entry.value("value",Json());auto text=scalar(value);
  if(text.empty()&&state=="ready")state="unknown";
  if((name=="ram_total"||name=="ram_available")&&value.is_number_unsigned())text=amount(value.get<double>()/1048576.0,"MiB");
  if(name=="build"&&!text.empty())text=label("local")+" · "+text;
  if(name=="uptime"&&value.is_number_integer()) {auto n=value.get<long long>();text=std::to_string(n/3600)+" h "+std::to_string(n/60%60)+" min";}
  if(name=="mode") {auto comma=text.find(',');if(comma!=text.npos)text.replace(comma,1,"×");}
  add(key,text,entry.value("source",std::string()),state);
 };
 const auto cores=systemInfoData.value("cores",Json::array()),cards=systemInfoData.value("cards",Json::array());
 if(systemInfoPage=="root") {
  add("device", "", "", "ready", "device");add("system", "", "", "ready", "system");
  add("cards", "", "", "ready", "cards");add("power", "", "", "ready", "power");
 } else if(systemInfoPage=="device") {
  field("model");field("cpu");field("architecture");field("ram_total");field("ram_available");field("panel");field("mode");field("cpu_max");
 } else if(systemInfoPage=="system") {
  field("rondo");field("build");field("baseos");field("kernel");
  add("cores",std::to_string(cores.size()),"installed build manifest",systemInfoError.empty()?"ready":"error","cores");field("uptime");
  add("wifi",label(!wifi.available?"unsupported":!wifi.enabled?"disabled":wifi.phase=="online"?"connected":"enabled"),"Rondo network service snapshot");
  add("ip",wifi.address,"Rondo network service snapshot",wifi.address.empty()?"unknown":"ready");
  bool connected=std::any_of(bt.devices.begin(),bt.devices.end(),[](const auto &d){return d.connected;});
  add("bluetooth",label(!bt.available?"unsupported":!bt.enabled?"disabled":connected?"connected":"enabled"),"Rondo Bluetooth service snapshot");
 } else if(systemInfoPage=="cores") {
  for(const auto &core:cores)rows.push_back({core.value("name",""),core.value("revision",""),core.value("source",""),core.value("revision","").empty()?"unknown":"ready",""});
  if(rows.empty())add("cores","","installed core manifest",systemInfoError.empty()?"unknown":"error");
 } else if(systemInfoPage=="cards") {
  for(int i=0;i<2;++i) {
   Json card=cards.size()>size_t(i)?cards[i]:Json::object();auto state=card.value("state",systemInfoError.empty()?"unknown":"error");
   auto summary=card.contains("physical_bytes")?amount(card["physical_bytes"].get<double>()/1073741824.0,"GiB"):std::string();
   rows.push_back({"TF"+std::to_string(i+1)+" · "+label(i==0?"primary":"extra"),summary,card.value("source",""),state,state=="ready"?"card"+std::to_string(i):""});
  }
 } else if(systemInfoPage.rfind("card",0)==0 && systemInfoPage!="cards") {
  int slot=systemInfoPage[4]-'0';Json card=cards.size()>size_t(slot)?cards[slot]:Json::object();
  if(systemInfoPage.size()==5) {
   if(card.contains("physical_bytes"))add("physical",amount(card["physical_bytes"].get<double>()/1073741824.0,"GiB"),"/sys/class/block/mmcblkN/size · entire card");else add("physical","",card.value("source",""),"unknown");
   const auto volumes=card.value("volumes",Json::array());
   for(size_t i=0;i<volumes.size();++i) {auto &v=volumes[i];std::string device=v.value("device","");
    rows.push_back({device, v.value("filesystem",""),v.value("mount",""),"ready",systemInfoPage+"v"+std::to_string(i)});
   }
  } else {
   size_t index=std::stoul(systemInfoPage.substr(6));auto volumes=card.value("volumes",Json::array());if(index>=volumes.size()){add("partition","","/proc/self/mounts","unknown");return rows;}const auto &v=volumes[index];const auto source=v.value("device","")+" · "+v.value("mount","");
   add("partition",v.value("device",""),source);add("filesystem",v.value("filesystem",""),source);
   for(auto key:{"total","used","free"})add(key,v.contains(key)?amount(v[key].get<double>()/1073741824.0,"GiB"):"",source+" · statvfs",v.contains(key)?"ready":"error");
   add("access",label(v.value("readonly",true)?"readonly":"readwrite"),"/proc/self/mounts");
  }
 } else if(systemInfoPage=="power"||systemInfoPage=="sensors") {
  const auto battery=systemInfoData.value("battery",Json::object()),supplies=battery.value("supplies",Json::object());
  const auto values=supplies.value("axp2202-battery",Json::object()),errors=battery.value("errors",Json::object());
  auto sensor=[&](std::string key,std::string name,double scale=1,std::string unit="") {
   const auto value=values.value(name,Json());std::string state=value.is_null()?(errors.contains("axp2202-battery/"+name)?"error":"unsupported"):"ready";
   if(!systemInfoError.empty())state="error";std::string display=scalar(value);
   if(state=="ready"&&(scale!=1||!unit.empty()))try{size_t used;double number=std::stod(display,&used);if(used!=display.size()||!std::isfinite(number))throw std::runtime_error("number");display=amount(number/scale,unit);}catch(...){state="error";display.clear();}
   add(key,display,"axp2202-battery/"+name,state);
  };
  if(systemInfoPage=="power") {
   sensor("charge","capacity");if(!rows.back().value.empty()){try{size_t used;int n=std::stoi(rows.back().value,&used);if(used!=rows.back().value.size()||n<0||n>100)throw std::runtime_error("capacity");rows.back().value+="%";}catch(...){rows.back().state="error";rows.back().value.clear();}}
   sensor("charge_status","status");if(rows.back().value=="Charging")rows.back().value=label("charging");else if(rows.back().value=="Discharging")rows.back().value=label("discharging");else if(rows.back().value=="Full")rows.back().value=label("full");else if(rows.back().value=="Not charging")rows.back().value=label("not_charging");
   const auto usb=supplies.value("axp2202-usb",Json::object()).value("online",Json());
   add("supply",usb=="1"?label("external"):usb=="0"?label("battery"):"","axp2202-usb/online",usb=="1"||usb=="0"?"ready":"unknown");
   field("rated_capacity");sensor("voltage","voltage_now",1000000,"V");add("sensors","","", "ready","sensors");
  } else {
   // Units are standard Linux power_supply units. Signed current has no inferred
   // charge/discharge meaning; unknown driver sign conventions remain explicit.
   sensor("current","current_now",1000,"mA");sensor("temperature","temp",10,"°C");
   for(auto item:{std::pair<const char *,const char *>{"design","charge_full_design"},{"full_capacity","charge_full"},{"remaining","charge_now"}})
    if(values.contains(item.second)&&!values[item.second].is_null())sensor(item.first,item.second,systemInfoData.value("profile",false)&&std::string(item.second)=="charge_full"?1:1000,"mAh");
   for(auto item:{std::pair<const char *,const char *>{"energy_full","energy_full"},{"energy_now","energy_now"}})
    if(values.contains(item.second)&&!values[item.second].is_null())sensor(item.first,item.second,1000000,"Wh");
   sensor("cycles","cycle_count");
  }
 }
 return rows;
}
void App::systemInfoRefresh() {
 if(systemInfoJob.busy())return;
 Json cores=Json::array();for(auto item:{std::tuple<const char *,const char *,const char *>{"NES","FCEUmm","fceumm"},{"GB","Gambatte","gambatte"},{"GBA","gpSP","gpsp"},{"SNES","Supafaust","supafaust"},{"MD","PicoDrive","picodrive"}}) {
  auto path=options.coreFor(std::get<0>(item));if(!path.empty())cores.push_back({{"path",path.string()},{"name",std::get<1>(item)},{"input",std::get<2>(item)}});
 }
 std::vector<std::string> command{options.executable.string(),"--system-info-probe",Json({{"app",options.assets.parent_path().string()},{"cores",cores}}).dump()};
#ifdef __linux__
 if(const char *loader=getenv("RONDO_RUNTIME_LOADER"))command.insert(command.begin(),loader);
#endif
 systemInfoError.clear();if(!systemInfoJob.start(command,monotonicMs())){systemInfoError="start";systemInfoData=Json::object();}
}
void App::systemInfoTick() {
 if(systemInfoJob.poll(monotonicMs())) {
  systemInfoError=systemInfoJob.error;systemInfoData=systemInfoError.empty()?systemInfoJob.result:Json::object();
 }
}
void App::systemInfoAction(const std::string &action) {
 if(systemInfoSource) {if(action=="B"||action=="X")systemInfoSource=false;if(action=="UP")systemInfoSourceScroll=std::max(0,systemInfoSourceScroll-1);if(action=="DOWN")++systemInfoSourceScroll;return;}
 const auto rows=systemInfoRows();systemInfoRow=std::clamp(systemInfoRow,0,std::max(0,int(rows.size())-1));
 if(action=="UP")systemInfoRow=std::max(0,systemInfoRow-1);
 if(action=="DOWN")systemInfoRow=std::min(std::max(0,int(rows.size())-1),systemInfoRow+1);
 if(action=="B") {
  if(systemInfoStack.empty())returnToSettings();
  else {auto state=systemInfoStack.back();systemInfoStack.pop_back();systemInfoPage=std::get<0>(state);systemInfoRow=std::get<1>(state);systemInfoScroll=std::get<2>(state);}
 }
 if(action=="X"&&!rows.empty()){systemInfoSource=true;systemInfoSourceScroll=0;}
 if(action=="Y"&&systemInfoPage!="root")systemInfoRefresh();
 if(action=="A"&&!rows.empty()&&!rows[systemInfoRow].page.empty()) {
  systemInfoStack.emplace_back(systemInfoPage,systemInfoRow,systemInfoScroll);systemInfoPage=rows[systemInfoRow].page;systemInfoRow=0;systemInfoScroll=0;
 }
}
void App::systemInfoDraw() {
 auto tr=[&](const std::string &key){return locale.tr(key);};
 auto rows=systemInfoRows();systemInfoRow=std::clamp(systemInfoRow,0,std::max(0,int(rows.size())-1));
 std::string key=systemInfoPage=="root"?"title":systemInfoPage.rfind("card",0)==0?"cards":systemInfoPage;
 ui->frame(tr("sysinfo."+key),systemInfoPage=="root"?"":tr("sysinfo.title"),store.data.value("timezone","Etc/UTC"));
 double content=rows.size()*54,viewport=280,top=systemInfoRow*54;
 if(top<systemInfoScroll)systemInfoScroll=top;if(top+54>systemInfoScroll+viewport)systemInfoScroll=top+54-viewport;
 systemInfoScroll=std::clamp(systemInfoScroll,0.0,std::max(0.0,content-viewport));
 SDL_Rect clip{16,128,680,280};SDL_RenderSetClipRect(ui->renderer,&clip);
 for(size_t i=0;i<rows.size();++i) {
  int y=128+int(i)*54-int(systemInfoScroll);if(y+52<=128||y>=408)continue;const auto &r=rows[i];
  auto value=r.state=="ready"?r.value:tr("sysinfo."+r.state);
  const bool link=!r.page.empty();int right=link?650:672;value=ui->fit(value,320,21,400);
  auto title=ui->fit(r.label,right-32-(value.empty()?0:ui->measure(value,21,400)+24),23,i==size_t(systemInfoRow)?700:500);
  ui->row(title,y,i==size_t(systemInfoRow),672,"",true,52,link?"chevron-right":"");
  if(!value.empty())ui->text(value,right-ui->measure(value,21,400),y+14,21,400,i==size_t(systemInfoRow)?ui->cream:ui->secondary);
 }
 SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar(content,viewport,systemInfoScroll,false,135,406);
 bool opens=!rows.empty()&&!rows[systemInfoRow].page.empty();
 if(systemInfoPage=="root")ui->hints({{"A",tr("action.open")},{"B",tr("action.back")}});
 else ui->hints({{opens?"A":"UP/DOWN",tr(opens?"action.open":"sysinfo.select")},{"B",tr("action.back")},{"X",tr("sysinfo.source")},{"Y",tr(systemInfoJob.busy()?"sysinfo.refreshing":"sysinfo.refresh")}});
 if(systemInfoSource&&!rows.empty()) {
  auto &r=rows[systemInfoRow];std::string body=(r.state=="ready"?r.value:tr("sysinfo."+r.state))+"\n\n"+tr("sysinfo.source")+": "+r.source;
  if(systemInfoData.contains("sampled_at")){time_t when=systemInfoData["sampled_at"].get<long long>();char stamp[64];std::tm utc{};gmtime_r(&when,&utc);strftime(stamp,sizeof(stamp),"%Y-%m-%d %H:%M:%S UTC",&utc);body+="\n"+tr("sysinfo.sample")+": "+stamp;}
  if(systemInfoPage=="power"&&systemInfoRow==3)body+="\n\n"+tr("sysinfo.rated_note");
  if(systemInfoPage=="sensors")body+="\n\n"+tr("sysinfo.sensor_note");
  systemInfoSourceScroll=ui->info(r.label,body,systemInfoSourceScroll);ui->hints({{"B",tr("footer.close")}});
 }
}
}
