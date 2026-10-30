// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
#include <iostream>
namespace rondo {
static bool gamesRoute(Screen screen){return screen==Screen::Library||screen==Screen::SearchResults;}
NavigationFrame App::navigationFrame() const {
  NavigationFrame f;f.route=screen;f.collection=section;f.query=searchQuery;f.draft=searchDraft;
  auto visibleGames=visible();if(!visibleGames.empty())f.selectedGame=games[visibleGames[std::clamp(selected,0,int(visibleGames.size())-1)]].id;
  f.selected=selected;f.row=row;f.category=category;f.system=systemFocus;f.quick=navigationFocus;f.settings=settingsFocus;
  f.scroll=scroll;f.regionScroll=regionScroll;f.settingsScroll=settingsScroll;f.featureScroll=featureScroll;f.interfaceScroll=interfaceScroll;
  f.actions=navigationActions;f.keyRow=searchRow;f.keyColumn=searchColumn;f.russian=searchRussian;f.region=region;
  f.regionFocus=regionFocus;f.wifiFocus=wifiListFocus;f.wifiKeyRow=wifiKeyRow;f.wifiKeyColumn=wifiKeyColumn;f.btFocus=btFocus;f.btScroll=btScroll;
  f.infoPage=systemInfoPage;f.systemInfoRow=systemInfoRow;f.systemInfoScroll=systemInfoScroll;
  if(wifiListFocus>=0&&wifiListFocus<int(wifi.networks.size())){const auto &network=wifi.networks[wifiListFocus];f.wifiIdentity=network.ssid+"\n"+network.security;}
  if(btFocus>=0&&btFocus<int(bt.devices.size()))f.btIdentity=bt.devices[btFocus].address;
  return f;
}
void App::navigationRestore(const NavigationFrame &f) {
  if(screen==Screen::Search)SDL_StopTextInput();
  screen=f.route;
  if(!explicitSystemsRoot&&navigation.empty()&&(screen==Screen::Systems||(screen==Screen::Library&&f.collection=="All")))
    screen=store.data.value("library_grouping","unified")=="systems"?Screen::Systems:Screen::Library;
  section=(navigation.empty()&&f.route==Screen::Systems&&screen==Screen::Library)?"All":f.collection;searchQuery=f.query;searchDraft=f.draft;
  selected=f.selected;row=f.row;category=f.category;systemFocus=f.system;if(screen==Screen::Settings)settingsFocus=f.settings;
  scroll=f.scroll;regionScroll=f.regionScroll;if(screen==Screen::Settings)settingsScroll=f.settingsScroll;featureScroll=f.featureScroll;interfaceScroll=f.interfaceScroll;
  navigationActions=f.actions;navigationFocus=f.quick;systemsQuick=gamesQuick=f.actions;
  searchRow=f.keyRow;searchColumn=f.keyColumn;searchRussian=f.russian;region=f.region;
  regionFocus=f.regionFocus;wifiListFocus=f.wifiFocus;wifiKeyRow=f.wifiKeyRow;wifiKeyColumn=f.wifiKeyColumn;btFocus=f.btFocus;btScroll=f.btScroll;
  systemInfoPage=f.infoPage;systemInfoRow=f.systemInfoRow;systemInfoScroll=f.systemInfoScroll;
  for(size_t i=0;i<wifi.networks.size();++i)if(wifi.networks[i].ssid+"\n"+wifi.networks[i].security==f.wifiIdentity)wifiListFocus=int(i);
  for(size_t i=0;i<bt.devices.size();++i)if(bt.devices[i].address==f.btIdentity)btFocus=int(i);
  auto visibleGames=visible();for(size_t i=0;i<visibleGames.size();++i)if(games[visibleGames[i]].id==f.selectedGame)selected=int(i);
  selected=std::clamp(selected,0,std::max(0,int(visibleGames.size())-1));
  if(screen==Screen::Search)SDL_StartTextInput();
  ++navigationRevision;
}
void App::navigationPush(Screen target) {
  navigation.push_back(navigationFrame());
  if(screen==Screen::Search)SDL_StopTextInput();
  screen=target;row=0;navigationActions=false;navigationFocus=0;systemsQuick=gamesQuick=false;++navigationRevision;
}
void App::navigationBack() {
  if(gamesRoute(screen))libraryRemember();
  if(navigation.empty()){navigationActions=true;navigationFocus=0;systemsQuick=gamesQuick=true;return;}
  auto parent=navigation.back();navigation.pop_back();navigationRestore(parent);
}
void App::navigationGameReturn() {
  if(gameOrigin){navigation.resize(std::min(navigation.size(),gameOriginDepth));navigationRestore(*gameOrigin);gameOrigin.reset();}
  else {screen=store.data.value("library_grouping","unified")=="systems"?Screen::Systems:Screen::Library;navigationActions=false;}
}
void App::startStatistics() {
  if(!core||statistics)return;
  try {statistics=std::make_unique<GameStatistics>(store.gameDir(game)/"statistics.json",game.id,monotonicMs());statisticsTick();}
  catch(const std::exception &error){std::cerr<<"Statistics: "<<error.what()<<"\n";}
}
void App::statisticsTick(bool flush) {
  if(!statistics)return;
  try {
    bool active=core&&screen==Screen::Running&&!core->paused&&!sleeping;
    if(flush){if(!statistics->flush(monotonicMs(),active))std::cerr<<"Statistics: "<<statistics->error<<"\n";}
    else statistics->tick(monotonicMs(),active);
  }catch(const std::exception &error){std::cerr<<"Statistics: "<<error.what()<<"\n";}
}
void App::openSystems() {
  libraryRemember();auto systems=librarySystems();
  for(size_t i=0;i<systems.size();++i)if(systems[i]==section)systemFocus=int(i);
  navigation.clear();explicitSystemsRoot=true;screen=Screen::Systems;section="All";searchQuery.clear();navigationActions=false;systemsQuick=gamesQuick=false;++navigationRevision;
}
void App::openGameInfo() {
  if(ui&&!ui->artworkCache)ui->artworkCache=std::make_unique<ArtworkCache>(options.state/"Cache/Artwork");
  if(screen!=Screen::GameMenu){auto v=visible();if(v.empty())return;game=games[v[std::clamp(selected,0,int(v.size())-1)]];}
  try {gameInfoStatistics=statistics?statistics->snapshot():GameStatistics::read(store.gameDir(game)/"statistics.json",game.id);}catch(...){gameInfoStatistics=Json::object();}
  navigationPush(Screen::GameDetails);
}
void App::navigationSettings(){navigationPush(Screen::Settings);row=settingsFocus;}
std::vector<App::NavigationAction> App::navigationItems() const {
  auto tr=[&](const char *key){return locale.tr(key);};
  std::vector<NavigationAction> items;
  bool root=navigation.empty()&&(screen==Screen::Systems||(screen==Screen::Library&&section=="All"&&store.data.value("library_grouping","unified")=="unified"));
  if(!root&&!navigation.empty()) {
    const auto &parent=navigation.back();
    Screen parentRoute=parent.route;
    if(!explicitSystemsRoot&&navigation.size()==1&&(parentRoute==Screen::Systems||(parentRoute==Screen::Library&&parent.collection=="All")))parentRoute=store.data.value("library_grouping","unified")=="systems"?Screen::Systems:Screen::Library;
    std::string label=parentRoute==Screen::Systems?tr("nav.systems"):parentRoute==Screen::Search?tr("search.title"):parentRoute==Screen::SearchResults?tr("nav.results"):parentRoute==Screen::Settings?tr("screen.settings"):parentRoute==Screen::GameMenu?tr("screen.game_menu"):gamesRoute(parentRoute)?tr("screen.library"):tr("action.back");
    if(parentRoute==Screen::SettingsSection){static const char *keys[]={"section.interface","section.display","section.sound","section.power","section.debug","section.network","clock.title"};label=tr(keys[std::clamp(parent.category,0,6)]);}
    if(parentRoute==Screen::Clock)label=tr("clock.title");
    if(parentRoute==Screen::Wifi)label=tr("wifi.title");
    if(parentRoute==Screen::WifiNetworks)label=tr("wifi.networks");
    if(parentRoute==Screen::Bluetooth)label=tr("bluetooth.title");
    if(parentRoute==Screen::CoreSettings)label=tr("section.cores");
    if(parentRoute==Screen::PsxCoreSettings)label="PlayStation";
    if(parentRoute==Screen::SaveSlots)label=tr("screen.save_slots");
    if(parentRoute==Screen::LoadSlots)label=tr("screen.load_slots");
    if(screen==Screen::Wizard)label=tr("nav.previous_step");
    if(screen==Screen::SystemInfo&&!systemInfoStack.empty()){
      auto page=std::get<0>(systemInfoStack.back());label=locale.tr("sysinfo."+(page=="root"?"title":page.rfind("card",0)==0?"cards":page));
    }
    if(!(screen==Screen::Wizard&&wizardStep==0))items.push_back({"back",label,"arrow-left"});
  }
  bool parentSystems=!navigation.empty()&&navigation.back().route==Screen::Systems;
  if(parentSystems&&navigation.size()==1&&!explicitSystemsRoot&&store.data.value("library_grouping","unified")=="unified")parentSystems=false;
  bool needSystems=gamesRoute(screen)&&!parentSystems;
  if(needSystems&&!root)items.push_back({"systems",tr("nav.systems"),"grid-2x2"});
  if(root) {
    if(screen==Screen::Systems)items.push_back({"all",tr("section.all"),"grid-2x2"});
    items.push_back({"favorites",tr("nav.favorites"),"star-filled"});items.push_back({"recent",tr("nav.recent"),"rotate-ccw-clock"});
  }
  if(screen==Screen::Systems||gamesRoute(screen)) {
    items.push_back({"search",tr("search.title"),"search"});
    if(gamesRoute(screen))items.push_back({"view",tr("library.view"),"layout-list"});
    items.push_back({"settings",tr("screen.settings"),"settings"});
  }else if(screen==Screen::Search){items.push_back({"find",tr("search.apply"),"search"});items.push_back({"settings",tr("screen.settings"),"settings"});}
  if(needSystems&&root)items.push_back({"systems",tr("nav.systems"),"grid-2x2"});
  return items;
}
bool App::libraryActionsAvailable() const {return screen==Screen::Systems||gamesRoute(screen)||screen==Screen::Search;}
bool App::navigationAction(const std::string &a) {
  if(a=="SELECT"&&libraryActionsAvailable()) {
    navigationActions=!navigationActions;systemsQuick=gamesQuick=navigationActions;
    navigationFocus=std::clamp(navigationFocus,0,std::max(0,int(navigationItems().size())-1));
    return true;
  }
  if(a=="B"&&gameOrigin&&(screen==Screen::Recovery||screen==Screen::PsxWarning||screen==Screen::PsxFirmwareError||(screen==Screen::PsxCoreSettings&&!navigation.empty()&&navigation.back().route==Screen::PsxFirmwareError))){navigationGameReturn();return true;}
  if(screen==Screen::InputTest&&a=="MENU") {
    if(motor)motor->stop();testButtons=0;testAxes={};testExtras.clear();mappingTest=false;
    if(!navigation.empty())navigationBack();else {screen=testFromCores?Screen::CoreSettings:Screen::SettingsSection;category=4;row=3;}
    return true;
  }
  const bool gameChild=screen==Screen::GameSettings||screen==Screen::SaveSlots||screen==Screen::LoadSlots||screen==Screen::DeleteSlot||screen==Screen::Overwrite||screen==Screen::LoadConfirm||screen==Screen::ExitConfirm||screen==Screen::DiscardConfirm;
  if(a=="MENU"&&core&&(screen==Screen::GameMenu||gameChild||(screen==Screen::GameDetails&&core))) {
    if(gameChild){while(!navigation.empty()&&navigation.back().route!=Screen::GameMenu)navigation.pop_back();if(!navigation.empty())navigationBack();else{screen=Screen::GameMenu;row=0;}}
    core->paused=true;return true;
  }
  if(screen==Screen::GameMenu&&a=="Y"){openGameInfo();return true;}
  if(screen==Screen::GameMenu||screen==Screen::InputTest||screen==Screen::ScreenshotCapture)return false;
  bool library=screen==Screen::Systems||gamesRoute(screen);
  if(a=="MENU"&&(library||screen==Screen::Search)){navigationSettings();return true;}
  if(screen==Screen::GameDetails) {
    if(a=="Y"&&!core&&game.available){progressOpen();return true;}
    if(a=="B"){navigationBack();return true;}
    if(a=="A"&&(core||game.available)) {
      bool pausedInfo=bool(core);auto entry=game;navigationBack();
      if(pausedInfo)action("B");
      else {gameOrigin=navigationFrame();gameOriginDepth=navigation.size();navigation.push_back(*gameOrigin);++navigationRevision;prepareGame(entry);}
    }
    else if(a=="X"&&!core){auto next=store.data;auto &favorites=next["favorites"];auto found=std::find(favorites.begin(),favorites.end(),game.id);if(found==favorites.end())favorites.push_back(game.id);else favorites.erase(found);persist(next);}
    return true;
  }
  // Editors and dialogs retain their service-specific cancellation handlers.
  // Their parent snapshots are restored by action() only after a real transition.
  if(!library&&screen!=Screen::Search) {
    if(infoOpen||systemInfoSource)return false;
    auto parents=navigationItems();
    if(parents.empty()||parents.front().id!="back")return false;
    if(navigationActions) {
      if(a=="DOWN"){navigationActions=false;return true;}
      if(a=="A"){auto old=screen;actionImpl("B");if(screen!=old&&!navigation.empty())navigationBack();else navigationActions=false;return true;}
      return a=="UP"||a=="LEFT"||a=="RIGHT";
    }
    bool top=screen==Screen::Settings?settingsFocus==0:screen==Screen::Wizard?wizardRow==0:screen==Screen::SystemInfo?systemInfoRow==0:screen==Screen::BluetoothDevices?btFocus==0:screen==Screen::WifiNetworks?wifiListFocus==0:row==0;
    if(top&&a=="UP"){navigationActions=true;navigationFocus=0;return true;}
    return false;
  }
  if((gamesRoute(screen)||screen==Screen::GameMenu)&&a=="Y"){openGameInfo();return true;}
  if(library&&a=="B"){if(gamesRoute(screen)&&navigation.empty()&&section!="All")openSystems();else navigationBack();return true;}
  if(library&&(a=="L1"||a=="R1"||a=="L2"||a=="R2")){navigationActions=false;systemsQuick=gamesQuick=false;}
  auto items=navigationItems();
  if(navigationActions&&!items.empty()) {
    navigationFocus=std::clamp(navigationFocus,0,int(items.size())-1);
    if(a=="LEFT")navigationFocus=std::max(0,navigationFocus-1);
    if(a=="RIGHT")navigationFocus=std::min(int(items.size())-1,navigationFocus+1);
    if(a=="DOWN"){navigationActions=false;systemsQuick=gamesQuick=false;return true;}
    if(a=="A") {
      auto id=items[navigationFocus].id;
      if(id=="systems")openSystems();
      else if(id=="back")navigationBack();
      else if(id=="settings")navigationSettings();
      else if(id=="search")openSearch(true);
      else if(id=="find")searchAction("SEARCH_SUBMIT");
      else if(id=="view")libraryConfigure(screen);
      else if(id=="game"){auto v=visible();if(!v.empty()){game=games[v[std::clamp(selected,0,int(v.size())-1)]];navigationPush(Screen::GameDetails);}}
      else {navigation.push_back(navigationFrame());++navigationRevision;libraryOpen(id=="all"?"All":id=="favorites"?"Favorites":"Recent");navigationActions=false;systemsQuick=gamesQuick=false;}
    }
    return a=="LEFT"||a=="RIGHT"||a=="UP"||a=="DOWN"||a=="A";
  }
  // Game arrows stay in content; Select explicitly toggles the action panel.
  if(a=="UP"&&!gamesRoute(screen)) {
    bool top=screen==Screen::Search?searchRow==0:screen==Screen::Systems&&systemFocus<(store.data.value("system_tiles","grid")=="grid"?3:store.data.value("system_tiles","grid")=="large"?2:1);
    if(top){navigationActions=true;navigationFocus=0;systemsQuick=gamesQuick=true;return true;}
  }
  return false;
}
void App::action(const std::string &a) {
  if(restoreInputFence)return;
  if(screen==Screen::ProgressBusy)return;
  if(screen==Screen::ProgressSources||screen==Screen::ProgressConfirm||screen==Screen::ProgressDone){progressAction(a);return;}
  if(screen==Screen::BackupBusy)return;
  if(screen==Screen::BackupRestart){backupAction(a);return;}
  if(stop||screen==Screen::SystemProgress||screen==Screen::SavingAutomatic)return;
  if(screen==Screen::Running&&a!="MENU"){actionImpl(a);return;}
  struct StatisticsGuard {App &app;~StatisticsGuard(){app.statisticsTick();}} statisticsGuard{*this};
  if(core&&a=="MENU")statisticsTick();
  if(activity())return;
  auto before=navigationFrame();auto revision=navigationRevision;
  try {if(navigationAction(a))return;}catch(const std::exception &error){notice=locale.tr("error.operation");std::cerr<<"Navigation: "<<error.what()<<"\n";return;}
  actionImpl(a);
  if(navigationRevision!=revision||screen==before.route)return;
  if(gamesRoute(before.route)&&(screen==Screen::ResumeChoice||screen==Screen::Launching||screen==Screen::Recovery||screen==Screen::ProgressBusy||screen==Screen::PsxFirmwareError||screen==Screen::CoreCatalog)) {gameOrigin=before;gameOriginDepth=navigation.size();}
  if(screen==Screen::Running){navigation.clear();navigationActions=false;gameInputBlocked=true;++navigationRevision;return;}
  if(a=="B"&&!navigation.empty()){navigationBack();return;}
  // An existing handler may apply/cancel a service editor and select its parent.
  // Recover the exact parent focus instead of its hard-coded fallback row.
  for(size_t i=navigation.size();i>0;--i)if(navigation[i-1].route==screen) {
    auto parent=navigation[i-1];navigation.resize(i-1);navigationRestore(parent);return;
  }
  navigation.push_back(before);navigationActions=false;systemsQuick=gamesQuick=false;++navigationRevision;
}
void App::navigationDraw() {
  if(screen==Screen::ProgressBusy||screen==Screen::ProgressSources||screen==Screen::ProgressConfirm||screen==Screen::ProgressDone)return;
  if(screen==Screen::BackupBusy||screen==Screen::BackupRestart)return;
  if(infoOpen||(screen==Screen::SystemInfo&&systemInfoSource)||screen==Screen::GameDetails||screen==Screen::Running||screen==Screen::GameMenu||screen==Screen::InputTest||screen==Screen::ScreenshotCapture||screen==Screen::SystemProgress||screen==Screen::SavingAutomatic)return;
  auto items=navigationItems();if(items.empty())return;
  ui->rect(20,76,680,46,ui->bg);
  int size=18,gap=6,padding=46,iconSize=20,textOffset=33;std::vector<int> widths;int total=0;
  for(const auto &item:items){int w=ui->measure(item.label,size,500)+46;widths.push_back(w);total+=w+gap;}
  if(total>678){size=17;gap=4;padding=34;iconSize=18;textOffset=28;widths.clear();for(const auto &item:items)widths.push_back(ui->measure(item.label,size,500)+padding);}
  int x=24;
  for(size_t i=0;i<items.size();++i){int w=widths[i];bool selected=navigationActions&&navigationFocus==int(i);
    ui->rect(x,79,w,40,selected?ui->accent:ui->separator,5);ui->rect(x+1,80,w-2,38,selected?ui->focus:ui->bg,4);
    ui->icon(items[i].icon,x+6,89,iconSize);ui->text(items[i].label,x+textOffset,88,size,500,ui->cream,w-textOffset-5);x+=w+gap;
  }
  if(libraryActionsAvailable()) {
    if(navigationActions) {
      auto content=screen==Screen::Systems?"nav.systems":screen==Screen::Search?"nav.keyboard":"nav.games";
      ui->hints({{"A",locale.tr("action.select")},{"SELECT",locale.tr(content)},{"MENU",locale.tr("screen.settings")}});
    }else if(screen==Screen::Systems) {
      ui->hints({{"A",locale.tr("action.open")},{"L1/R1",locale.tr("library.pages")},{"SELECT",locale.tr("nav.panel")},{"MENU",locale.tr("screen.settings")}});
    }else if(gamesRoute(screen)) {
      std::vector<std::pair<std::string,std::string>> hints;
      auto v=visible();if(!v.empty()&&games[v[selected]].available)hints.push_back({"A",locale.tr("action.play")});
      if(!navigation.empty()||section!="All")hints.push_back({"B",items.front().label});
      if(!v.empty()){hints.push_back({"X",locale.tr("nav.favorites")});hints.push_back({"Y",locale.tr("nav.game_info")});}
      hints.push_back({"SELECT",locale.tr("nav.panel")});ui->hints(hints);
    }else if(screen==Screen::Search) {
      if(ui->keyboardHints)ui->hints({{"Enter",locale.tr("search.apply")},{"Esc",locale.tr("action.cancel")},{"Backspace",locale.tr("wifi.delete")},{"SELECT",locale.tr("nav.panel")}});
      else ui->hints({{"A",locale.tr("wifi.choose")},{"B",locale.tr("action.cancel")},{"X",locale.tr("wifi.delete")},{"Y",locale.tr("search.language")},{"SELECT",locale.tr("nav.panel")}});
    }
  }
}
} // namespace rondo
