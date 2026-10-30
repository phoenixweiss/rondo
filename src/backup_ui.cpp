// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
#include <fstream>
#include <cctype>
#include <iostream>
namespace rondo {
namespace {
BackupContext context(const Options &options) {
  BackupContext result;
  try {result.version=readJson(options.assets.parent_path()/"build-info.json").value("version",std::string());}catch(...){}
  if(result.version.empty()) {
    std::ifstream input(options.assets.parent_path()/"VERSION");std::getline(input,result.version);
    while(!result.version.empty()&&std::isspace(static_cast<unsigned char>(result.version.back())))result.version.pop_back();
  }
  std::set<fs::path> paths;
  for(const auto *system:{"NES","GB","GBA","SNES","MD","PSX"}) {
    auto path=options.coreFor(system);if(!path.empty()&&fs::is_regular_file(path)&&paths.insert(path).second)result.cores.insert(hashFile(path));
  }
  return result;
}
Json summary(const fs::path &folder) {
  std::string digest;auto manifest=backupManifest(folder,&digest);std::set<std::string> games;
  for(const auto &file:manifest["files"]) {auto name=file["path"].get<std::string>();if(name.find("games/")==0)games.insert(fs::path(name).parent_path().filename().string());}
  return {{"created_utc",manifest.at("created_utc")},{"files",manifest["files"].size()},
          {"games",games.size()},{"bytes",manifest.at("total_bytes")},{"version",manifest.at("rondo_version")},{"sha256",digest}};
}
}
void App::backupOpen() {
  if(core||statistics){notice=locale.tr("backup.close_game");return;}
  backupList=backupFolders(options.state);backupMetadata.clear();backupError.clear();backupScroll=0;row=0;screen=Screen::Backups;
}
void App::backupStart(bool restore,bool deleting) {
  if(core||statistics){notice=locale.tr("backup.close_game");return;}
  if(bootPending()||btJob.valid()||btQueued||wifiJob.valid()||wifiQueued||clockJob.valid()||clockQueued){notice=locale.tr("binary.applying");return;}
  volumeTick(true);
  backupProgress=std::make_shared<BackupProgress>();auto progress=backupProgress;
  auto opts=options;auto folder=backupSelected;auto expected=backupSummary.value("sha256",std::string());
  Json catalog=Json::array();for(const auto &game:games)catalog.push_back({{"id",game.id},{"title",game.title},{"system",game.system}});
  backupDeleting=deleting;backupRestoring=restore;backupError.clear();notice.clear();infoOpen=false;
  backupJob=std::async(std::launch::async,[opts,folder,restore,deleting,progress,expected,catalog] {
    auto reload=[&] {std::error_code error;bool pending=fs::exists(opts.state/".restore-transaction",error);return restore&&(progress->phase>=3||pending||bool(error));};
    try {
      if(deleting){progress->phase=5;deleteBackup(opts.state,folder,expected);return Json{{"ok",true},{"reload",false}};}
      auto inputs=context(opts);inputs.expectedManifest=expected;inputs.games=catalog;
      if(restore){restoreBackup(opts.state,folder,inputs,progress.get());return Json{{"ok",true},{"reload",true}};}
      return Json{{"ok",true},{"folder",createBackup(opts.state,inputs,progress.get()).string()}};
    }catch(const BackupError &error){return Json{{"ok",false},{"error",error.code},{"reload",reload()}};}
    catch(const std::exception &error){std::cerr<<"Backup: "<<error.what()<<"\n";return Json{{"ok",false},{"error","backup.io"},{"reload",reload()}};}
  });
  screen=Screen::BackupBusy;
}
void App::backupTick() {
  if(!backupJob.valid()||backupJob.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
  auto result=backupJob.get();backupError=result.value("ok",false)?"":result.value("error","backup.io");
  if(backupRestoring&&result.value("reload",true)) {
    // Resolve any post-rename failure before another settings writer is allowed.
    try {store=Store(options.state);previewVolume=store.data.value("volume",80);}
    catch(const std::exception &error){backupError="backup.unfinished";std::cerr<<"Restore recovery: "<<error.what()<<"\n";}
    screen=Screen::BackupRestart;return;
  }
  screen=Screen::Backups;row=0;backupMetadata.clear();
  while(!navigation.empty()) {
    auto route=navigation.back().route;
    if(route!=Screen::Backups&&route!=Screen::BackupDetails&&route!=Screen::BackupGames&&route!=Screen::BackupDelete&&route!=Screen::BackupConfirm)break;
    navigation.pop_back();
  }
  try {backupList=backupFolders(options.state);}catch(...){backupError="backup.io";}
  notice=locale.tr(backupError.empty()?(backupDeleting?"backup.deleted":"backup.created"):backupError);noticeUntil=monotonicMs()+6000;
}
void App::backupAction(const std::string &action) {
  if(screen==Screen::BackupRestart){if(action=="A"){systemExitCode=66;stop=true;}return;}
  if(screen==Screen::BackupConfirm||screen==Screen::BackupDelete) {
    bool deleting=screen==Screen::BackupDelete;
    if(action=="B"){screen=Screen::BackupDetails;row=deleting?2:1;return;}
    if(action=="UP")row=0;
    if(action=="DOWN")row=1;
    if(action=="A"){if(row==0){screen=Screen::BackupDetails;row=deleting?2:1;}else backupStart(!deleting,deleting);}return;
  }
  if(screen==Screen::BackupGames) {
    if(action=="UP")row=std::max(0,row-1);
    if(action=="DOWN")row=std::min(std::max(0,int(backupGameList.size())-1),row+1);
    if(action=="L1")row=std::max(0,row-3);
    if(action=="R1")row=std::min(std::max(0,int(backupGameList.size())-1),row+3);
    if(action=="B"){screen=Screen::BackupDetails;row=0;}
    return;
  }
  if(screen==Screen::BackupDetails) {
    if(action=="UP")row=std::max(0,row-1);
    if(action=="DOWN")row=std::min(2,row+1);
    if(action=="B"){screen=Screen::Backups;return;}
    if(action=="A") {
      if(row==0){if(!backupSummary.empty()){screen=Screen::BackupGames;row=0;backupScroll=0;}}
      else if(row==1){if(!backupSummary.empty()){screen=Screen::BackupConfirm;row=0;}}
      else {screen=Screen::BackupDelete;row=0;}
    }
    return;
  }
  if(action=="UP")row=std::max(0,row-1);
  if(action=="DOWN")row=std::min(int(backupList.size()),row+1);
  if(action=="L1")row=std::max(0,row-5);
  if(action=="R1")row=std::min(int(backupList.size()),row+5);
  if(action=="B"){settingsFocus=9;row=9;screen=Screen::Settings;return;}
  if(action!="A")return;
  if(row==0){backupStart(false);return;}
  try {
    backupSelected=backupList.at(row-1);backupSummary=Json::object();backupGameList=Json::array();
    try {backupSummary=summary(backupSelected);backupGameList=backupGames(backupSelected,games);}catch(...){}
    screen=Screen::BackupDetails;row=0;notice.clear();
  }catch(...){notice=locale.tr("backup.invalid");}
}
void App::backupDraw(const std::string &zone) {
  auto tr=[this](const std::string &key){return locale.tr(key);};
  ui->frame(tr(screen==Screen::BackupConfirm?"backup.restore":screen==Screen::BackupDelete?"backup.delete":screen==Screen::BackupGames?"backup.games":"backup.title"),tr("screen.settings"),zone);
  if(screen==Screen::BackupBusy) {
    const char *phases[]={"backup.preparing","backup.copying","backup.checking","backup.applying","backup.rolling_back","backup.deleting"};
    ui->text(tr(phases[std::clamp(backupProgress->phase.load(),0,5)]),32,164,26,600,ui->cream,656);
    auto total=backupProgress->total.load(),done=backupProgress->done.load();
    ui->rect(32,234,656,8,ui->separator,4);
    if(total)ui->rect(32,234,int(656*std::min(1.0,double(done)/total)),8,ui->accent,4);
    if(total)ui->text(std::to_string(done)+" / "+std::to_string(total),32,266,23,400,ui->secondary,656);
    ui->hints({});return;
  }
  if(screen==Screen::BackupRestart) {
    ui->text(tr(backupError.empty()?"backup.restored":backupError),32,150,24,600,ui->cream,656);
    ui->text(tr("backup.restart_note"),32,214,23,400,ui->secondary,656);
    ui->row(tr("backup.restart"),312,true,672,"",true,52,"chevron-right");
    ui->hints({{"A",tr("backup.restart")}});return;
  }
  if(screen==Screen::BackupDetails||screen==Screen::BackupDelete) {
    bool valid=!backupSummary.empty();
    ui->text(valid?slotDate(backupSummary.at("created_utc")):tr("backup.invalid"),32,132,24,600,ui->cream,656);
    std::string detail=valid?std::to_string((backupSummary.at("bytes").get<uint64_t>()+1048575)/1048576)+" MB · Rondo "+backupSummary.at("version").get<std::string>():backupSelected.filename().string();
    ui->text(detail,32,174,22,400,ui->secondary,656);
    if(screen==Screen::BackupDelete) {
      ui->text(tr("backup.delete_question"),32,222,23,600,ui->cream,656);
      ui->text(tr("backup.delete_preserved"),32,256,22,400,ui->secondary,656);
      ui->row(tr("action.cancel"),304,row==0);
      ui->row(tr("backup.delete"),358,row==1,672,"",true,52,"","trash");
    } else {
      ui->row(tr("backup.games"),220,row==0,672,valid?std::to_string(backupGameList.size()):"",valid,52,"chevron-right","gamepad-2");
      ui->row(tr("backup.restore"),274,row==1,672,"",valid,52,"chevron-right","rotate-ccw-clock");
      ui->row(tr("backup.delete"),328,row==2,672,"",true,52,"chevron-right","trash");
    }
    ui->hints({{"A",tr("action.select")},{"B",tr("action.back")},{"X",tr("footer.info")}});return;
  }
  if(screen==Screen::BackupGames) {
    int count=int(backupGameList.size()),viewport=266,content=0;std::vector<int> offsets,heights;
    for(const auto &game:backupGameList) {
      auto title=game.value("title",std::string());if(title.empty())title=tr("backup.unknown_game");
      int available=596-ui->measure(game.value("system",std::string()),21,500);
      offsets.push_back(content);int h=ui->measure(title,23,700)>available?78:54;heights.push_back(h);content+=h;
    }
    if(count) {
      if(offsets[row]<backupScroll)backupScroll=offsets[row];
      if(offsets[row]+heights[row]>backupScroll+viewport)backupScroll=offsets[row]+heights[row]-viewport;
    }
    backupScroll=std::clamp(backupScroll,0.0,double(std::max(0,content-viewport)));
    SDL_Rect clip{16,128,688,viewport};SDL_RenderSetClipRect(ui->renderer,&clip);
    for(int i=0;i<count;++i) {
      int y=128+offsets[i]-int(backupScroll);if(y+heights[i]<128||y>=394)continue;
      auto title=backupGameList[i].value("title",std::string());
      ui->row(title.empty()?tr("backup.unknown_game"):title,y,row==i,672,backupGameList[i].value("system",std::string()),true,heights[i]);
    }
    SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar(content,viewport,backupScroll,false,128,394);
    if(count==0)ui->text(tr("backup.settings_only"),32,164,23,400,ui->secondary,656);
    if(count)ui->text(std::to_string(row+1)+" / "+std::to_string(count),32,400,20,400,ui->secondary,656);
    ui->hints({{"B",tr("action.back")},{"L1/R1",tr("library.pages")}});return;
  }
  if(screen==Screen::BackupConfirm) {
    ui->text(slotDate(backupSummary.at("created_utc")),32,132,24,600,ui->cream,656);
    ui->text(locale.tr("backup.details",{{"games",backupSummary.at("games")},{"files",backupSummary.at("files")}}),32,174,22,400,ui->secondary,656);
    ui->text(tr("backup.replace"),32,222,23,400,ui->cream,656);
    ui->text(tr("backup.other_games"),32,256,23,400,ui->secondary,656);
    ui->row(tr("action.cancel"),304,row==0);
    ui->row(tr("backup.restore"),358,row==1);
    ui->hints({{"A",tr("action.select")},{"B",tr("action.back")},{"X",tr("footer.info")}});return;
  }
  int count=int(backupList.size())+1,viewport=notice.empty()?280:244;
  if(row*54<backupScroll)backupScroll=row*54;
  if(row*54+52>backupScroll+viewport)backupScroll=row*54+52-viewport;
  backupScroll=std::clamp(backupScroll,0.0,double(std::max(0,count*54-viewport)));
  SDL_Rect clip{16,128,688,viewport};SDL_RenderSetClipRect(ui->renderer,&clip);
  for(int i=0;i<count;++i) {
    int y=128+i*54-int(backupScroll);if(y+52<128||y>=128+viewport)continue;
    if(i==0){ui->row(tr("backup.create"),y,row==i,672,"",true,52,"chevron-right","save");continue;}
    auto path=backupList[i-1].string();
    if(!backupMetadata.count(path)) {try{backupMetadata[path]=summary(backupList[i-1]);}catch(...){backupMetadata[path]=Json::object();}}
    const auto &item=backupMetadata[path];std::string label=tr("backup.invalid"),detail;
    if(!item.empty()) {label=slotDate(item.at("created_utc"));detail=std::to_string((item.at("bytes").get<uint64_t>()+1048575)/1048576)+" MB";}
    ui->row(label,y,row==i,672,detail,true,52,"chevron-right","folder");
  }
  SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar(count*54,viewport,backupScroll,false,128,128+viewport);
  if(!notice.empty())ui->text(notice,32,388,20,400,ui->secondary,656);
  if(backupList.empty())ui->text(tr("backup.empty"),32,204,23,400,ui->secondary,656);
  if(backupMetadata.size()>200)backupMetadata.clear();
  ui->hints({{"A",tr("action.select")},{"B",tr("action.back")},{"X",tr("footer.info")}});
}
}
