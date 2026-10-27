// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
namespace rondo {
static const std::vector<std::string> keys{"library_grouping","game_view","artwork","system_tiles"};
static const std::vector<std::vector<std::string>> values{{"unified","systems"},{"list","tiles"},{"cover","title","screenshot"},{"grid","large","rail"}};
static const std::vector<std::string> collections{"All","Favorites","Recent"};
std::vector<std::string> App::librarySystems() const {
  std::vector<std::string> out;
  for(const auto &s:{"NES","GB","GBC","GBA","SNES","MD","PSX"})
    if(std::find(sections.begin(),sections.end(),s)!=sections.end())out.push_back(s);
  return out;
}
void App::libraryRemember() {
  auto v=visible();if(!v.empty())collectionFocus[section]=games[v[std::clamp(selected,0,int(v.size())-1)]].id;
}
void App::libraryRestore() {
  selected=0;scroll=0;gamesQuick=false;auto v=visible();auto id=collectionFocus[section];
  for(size_t i=0;i<v.size();++i)if(games[v[i]].id==id)selected=int(i);
}
void App::libraryOpen(const std::string &target) {
  libraryRemember();section=target;searchQuery.clear();libraryRestore();screen=Screen::Library;navigationActions=false;
}
void App::openSearch(bool) {
  if(screen==Screen::SearchResults&&!navigation.empty()&&navigation.back().route==Screen::Search){navigationBack();return;}
  navigationPush(Screen::Search);
  searchNames.clear();for(const auto &entry:games)searchNames.push_back(searchFold(entry.title));
  searchDraft.clear();searchRow=searchColumn=0;SDL_StartTextInput();
}
std::string App::libraryValue(const std::string &key,const Json &data) const {
  auto it=std::find(keys.begin(),keys.end(),key);if(it==keys.end())return "";
  auto i=size_t(it-keys.begin());return locale.tr("library.value."+data.value(key,values[i][0]));
}
void App::libraryCycle(Json &data,int index,int delta) {
  const auto &list=values.at(index);auto current=data.value(keys[index],list[0]);
  int pos=int(std::find(list.begin(),list.end(),current)-list.begin());
  data[keys[index]]=list[(pos+delta+int(list.size()))%int(list.size())];
}
void App::libraryConfigure(Screen) {
  libraryRemember();libraryDraft=store.data;navigationPush(Screen::LibraryPreferences);row=0;
}
void App::libraryPreferenceAction(const std::string &a) {
  if(screen==Screen::LibraryMenu) {
    if(a=="UP")row=std::max(0,row-1);if(a=="DOWN")row=std::min(2,row+1);
    if(a=="B")navigationBack();
    if(a=="A") {if(row==0)navigationSettings();else if(row==1)openSearch(true);else libraryConfigure(screen);}
    return;
  }
  if(a=="UP")row=std::max(0,row-1);if(a=="DOWN")row=std::min(4,row+1);
  if(a=="B"){navigationBack();return;}
  if(row<4&&(a=="LEFT"||a=="RIGHT"||a=="A"))libraryCycle(libraryDraft,row,a=="LEFT"?-1:1);
  if(row==4&&a=="A") {
    auto next=store.data;for(const auto &key:keys)next[key]=libraryDraft.at(key);
    persist(next);artwork=store.data.value("artwork","cover");
    navigationBack();
    // Resolve a changed root grouping without discarding an open collection.
    if(navigation.empty()&&section=="All"&&screen!=Screen::SettingsSection)
      screen=store.data.value("library_grouping","unified")=="systems"?Screen::Systems:Screen::Library;
  }
}
void App::libraryPreferenceDraw(const std::string &zone) {
  auto tr=[&](const std::string &s){return locale.tr(s);};
  ui->frame(tr(screen==Screen::LibraryMenu?"library.menu":"library.appearance"),screen==Screen::LibraryMenu?"":tr("section.interface"),zone);
  if(screen==Screen::LibraryMenu) {
    for(int i=0;i<3;++i)ui->row(tr(i==0?"screen.settings":i==1?"search.title":"library.appearance"),128+i*54,row==i,672,"",true,52,"chevron-right");
    ui->hints({{"A",tr("action.open")},{"B",tr("action.back")}});return;
  }
  for(int i=0;i<4;++i) {
    auto label=libraryValue(keys[i],libraryDraft);
    ui->valueRow(tr("library."+keys[i]),128+i*54,row==i,672,label);
    if(i==1) {int x=672-8-ui->measure(label,21)-24-30;ui->icon(libraryDraft.value("game_view","list")=="list"?"layout-list":"layout-grid",x,128+i*54+14,24);}
  }
  ui->row(tr("action.apply"),344,row==4,672,"",true,52,"chevron-right");
  ui->hints({{"LEFT/RIGHT",tr("footer.value")},{"A",tr(row==4?"action.apply":"action.change")},{"B",tr("action.cancel")}});
}
void App::systemsAction(const std::string &a) {
  auto systems=librarySystems();int count=int(systems.size());auto layout=store.data.value("system_tiles","grid");int page=layout=="grid"?6:layout=="large"?4:3,cols=layout=="grid"?3:layout=="large"?2:1;
  systemFocus=std::clamp(systemFocus,0,std::max(0,count-1));
  if(a=="A"&&count) {auto target=systems[systemFocus];navigationPush(Screen::Library);libraryOpen(target);return;}
  if(a=="L1"||a=="R1"){systemFocus=std::clamp(systemFocus+(a=="R1"?page:-page),0,std::max(0,count-1));systemsQuick=false;return;}
  if(a=="UP")systemFocus=std::max(0,systemFocus-cols);
  if(a=="DOWN")systemFocus=std::min(std::max(0,count-1),systemFocus+cols);
  if(a=="LEFT")systemFocus=std::max(0,systemFocus-1);
  if(a=="RIGHT")systemFocus=std::min(std::max(0,count-1),systemFocus+1);
}
bool App::libraryAction(const std::string &a) {
  bool tiles=store.data.value("game_view","list")=="tiles";
  if(a=="L2"||a=="R2") {libraryRemember();auto next=store.data;next["game_view"]=tiles?"list":"tiles";persist(next);scroll=0;return true;}
  auto v=visible();int count=int(v.size());
  if(a=="L1"||a=="R1") {selected=std::clamp(selected+(a=="R1"?1:-1)*(tiles?4:5),0,std::max(0,count-1));libraryRemember();return true;}
  if(tiles&&(a=="LEFT"||a=="RIGHT"||a=="UP"||a=="DOWN")) {
    selected=std::clamp(selected+(a=="LEFT"?-1:a=="RIGHT"?1:a=="UP"?-4:4),0,std::max(0,count-1));libraryRemember();return true;
  }
  return a=="LEFT"||a=="RIGHT";
}
void App::libraryQuickDraw() {} // Shared visible actions are rendered by navigationDraw.
void App::systemsDraw(const std::string &zone) {
  ui->frame(locale.tr("screen.library"),"",zone);libraryQuickDraw();auto systems=librarySystems();
  if(systems.empty()){systemsQuick=true;ui->text(locale.tr("empty.library"),32,185,27,700);return;}
  systemFocus=std::clamp(systemFocus,0,int(systems.size())-1);auto layout=store.data.value("system_tiles","grid");
  int count=layout=="grid"?6:layout=="large"?4:3,cols=layout=="grid"?3:layout=="large"?2:3;
  int start=systemFocus/count*count;
  int railX=24;
  for(int j=start;j<std::min(start+count,int(systems.size()));++j) {
    bool focus=j==systemFocus&&!systemsQuick;int i=j-start;
    int w=layout=="rail"?(j==systemFocus?320:160):(660-(cols-1)*10)/cols;
    int h=layout=="rail"?243:122,x=layout=="rail"?railX:24+(i%cols)*(w+10),y=layout=="rail"?136:131+(i/cols)*132;railX+=w+10;
    ui->rect(x,y,w,h,focus?ui->accent:ui->separator,9);ui->rect(x+1,y+1,w-2,h-2,focus?ui->focus:ui->bg,8);
    std::string name=systems[j],icon="system-"+name;std::transform(icon.begin(),icon.end(),icon.begin(),[](unsigned char c){return std::tolower(c);});
    int iw=layout=="grid"?96:layout=="rail"&&j==systemFocus?220:120,ih=iw*100/160;
    int ix=layout=="large"?x+16:x+(w-iw)/2,iy=layout=="rail"?y+27:layout=="large"?y+22:y+10;
    ui->icon(icon,ix,iy,iw,ih,focus?ui->accent:ui->cream);
    int tx=layout=="large"?x+156:x,ty=layout=="rail"?y+170:layout=="large"?y+31:y+73,tw=layout=="large"?w-165:w;
    auto label=name=="MD"?"Mega Drive":name=="PSX"?"PlayStation":name;
    ui->text(label,tx+std::max(0,(tw-ui->measure(label,21))/2),ty,21,500,ui->cream,tw);
    int gamesCount=0;for(const auto &g:games)if(g.available&&g.system==name)++gamesCount;
    auto number=std::to_string(gamesCount);ui->text(number,tx+(tw-ui->measure(number,17))/2,ty+25,17,400,ui->secondary,tw);
  }
  ui->text(systemName(systems[systemFocus]),24,397,18,400,ui->secondary,570);
  auto pos=std::to_string(systemFocus+1)+" / "+std::to_string(systems.size());ui->text(pos,696-ui->measure(pos,18),397,18,400,ui->secondary);
  ui->scrollbar(systems.size(),count,start,false,131,385);
}
static void tileTitle(UI &ui,std::string text,int x,int y,int width) {
  std::string first=text;while(!first.empty()&&ui.measure(first,18)>width)first=utf8Prefix(first,first.size()-1);
  if(first!=text){auto pos=first.find_last_of(' ');if(pos!=std::string::npos&&pos>first.size()/2)first.resize(pos);}
  auto second=text.substr(first.size());while(!second.empty()&&second.front()==' ')second.erase(0,1);
  ui.text(first,x,y,18,500,ui.cream,width);ui.text(second,x,y+22,18,500,ui.cream,width);
}
void App::gamePositionDraw(const Game &g,size_t count) {
  constexpr int y=403;
  auto position=std::to_string(selected+1)+"/"+std::to_string(count);
  int counterX=696-ui->measure(position,18,400);
  if(store.data.value("game_view","list")=="tiles") {
    auto label=systemName(g.system);
    if(screen==Screen::SearchResults)label+=" · "+locale.tr("search.filter",{{"query",searchQuery}});
    ui->text(label,24,y,18,400,ui->secondary,std::max(0,counterX-24-24));
  } else {
    if(screen==Screen::SearchResults)ui->text(locale.tr("search.filter",{{"query",searchQuery}}),24,y,18,400,ui->secondary,444);
    ui->text(g.system,492,y,18,400,ui->secondary,std::max(0,counterX-492-16));
  }
  ui->text(position,counterX,y,18,400,ui->secondary);
}
void App::gameTilesDraw(const std::string &zone) {
  auto tr=[&](const std::string &s){return locale.tr(s);};ui->frame(libraryTitle(),"",zone);
  libraryQuickDraw();
  auto v=visible();if(v.empty()) {
    ui->text(tr(!searchQuery.empty()?"search.empty":section=="Favorites"?"empty.favorites":section=="Recent"?"empty.recent":"empty.library"),32,185,27,700,ui->cream,656);
    return;
  }
  selected=std::clamp(selected,0,int(v.size())-1);int start=selected/4*4;
  std::vector<ArtworkRequest> requests;
  auto request=[&](int i){return ArtworkRequest{games[v[i]].path.parent_path()/(artwork+".png"),140,204,artwork=="cover"};};
  if(games[v[selected]].available)requests.push_back(request(selected));
  for(int i=start;i<std::min(start+4,int(v.size()));++i)if(i!=selected&&games[v[i]].available)requests.push_back(request(i));
  if(start+4<int(v.size())&&games[v[start+4]].available)requests.push_back(request(start+4));
  if(ui->artworkCache)ui->artworkCache->select(requests);bool pending=false;
  auto favorites=store.data.value("favorites",Json::array());
  for(int i=start;i<std::min(start+4,int(v.size()));++i) {
    const auto &g=games[v[i]];int x=24+(i-start)*168,y=128;bool focus=i==selected&&!gamesQuick;
    int border=focus?2:1;
    ui->rect(x,y,158,260,focus?ui->accent:ui->separator,7);ui->rect(x+border,y+border,158-border*2,260-border*2,focus?ui->focus:ui->bg,6);
    bool loaded=g.available&&ui->artworkImage({request(i)},x+9,y+9,140,198,false);if(g.available)pending|=ui->artworkPending;
    if(!loaded){auto label=tr(g.available?(artwork=="cover"?"art.missing_cover":artwork=="title"?"art.missing_title":"art.missing_gameplay"):"recent.unavailable_short");if(!g.available||!ui->artworkPending)ui->artworkPlaceholder(x+9,y+9,140,198,label,!g.available);}
    tileTitle(*ui,g.title,x+8,y+211,142);
    if(std::find(favorites.begin(),favorites.end(),g.id)!=favorites.end()) {ui->circle(x+124,y+7,28,ui->bg,true);ui->icon("star-filled",x+129,y+12,18);}
  }
  ui->artworkPending=pending;const auto &g=games[v[selected]];
  gamePositionDraw(g,v.size());
}
} // namespace rondo
