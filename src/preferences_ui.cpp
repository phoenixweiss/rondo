// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include "preferences.hpp"
#include <algorithm>
#include <cmath>
namespace rondo {
std::vector<CoreEntry> App::installedCores() const {
  if(!coreCatalogCache) {
    std::vector<CoreEntry> entries;
    auto registry=readJson(options.assets/"core-registry.json");
    for(const auto &c:registry.at("cores")) {
      auto systems=c.at("systems").get<std::vector<std::string>>();if(systems.empty())continue;
      auto id=c.at("id").get<std::string>();if(coreId(systems.front())!=id)continue;
      std::string label;for(const auto &system:systems){if(!label.empty())label+="/";label+=system;}
      CoreEntry entry{id,c.at("name"),label,options.coreFor(systems.front()),c.value("experimental",false),{}};
      entry.available();entries.push_back(std::move(entry));
    }
    coreCatalogCache=std::move(entries);
  }
  return *coreCatalogCache;
}
bool App::coreEnabled(const std::string &system) const {
  for(const auto &c:installedCores())if(c.id==coreId(system))return c.enabled(store.data);
  return false;
}
static int themeIndex(const Json &themes,const std::string &id) {
  for(size_t i=0;i<themes.size();++i)if(themes[i].at("id")==themeId(id))return int(i);
  return 0;
}
std::string App::themeName(const std::string &id) const {
  const auto &themes=ui->themes.at("themes");const auto &t=themes[themeIndex(themes,id)];
  return t.at("name").value(locale.current,t.at("name").value("en",id));
}
void App::openThemes() {
  screen=Screen::Themes;row=themeIndex(ui->themes.at("themes"),store.data.value("theme_id","rondo"));featureScroll=0;
  ui->setTheme(ui->themes.at("themes")[row].at("id"));
}
void App::preferenceAction(const std::string &a) {
  if(screen==Screen::Themes) {
    const auto &themes=ui->themes.at("themes");
    if(a=="UP")row=std::max(0,row-1);if(a=="DOWN")row=std::min(int(themes.size())-1,row+1);
    ui->setTheme(themes[row].at("id"));
    if(a=="B") {ui->setTheme(store.data.value("theme_id","rondo"));screen=Screen::SettingsSection;category=0;row=1;}
    else if(a=="A") {
      auto next=store.data;next["theme_id"]=themes[row].at("id");
      try {persist(next);}catch(...) {ui->setTheme(store.data.value("theme_id","rondo"));throw;}
      screen=Screen::SettingsSection;category=0;row=1;
      if(bootLogoSupported())requestBootLogo(store.data.value("replace_boot_logo",true),true);
    }
  } else if(screen==Screen::CoreCatalog) {
    auto cores=installedCores();if(a=="UP")row=std::max(0,row-1);if(a=="DOWN")row=std::min(int(cores.size())-1,row+1);
    if(a=="B"){screen=Screen::CoreSettings;row=4;}
    else if(a=="LEFT"||a=="RIGHT"||a=="A") {
      const auto &c=cores.at(row);if(!c.available()){notice=locale.tr("core.missing");return;}
      auto next=store.data;next["enabled_cores"][c.id]=a=="LEFT"?false:a=="RIGHT"?true:!c.enabled(store.data);persist(next);
    }
  } else if(screen==Screen::MappingCores) {
    auto cores=installedCores();if(a=="UP")row=std::max(0,row-1);if(a=="DOWN")row=std::min(int(cores.size())-1,row+1);
    if(a=="B"){screen=Screen::CoreSettings;row=5;}
    if(a=="A"){mappingCore=cores.at(row).id;mappingCoreRow=row;screen=Screen::Mapping;row=0;featureScroll=0;}
  } else if(screen==Screen::Mapping) {
    auto controls=coreControls(mappingCore);
    if(a=="UP")row=std::max(0,row-1);if(a=="DOWN")row=std::min(int(controls.size())-1,row+1);
    if(a=="B"){screen=Screen::MappingCores;row=mappingCoreRow;featureScroll=0;}
    else if(a=="Y") {screen=Screen::MappingReset;row=0;}
    else if(a=="X") {mappingTest=true;mappingReturnRow=row;openInputTest(true);}
    else if(a=="LEFT"||a=="RIGHT"||a=="A") {
      auto target=controls.at(row).first;int current=mappedSource(store.data,mappingCore,target);
      int value=((current+1+(a=="LEFT"?16:1))%17)-1;
      auto next=store.data;next["input_mapping"][mappingCore][std::to_string(target)]=value;persist(next);
    }
  } else if(screen==Screen::MappingReset) {
    if(a=="UP")row=0;if(a=="DOWN")row=1;
    if(a=="B"||(a=="A"&&row==0))screen=Screen::Mapping;
    else if(a=="A"&&row==1) {auto next=store.data;next["input_mapping"].erase(mappingCore);persist(next);screen=Screen::Mapping;}
  }
}
static double listScroll(int row,int count,double old,int viewport=280) {
  if(row*54<old)old=row*54;if((row+1)*54>old+viewport)old=(row+1)*54-viewport;
  return std::clamp(old,0.0,std::max(0.0,count*54.0-viewport));
}
std::string App::gamepadPlayerLabel(int player) const {
  if(!player)return locale.tr("gamepads.unassigned");
  return locale.tr("gamepads.player",{{"number",std::to_string(player)}});
}
std::string App::gamepadRoleLabel(const std::string &key,std::optional<SDL_JoystickID> live) const {
  const int assigned=gamepadPlayer(store.data,key);
  if(assigned>0)return locale.tr("gamepads.player_fixed",{{"number",std::to_string(assigned)}});
  if(assigned==0)return gamepadPlayerLabel(0);
  if(!live)return locale.tr("gamepads.auto");
  const int port=gamepadPort(*live);
  return port>0?locale.tr("gamepads.auto_player",{{"number",std::to_string(port)}}):locale.tr("gamepads.auto_menu");
}
int App::gamepadPort(SDL_JoystickID id) const {
  const int assigned=gamepadPlayer(store.data,ui->controllerKeys.at(id));
  if(assigned!=AutomaticGamepadPlayer)return assigned;
  const auto active=automaticGamepadPorts.find(id);
  return active==automaticGamepadPorts.end()?0:active->second;
}
void App::syncGamepadPlayers() {
  std::array<bool,GamepadPlayerCount+1> used{};
  std::vector<SDL_JoystickID> automatic;
  for(const auto &[id,pad]:ui->controllers) {
    if(!SDL_GameControllerGetAttached(pad))continue;
    const int assigned=gamepadPlayer(store.data,ui->controllerKeys.at(id));
    if(assigned==AutomaticGamepadPlayer)automatic.push_back(id);
    else used[assigned]=true;
  }
  for(auto it=automaticGamepadPorts.begin();it!=automaticGamepadPorts.end();) {
    if(std::find(automatic.begin(),automatic.end(),it->first)==automatic.end())
      it=automaticGamepadPorts.erase(it);
    else {used[it->second]=true;++it;}
  }
  // SDL instance IDs are allocated in connection order. A live auto port
  // survives gaps and deliberate manual sharing until the pad disconnects.
  for(auto id:automatic) {
    if(automaticGamepadPorts.count(id))continue;
    const auto free=std::find(used.begin()+1,used.end(),false);
    if(free==used.end())break;
    automaticGamepadPorts[id]=int(free-used.begin());
    *free=true;
  }
}
Json App::gamepadPortHandoff() const {
  Json ports=Json::object();
  for(const auto &[id,port]:automaticGamepadPorts) {
    const auto pad=ui->controllers.find(id);
    if(pad==ui->controllers.end() || !SDL_GameControllerGetAttached(pad->second))continue;
    const auto &key=ui->controllerKeys.at(id);
    if(!key.empty())ports[key]=port;
  }
  return ports;
}
void App::restoreGamepadPortHandoff(const Json &ports) {
  automaticGamepadPorts.clear();
  std::array<bool,GamepadPlayerCount+1> used{};
  if(ports.is_object())for(const auto &[id,key]:ui->controllerKeys) {
    auto port=ports.find(key);
    if(key.empty() || port==ports.end() || !port->is_number_integer() || *port<1 || *port>GamepadPlayerCount)continue;
    const int number=port->get<int>();
    if(used[number])continue;
    used[number]=true;
    automaticGamepadPorts[id]=number;
  }
  syncGamepadPlayers();
}
void App::changeGamepadPlayer(const std::string &key,int direction) {
  if(key.empty())return;
  auto next=store.data;
  const int choices=GamepadPlayerCount+2;
  const int assigned=(gamepadPlayer(next,key)+1+direction+choices)%choices-1;
  if(assigned==AutomaticGamepadPlayer)next["gamepad_players"].erase(key);
  else next["gamepad_players"][key]=assigned;
  persist(next);syncGamepadPlayers();
}
bool App::controlsRoute() const {
  return screen==Screen::Controls || screen==Screen::Gamepads || screen==Screen::GamepadFind ||
      screen==Screen::GamepadDetails || screen==Screen::GamepadColor || screen==Screen::GamepadPlayer;
}
std::vector<App::GamepadEntry> App::gamepadEntries() const {
  std::map<std::string,GamepadEntry> entries;
  for(const auto &[id,pad]:ui->controllers) {
    if(!SDL_GameControllerGetAttached(pad))continue;
    const auto &key=ui->controllerKeys.at(id);
    auto token=key.empty()?"live:"+std::to_string(id):key;
    const char *name=SDL_GameControllerName(pad);
    entries.emplace(token,GamepadEntry{token,key,name&&*name?name:locale.tr("input.source_controller"),id});
  }
  for(const auto &device:bt.devices) {
    if(!device.paired || !device.input || !bluetoothAddress(device.address))continue;
    const auto key=bluetoothGamepadKey(device.address);
    entries.emplace(key,GamepadEntry{key,key,device.named?device.name:locale.tr("input.source_controller"),{}});
  }
  std::vector<GamepadEntry> list{{"handheld","",locale.tr("input.source_handheld"),{}}};
  for(const auto &token:gamepadOrder) {
    auto entry=entries.extract(token);
    if(entry)list.push_back(std::move(entry.mapped()));
  }
  for(auto &[token,entry]:entries)list.push_back(std::move(entry));
  return list;
}
void App::refreshGamepads() {
  syncGamepadPlayers();
  const auto entries=gamepadEntries();
  gamepadOrder.erase(std::remove_if(gamepadOrder.begin(),gamepadOrder.end(),[&](const auto &token){
    return token.rfind("live:",0)==0 &&
        std::none_of(entries.begin(),entries.end(),[&](const auto &entry){return entry.token==token;});
  }),gamepadOrder.end());
  auto next=store.data;
  bool changed=false;
  for(size_t i=1;i<entries.size();++i) {
    const auto &entry=entries[i];
    if(std::find(gamepadOrder.begin(),gamepadOrder.end(),entry.token)==gamepadOrder.end())
      gamepadOrder.push_back(entry.token);
    if(entry.key.empty() || gamepadColor(next,entry.key)>=0)continue;
    std::array<bool,GamepadSwatches.size()> taken{};
    for(const auto &peer:entries) {
      if(peer.token==entry.token || peer.name!=entry.name)continue;
      const int color=gamepadColor(next,peer.key);
      if(color>=0)taken[color]=true;
    }
    const auto preferred=btColors.find(entry.key);
    const int color=preferred!=btColors.end()?preferred->second:chooseGamepadColor(entry.key,taken);
    next["gamepad_colors"][entry.key]=GamepadSwatches[color].id;
    changed=true;
  }
  if(changed)try {persist(std::move(next));}catch(const std::exception &) {}
  if(screen==Screen::Gamepads) {
    auto focused=std::find_if(entries.begin(),entries.end(),[&](const auto &entry){return entry.token==gamepadFocus;});
    gamepadRow=focused!=entries.end()?int(focused-entries.begin()):std::clamp(gamepadRow,0,int(entries.size())-1);
    gamepadFocus=entries[gamepadRow].token;
  }
  if(screen==Screen::GamepadFind && std::none_of(entries.begin(),entries.end(),[](const auto &entry){return entry.live.has_value();})) {
    navigationBack();
    notice=locale.tr("gamepads.find_disconnected");
  }
}
bool App::identifyGamepad(SDL_JoystickID id) {
  if(screen!=Screen::GamepadFind)return false;
  if(stop || sleeping || restoreInputFence || activity() || gamepadIdentifyHeld.count(id))return true;
  const auto entries=gamepadEntries();
  for(size_t i=1;i<entries.size();++i) {
    if(entries[i].live!=id)continue;
    gamepadRow=int(i);gamepadFocus=entries[i].token;
    ui->inputHints(false);
    navigationBack();
    break;
  }
  return true;
}
void App::gamepadsAction(const std::string &action) {
  const auto move=[&](int max){
    if(action=="UP")row=std::max(0,row-1);
    if(action=="DOWN")row=std::min(max,row+1);
  };
  if(action=="B") {
    if(screen==Screen::Controls && navigation.empty())returnToSettings();
    else navigationBack();
    return;
  }
  if(screen==Screen::Controls) {
    move(1);
    if(action!="A")return;
    if(row==1){openInputTest(false);return;}
    navigationPush(Screen::Gamepads);
    const auto entries=gamepadEntries();
    gamepadRow=entries.size()>1?1:0;gamepadFocus=entries[gamepadRow].token;gamepadFirstRow=0;
    refreshGamepads();
    if(bt.available && !btJob.valid()) {
      auto adapter=btDevice;
      btRun("poll",[adapter]{return adapter->status();});
    }
    return;
  }
  const auto entries=gamepadEntries();
  auto selected=std::find_if(entries.begin(),entries.end(),[&](const auto &entry){return entry.token==gamepadFocus;});
  if(screen==Screen::Gamepads) {
    int index=selected==entries.end()?std::min(gamepadRow,int(entries.size())-1):int(selected-entries.begin());
    if(action=="UP")index=std::max(0,index-1);
    if(action=="DOWN")index=std::min(int(entries.size())-1,index+1);
    gamepadRow=index;gamepadFocus=entries[index].token;
    if(action=="A" && index>0)navigationPush(Screen::GamepadDetails);
    if(action=="X" && std::any_of(entries.begin(),entries.end(),[](const auto &entry){return entry.live.has_value();})) {
      gamepadIdentifyHeld.clear();
      for(const auto &[id,pad]:ui->controllers)
        if(SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_A))gamepadIdentifyHeld.insert(id);
      navigationPush(Screen::GamepadFind);
    }
    return;
  }
  if(screen==Screen::GamepadFind || selected==entries.end() || selected->key.empty())return;
  if(screen==Screen::GamepadDetails) {
    move(1);
    if(action=="A") {
      if(row==0) {
        navigationPush(Screen::GamepadColor);
        row=std::max(0,gamepadColor(store.data,selected->key));
      } else {
        navigationPush(Screen::GamepadPlayer);
        row=gamepadPlayer(store.data,selected->key)+1;
      }
    }
    return;
  }
  auto next=store.data;
  if(screen==Screen::GamepadPlayer) {
    move(GamepadPlayerCount+1);
    if(action!="A")return;
    if(row==0)next["gamepad_players"].erase(selected->key);
    else next["gamepad_players"][selected->key]=row-1;
  } else {
    constexpr int columns=3;
    const int last=int(GamepadSwatches.size())-1;
    if(action=="LEFT" && row%columns>0)--row;
    if(action=="RIGHT" && row%columns<columns-1 && row<last)++row;
    if(action=="UP" && row>=columns)row-=columns;
    if(action=="DOWN" && row+columns<=last)row+=columns;
    if(action!="A")return;
    next["gamepad_colors"][selected->key]=GamepadSwatches[row].id;
  }
  if(next!=store.data)persist(std::move(next));
  syncGamepadPlayers();
  notice.clear();navigationBack();
}
static SDL_Color swatch(int index) {
  const auto rgb=GamepadSwatches[index].rgb;
  return {Uint8(rgb>>16),Uint8(rgb>>8),Uint8(rgb),255};
}
void App::drawGamepadDot(const std::string &key,int x,int y) {
  drawDeviceColor(gamepadColor(store.data,key),x,y);
}
void App::drawDeviceColor(int color,int x,int y) {
  ui->circle(x,y,14,ui->secondary,false);
  if(color>=0)ui->circle(x+1,y+1,12,swatch(color),true);
}
void App::drawGamepads(const std::string &zone) {
  const auto tr=[&](const std::string &key){return locale.tr(key);};
  if(screen==Screen::Controls) {
    ui->frame(tr("controls.title"),tr("screen.settings"),zone);
    ui->row(tr("gamepads.title"),128,row==0,672,"",true,52,"chevron-right","gamepad-2");
    ui->row(tr("input.test"),182,row==1,672,"",true,52,"chevron-right","joystick");
    ui->hints({{"A",tr("action.open")},{"B",tr("action.back")}});
    return;
  }
  const auto entries=gamepadEntries();
  const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto &entry){return entry.token==gamepadFocus;});
  const auto colorName=[&](int i){return i<0?tr("gamepads.color_unavailable"):tr("gamepads.color."+std::string(GamepadSwatches[i].id));};
  if(screen==Screen::Gamepads) {
    ui->frame(tr("gamepads.title"),tr("controls.title"),zone);
    constexpr int visible=4,top=122,step=65;
    const int count=int(entries.size());
    const int focused=found==entries.end()?std::min(gamepadRow,count-1):int(found-entries.begin());
    if(focused<gamepadFirstRow)gamepadFirstRow=focused;
    if(focused>=gamepadFirstRow+visible)gamepadFirstRow=focused-visible+1;
    const int start=gamepadFirstRow=std::clamp(gamepadFirstRow,0,std::max(0,count-visible));
    for(int i=start;i<std::min(count,start+visible);++i) {
      const auto &entry=entries[i];
      const int y=top+(i-start)*step;
      const auto player=i?gamepadRoleLabel(entry.key,entry.live):gamepadPlayerLabel(1);
      ui->row("",y,i==focused,672,player,true,60,i?"chevron-right":"");
      if(i)drawGamepadDot(entry.key,33,y+14);
      const int nameWidth=std::min(398,626-ui->measure(player,21)-58-16);
      ui->text(entry.name,i?58:33,y+6,22,i==focused?700:500,ui->cream,nameWidth);
      ui->text(tr(!i?"gamepads.built_in":entry.live?"gamepads.connected":"gamepads.disconnected"),i?58:33,y+33,17,500,ui->secondary,398);
    }
    ui->scrollbar(count*step,visible*step,start*step,false,top,top+visible*step);
    auto position=std::to_string(focused+1)+" / "+std::to_string(count);
    ui->text(position,688-ui->measure(position,18),401,18,500,ui->secondary);
    if(count==1)ui->text(tr("gamepads.empty"),24,401,18,500,ui->secondary,580);
    std::vector<std::pair<std::string,std::string>> hints;
    if(focused)hints.push_back({"A",tr("action.open")});
    hints.push_back({"B",tr("action.back")});
    if(std::any_of(entries.begin(),entries.end(),[](const auto &entry){return entry.live.has_value();}))
      hints.push_back({"X",tr("gamepads.find")});
    ui->hints(hints);
  } else if(screen==Screen::GamepadFind) {
    ui->frame(tr("gamepads.find"),tr("gamepads.title"),zone);
    ui->icon("gamepad-2",328,144,64);
    const auto instruction=tr("gamepads.identify");
    ui->text(instruction,(720-ui->measure(instruction,23))/2,238,23,500,ui->cream,672);
    ui->hints({{"B",tr("action.cancel")}});
  } else if(screen==Screen::GamepadDetails) {
    ui->frame(tr("gamepads.settings"),tr("gamepads.title"),zone);
    if(found==entries.end() || found->key.empty()) {
      ui->text(tr(found==entries.end()?"gamepads.find_disconnected":"gamepads.identity_help"),24,140,21,500,ui->cream,672);
      ui->hints({{"B",tr("action.back")}});return;
    }
    const auto &entry=*found;
    drawGamepadDot(entry.key,27,132);
    ui->text(entry.name,50,124,23,700,ui->cream,640);
    ui->text(tr(entry.live?"gamepads.connected":"gamepads.disconnected"),50,154,18,500,ui->secondary);
    ui->row(tr("gamepads.label_color"),198,row==0,672,colorName(gamepadColor(store.data,entry.key)),true,52,"chevron-right");
    ui->row(tr("gamepads.role"),252,row==1,672,gamepadRoleLabel(entry.key,entry.live),true,52,"chevron-right");
    ui->hints({{"A",tr("action.open")},{"B",tr("action.back")}});
  } else {
    const bool choosingColor=screen==Screen::GamepadColor;
    ui->frame(tr(choosingColor?"gamepads.label_color":"gamepads.role"),found==entries.end()?tr("gamepads.title"):found->name,zone);
    if(found==entries.end()) {
      ui->text(tr("gamepads.find_disconnected"),24,140,21,500,ui->cream,672);
      ui->hints({{"B",tr("action.back")}});return;
    }
    if(choosingColor) {
      const int saved=gamepadColor(store.data,found->key);
      for(int i=0;i<int(GamepadSwatches.size());++i) {
        int x=24+(i%3)*228,y=126+(i/3)*84;
        ui->rect(x,y,216,73,i==row?ui->accent:ui->separator,8);
        ui->rect(x+2,y+2,212,69,i==row?ui->focus:ui->bg,6);
        ui->circle(x+16,y+26,20,ui->secondary,false);
        ui->circle(x+17,y+27,18,swatch(i),true);
        ui->text(colorName(i),x+44,y+24,19,i==row?700:500,ui->cream,138);
        if(i==saved)ui->icon("check",x+184,y+26,20);
      }
    } else {
      constexpr int visible=5,top=119,step=54;
      constexpr int choices=GamepadPlayerCount+2;
      const int saved=gamepadPlayer(store.data,found->key)+1;
      const int start=std::clamp(row-visible+1,0,choices-visible);
      for(int i=start;i<std::min(choices,start+visible);++i) {
        const auto label=i==0?tr("gamepads.auto"):i==1?gamepadPlayerLabel(0):
            locale.tr("gamepads.player_fixed",{{"number",std::to_string(i-1)}});
        ui->row(label,top+(i-start)*step,i==row,672,"",true,52,i==saved?"check":"");
      }
      ui->scrollbar(choices*step,visible*step,start*step,false,top,top+visible*step);
      ui->text(tr(row==0?"gamepads.auto_help":row==1?"gamepads.menu_only_help":"gamepads.fixed_help"),24,400,18,500,ui->secondary,672);
    }
    ui->hints({{"A",tr("action.apply")},{"B",tr("action.cancel")}});
  }
}
void App::preferenceDraw(const std::string &zone) {
  auto tr=[&](const std::string &key){return locale.tr(key);};
  if(screen==Screen::Themes) {
    ui->frame(tr("theme.title"),tr("section.interface"),zone);const auto &themes=ui->themes.at("themes");
    featureScroll=listScroll(row,int(themes.size()),featureScroll);
    SDL_Rect clip{16,128,688,280};SDL_RenderSetClipRect(ui->renderer,&clip);
    for(size_t i=0;i<themes.size();++i) {
      int y=128+int(i)*54-int(featureScroll);const auto &t=themes[i];
      ui->row(themeName(t.at("id")),y,int(i)==row,672,"",true,52,themeId(store.data.value("theme_id","rondo"))==t.at("id")?"check":"");
      int x=534;for(const auto *key:{"background","text","accent"}) {auto c=UI::parseColor(t.at("colors").at(key));ui->circle(x,y+15,22,ui->secondary,true);ui->circle(x+2,y+17,18,c,true);x+=30;}
    }
    SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar(themes.size()*54,280,featureScroll,false,128,408);
    ui->hints({{"A",tr("action.apply")},{"B",tr("action.cancel")}});
  } else if(screen==Screen::MappingReset) {
    ui->frame(tr("mapping.title"),tr("mapping.cores"),zone);
    ui->text(tr("mapping.reset_confirm"),32,136,23,500,ui->cream,656);
    ui->row(tr("action.cancel"),236,row==0);
    ui->row(tr("mapping.reset"),290,row==1);
    ui->hints({{"A",tr("action.select")},{"B",tr("action.cancel")}});
  } else {
    bool mapping=screen==Screen::Mapping;
    auto entries=installedCores();auto controls=coreControls(mappingCore);
    std::string displayCore=mappingCore;for(const auto &c:entries)if(c.id==mappingCore)displayCore=c.name;
    int count=mapping?int(controls.size()):int(entries.size());
    ui->frame(tr(mapping?"mapping.title":screen==Screen::CoreCatalog?"core.catalog":"mapping.cores"),mapping?displayCore:tr("section.cores"),zone);
    featureScroll=listScroll(row,count,featureScroll);SDL_Rect clip{16,128,688,280};SDL_RenderSetClipRect(ui->renderer,&clip);
    for(int i=0;i<count;++i){int y=128+i*54-int(featureScroll);
      if(mapping)ui->valueRow(locale.tr("control."+controls[i].second),y,row==i,672,physicalButton(mappedSource(store.data,mappingCore,controls[i].first)));
      else if(screen==Screen::CoreCatalog){const auto &c=entries[i];if(c.available())ui->binaryRow(c.name,y,row==i,c.enabled(store.data),locale);else ui->row(c.name,y,row==i,672,tr("core.missing"),false);}
      else ui->row(entries[i].name,y,row==i,672,"",true,52,"chevron-right");
    }
    SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar(count*54,280,featureScroll,false,128,408);
    if(mapping)ui->hints({{"LEFT/RIGHT",tr("footer.value")},{"B",tr("action.back")},{"X",tr("mapping.test")},{"Y",tr("mapping.reset")}});
    else ui->hints({{screen==Screen::CoreCatalog?"LEFT/RIGHT":"A",tr(screen==Screen::CoreCatalog?"footer.value":"action.open")},{"B",tr("action.back")}});
  }
}
} // namespace rondo
