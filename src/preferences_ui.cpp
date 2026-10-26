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
    else if(a=="Y") {screen=Screen::MappingReset;}
    else if(a=="X") {mappingTest=true;mappingReturnRow=row;openInputTest(true);}
    else if(a=="LEFT"||a=="RIGHT"||a=="A") {
      auto target=controls.at(row).first;int current=mappedSource(store.data,mappingCore,target);
      int value=((current+1+(a=="LEFT"?16:1))%17)-1;
      auto next=store.data;next["input_mapping"][mappingCore][std::to_string(target)]=value;persist(next);
    }
  } else if(screen==Screen::MappingReset) {
    if(a=="B")screen=Screen::Mapping;
    if(a=="A") {auto next=store.data;next["input_mapping"].erase(mappingCore);persist(next);screen=Screen::Mapping;}
  }
}
static double listScroll(int row,int count,double old,int viewport=280) {
  if(row*54<old)old=row*54;if((row+1)*54>old+viewport)old=(row+1)*54-viewport;
  return std::clamp(old,0.0,std::max(0.0,count*54.0-viewport));
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
    ui->hints({{"A",tr("mapping.reset")},{"B",tr("action.cancel")}});
  } else {
    bool mapping=screen==Screen::Mapping;
    auto entries=installedCores();auto controls=coreControls(mappingCore);
    std::string displayCore=mappingCore;for(const auto &c:entries)if(c.id==mappingCore)displayCore=c.name;
    int count=mapping?int(controls.size()):int(entries.size());
    ui->frame(tr(mapping?"mapping.title":screen==Screen::CoreCatalog?"core.catalog":"mapping.cores"),mapping?displayCore:tr("section.cores"),zone);
    featureScroll=listScroll(row,count,featureScroll);SDL_Rect clip{16,128,688,280};SDL_RenderSetClipRect(ui->renderer,&clip);
    for(int i=0;i<count;++i){int y=128+i*54-int(featureScroll);
      if(mapping)ui->row(locale.tr("control."+controls[i].second),y,row==i,672,physicalButton(mappedSource(store.data,mappingCore,controls[i].first)));
      else if(screen==Screen::CoreCatalog){const auto &c=entries[i];if(c.available())ui->binaryRow(c.name,y,row==i,c.enabled(store.data),locale);else ui->row(c.name,y,row==i,672,tr("core.missing"),false);}
      else ui->row(entries[i].name,y,row==i,672,"",true,52,"chevron-right");
    }
    SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar(count*54,280,featureScroll,false,128,408);
    if(mapping)ui->hints({{"LEFT/RIGHT",tr("footer.value")},{"B",tr("action.back")},{"X",tr("mapping.test")},{"Y",tr("mapping.reset")}});
    else ui->hints({{screen==Screen::CoreCatalog?"LEFT/RIGHT":"A",tr(screen==Screen::CoreCatalog?"footer.value":"action.open")},{"B",tr("action.back")}});
  }
}
} // namespace rondo
