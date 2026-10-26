// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cstring>
namespace rondo {
static const std::vector<std::vector<std::string>> stepKeys{
 {"locale"},{"brightness","theme_id"},{"volume"},{"wifi_enabled"},
 {"timezone","ntp_auto"},{"enabled_cores"},{"autosave_enabled","startup_resume","idle_screen_minutes"}};
static const std::vector<int> stepRows{1,2,2,2,3,6,3,1};
int App::wizardNextRow() const {return wizardStep==7?0:stepRows.at(wizardStep);}
void App::wizardRestore() {
  if(soundTest){SDL_CloseAudioDevice(soundTest);soundTest=0;}
  locale.select(options.assets,store.data.value("locale","en"));
  ui->setTheme(store.data.value("theme_id","rondo"));
  applyVolume(store.data.value("volume",80));
  if(brightness&&*brightness!=store.data.value("brightness",*brightness)){device->brightness(store.data.value("brightness",*brightness));brightness=device->brightness();}
}
void App::wizardEnter(int step) {
  if(soundTest){SDL_CloseAudioDevice(soundTest);soundTest=0;}
  wizardStep=std::clamp(step,0,7);wizardRow=0;featureScroll=0;wizardDraft=store.data;screen=Screen::Wizard;notice.clear();
}
void App::startWizard(bool repeat) {
  if(core||store.readOnly||!store.warning.empty()){notice=locale.tr("wizard.unavailable");return;}
  if(store.fresh&&!store.data.contains("wizard")) {
    auto next=store.data;next["volume"]=30;next["brightness"]=70;applyVolume(30);
    if(brightness) {
      auto old=*brightness;
      try {device->brightness(70);brightness=device->brightness();if(!brightness||*brightness!=70)throw std::runtime_error("Brightness not acknowledged");}
      catch(...) {try{device->brightness(old);brightness=device->brightness();}catch(...){}next["brightness"]=old;}
    }
    const char *zone=getenv("TZ");if(zone&&zones.valid(zone))next["timezone"]=zone;
    next["wizard"]={{"schema",1},{"completed",false},{"deferred",false},{"step",0}};
    try{persist(next);}catch(...){wizardRestore();throw;}
  }
  wizardRepeat=repeat;wizardActive=true;wizardExternal=false;wizardPending=false;wizardPendingNetworks=false;
  wizardEnter(repeat?0:store.data.value("wizard",Json::object()).value("step",0));
}
void App::wizardAdvance(bool apply) {
  if(wizardPending)return;
  if(wizardStep==7) {
    auto next=store.data;next["wizard"]={{"schema",1},{"completed",true},{"deferred",false},{"step",7}};
    persist(next);wizardActive=false;screen=Screen::Library;return;
  }
  // Radio and clock operations use the ordinary service/ack paths before progress advances.
  if(apply&&wizardStep==3&&wizardDraft.value("wifi_enabled",false)!=store.data.value("wifi_enabled",false)) {
    if(!wifi.available||wifiBusy()){notice=locale.tr("wizard.wait");return;}
    auto wanted=wizardDraft.value("wifi_enabled",false);wifiToggle=wanted;auto adapter=wifiDevice;wifi.error.clear();
    wifiRun("toggle",[adapter,wanted]{return adapter->enable(wanted);});wizardPending=true;return;
  }
  if(apply&&wizardStep==4&&wizardDraft.value("ntp_auto",true)!=store.data.value("ntp_auto",true)) {
    if(!clockState.available||clockBusy()){notice=locale.tr("wizard.wait");return;}
    bool wanted=wizardDraft.value("ntp_auto",true);clockToggle=wanted;auto d=clockDevice;clockError.clear();
    clockRun("toggle",[d,wanted]{return wanted?d->restart():d->enable(false);});wizardPending=true;return;
  }
  auto next=store.data;
  if(apply)for(const auto &key:stepKeys.at(wizardStep))next[key]=wizardDraft.at(key);
  else wizardRestore();
  next["wizard"]={{"schema",1},{"completed",false},{"deferred",false},{"step",wizardStep+1}};
  try{persist(next);}catch(...){wizardRestore();throw;}
  locale.select(options.assets,store.data.value("locale","en"));ui->setTheme(store.data.value("theme_id","rondo"));
  if(wizardStep==1&&bootLogoSupported())requestBootLogo(store.data.value("replace_boot_logo",true),true);
  wizardEnter(wizardStep+1);
}
void App::wizardTestSound() {
  if(soundTest){SDL_CloseAudioDevice(soundTest);soundTest=0;}
  if(SDL_InitSubSystem(SDL_INIT_AUDIO)) {notice=locale.tr("wizard.sound_error");return;}
  SDL_AudioSpec spec{};Uint8 *data=nullptr;Uint32 length=0;
  if(!SDL_LoadWAV((options.assets/"Audio/sound-check.wav").c_str(),&spec,&data,&length)) {notice=locale.tr("wizard.sound_error");return;}
  std::unique_ptr<Uint8,decltype(&SDL_FreeWAV)> owned(data,SDL_FreeWAV);
  if(spec.format!=AUDIO_S16SYS||spec.channels!=2||spec.freq!=44100||length>1024*1024||length%4) {notice=locale.tr("wizard.sound_error");return;}
  soundTest=SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0);
  if(!soundTest){notice=locale.tr("wizard.sound_error");return;}
  std::vector<int16_t> samples(length/2);
  for(size_t i=0;i<samples.size();++i){int16_t value;std::memcpy(&value,data+i*2,2);samples[i]=int16_t(int(value)*previewVolume/100);}
  if(SDL_QueueAudio(soundTest,samples.data(),length)){SDL_CloseAudioDevice(soundTest);soundTest=0;notice=locale.tr("wizard.sound_error");return;}
  SDL_PauseAudioDevice(soundTest,0);soundTestUntil=monotonicMs()+uint64_t(length)*1000/(spec.freq*spec.channels*2)+100;
}
void App::wizardReturn() {
  if(!wizardActive||!wizardExternal)return;
  for(const auto &key:stepKeys.at(wizardStep))
    if(store.data.at(key)!=wizardChildBaseline.at(key))wizardDraft[key]=store.data.at(key);
  wizardExternal=false;screen=Screen::Wizard;wizardRow=wizardExternalRow;
}
void App::wizardMaybeReturn() {
  if(!wizardActive||!wizardExternal)return;
  if(screen==Screen::SettingsSection||screen==Screen::Settings||
     (wizardChildRoot==Screen::WifiNetworks&&screen==Screen::Wifi)||
     ((wizardChildRoot==Screen::Regions||wizardChildRoot==Screen::ClockEdit)&&screen==Screen::Clock))wizardReturn();
}
void App::wizardOpenChild(Screen target) {
  if(target==Screen::WifiNetworks) {
    if(!wifi.available){notice=locale.tr("wizard.wifi_unavailable");return;}
    if(!wizardDraft.value("wifi_enabled",false)){notice=locale.tr("wizard.wifi_enable");return;}
    if(wifiBusy()){notice=locale.tr("wizard.wait");return;}
    if(!wifi.enabled||!store.data.value("wifi_enabled",false)) {
      wifi.error.clear();wifiToggle=true;auto adapter=wifiDevice;
      wifiRun("toggle",[adapter]{return adapter->enable(true);});
      wizardPending=true;wizardPendingNetworks=true;return;
    }
  }
  if(target==Screen::ClockEdit&&(!clockState.available||clockBusy())) {notice=locale.tr(clockBusy()?"wizard.wait":"clock.phase.unavailable");return;}
  wizardExternal=true;wizardExternalRow=wizardRow;wizardChildRoot=target;wizardChildBaseline=store.data;
  if(target==Screen::Regions)openTimezones();
  else if(target==Screen::ClockEdit){screen=Screen::Clock;row=2;clockAction("A");if(screen!=Screen::ClockEdit){wizardReturn();return;}}
  else {
    screen=Screen::WifiNetworks;row=0;wifiListFocus=0;wifi.error.clear();
    auto adapter=wifiDevice;wifiRun("scan",[adapter]{return adapter->scan();});
  }
}
void App::wizardTick() {
  if(soundTest&&monotonicMs()>=soundTestUntil){SDL_CloseAudioDevice(soundTest);soundTest=0;}
  if(!wizardActive)return;
  wizardMaybeReturn();
  if(wizardPending) {
    if((wizardStep==3&&wifiBusy())||(wizardStep==4&&clockBusy()))return;
    wizardPending=false;
    bool ack=wizardStep==3?store.data.value("wifi_enabled",false)==wizardDraft.value("wifi_enabled",false)&&wifi.error.empty():store.data.value("ntp_auto",true)==wizardDraft.value("ntp_auto",true)&&clockError.empty();
    bool openNetworks=wizardPendingNetworks;wizardPendingNetworks=false;
    if(!ack){notice=locale.tr("error.operation");wizardDraft=store.data;return;}
    try{if(openNetworks)wizardOpenChild(Screen::WifiNetworks);else wizardAdvance(true);}catch(...){notice=locale.tr("notice.not_persisted");}
  }
}
void App::wizardAction(const std::string &a) {
  if(wizardPending){if(a!="B")notice=locale.tr("wizard.wait");return;}
  if(screen==Screen::WizardExit) {
    if(a=="B"){screen=Screen::Wizard;return;}
    if(a=="A") {
      wizardRestore();auto next=store.data;
      next["wizard"]={{"schema",1},{"completed",false},{"deferred",true},{"step",wizardStep}};
      persist(next);wizardActive=false;screen=Screen::Library;
    }
    return;
  }
  if(a=="MENU"){screen=Screen::WizardExit;return;}
  if(a=="B") {wizardRestore();if(wizardStep>0){
    auto next=store.data;next["wizard"]={{"schema",1},{"completed",false},{"deferred",false},{"step",wizardStep-1}};persist(next);wizardEnter(wizardStep-1);
  }return;}
  if(a=="Y"){if(wizardStep<7)wizardAdvance(false);return;}
  if(a=="A"){
    if(wizardStep==7||wizardRow==wizardNextRow())wizardAdvance(true);
    else if(wizardStep==2&&wizardRow==1)wizardTestSound();
    else if(wizardStep==3&&wizardRow==1)wizardOpenChild(Screen::WifiNetworks);
    else if(wizardStep==4&&wizardRow>0)wizardOpenChild(wizardRow==1?Screen::Regions:Screen::ClockEdit);
    else if(wizardStep==3)wizardAction(wizardDraft.value("wifi_enabled",false)?"LEFT":"RIGHT");
    else if(wizardStep==4)wizardAction(wizardDraft.value("ntp_auto",true)?"LEFT":"RIGHT");
    else if(wizardStep==5)wizardAction(installedCores().at(wizardRow).enabled(wizardDraft)?"LEFT":"RIGHT");
    else if(wizardStep==6&&wizardRow<2)wizardAction(wizardDraft.value(wizardRow==0?"autosave_enabled":"startup_resume",true)?"LEFT":"RIGHT");
    else wizardAction("RIGHT");
    return;
  }
  if(a=="UP")wizardRow=std::max(0,wizardRow-1);
  if(a=="DOWN")wizardRow=std::min(wizardNextRow(),wizardRow+1);
  if(a=="X") {
    if(wizardStep==2)wizardTestSound();
    else if(wizardStep==3)wizardOpenChild(Screen::WifiNetworks);
    else if(wizardStep==4)wizardOpenChild(wizardRow==1?Screen::Regions:Screen::ClockEdit);
    return;
  }
  if(wizardRow==wizardNextRow())return;
  if(a!="LEFT"&&a!="RIGHT")return;
  int delta=a=="LEFT"?-1:1;
  if(wizardStep==0) {
    auto code=wizardDraft.value("locale","en")=="en"?"ru":"en";wizardDraft["locale"]=code;locale.select(options.assets,code);
  } else if(wizardStep==1) {
    if(wizardRow==0&&brightness){int value=std::clamp(wizardDraft.value("brightness",70)+delta*5,0,100);device->brightness(value);brightness=device->brightness();if(!brightness||*brightness!=value)throw std::runtime_error("Brightness not acknowledged");wizardDraft["brightness"]=value;}
    else if(wizardRow==1) {
      const auto &themes=ui->themes.at("themes");int index=0;for(size_t i=0;i<themes.size();++i)if(themes[i].at("id")==themeId(wizardDraft.value("theme_id","rondo")))index=int(i);
      auto id=themes[(index+delta+int(themes.size()))%int(themes.size())].at("id");wizardDraft["theme_id"]=id;ui->setTheme(id);
    }
  } else if(wizardStep==2&&wizardRow==0){int value=std::clamp(wizardDraft.value("volume",30)+delta*5,0,100);wizardDraft["volume"]=value;applyVolume(value);}
  else if(wizardStep==3&&wizardRow==0&&wifi.available)wizardDraft["wifi_enabled"]=delta>0;
  else if(wizardStep==4&&wizardRow==0&&clockState.available)wizardDraft["ntp_auto"]=delta>0;
  else if(wizardStep==5){auto cores=installedCores();const auto &c=cores.at(wizardRow);if(c.available())wizardDraft["enabled_cores"][c.id]=delta>0;}
  else if(wizardStep==6){if(wizardRow<2)wizardDraft[wizardRow==0?"autosave_enabled":"startup_resume"]=delta>0;else {std::vector<int> v{0,1,2,5,10,15};auto i=std::find(v.begin(),v.end(),wizardDraft.value("idle_screen_minutes",5));int ix=int(i-v.begin());wizardDraft["idle_screen_minutes"]=v[(ix+delta+int(v.size()))%int(v.size())];}}
}
void App::wizardDraw(const std::string &zone) {
  auto tr=[&](const std::string &key){return locale.tr(key);};
  if(screen==Screen::WizardExit){ui->frame(tr("wizard.title"),"",zone);ui->text(tr("wizard.exit_question"),32,136,27,700,ui->cream,656);ui->text(tr("wizard.exit_note"),32,204,21,500,ui->secondary,656);ui->hints({{"A",tr("wizard.action.exit")},{"B",tr("wizard.action.return")}});return;}
  if(wizardStep==7) {
    ui->frame(tr("wizard.summary.title"),"",zone);
    auto v=[&](bool value){return tr(value?"value.on":"value.off");};
    ui->row(tr("wizard.summary.language_theme"),128,false,672,store.data.value("locale","en")+" · "+themeName(store.data.value("theme_id","rondo")));
    ui->row(tr("wizard.summary.brightness_volume"),182,false,672,std::to_string(store.data.value("brightness",70))+"% · "+std::to_string(store.data.value("volume",30))+"%");
    ui->row(tr("wizard.summary.wifi_timezone"),236,false,672,v(store.data.value("wifi_enabled",false))+" · "+zoneLabel(store.data.value("timezone","Etc/UTC")));
    int count=0;for(const auto &c:installedCores())if(c.enabled(store.data))++count;
    ui->row(tr("core.catalog"),290,false,672,std::to_string(count));
    ui->row(tr("wizard.summary.autosave_screen"),344,false,672,v(store.data.value("autosave_enabled",true))+" · "+std::to_string(store.data.value("idle_screen_minutes",5))+" "+tr("wizard.minutes"));
    ui->hints({{"A",tr("wizard.finish")},{"B",tr("action.back")}});return;
  }
  ui->frame(tr("wizard.step."+std::to_string(wizardStep)),locale.tr("wizard.progress",{{"step",wizardStep+1}}),zone);
  auto value=[&](bool v){return tr(v?"value.on":"value.off");};
  auto label=[&](const std::string &key,const Json &data)->std::string {
    if(key=="locale")return data.value(key,"en")=="ru"?"Русский":"English";
    if(key=="theme_id")return themeName(data.value(key,"rondo"));
    if(key=="brightness"||key=="volume")return std::to_string(data.value(key,0))+"%";
    if(key=="timezone")return zoneLabel(data.value(key,"Etc/UTC"));
    if(key=="idle_screen_minutes"){int m=data.value(key,5);return m?locale.tr("wizard.screen_idle.after",{{"minutes",m}}):tr("wizard.screen_idle.disabled");}
    if(key=="enabled_cores"){int n=0;for(const auto &c:installedCores())if(c.enabled(data))++n;return std::to_string(n)+" / 6";}
    return value(data.value(key,false));
  };
  if(wizardStep==5) {
    std::string baseline=tr(wizardRepeat?"wizard.current_values":"wizard.skip_values")+" "+label("enabled_cores",store.data);
    ui->text(baseline,32,127,21,500,ui->cream,656);
    auto cores=installedCores();const int viewport=238;featureScroll=std::clamp(featureScroll,0.0,140.0);
    if(wizardRow*54<featureScroll)featureScroll=wizardRow*54;if((wizardRow+1)*54>featureScroll+viewport)featureScroll=(wizardRow+1)*54-viewport;
    SDL_Rect clip{16,174,688,viewport};SDL_RenderSetClipRect(ui->renderer,&clip);
    for(size_t i=0;i<cores.size();++i){const auto &c=cores[i];int y=174+int(i)*54-int(featureScroll);if(c.available())ui->binaryRow(c.name,y,wizardRow==int(i),c.enabled(wizardDraft),locale);else ui->row(c.name,y,wizardRow==int(i),672,tr("core.missing"),false);}
    ui->row(tr("wizard.next"),174+int(cores.size())*54-int(featureScroll),wizardRow==wizardNextRow(),672,"",true,52,"chevron-right");
    SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar((cores.size()+1)*54,viewport,featureScroll,false,174,412);
  } else {
    std::vector<std::pair<std::string,std::string>> fields;
    if(wizardStep==0)fields={{"screen.language","locale"}};
    if(wizardStep==1)fields={{"setting.brightness","brightness"},{"theme.title","theme_id"}};
    if(wizardStep==2)fields={{"setting.volume","volume"},{"wizard.sound.test",""}};
    if(wizardStep==3)fields={{"wifi.title","wifi_enabled"},{"wifi.networks",""}};
    if(wizardStep==4)fields={{"wizard.time.network","ntp_auto"},{"setting.timezone","timezone"},{"wizard.time.manual",""}};
    if(wizardStep==6)fields={{"setting.autosave","autosave_enabled"},{"wizard.resume","startup_resume"},{"wizard.screen_idle","idle_screen_minutes"}};
    int viewport=wizardStep==6?162:216;
    int content=int(fields.size()+1)*54;
    if(wizardRow*54<featureScroll)featureScroll=wizardRow*54;
    if((wizardRow+1)*54>featureScroll+viewport)featureScroll=(wizardRow+1)*54-viewport;
    featureScroll=std::clamp(featureScroll,0.0,double(std::max(0,content-viewport)));
    SDL_Rect clip{16,128,688,viewport};SDL_RenderSetClipRect(ui->renderer,&clip);
    for(size_t i=0;i<fields.size();++i){const auto &f=fields[i];int y=128+int(i)*54-int(featureScroll);
      if(f.second=="wifi_enabled"||f.second=="ntp_auto"||f.second=="autosave_enabled"||f.second=="startup_resume") {
        bool supported=f.second=="wifi_enabled"?wifi.available:f.second=="ntp_auto"?clockState.available:true;
        ui->binaryRow(tr(f.first),y,wizardRow==int(i),wizardDraft.value(f.second,false),locale,supported);
      } else if(f.second=="brightness"&&!brightness)ui->row(tr(f.first),y,wizardRow==int(i),672,"—",false);
      else {
        bool opens=(wizardStep==3&&i==1)||(wizardStep==4&&i>0);
        bool supported=wizardStep==3&&i==1?wifi.available&&wizardDraft.value("wifi_enabled",false):wizardStep==4&&i==2?clockState.available:true;
        auto val=f.second.empty()?"":label(f.second,wizardDraft);
        ui->row(tr(f.first),y,wizardRow==int(i),672,val,supported,52,opens?"chevron-right":"",wizardStep==2&&i==1?"volume-2":"");
      }
    }
    ui->row(tr("wizard.next"),128+int(fields.size())*54-int(featureScroll),wizardRow==wizardNextRow(),672,"",true,52,"chevron-right");
    SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar(content,viewport,featureScroll,false,128,128+viewport);
    if(fields.size()<3)ui->text(tr(wizardStep==2?"wizard.sound.hint":wizardStep==3?"wizard.network_hint":"wizard.preview_note"),32,304,19,500,ui->secondary,656);
    int boxY=wizardStep==6?302:354;
    ui->rect(24,boxY,672,410-boxY,ui->focus);ui->rect(24,boxY,3,410-boxY,ui->accent);
    ui->text(tr(wizardRepeat?"wizard.current_values":"wizard.skip_values"),40,boxY+7,19,500,ui->secondary);
    if(wizardStep==6) {
      auto data=store.data;
      std::string first=tr(data.value("autosave_enabled",true)?"wizard.baseline.autosave_on":"wizard.baseline.autosave_off");
      first+=" "+tr(data.value("startup_resume",true)?"wizard.baseline.resume_on":"wizard.baseline.resume_off");
      // Wrap by words using the same font metrics as the actual SDL renderer.
      first+=" "+locale.tr(data.value("idle_screen_minutes",5)?"wizard.baseline.idle":"wizard.baseline.keep_on",{{"minutes",data.value("idle_screen_minutes",5)}});
      std::istringstream words(first);std::string word,line;int y=boxY+37;
      while(words>>word){auto candidate=line.empty()?word:line+" "+word;if(ui->measure(candidate,20,700)>638){ui->text(line,40,y,20,700,ui->cream);line=word;y+=27;}else line=candidate;}
      ui->text(line,40,y,20,700,ui->cream);
    } else {
      std::string summary;for(const auto &key:stepKeys[wizardStep]){if(!summary.empty())summary+=" · ";summary+=label(key,store.data);}
      ui->text(summary,40,381,21,700,ui->cream,640);
    }
  }
  bool next=wizardRow==wizardNextRow();
  auto action=next?"wizard.next":wizardStep==2&&wizardRow==1?"wizard.play":
    (wizardStep==3&&wizardRow==1)||(wizardStep==4&&wizardRow>0)?"action.open":"action.change";
  std::vector<std::pair<std::string,std::string>> hints{{"A",tr(action)},{"B",tr("action.back")},{"Y",tr("wizard.action.skip")},{"MENU",tr("wizard.action.exit")}};

  ui->hints(hints);
}
} // namespace rondo
