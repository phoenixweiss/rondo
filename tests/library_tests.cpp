// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include "stb_image_write.h"
#include <cassert>
#include <iostream>
using namespace rondo;
struct MenuSleep:DeviceControl {int count=0;bool canSleep()const override{return true;}std::optional<bool> lidOpen()override{return true;}void sleep(bool,bool)override{++count;}};
int main(int argc,char **argv) {
  assert(argc==3);SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);
  fs::path work=argv[2];Options o;o.assets=argv[1];o.state=work/"state";o.headless=true;o.output=work/"captures";
  o.volumes={{"TF1",work/"roms"}};
  for(const auto &system:{"NES","GB","GBC","GBA","SNES","MD","PSX"}) {
    std::string ext=std::string(system)=="PSX"?"chd":system==std::string("GB")?"gb":system==std::string("GBC")?"gbc":system==std::string("GBA")?"gba":system==std::string("SNES")?"sfc":system==std::string("MD")?"md":"nes";
    for(int i=0;i<9;++i){auto dir=work/"roms"/system/("Library test "+std::to_string(i));fs::create_directories(dir);atomicWrite(dir/("test."+ext),{0});
      std::vector<uint32_t> pixels(90*120,0xff4d8fb9u+unsigned(i)*0x070000);assert(stbi_write_png((dir/"cover.png").c_str(),90,120,4,pixels.data(),90*4));
    }
  }
  App a(o);assert(a.games.size()==63&&a.store.data["game_view"]=="list"&&a.screen==Screen::Library);
  a.selected=4;auto id=a.games[a.visible()[a.selected]].id;auto favorites=a.store.data["favorites"];
  a.action("R2");assert(a.store.data["game_view"]=="tiles"&&a.games[a.visible()[a.selected]].id==id);
  a.action("R2");assert(a.store.data["game_view"]=="list"&&a.selected==4&&a.store.data["favorites"]==favorites);
  a.store.data["game_view"]="tiles";a.selected=8;a.navigationActions=false;a.action("UP");assert(a.navigationActions&&a.selected==8);a.action("DOWN");assert(!a.navigationActions&&a.selected==8);
  a.store.data["game_view"]="list";a.scroll=432;a.action("UP");assert(!a.navigationActions&&a.selected==7);a.selected=4;a.scroll=0;
  // Scrolling back through a long list must not escape at the viewport edge.
  a.selected=24;a.draw();double deepScroll=a.scroll;assert(deepScroll>0);
  for(int i=23;i>=0;--i){a.action("UP");a.draw();assert(a.selected==i&&!a.navigationActions);}
  assert(a.scroll==0);a.action("UP");assert(a.navigationActions);a.action("DOWN");assert(a.selected==0&&!a.navigationActions);a.selected=4;
  a.selected=24;a.draw();auto position=a.navigationFrame();a.navigationFocus=1;
  for(int i=0;i<10;++i){a.action("SELECT");assert(a.navigationActions&&a.selected==position.selected&&a.scroll==position.scroll);a.action("SELECT");assert(!a.navigationActions&&a.selected==position.selected&&a.scroll==position.scroll);}
  a.action("SELECT");a.action("RIGHT");int panelFocus=a.navigationFocus;a.action("SELECT");a.action("SELECT");assert(a.navigationFocus==panelFocus);a.action("SELECT");a.selected=4;a.scroll=0;
  {auto catalog=a.games;auto viewBefore=a.navigationFrame();a.games.clear();a.section="All";
    for(int i=0;i<1000;++i){auto g=catalog.front();g.id="large-library-"+std::to_string(i);g.title="Game "+std::to_string(i);a.games.push_back(g);}
    a.selected=999;a.scroll=0;a.draw();auto last=a.navigationFrame();assert(last.scroll>50000);
    a.action("SELECT");assert(a.navigationActions&&a.selected==999&&a.scroll==last.scroll);a.action("SELECT");assert(!a.navigationActions&&a.selected==999&&a.scroll==last.scroll);
    a.games=std::move(catalog);a.navigationRestore(viewBefore);
  }
  // Replacing a footer must produce the same pixels as a clean render.
  auto footer=[&](){std::vector<uint8_t> out;for(int y=436;y<480;++y){auto *p=static_cast<uint8_t *>(a.ui->surface->pixels)+y*a.ui->surface->pitch;out.insert(out.end(),p,p+720*4);}return out;};
  std::vector<std::pair<std::string,std::string>> finalHints={{"A","Open"},{"MENU","Settings"}};
  a.ui->hints(finalHints);auto cleanFooter=footer();a.ui->hints({{"Y","STALE SEARCH"},{"MENU","STALE LIBRARY MENU"}});a.ui->hints(finalHints);assert(footer()==cleanFooter);
  a.openSearch(true);assert(a.screen==Screen::Search);a.action("B");assert(a.screen==Screen::Library);
  a.action("MENU");assert(a.screen==Screen::Settings);a.action("B");assert(a.screen==Screen::Library&&a.selected==4);
  a.openSearch(true);a.searchText("Library test 4");a.action("SEARCH_SUBMIT");assert(a.screen==Screen::SearchResults&&a.visible().size()==7);a.action("B");assert(a.screen==Screen::Search&&a.searchDraft=="Library test 4");a.action("B");assert(a.screen==Screen::Library&&a.searchQuery.empty());
  a.libraryConfigure(Screen::Library);a.row=0;a.action("RIGHT");a.action("B");assert(a.store.data["library_grouping"]=="unified");
  a.libraryConfigure(Screen::Library);a.row=0;a.action("RIGHT");a.row=4;a.action("A");assert(a.screen==Screen::Systems&&a.store.data["library_grouping"]=="systems");
  a.systemFocus=4;a.action("A");assert(a.section=="SNES");a.selected=5;id=a.games[a.visible()[5]].id;a.action("MENU");assert(a.screen==Screen::Settings);a.action("B");assert(a.screen==Screen::Library&&a.selected==5);a.openSearch(true);assert(a.screen==Screen::Search);a.action("B");assert(a.section=="SNES"&&a.selected==5);a.action("R2");a.action("B");assert(a.screen==Screen::Systems&&a.systemFocus==4);a.action("A");assert(a.games[a.visible()[a.selected]].id==id);
  a.action("X");assert(a.store.data["favorites"].size()==1);a.action("B");a.action("UP");assert(a.systemsQuick==false); // second row moves into first row
  a.navigationActions=true;a.navigationFocus=1;a.action("A");assert(a.section=="Favorites"&&a.visible().size()==1);a.action("X");assert(a.visible().empty()&&a.selected==0);a.action("B");
  for(auto layout:{"grid","large","rail"}){a.store.data["system_tiles"]=layout;a.systemFocus=0;a.systemsQuick=false;a.navigationActions=false;a.action("R1");assert(a.systemFocus==(std::string(layout)=="grid"?6:std::string(layout)=="large"?4:3));a.action("R1");a.action("R1");assert(a.systemFocus==6);a.action("A");assert(a.section=="PSX");a.action("B");assert(a.systemFocus==6);}
  a.store.data["system_tiles"]="grid";a.systemFocus=6;a.navigationActions=false;a.action("UP");assert(a.systemFocus==3&&!a.navigationActions);a.action("UP");assert(a.systemFocus==0&&!a.navigationActions);a.action("UP");assert(a.navigationActions);a.action("DOWN");a.systemFocus=6;
  a.openSearch(true);assert(a.screen==Screen::Search);a.action("B");assert(a.screen==Screen::Systems&&a.systemFocus==6);
  a.libraryOpen("NES");a.selected=8;a.libraryRemember();a.libraryConfigure(Screen::Library);a.row=1;a.action("RIGHT");a.requestSettings=[](const Json &){return false;};auto before=a.store.data;a.row=4;a.action("A");assert(a.store.data==before&&a.screen==Screen::LibraryPreferences);a.requestSettings={};a.action("B");
  a.startWizard(true);a.wizardEnter(1);a.wizardRow=0;a.action("RIGHT");assert(a.wizardStep==1);a.action("Y");assert(a.wizardStep==2&&a.store.data["library_grouping"]==before["library_grouping"]);
  a.wizardEnter(1);a.wizardRow=1;a.action("RIGHT");auto draft=a.wizardDraft["game_view"];a.wizardRow=a.wizardNextRow();a.action("A");assert(a.wizardStep==2&&a.store.data["game_view"]==draft);a.wizardActive=false;
  for(auto lang:{"en","ru"})for(const auto &theme:a.ui->themes["themes"]) {
    auto themeId=theme["id"].get<std::string>();a.locale.select(o.assets,lang);a.ui->setTheme(themeId);a.notice.clear();
    for(auto layout:{"grid","large","rail"}) {a.screen=Screen::Systems;a.systemsQuick=false;a.systemFocus=0;a.store.data["system_tiles"]=layout;a.capture(std::string("systems-")+layout+"-"+themeId+"-"+lang);a.systemFocus=6;a.capture(std::string("systems-last-")+layout+"-"+themeId+"-"+lang);}
    a.libraryOpen("NES");a.selected=4;
    for(auto view:{"list","tiles"}){a.store.data["game_view"]=view;a.capture(std::string("games-")+view+"-"+themeId+"-"+lang);}
    a.libraryConfigure(Screen::Library);a.row=2;a.capture("library-preferences-"+themeId+"-"+lang);a.action("B");
    a.screen=Screen::SettingsSection;a.category=0;a.row=7;a.capture("interface-bottom-"+themeId+"-"+lang);
    a.startWizard(true);a.wizardEnter(1);a.capture("wizard-library-"+themeId+"-"+lang);a.wizardActive=false;
  }
  // Match the real system -> game-list route, including Back in the footer.
  a.navigation.clear();a.store.data["library_grouping"]="systems";a.store.data["system_tiles"]="grid";a.store.data["game_view"]="list";a.screen=Screen::Systems;a.systemFocus=0;a.navigationActions=false;a.systemsQuick=false;a.ui->setTheme("rondo");
  for(auto language:{"en","ru"}){a.locale.select(o.assets,language);a.capture(std::string("console-footer-fixed-")+language);}
  auto *marker=static_cast<uint8_t *>(a.ui->surface->pixels)+145*a.ui->surface->pitch+32*4;
  assert(*reinterpret_cast<uint32_t *>(marker)==SDL_MapRGBA(a.ui->surface->format,a.ui->focus.r,a.ui->focus.g,a.ui->focus.b,255));
  a.action("A");assert(a.screen==Screen::Library&&!a.navigation.empty());
  for(auto language:{"en","ru"}){a.locale.select(o.assets,language);a.capture(std::string("list-parent-footer-")+language);}
  a.action("B");assert(a.screen==Screen::Systems&&a.systemFocus==0);
  a.navigation.clear();a.screen=Screen::Library;a.section="All";a.searchQuery.clear();a.navigationActions=false;a.store.data["library_grouping"]="unified";
  a.selected=3;auto infoOrigin=a.navigationFrame();a.action("Y");assert(a.screen==Screen::GameDetails);a.capture("game-info-library");a.action("B");assert(a.selected==infoOrigin.selected&&a.screen==Screen::Library);
  a.action("B");a.action("RIGHT");a.action("RIGHT");a.action("A");assert(a.screen==Screen::Search);a.action("B");a.navigationActions=false;
  // One typed route snapshot works for every concrete Screen. Confirmed view
  // choices live outside this snapshot and must never be rolled back by Back.
  a.navigation.clear();a.wizardActive=false;a.store.data["library_grouping"]="unified";
  for(int screen=0;screen<=int(Screen::Recovery);++screen) {
    a.store.data["library_grouping"]=screen==int(Screen::Systems)?"systems":"unified";a.screen=Screen(screen);a.section="NES";a.selected=3;a.row=2;a.scroll=37;a.searchDraft="query";a.searchRow=2;a.searchColumn=3;a.navigationActions=true;a.navigationFocus=1;
    auto frame=a.navigationFrame();a.screen=Screen::Settings;a.row=0;a.scroll=0;a.navigationActions=false;a.navigationRestore(frame);
    assert(a.screen==frame.route&&a.row==2&&a.scroll==37&&a.searchDraft=="query"&&a.searchRow==2&&a.searchColumn==3&&a.navigationActions);
  }
  a.navigation.clear();a.screen=Screen::Library;a.section="NES";a.searchQuery.clear();a.navigationActions=false;a.selected=3;
  auto origin=a.navigationFrame();a.openSearch(true);a.searchText("Library test 4");a.action("SEARCH_SUBMIT");assert(a.screen==Screen::SearchResults);
  a.selected=3;auto result=a.navigationFrame();a.gameOrigin=result;a.gameOriginDepth=a.navigation.size();a.screen=Screen::Launching;
  a.navigationGameReturn();assert(a.screen==Screen::SearchResults&&a.selected==3&&a.searchQuery==result.query);
  a.action("MENU");assert(a.screen==Screen::Settings);a.action("A");assert(a.screen==Screen::SettingsSection);a.action("B");a.action("B");assert(a.screen==Screen::SearchResults&&a.selected==3);
  a.action("B");assert(a.screen==Screen::Search&&a.searchDraft=="Library test 4");a.action("B");assert(a.screen==origin.route&&a.selected==origin.selected&&a.section=="NES");
  a.navigationActions=true;a.navigationFocus=0;auto nav=a.navigationItems();
  for(const auto &item:nav)assert(fs::is_regular_file(o.assets/"IconsPNG"/(item.icon+".png")));
  a.navigation.clear();a.navigationActions=false;a.screen=Screen::Settings;a.settingsFocus=9;a.row=9;
  auto sleep=std::make_unique<MenuSleep>();auto *sleepDevice=sleep.get();a.device=std::move(sleep);
  a.action("A");assert(sleepDevice->count==1&&a.screen==Screen::Settings&&a.settingsFocus==9&&!a.stop);
  // A missing ROM has its own symbol, centred with its text as one group.
  a.ui->setTheme("rondo");auto savedGames=a.games;auto savedHistory=a.store.data["recent_games"];
  auto missing=savedGames.front();missing.available=false;a.games={missing};a.section="Recent";a.store.data["recent_games"]=Json::array({{{"id",missing.id}}});a.screen=Screen::Library;a.selected=0;a.scroll=0;a.navigation.clear();a.navigationActions=false;
  for(auto lang:{"en","ru"})for(auto view:{"list","tiles"}){a.locale.select(o.assets,lang);a.store.data["game_view"]=view;a.capture(std::string("file-unavailable-")+view+"-"+lang);}
  a.store.data["game_view"]="list";a.capture("file-unavailable-centering");
  int left=696,right=492,top=344,bottom=140;
  for(int y=140;y<344;++y)for(int x=492;x<696;++x){auto *p=static_cast<uint8_t *>(a.ui->surface->pixels)+y*a.ui->surface->pitch+x*4;Uint8 r,g,b,alpha;SDL_GetRGBA(*reinterpret_cast<uint32_t *>(p),a.ui->surface->format,&r,&g,&b,&alpha);if(r>170&&g>170&&b>170){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}}
  assert(right>left&&bottom>top&&std::abs((left+right)/2-594)<=3&&std::abs((top+bottom)/2-242)<=4);
  a.games=std::move(savedGames);a.store.data["recent_games"]=savedHistory;a.section="All";
  // Every browser view has the same non-destructive panel toggle and clear hint.
  a.ui->setTheme("rondo");a.navigation.clear();a.searchQuery.clear();a.store.data["library_grouping"]="unified";
  for(auto lang:{"en","ru"})for(bool keyboard:{false,true}) {
    a.locale.select(o.assets,lang);a.ui->inputHints(keyboard);
    for(auto screen:{Screen::Systems,Screen::Library,Screen::SearchResults,Screen::Search})for(auto view:{"list","tiles"}) {
      a.screen=screen;a.store.data["game_view"]=view;a.navigationActions=false;a.systemsQuick=a.gamesQuick=false;a.systemFocus=6;a.selected=8;a.scroll=216;a.searchDraft="Library";a.searchRow=2;a.searchColumn=3;
      if(screen==Screen::SearchResults)a.searchQuery="Library";else a.searchQuery.clear();
      a.draw();auto original=a.navigationFrame();auto cfg=a.store.data;
      std::string tag=std::to_string(int(screen))+"-"+view+"-"+lang+(keyboard?"-keyboard":"-pad");
      a.capture("select-content-"+tag);a.action("SELECT");assert(a.navigationActions);a.capture("select-panel-"+tag);a.action("SELECT");
      assert(!a.navigationActions&&a.selected==original.selected&&a.systemFocus==original.system&&a.scroll==original.scroll&&a.searchDraft==original.draft&&a.searchRow==original.keyRow&&a.searchColumn==original.keyColumn&&a.store.data==cfg);
    }
  }
  a.ui->inputHints(false);a.searchQuery.clear();a.screen=Screen::Library;a.selected=0;a.navigation.clear();a.navigationActions=false;
  a.openSearch(true);a.searchText("test");auto panelDraft=a.searchDraft;a.action("SELECT");a.action("MENU");assert(a.screen==Screen::Settings);a.action("B");assert(a.screen==Screen::Search&&a.navigationActions&&a.searchDraft==panelDraft);a.action("SELECT");assert(!a.navigationActions);
  // An open platform remains escapable after View changed the grouping/layout.
  a.screen=Screen::Library;a.section="NES";a.navigation.clear();a.explicitSystemsRoot=false;a.navigationActions=false;a.store.data["library_grouping"]="unified";a.selected=5;
  a.libraryConfigure(Screen::Library);a.row=1;a.action("RIGHT");a.row=4;a.action("A");assert(a.screen==Screen::Library&&a.section=="NES");
  auto orphanActions=a.navigationItems();assert(!orphanActions.empty()&&orphanActions.front().id=="systems");a.capture("systems-return-after-view");
  a.action("B");assert(a.screen==Screen::Systems&&a.systemFocus==0);a.action("A");assert(a.section=="NES"&&a.selected==5);a.action("B");assert(a.screen==Screen::Systems);
  a.screen=Screen::Library;a.section="NES";a.navigation.clear();a.navigationActions=false;a.action("SELECT");a.action("A");assert(a.screen==Screen::Systems);a.action("MENU");a.action("B");assert(a.screen==Screen::Systems);
  a.explicitSystemsRoot=false;a.screen=Screen::Library;a.section="All";a.navigation.clear();a.navigationActions=false;a.store.data["library_grouping"]="unified";
  a.capture("unified-root-systems-shortcut");
  // Same-title games stay distinguishable in both views and search results.
  {auto catalog=a.games;auto names=a.searchNames;auto state=a.navigationFrame();auto cfg=a.store.data;
    Game nes=catalog.front(),md=catalog.front();nes.id="alien-nes";md.id="alien-md";nes.title=md.title="Alien 3";nes.system="NES";md.system="MD";a.games={nes,md};a.searchNames={searchFold(nes.title),searchFold(md.title)};a.section="All";a.navigation.clear();a.navigationActions=false;
    for(auto lang:{"en","ru"})for(auto view:{"list","tiles"}) {
      a.locale.select(o.assets,lang);a.store.data["game_view"]=view;a.screen=Screen::Library;a.searchQuery.clear();a.selected=0;a.capture(std::string("system-label-")+lang+"-"+view+"-nes");
      auto pixels=[&](){std::vector<uint8_t> out;int y=403;for(int row=y;row<y+21;++row){auto *p=static_cast<uint8_t *>(a.ui->surface->pixels)+row*a.ui->surface->pitch;int x=std::string(view)=="list"?492:24;out.insert(out.end(),p+x*4,p+(x+98)*4);}return out;};auto first=pixels();
      a.selected=1;a.capture(std::string("system-label-")+lang+"-"+view+"-md");assert(first!=pixels());
      a.screen=Screen::SearchResults;a.searchQuery=searchFold("Alien");assert(a.visible().size()==2);a.capture(std::string("system-label-")+lang+"-"+view+"-search");
    }
    a.screen=Screen::Library;a.searchQuery.clear();a.locale.select(o.assets,"ru");
    for(const auto &system:{"NES","SNES","GBA","MD","PSX"}) {
      a.games[0].system=system;a.selected=0;a.screen=Screen::Library;a.openGameInfo();a.capture(std::string("full-system-info-")+system);a.action("B");
    }
    a.games.clear();for(int i=0;i<3000;++i){auto g=nes;g.id="large-count-"+std::to_string(i);g.system="MD";a.games.push_back(g);}
    std::vector<uint8_t> positionPixels;
    for(auto view:{"list","tiles"}) {
      a.store.data["game_view"]=view;a.screen=Screen::Library;a.selected=1499;a.capture(std::string("system-label-3000-")+view);
      std::vector<uint8_t> current;for(int y=403;y<427;++y){auto *p=static_cast<uint8_t *>(a.ui->surface->pixels)+y*a.ui->surface->pitch;current.insert(current.end(),p+600*4,p+697*4);}
      if(positionPixels.empty())positionPixels=current;else assert(current==positionPixels);
    }
    a.games=std::move(catalog);a.searchNames=std::move(names);a.store.data=cfg;a.navigationRestore(state);
  }
  // Old wizard progress resumes the same logical step after the new step is inserted.
  auto legacy=before;legacy["wizard"]={{"schema",1},{"step",3},{"completed",false},{"deferred",false}};Store old(work/"legacy");old.commit(legacy);Store migrated(work/"legacy");assert(migrated.data["wizard"]["schema"]==2&&migrated.data["wizard"]["step"]==4);
  for(auto key:{"library_grouping","game_view","system_tiles"}){auto bad=before;bad[key]="unknown";bool rejected=false;try{a.store.commit(bad);}catch(...){rejected=true;}assert(rejected);}
  std::cout<<"PASS: independent grouping/view/layout, legacy defaults/progress, shared search/favorites, focus/back/pages, apply/cancel/rejected writes, setup and EN/RU seven-theme renders.\n";
}
