// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
namespace rondo {
void App::progressCopy() {
 if(core||statistics)return;
 if(bootPending()||btJob.valid()||btQueued||wifiJob.valid()||wifiQueued||clockJob.valid()||clockQueued){progressError="binary.applying";return;}
 volumeTick(true);progressError.clear();progressCopying=true;progressStatus=std::make_shared<BackupProgress>();auto status=progressStatus;
 auto root=options.state;auto target=game;auto volumes=options.volumes;auto library=games;auto source=progressSelected;
 auto fingerprint=progressFingerprintValue,legacy=progressLegacyRom,core=progressLegacyCore;
 progressJob=std::async(std::launch::async,[root,target,volumes,library,source,fingerprint,legacy,core,status] {
  try{relocateProgress(root,source,target,volumes,library,fingerprint,legacy,core,status.get());return Json{{"ok",true}};}
  catch(const BackupError &error){return Json{{"ok",false},{"error",error.code}};}
  catch(...){return Json{{"ok",false},{"error","backup.io"}};}
 });screen=Screen::ProgressBusy;
}
void App::progressTick() {
 if(!progressJob.valid()||progressJob.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
 auto result=progressJob.get();progressError=result.value("ok",false)?"":result.value("error","backup.io");
 if(progressCopying) {
  try{store=Store(options.state);previewVolume=store.data.value("volume",80);}catch(...){progressError="backup.unfinished";}
  screen=Screen::ProgressDone;return;
 }
 progressSources=result.value("sources",Json::array());row=0;screen=Screen::ProgressSources;
 if(result.value("ok",false)) {
  progressFingerprintValue=result.at("fingerprint");progressLegacyRom=result.at("legacy");progressLegacyCore=result.at("core");
  if(progressSources.empty()&&progressAutomatic){prepareGame(game,false);return;}
 }
}
void App::progressAction(const std::string &action) {
 if(screen==Screen::ProgressDone){if(action=="A"){systemExitCode=66;stop=true;}return;}
 if(screen==Screen::ProgressConfirm) {
  if(action=="B"){screen=Screen::ProgressSources;row=progressFocus;return;}
  if(action=="UP")row=0;
  if(action=="DOWN")row=1;
  if(action=="A"){if(row==0){screen=Screen::ProgressSources;row=progressFocus;}else progressCopy();}return;
 }
 if(action=="B"){if(!navigation.empty())navigationBack();else screen=Screen::GameDetails;return;}
 if(action=="START"&&progressAutomatic){prepareGame(game,false);return;}
 if(action=="UP")row=std::max(0,row-1);
 if(action=="DOWN")row=std::min(std::max(0,int(progressSources.size())-1),row+1);
 if(action=="L1")row=std::max(0,row-3);
 if(action=="R1")row=std::min(std::max(0,int(progressSources.size())-1),row+3);
 if(action=="A"&&!progressSources.empty()){progressFocus=row;progressSelected=progressSources[row];screen=Screen::ProgressConfirm;row=0;}
}
void App::progressDraw(const std::string &zone) {
 auto tr=[this](const std::string &key){return locale.tr(key);};ui->frame(tr("progress.relocate"),game.title,zone);
 if(screen==Screen::ProgressBusy) {
  const char *phase[]={"progress.finding","backup.copying","backup.checking","backup.applying","backup.rolling_back"};
  ui->text(tr(phase[std::clamp(progressStatus->phase.load(),0,4)]),32,164,25,600,ui->cream,656);
  auto total=progressStatus->total.load(),done=progressStatus->done.load();
  ui->rect(32,234,656,8,ui->separator,4);if(total)ui->rect(32,234,int(656*std::min(1.0,double(done)/total)),8,ui->accent,4);
  ui->hints({});return;
 }
 if(screen==Screen::ProgressDone) {
  ui->text(tr(progressError.empty()?"progress.relocated":progressError),32,150,23,600,ui->cream,656);
  ui->text(tr("backup.restart_note"),32,214,23,400,ui->secondary,656);
  ui->row(tr("backup.restart"),312,true,672,"",true,52,"chevron-right");ui->hints({{"A",tr("backup.restart")}});return;
 }
 if(screen==Screen::ProgressConfirm) {
  auto title=progressSelected.value("title",std::string());ui->text(title.empty()?tr("backup.unknown_game"):title,32,132,24,600,ui->cream,656);
  auto utc=progressSelected.value("utc",0LL);ui->text(utc>0?slotDate(utc):tr("progress.verified"),32,174,21,400,ui->secondary,656);
  auto path=progressSelected.value("relative_path",std::string());if(path.empty())path=tr("progress.old_path_unknown");
  for(int line=0;line<2&&!path.empty();++line){auto part=path;while(!part.empty()&&ui->measure(part,20,400)>656)part=utf8Prefix(part,part.size()-1);ui->text(line==1?ui->fit(path,656,20,400):part,32,210+line*26,20,400,ui->secondary,656);path.erase(0,part.size());}
  ui->text(tr(progressError.empty()?"progress.source_kept":progressError),32,273,21,400,ui->cream,656);
  ui->row(tr("action.cancel"),312,row==0);ui->row(tr("progress.relocate"),366,row==1);
  ui->hints({{"A",tr("action.select")},{"B",tr("action.back")}});return;
 }
 if(progressSources.empty()) {
  ui->text(tr(progressError.empty()?"progress.no_match":progressError),32,164,23,600,ui->cream,656);
  ui->text(tr("progress.match_note"),32,224,21,400,ui->secondary,656);
 } else {
  int content=int(progressSources.size())*78,viewport=270;
  if(row*78<progressScroll)progressScroll=row*78;
  if(row*78+76>progressScroll+viewport)progressScroll=row*78+76-viewport;
  progressScroll=std::clamp(progressScroll,0.0,double(std::max(0,content-viewport)));
  SDL_Rect clip{16,128,688,viewport};SDL_RenderSetClipRect(ui->renderer,&clip);
  for(size_t i=0;i<progressSources.size();++i){int y=128+int(i)*78-int(progressScroll);if(y+76<128||y>=398)continue;
   auto title=progressSources[i].value("title",std::string());auto utc=progressSources[i].value("utc",0LL);
   ui->row(title.empty()?tr("backup.unknown_game"):title,y,row==int(i),672,"",true,76);
   ui->text(utc>0?slotDate(utc):tr("progress.verified"),32,y+48,18,400,ui->secondary,626);
  }
  SDL_RenderSetClipRect(ui->renderer,nullptr);ui->scrollbar(content,viewport,progressScroll,false,128,398);
 }
 std::vector<std::pair<std::string,std::string>> hints;if(!progressSources.empty())hints.push_back({"A",tr("action.select")});hints.push_back({"B",tr("action.back")});if(progressAutomatic)hints.push_back({"START",tr("progress.separate")});ui->hints(hints);
}
}
