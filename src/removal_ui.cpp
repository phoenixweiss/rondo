// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
namespace rondo {
namespace {
const char *categories[]={"removal.rom","removal.media","removal.memory","removal.slots",
                         "removal.screenshots","removal.statistics","removal.library","removal.kept"};
}
bool App::removalRoute() const {return screen==Screen::GameFiles||screen==Screen::RemovalReview||screen==Screen::RemovalFiles||screen==Screen::RemovalPath||screen==Screen::RemovalConfirm||screen==Screen::RemovalBusy||screen==Screen::RemovalResult||screen==Screen::RemovalReport;}
std::vector<const RemovalEntry *> App::removalFiles() const {
  std::vector<const RemovalEntry *> files;
  for(const auto &entry:removalPreview.entries)if(int(entry.category)==removalCategory)files.push_back(&entry);
  return files;
}
void App::removalOpen() {
  if(core)return;
  notice.clear();screen=Screen::GameFiles;row=0;removalCategory=removalIndex=0;removalScroll=0;
  removalChoices.fill(false);removalChoices[0]=true;
}
void App::removalRefresh() {
  if(removalScan.valid())return;
  removalPreview={};removalLoadingId=game.id;
  auto entry=game;auto library=games;auto volumes=options.volumes;auto paths=options.paths();auto preferences=store.data;
  removalScan=std::async(std::launch::async,[entry,library,volumes,paths,preferences]{return previewRemoval(entry,library,volumes,paths,preferences);});
}
void App::removalLibrary() {
  try {
    store=Store(options.paths());games=scan(options.volumes);
    auto missing=recentUnavailable(store.data.at("recent_games"),games);games.insert(games.end(),missing.begin(),missing.end());
    sections={"All","Favorites","Recent"};for(const auto &entry:games)if(std::find(sections.begin(),sections.end(),entry.system)==sections.end())sections.push_back(entry.system);
  }catch(...){notice=locale.tr("removal.unavailable");}
  navigation.clear();navigationActions=false;systemsQuick=gamesQuick=false;section="All";searchQuery.clear();selected=0;scroll=0;
  screen=store.data.value("library_grouping","unified")=="systems"?Screen::Systems:Screen::Library;++navigationRevision;
}
void App::removalStart() {
  if(core||statistics||removalScan.valid()||removalJob.valid()){notice=locale.tr("binary.applying");return;}
  if(screenshotWriter.busy()){notice=locale.tr("screenshot.busy");return;}
  if(bootPending()||btJob.valid()||btQueued||wifiJob.valid()||wifiQueued||clockJob.valid()||clockQueued){notice=locale.tr("binary.applying");return;}
  notice.clear();screenshotMessageUntil=0;removalResult={};removalTotal=0;
  for(const auto &entry:removalPreview.entries)if(removalChoices[size_t(entry.category)])++removalTotal;
  removalDone=std::make_shared<std::atomic<size_t>>(0);
  auto entry=game;auto volumes=options.volumes;auto paths=options.paths();auto preview=removalPreview;auto choices=removalChoices;auto done=removalDone;
  removalJob=std::async(std::launch::async,[entry,volumes,paths,preview,choices,done] {
    return executeRemoval(entry,volumes,paths,preview,choices,[done](size_t count){done->store(count);});
  });screen=Screen::RemovalBusy;
}
void App::removalTick() {
  if(removalJob.valid()&&removalJob.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
    try{removalResult=removalJob.get();}catch(...){removalResult.error="removal.io";}
    try{store=Store(options.paths());}catch(...){store.readOnly=true;}
    screen=Screen::RemovalResult;return;
  }
  if(!removalScan.valid()||removalScan.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
  try {auto result=removalScan.get();if(removalRoute()&&game.id==removalLoadingId)removalPreview=std::move(result);}
  catch(...) {if(removalRoute()&&game.id==removalLoadingId){removalPreview={};removalPreview.warnings={"removal.unavailable"};removalPreview.entries.push_back({RemovalCategory::Kept,{},"","removal.unavailable",0,true});}}
  if(screen==Screen::RemovalReview&&game.id!=removalLoadingId)removalRefresh();
}
void App::removalAction(const std::string &a) {
  if(screen==Screen::RemovalBusy)return;
  if(screen==Screen::RemovalReport){if(a=="B")screen=Screen::RemovalResult;if(a=="UP")removalPathScroll=std::max(0,removalPathScroll-40);if(a=="DOWN")removalPathScroll+=40;return;}
  if(screen==Screen::RemovalResult) {
    if(a=="Y"){removalPathScroll=0;screen=Screen::RemovalReport;}
    if(a=="B")removalLibrary();
    if(a=="A") {
      if(removalResult.completed||removalResult.removed)removalLibrary();
      else {screen=Screen::RemovalReview;removalRefresh();}
    }
    return;
  }
  if(screen==Screen::RemovalConfirm) {
    if(a=="B"||(a=="A"&&row==0)){screen=Screen::RemovalReview;return;}
    if(a=="UP")row=0;
    if(a=="DOWN")row=1;
    if(a=="A"&&row==1)removalStart();
    return;
  }
  if(screen==Screen::GameFiles) {
    if(a=="B"){screen=Screen::GameDetails;return;}
    if(a=="UP")row=0;
    if(a=="DOWN")row=1;
    if(a=="A") {
      if(row==0){screen=Screen::RemovalReview;removalRefresh();}
      else if(game.available) {
        auto parent=navigationFrame();progressOpen();
        if(screen==Screen::ProgressBusy){navigation.push_back(parent);++navigationRevision;}
      }
    }
    return;
  }
  if(a=="B") {
    if(screen==Screen::RemovalPath)screen=Screen::RemovalFiles;
    else if(screen==Screen::RemovalFiles)screen=Screen::RemovalReview;
    else {screen=Screen::GameFiles;row=0;}
    return;
  }
  if(screen==Screen::RemovalPath) {
    if(a=="UP")removalPathScroll=std::max(0,removalPathScroll-40);
    if(a=="DOWN")removalPathScroll+=40;
    return;
  }
  if(screen==Screen::RemovalReview) {
    if(a=="Y"){removalRefresh();return;}
    if(a=="UP")removalCategory=std::max(0,removalCategory-1);
    if(a=="DOWN")removalCategory=std::min(7,removalCategory+1);
    if(removalScan.valid())return;
    if(a=="START"){notice.clear();screen=Screen::RemovalConfirm;row=0;return;}
    if(a=="X"&&removalCategory>0&&removalCategory<7&&!removalFiles().empty())removalChoices[removalCategory]=!removalChoices[removalCategory];
    if(a=="A"){removalIndex=0;screen=Screen::RemovalFiles;}
    return;
  }
  auto files=removalFiles();if(files.empty())return;
  if(a=="UP")removalIndex=std::max(0,removalIndex-1);
  if(a=="DOWN")removalIndex=std::min(int(files.size())-1,removalIndex+1);
  if(a=="L1")removalIndex=std::max(0,removalIndex-3);
  if(a=="R1")removalIndex=std::min(int(files.size())-1,removalIndex+3);
  if(a=="A"){removalPathScroll=0;screen=Screen::RemovalPath;}
}
void App::removalDraw(const std::string &zone) {
  auto tr=[this](const char *key){return locale.tr(key);};
  if(screen==Screen::RemovalReport) {
    auto text=game.title+"\n"+locale.tr(removalResult.completed?"removal.done":removalResult.removed?"removal.partial_result":"removal.stopped");
    if(!removalResult.error.empty())text+="\n\n"+locale.tr(removalResult.error);
    if(!removalResult.failed.empty())text+="\n"+removalResult.failed.generic_string();
    for(const auto &path:removalResult.removedPaths)text+="\n\n"+tr("removal.removed_file")+"\n"+path.generic_string();
    if(removalResult.recordsApplied)text+="\n\n"+tr("removal.records_removed");
    if(!removalResult.receipt.empty())text+="\n\n"+tr("removal.receipt")+"\n"+removalResult.receipt.generic_string();
    removalPathScroll=ui->info(tr("removal.details"),text,removalPathScroll);
    ui->hints({{"UP/DOWN",tr("removal.scroll")},{"B",tr("action.back")}});return;
  }
  auto files=removalFiles();
  auto context=game.volume+" · "+game.system+" · "+game.title;
  if(screen==Screen::RemovalConfirm) {
    ui->frame(tr("removal.confirm"),context,zone);
    uintmax_t bytes=0;int count=0;for(const auto &entry:removalPreview.entries)if(removalChoices[size_t(entry.category)]){++count;bytes+=entry.bytes;}
    ui->text(locale.tr("removal.selected",{{"count",count}})+" · "+storageBytes(bytes,locale.current),32,142,25,600,ui->cream,656);
    ui->text(tr("removal.permanent"),32,202,23,500,ui->cream,656);
    ui->text(tr("removal.unselected_kept"),32,246,21,400,ui->secondary,656);
    ui->row(tr("action.cancel"),312,row==0);
    ui->row(tr("removal.delete"),366,row==1,672,"",true,52,"","trash");
    ui->hints({{"A",tr("action.select")},{"B",tr("action.back")}});return;
  }
  if(screen==Screen::RemovalBusy) {
    ui->frame(tr("removal.deleting"),context,zone);
    ui->text(tr("removal.wait"),32,164,24,500,ui->cream,656);
    auto count=removalDone?removalDone->load():0;
    ui->text(std::to_string(count)+" / "+std::to_string(removalTotal),32,224,24,500,ui->cream,656);
    ui->hints({});return;
  }
  if(screen==Screen::RemovalResult) {
    ui->frame(tr(removalResult.completed?"removal.done":removalResult.removed?"removal.partial_result":"removal.stopped"),context,zone);
    ui->text(locale.tr("removal.removed_count",{{"done",removalResult.removed},{"total",removalResult.total}}),32,142,25,600,ui->cream,656);
    ui->text(storageBytes(removalResult.bytes,locale.current),32,188,23,500,ui->secondary,656);
    if(!removalResult.error.empty())ui->text(locale.tr(removalResult.error),32,244,23,500,ui->cream,656);
    if(!removalResult.failed.empty())ui->text(removalResult.failed.filename().string(),32,290,21,400,ui->secondary,656);
    ui->text(tr("removal.unselected_kept"),32,374,21,400,ui->secondary,656);
    std::vector<std::pair<std::string,std::string>> hints{{"A",tr(removalResult.completed||removalResult.removed?"screen.library":"gallery.refresh")},{"Y",tr("removal.details")}};
    if(!removalResult.completed&&!removalResult.removed)hints.push_back({"B",tr("screen.library")});
    ui->hints(hints);return;
  }
  if(screen==Screen::GameFiles) {
    ui->frame(tr("removal.files"),context,zone);
    ui->row(tr("removal.title"),128,row==0,672,"",true,52,"chevron-right","trash");
    ui->row(tr("progress.relocate"),182,row==1,672,"",game.available,52,"chevron-right");
    ui->hints({{"A",tr("action.open")},{"B",tr("action.back")}});return;
  }
  if(screen==Screen::RemovalPath&&!files.empty()) {
    const auto &entry=*files.at(removalIndex);
    auto text=entry.path.generic_string();
    if(entry.record&&!entry.location.empty())text+="\n"+entry.location+" · "+tr("removal.record");
    if(!entry.reason.empty())text+="\n\n"+locale.tr(entry.reason);
    if(!entry.record)text+="\n\n"+storageBytes(entry.bytes,locale.current);
    removalPathScroll=ui->info(tr(categories[removalCategory]),text,removalPathScroll);
    ui->hints({{"UP/DOWN",tr("removal.scroll")},{"B",tr("action.back")}});return;
  }
  ui->frame(tr(screen==Screen::RemovalReview?"removal.title":categories[removalCategory]),context,zone);
  if(screen==Screen::RemovalReview) {
    if(removalScan.valid()){ui->text(tr("removal.loading"),32,164,24,500,ui->cream,656);ui->hints({{"B",tr("action.back")}});return;}
    int viewport=208,height=52;
    if(removalCategory*height<removalScroll)removalScroll=removalCategory*height;
    if(removalCategory*height+height>removalScroll+viewport)removalScroll=removalCategory*height+height-viewport;
    removalScroll=std::clamp(removalScroll,0.0,double(8*height-viewport));
    SDL_Rect clip{16,128,688,viewport};SDL_RenderSetClipRect(ui->renderer,&clip);
    for(int category=0;category<8;++category) {
      uintmax_t bytes=0;int count=0;
      for(const auto &entry:removalPreview.entries)if(int(entry.category)==category){++count;bytes+=entry.bytes;}
      int y=128+category*height-int(removalScroll);if(y+height<128||y>=336)continue;
      auto detail=std::to_string(count);if(bytes)detail+=" · "+storageBytes(bytes,locale.current);
      ui->row("",y,category==removalCategory,672,detail,true,50,"chevron-right");
      if(category<7){ui->rect(32,y+16,20,20,ui->secondary,3);ui->rect(34,y+18,16,16,ui->bg,1);if(removalChoices[category])ui->icon("check",32,y+16,20);}
      ui->text(tr(categories[category]),category<7?64:32,y+12,22,category==removalCategory?700:500,ui->cream,400);
    }
    SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar(8*height,viewport,removalScroll,false,128,336);
    uintmax_t bytes=0;int count=0;
    for(const auto &entry:removalPreview.entries)if(removalChoices[size_t(entry.category)]){++count;bytes+=entry.bytes;}
    auto total=locale.tr("removal.selected",{{"count",count}})+" · "+storageBytes(bytes,locale.current);
    ui->text(total,32,360,21,600,ui->cream,656);
    ui->text(tr(removalPreview.warnings.empty()?"removal.unselected_kept":"removal.partial"),32,400,20,400,ui->secondary,656);
    std::vector<std::pair<std::string,std::string>> hints{{"A",tr("removal.files")},{"B",tr("action.back")}};
    if(removalCategory>0&&removalCategory<7&&!files.empty())hints.push_back({"X",tr("removal.include")});
    hints.push_back({"START",tr("removal.delete")});ui->hints(hints);return;
  }
  if(files.empty())ui->text(tr("removal.empty"),32,164,24,500,ui->secondary,656);
  for(int i=removalIndex/3*3;i<std::min(int(files.size()),removalIndex/3*3+3);++i) {
    const auto &entry=*files[i];int y=128+(i%3)*86;
    auto title=entry.path.filename().string();if(title.empty())title=locale.tr(entry.reason);
    if(entry.record&&!entry.location.empty())title=entry.location;
    ui->row("",y,i==removalIndex,672,"",true,82);
    ui->text(title,32,y+10,23,i==removalIndex?700:500,ui->cream,640);
    auto detail=entry.location;if(entry.record)detail=tr("removal.record");else detail+=" · "+storageBytes(entry.bytes,locale.current);
    if(!entry.reason.empty())detail+=(detail.empty()?"":" · ")+locale.tr(entry.reason);
    ui->text(detail,32,y+46,19,400,ui->secondary,640);
  }
  ui->text(tr("removal.preserved"),32,396,20,400,ui->secondary,656);
  ui->hints(files.empty()?std::vector<std::pair<std::string,std::string>>{{"B",tr("action.back")}}:
    std::vector<std::pair<std::string,std::string>>{{"A",tr("removal.path")},{"B",tr("action.back")},{"L1/R1",tr("library.pages")}});
}
}
