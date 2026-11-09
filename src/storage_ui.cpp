// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>
namespace rondo {
namespace {
const std::vector<std::string> writable{"saves","screenshots","bios","backups","logs","cache"};
const std::vector<std::string> categories{"games","saves","screenshots","bios","backups","logs","cache","system"};
std::string cardSlot(int col){return col?"TF2":"TF1";}
std::string savedSlot(const Options &o,const std::string &category){return o.storage->profile().at("placements").value(category,Json{{"slot","TF1"}}).at("slot");}
bool active(Screen s){return s==Screen::StorageBusy||s==Screen::StorageStop;}
}
void App::storageOpen() {
 if(!options.storage){notice=locale.tr("storage.unavailable");return;}
 storageNavigationDepth=navigation.size();storagePanel=false;storageError.clear();notice.clear();screen=Screen::Storage;storageScan();
}
void App::storageScan() {
 if(storageJob.valid())return;
 auto layout=options.storage->reopen();storageTask="scan";
 auto cancel=std::make_shared<std::atomic<bool>>(false);storageCancel=cancel;
 storageJob=std::async(std::launch::async,[layout,cancel]{Json result{{"usage",layout->usage([cancel]{return cancel->load();})}};try{result["pending"]=layout->pendingStatus();}catch(const std::exception &){result["pending"]={{"error","storage.journal"}};}return result;});
}
void App::storageInspect() {
 if(storageJob.valid()){storageReviewQueued=true;return;}
 storageReviewQueued=false;storageTask="review";storageError.clear();storageReview=Json::object();storageChoices=Json::object();
 if(core||statistics||backupJob.valid()||progressJob.valid()||screenshotWriter.busy()||bootPending()||wifiJob.valid()||btJob.valid()||clockJob.valid()||wifiQueued||btQueued||clockQueued){storageError="storage.close_game";return;}
 volumeTick(true);batteryLog.reset();ui->artworkCache.reset();storageQuiesced=true;
 auto layout=options.storage->reopen();auto category=storageCategory,target=storageTarget,source=storageCollect?storageFrom:std::string();
 storageJob=std::async(std::launch::async,[layout,category,target,source]{try{auto data=layout->review(category,target,source);if(data.at("intent")=="collect"&&data.at("files").empty())throw StorageError("storage.empty_source");if(data.at("free").get<uintmax_t>()<data.at("bytes").get<uintmax_t>()+1048576)throw StorageError("storage.space");return Json{{"review",data}};}
 catch(const StorageError &e){return Json{{"error",e.code}};}catch(...){return Json{{"error","storage.io"}};}});
}
void App::storageRun(const std::string &task) {
 if(storageJob.valid()||core||statistics||screenshotWriter.busy()||backupJob.valid()||progressJob.valid()||bootPending()||wifiJob.valid()||btJob.valid()||clockJob.valid()||wifiQueued||btQueued||clockQueued) {storageError="storage.close_game";return;}
 if(task=="finish"||task=="discard"){storageCategory=storagePending.value("category","saves");storageTarget=storagePending.value("target","TF2");}
 if(task=="format"&&!options.tf2Formatter) {
  const auto battery=readTelemetry();
  if(!battery.capacity||*battery.capacity<30){storageError="storage.format_power";return;}
 }
 volumeTick(true);
 if(soundTest){SDL_CloseAudioDevice(soundTest);soundTest=0;}
 // Join/drain every category writer before a worker can publish a new placement.
 batteryLog.reset();ui->artworkCache.reset();
 auto layout=options.storage->reopen();auto review=storageReview;auto category=storageCategory,target=storageTarget;auto choices=storageChoices;
 auto progress=std::make_shared<BackupProgress>();storageProgress=progress;
 auto cancel=std::make_shared<std::atomic<bool>>(false);storageCancel=cancel;
 storageTask=task;storageError.clear();storageCommitted=false;screen=Screen::StorageBusy;
 const auto formatTarget=storageFormatTarget;
 auto format=options.tf2Formatter;
 storageJob=std::async(std::launch::async,[layout,review,category,target,task,progress,cancel,choices,formatTarget,format] {
  Json result{{"ok",false},{"committed",false}};auto before=layout->profile();
  try {
   if(task=="finish")layout->finishPending();
   else if(task=="discard")layout->discardPending();
   else if(task=="format")layout->formatTf2(formatTarget,[&](const fs::path &tf1) {
    auto stage=[progress](int phase){progress->phase=phase;};
    if(format)format(formatTarget,tf1,stage);
    else formatPhysicalTf2(formatTarget,tf1,stage);
   });
   else {
    auto current=layout->review(category,target,review.at("intent")=="collect"?review.at("source").at("slot").get<std::string>():std::string());
    for(const auto *key:{"source","target","profile","files","target_files","conflicts","intent"})if(current.at(key)!=review.at(key))throw StorageError("storage.changed");
    StorageTransferHooks hooks;hooks.cancelled=[cancel]{return cancel->load();};
    hooks.progress=[progress](uintmax_t done,uintmax_t total){progress->done=done;progress->total=total;};
    hooks.checkpoint=[progress](const std::string &phase){progress->phase=phase=="copied"?1:(phase=="published"||phase=="retired")?2:3;};
    if(review.at("conflict")==true||review.at("intent")=="collect")layout->resolveCategory(review,choices,task=="move",true,hooks);
    else layout->copyCategory(category,target,true,hooks,task=="move");
   }
   result["ok"]=true;
  }catch(const StorageError &e){result["error"]=e.code;}catch(...){result["error"]="storage.io";}
  try{result["committed"]=layout->reopen()->profile()!=before;}catch(...){result["error"]="storage.profile";}
  return result;
 });
}
void App::storageTick() {
 if(storageQuiesced&&!storageJob.valid()&&screen!=Screen::StorageReview&&screen!=Screen::StorageConfirm&&screen!=Screen::StorageConflicts&&screen!=Screen::StorageCompare&&!active(screen)) {
  configureArtwork();configureDiagnostics();storageQuiesced=false;
 }
 if(!storageJob.valid()||storageJob.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
 Json result;try{result=storageJob.get();}catch(...){result={{"error","storage.io"}};}
 auto task=storageTask;
 if(task=="scan") {storageUsage=result.value("usage",Json::object());storagePending=result.value("pending",Json());}
 else if(task=="review"){storageError=result.value("error",std::string());storageReview=result.value("review",Json::object());}
 else {
  storageCommitted=result.value("committed",false);storageError=result.value("error",std::string());
  try {options.storage=options.storage->reopen();store=Store(options.paths());configureArtwork();configureDiagnostics();storageQuiesced=false;}
  catch(...){storageError="storage.profile";}
  row=0;storageReviewQueued=false;storageUsage=Json::object();
  if(task=="format"){
   // Once formatting reached the device, old TF2 entries cannot be trusted.
   if(storageProgress->phase>=1||storageError.empty()) {
    games.erase(std::remove_if(games.begin(),games.end(),[](const Game &game){return game.volume=="TF2";}),games.end());
   }
   screen=Screen::StorageFormatResult;
  }else {screen=Screen::StorageResult;storageScan();}
  return;
 }
 if(storageReviewQueued&&screen==Screen::StorageReview)storageInspect();
}
void App::storageAction(const std::string &action) {
 if(!active(screen)&&(action=="VOLUME_UP"||action=="VOLUME_DOWN")){volumeFeedback.begin(monotonicMs());changeVolume(previewVolume+(action=="VOLUME_UP"?5:-5));volumeFeedback.result(volumeFeedback.sequence,previewVolume,true,monotonicMs());return;}
 if(!active(screen)&&action=="SLEEP"){storageReviewQueued=false;if(storageJob.valid()){if(storageCancel)storageCancel->store(true);storageJob.wait();storageTick();}sleep();return;}
 if(active(screen)) {
  if(storageTask=="format")return;
  if(screen==Screen::StorageStop){if(action=="UP")row=0;if(action=="DOWN")row=1;if(action=="B"||(action=="A"&&row==0))screen=Screen::StorageBusy;
   else if(action=="A"&&row==1){if(storageCancel)storageCancel->store(true);screen=Screen::StorageBusy;}return;}
  if(action=="B"&&storageTask!="finish"&&storageTask!="discard"&&storageProgress->phase<2){screen=Screen::StorageStop;row=0;}return;
 }
 if(action=="SCREENSHOT"){takeScreenshot();return;}
 if(screen==Screen::StorageFormatResult){
  if(action=="A"||action=="B"){screen=Screen::StorageCards;storageError.clear();storageScan();}return;
 }
 if(screen==Screen::StorageFormat) {
  if(action=="B"||(action=="A"&&row==0)){screen=Screen::StorageCards;storageError.clear();return;}
  if(action=="UP")row=0;
  if(action=="DOWN"&&storageError.empty())row=1;
  if(action=="A"&&row==1&&storageError.empty()&&monotonicMs()>=storageFormatReady) {
   try {
    auto current=options.tf2FormatProbe?options.tf2FormatProbe():physicalTf2FormatTarget();
    if(!(current==storageFormatTarget))throw StorageError("storage.changed","TF2");
    storageRun("format");
   }catch(const StorageError &e){storageError=e.code;}
   catch(...){storageError="storage.io";}
  }
  return;
 }
 if(screen==Screen::StorageResult){if(action=="A"||action=="B"){screen=Screen::Storage;storagePanel=false;}return;}
 if(screen==Screen::StorageRecovery||screen==Screen::StorageDiscard) {
  if(action=="X")storageScan();
  if(action=="B"){screen=Screen::Storage;storagePanel=true;return;}
  if(action=="UP")row=0;if(action=="DOWN")row=1;
  if(action=="A"&&row==1&&storagePending.is_object()&&!storagePending.contains("error")) {
   if(screen==Screen::StorageDiscard)storageRun("discard");
   else if(storagePending.value("verified",false))storageRun("finish");
   else if(storagePending.value("committed",false))return;
   else {screen=Screen::StorageDiscard;row=0;}
  }else if(action=="A"&&row==0){screen=Screen::Storage;storagePanel=true;}return;
 }
 if(screen==Screen::StorageCompare){if(action=="B"||action=="X")screen=Screen::StorageConflicts;return;}
 if(screen==Screen::StorageConflicts) {
  const auto &conflicts=storageReview.at("conflicts");int count=int(conflicts.size());
  if(action=="B"){screen=Screen::StorageReview;return;}
  if(action=="UP")storageConflictFocus=std::max(0,storageConflictFocus-1);if(action=="DOWN")storageConflictFocus=std::min(count,storageConflictFocus+1);
  if(action=="L1")storageConflictFocus=std::max(0,storageConflictFocus-4);if(action=="R1")storageConflictFocus=std::min(count,storageConflictFocus+4);
  if(action=="LEFT")storageConflictColumn=0;if(action=="RIGHT")storageConflictColumn=1;
  if(action=="X"&&storageConflictFocus<count)screen=Screen::StorageCompare;
  if(action=="A") {
   if(storageConflictFocus<count){const auto &c=conflicts.at(storageConflictFocus);if(c.at("compatible")==true)storageChoices[c.at("key").get<std::string>()]=cardSlot(storageConflictColumn)==storageReview.at("source").at("slot")?"source":"target";}
   else {bool ready=true;for(const auto &c:conflicts)ready=ready&&c.at("compatible").get<bool>()&&storageChoices.contains(c.at("key").get<std::string>());if(ready){screen=Screen::StorageConfirm;row=0;}}
  }return;
 }
 if(screen==Screen::StorageConfirm) {
  if(action=="UP")row=0;if(action=="DOWN")row=1;
  if(action=="B"||(action=="A"&&row==0)){screen=storageReview.at("conflicts").empty()?Screen::StorageReview:Screen::StorageConflicts;return;}
  if(action=="A"&&row==1)storageRun(storageMethod?"move":"copy");return;
 }
 if(screen==Screen::StorageReview) {
  if(action=="B"){storageReviewQueued=false;screen=storageCollect?Screen::StorageDetails:Screen::Storage;return;}
  if(storageJob.valid()||storageReview.empty()||!storageError.empty())return;
  if(action=="UP")storageMethod=0;if(action=="DOWN"&&!storageReview.value("readonly_source",false))storageMethod=1;
  if(action=="A"){if(!storageReview.at("conflicts").empty()){screen=Screen::StorageConflicts;storageConflictFocus=0;storageConflictColumn=0;}else{screen=Screen::StorageConfirm;row=0;}}return;
 }
 if(screen==Screen::StorageUsage) {
  if(action=="B"){screen=Screen::StorageCards;return;}
  auto card=storageUsage.value(cardSlot(storageCard),Json::object());auto sizes=card.value("sizes",Json::object());std::vector<std::string> list;
  for(auto &entry:sizes.items())if(entry.key()!="free")list.push_back(entry.key());
  std::sort(list.begin(),list.end(),[&](auto a,auto b){return sizes.at(a)==sizes.at(b)?a<b:sizes.at(a)>sizes.at(b);});if(!list.empty())list.push_back("free");
  int max=std::max(0,int(list.size())-1);
  if(action=="UP")storageItem=std::max(0,storageItem-1);if(action=="DOWN")storageItem=std::min(max,storageItem+1);
  if(action=="L1")storageItem=std::max(0,storageItem-5);if(action=="R1")storageItem=std::min(max,storageItem+5);
  if(action=="X")storageScan();
  if(action=="Y"&&card.value("usage_state","")=="ready"&&!storageJob.valid()){auto next=store.data;next["storage_chart_colors"]=!next.value("storage_chart_colors",false);persist(next);}return;
 }
 if(screen==Screen::StorageCards) {
  if(action=="B"){screen=Screen::Storage;storagePanel=true;return;}
  if(action=="UP")storageCard=0;if(action=="DOWN")storageCard=1;
  if(action=="A"){screen=Screen::StorageUsage;storageItem=0;storageListScroll=0;}
  if(action=="Y"&&storageCard==1&&(options.physicalStorage||options.tf2FormatProbe)){
   row=0;storageFormatTarget={};storageError.clear();screen=Screen::StorageFormat;
   try {
    storageFormatTarget=options.tf2FormatProbe?options.tf2FormatProbe():physicalTf2FormatTarget();
    storageFormatReady=monotonicMs()+3000;
   }catch(const StorageError &e){storageError=e.code;}
   catch(...){storageError="storage.io";}
  }
  if(action=="X")storageScan();return;
 }
 if(screen==Screen::StorageDetails){
  if(action=="B"){screen=Screen::StorageFiles;return;}
  if(storageItem<1||storageItem>6)return;
  auto category=categories.at(storageItem);std::string chosen=savedSlot(options,category),other=chosen=="TF1"?"TF2":"TF1";
  auto c=storageUsage.value(other,Json::object());bool collect=c.value("present",Json::object()).value(category,false);
  if(action=="UP")storageDetailsAction=0;if(action=="DOWN"&&collect)storageDetailsAction=1;
  if(action=="A"){if(storageDetailsAction==0){storageFocus=storageItem-1;screen=Screen::Storage;storagePanel=false;}else if(collect){storageCategory=category;storageTarget=chosen;storageFrom=other;storageCollect=true;storageMethod=0;screen=Screen::StorageReview;storageInspect();}}return;
 }
 if(screen==Screen::StorageFiles) {
  if(action=="B"){screen=storageFilesOrigin;storagePanel=false;return;}
  if(action=="UP")storageItem=std::max(0,storageItem-1);if(action=="DOWN")storageItem=std::min(7,storageItem+1);
  if(action=="L1")storageItem=std::max(0,storageItem-5);if(action=="R1")storageItem=std::min(7,storageItem+5);
  if(action=="X")storageScan();if(action=="A"){screen=Screen::StorageDetails;storageDetailsAction=0;}return;
 }
 if(screen!=Screen::Storage)return;
 if(action=="B"){screen=Screen::Settings;settingsFocus=13;row=13;return;}
 if(action=="SELECT"){storagePanel=!storagePanel;return;}
 if(storagePanel) {
  int max=storagePending.is_null()?2:3;
  if(action=="LEFT")storageNav=std::max(0,storageNav-1);if(action=="RIGHT")storageNav=std::min(max,storageNav+1);
  if(action=="A") {
   if(storageNav==0){screen=Screen::Settings;settingsFocus=13;row=13;}
   else if(storageNav==1){screen=Screen::StorageFiles;storageFilesOrigin=Screen::Storage;storageItem=storageFocus+1;}
   else if(storageNav==2)screen=Screen::StorageCards;
   else {screen=Screen::StorageRecovery;row=0;}
  }return;
 }
 if(action=="UP")storageFocus=std::max(0,storageFocus-1);if(action=="DOWN")storageFocus=std::min(5,storageFocus+1);
 if(action=="LEFT")storageColumn=0;if(action=="RIGHT")storageColumn=1;
 if(action=="X"){screen=Screen::StorageFiles;storageFilesOrigin=Screen::Storage;storageItem=storageFocus+1;return;}
 if(action=="A") {
  if(!storagePending.is_null()){screen=Screen::StorageRecovery;row=0;return;}
  storageCategory=writable.at(storageFocus);storageTarget=cardSlot(storageColumn);storageCollect=false;
  if(savedSlot(options,storageCategory)==storageTarget){
   try{options.storage->path(storageCategory,true);}catch(const StorageError &e){storageError=e.code;storageReview=Json::object();screen=Screen::StorageReview;}
   return;
  }
  storageMethod=0;screen=Screen::StorageReview;storageInspect();
 }
}
void App::storageDraw(const std::string &zone) {
 auto tr=[this](const std::string &k){return locale.tr(k);};
 auto amount=[this](uintmax_t n){return storageBytes(n,locale.current);};
 auto cat=[&](const std::string &k){return tr("storage.category."+k);};
 auto center=[&](const std::string &text,int x,int y,int size,SDL_Color color){ui->text(text,x-ui->measure(text,size,500)/2,y,size,500,color);};
 auto border=[&](int x,int y,int w,int h,SDL_Color color){ui->rect(x,y,w,h,color,6);ui->rect(x+1,y+1,w-2,h-2,ui->bg,5);};
 auto block=[&](std::string text,int y,int size=22,SDL_Color color=SDL_Color{}) {
  if(!color.a)color=ui->cream;
  for(int line=0;line<3&&!text.empty();++line){auto part=text;while(!part.empty()&&ui->measure(part,size,500)>656)part=utf8Prefix(part,part.size()-1);
   if(part.size()<text.size()){auto space=part.rfind(' ');if(space!=std::string::npos)part.resize(space);}
   ui->text(part,32,y+line*(size+6),size,500,color,656);text.erase(0,part.size());while(!text.empty()&&text.front()==' ')text.erase(text.begin());}
 };
 auto card=[&](int n){if(storageJob.valid()&&storageTask=="scan")return Json::object();return storageUsage.value(cardSlot(n),Json::object());};
 auto sizeText=[&](int n,const std::string &category) {
  auto c=card(n);if(c.empty())return tr("storage.calculating");if(c.value("state","")=="missing")return tr("storage.no_card");
  if(c.value("usage_state","")!="ready")return tr(c.value("usage_state","")=="stale"?"storage.stale_usage":"storage.unknown_size");
  auto bytes=c.at("sizes").value(category,uintmax_t(0));return bytes?amount(bytes):tr("storage.empty");
 };
 std::string title="storage.title";
 if(screen==Screen::StorageFiles)title="storage.files";
 if(screen==Screen::StorageCards)title="storage.cards";
 if(screen==Screen::StorageUsage)title="storage.usage";
 if(screen==Screen::StorageDetails)title="storage.files";
 if(screen==Screen::StorageReview||screen==Screen::StorageConfirm)title=storageCollect?"storage.collect_title":"storage.change";
 if(screen==Screen::StorageConflicts)title="storage.versions";
 if(screen==Screen::StorageCompare)title="storage.compare";
 if(active(screen))title=storageTask=="discard"?"storage.discarding":storageTask=="finish"?"storage.finishing":storageTask=="move"?"storage.moving":"storage.copying";
 if(screen==Screen::StorageResult)title=storageError.empty()?"storage.complete":"storage.problem";
 if(screen==Screen::StorageRecovery||screen==Screen::StorageDiscard)title="storage.incomplete";
 if(screen==Screen::StorageFormat||(active(screen)&&storageTask=="format"))title="storage.format_title";
 if(screen==Screen::StorageFormatResult)title=storageError.empty()?"storage.format_done":"storage.problem";
 ui->frame(tr(title),"",zone);
 if(screen==Screen::StorageFormat) {
  ui->text("TF2",32,102,30,700,ui->cream);
  if(storageFormatTarget.bytes)ui->text(amount(storageFormatTarget.bytes),120,106,25,500,ui->secondary);
  if(!storageError.empty())block(tr(storageError),172,24);
  else {
   block(tr("storage.format_warning"),164,25);
   block(tr("storage.format_fat32"),223,21,ui->secondary);
  }
  const auto now=monotonicMs();
  const auto remaining=now>=storageFormatReady?0:(storageFormatReady-now+999)/1000;
  const bool ready=!remaining&&storageError.empty();
  auto label=tr("storage.format_erase");
  if(remaining&&storageError.empty())label+=" ("+std::to_string(remaining)+")";
  ui->row(tr("action.cancel"),304,row==0);
  ui->row(label,360,row==1,672,"",ready);
  std::vector<std::pair<std::string,std::string>> hints;
  if(!row||ready)hints.push_back({"A",tr("action.select")});
  hints.push_back({"B",tr("action.back")});ui->hints(hints);return;
 }
 if(active(screen)&&storageTask=="format"){
  static const char *stages[]{"storage.format_unmounting","storage.format_writing",
                              "storage.verifying","storage.format_preparing"};
  ui->text("TF2  ·  "+amount(storageFormatTarget.bytes),32,103,26,700,ui->cream);
  block(tr(stages[std::clamp(storageProgress->phase.load(),0,3)]),171,24);
  block(tr("storage.format_wait"),239,22,ui->secondary);return;
 }
 if(screen==Screen::StorageFormatResult){
  block(tr(storageError.empty()?"storage.format_ready":storageError),134,24);
  if(!storageError.empty()&&storageProgress->phase>=1)
   block(tr("storage.format_recovery"),215,21,ui->secondary);
  ui->row(tr("storage.cards"),352,true,672,"",true,52,"chevron-right");
  ui->hints({{"A",tr("action.open")},{"B",tr("action.back")}});return;
 }
 if(screen==Screen::Storage) {
  const std::vector<std::string> actions=storagePending.is_null()?std::vector<std::string>{"screen.settings","storage.files","storage.cards_short"}:std::vector<std::string>{"screen.settings","storage.files","storage.cards_short","storage.pending_short"};
  int x=24;const int widths[]{140,218,102,188};
  for(size_t i=0;i<actions.size();++i){int width=widths[i];border(x,80,width,38,storagePanel&&storageNav==int(i)?ui->accent:ui->separator);center(tr(actions[i]),x+width/2,87,20,ui->cream);x+=width+8;}
  ui->text(tr("storage.category"),32,134,20,500,ui->secondary);
  for(int n=0;n<2;++n){center(cardSlot(n),n?598:402,132,22,ui->cream);auto c=card(n);std::string note;
   if(c.empty())note=tr("storage.calculating");else if(c.value("state","")!="ready")note=tr("storage.no_card");else note=c.value("readonly",false)?tr("storage.read_only"):locale.tr("storage.free_inline",{{"size",amount(c.at("free"))}});
   center(note,n?598:402,158,18,ui->secondary);}
  for(int i=0;i<6;++i){int y=181+i*38;
   if(i==storageFocus&&!storagePanel){ui->rect(16,y,688,36,ui->focus,5);ui->rect(16,y+6,4,24,ui->accent,2);}
   ui->text(cat(writable[i]),32,y+5,22,500,ui->cream,270);
   auto current=savedSlot(options,writable[i]);
   for(int n=0;n<2;++n){int x=n?598:402;
    if(i==storageFocus&&n==storageColumn&&!storagePanel){ui->rect(x-97,y,194,36,ui->accent,5);ui->rect(x-96,y+1,192,34,ui->focus,4);}
    ui->circle(x-10,y+8,20,ui->cream,false);
    if(current==cardSlot(n))ui->circle(x-6,y+12,12,ui->cream,true);
   }
  }
  std::vector<std::pair<std::string,std::string>> hints;
  if(storagePanel)hints.push_back({"A",tr("action.open")});
  else if(savedSlot(options,writable[storageFocus])!=cardSlot(storageColumn))hints.push_back({"A",tr("storage.change_short")});
  else {auto c=card(storageColumn);if(!c.empty()&&(c.value("state","")!="ready"||c.value("readonly",false)))hints.push_back({"A",tr("storage.recheck")});}
  hints.push_back({"B",tr("action.back")});hints.push_back({"X",tr("storage.files_short")});hints.push_back({"SELECT",tr("nav.panel")});ui->hints(hints);return;
 }
 if(screen==Screen::StorageFiles) {
  ui->text(tr("storage.category"),32,90,21,500,ui->secondary);center("TF1",402,90,21,ui->cream);center("TF2",598,90,21,ui->cream);
  int start=storageItem<5?0:storageItem-4;
  for(int i=start;i<std::min(8,start+5);++i){int y=132+(i-start)*54;ui->row(cat(categories[i]),y,i==storageItem,300,"",true,52,"","");
   center(sizeText(0,categories[i]),402,y+16,18,ui->secondary);center(sizeText(1,categories[i]),598,y+16,18,ui->secondary);}
  ui->scrollbar(8*54,5*54,start*54,false,132,402);ui->hints({{"A",tr("action.open")},{"B",tr("action.back")},{"L1/R1",tr("library.pages")},{"X",tr("storage.refresh")}});return;
 }
 if(screen==Screen::StorageDetails) {
  auto key=categories.at(storageItem);ui->text(cat(key),32,96,26,700,ui->cream,656);
  if(key=="system")ui->text(tr("storage.fixed_tf1"),32,145,22,500,ui->secondary,656);
  else if(key!="games")ui->text(tr("storage.selected")+": "+savedSlot(options,key),32,145,22,500,ui->secondary,656);
  for(int n=0;n<2;++n)ui->row(cardSlot(n),200+n*54,false,672,sizeText(n,key));
  if(key!="games"&&key!="system") {
   ui->row(tr("storage.change"),310,storageDetailsAction==0,672,"",true,52,"chevron-right");
   std::string chosen=savedSlot(options,key),other=chosen=="TF1"?"TF2":"TF1";auto c=card(other=="TF1"?0:1);
   if(c.value("present",Json::object()).value(key,false))ui->row(locale.tr("storage.collect_from",{{"slot",other}}),364,storageDetailsAction==1,672,"",true,52,"chevron-right");
  }
  ui->hints(key!="games"&&key!="system"?std::vector<std::pair<std::string,std::string>>{{"A",tr(storageDetailsAction==1?"action.select":"storage.change_short")},{"B",tr("action.back")}}:std::vector<std::pair<std::string,std::string>>{{"B",tr("action.back")}});return;
 }
 if(screen==Screen::StorageCards) {
  for(int n=0;n<2;++n){auto c=card(n);std::string text=c.empty()?tr("storage.calculating"):c.value("state","")=="ready"?amount(c.at("total")):tr("storage.no_card");ui->row(cardSlot(n),128+n*64,storageCard==n,672,text,true,52,"chevron-right","folder-symlink");}
  ui->text(tr("storage.fixed_tf1"),32,304,22,500,ui->secondary,656);
  std::vector<std::pair<std::string,std::string>> hints{{"A",tr("action.open")},{"B",tr("action.back")},{"X",tr("storage.refresh")}};
  if(storageCard==1&&(options.physicalStorage||options.tf2FormatProbe))hints.push_back({"Y",tr("storage.format_short")});
  ui->hints(hints);return;
 }
 if(screen==Screen::StorageUsage) {
  auto c=card(storageCard);bool ready=c.value("usage_state","")=="ready"&&!storageJob.valid();
  ui->text(cardSlot(storageCard),32,88,25,700,ui->cream);std::string state;
  if(storageJob.valid())state=tr("storage.calculating");else if(c.value("state","")=="missing")state=tr("storage.no_card");else state=tr(c.value("usage_state","")=="stale"?"storage.stale_usage":"storage.unknown_size");
  if(!ready){ui->text(state,32,158,23,500,ui->secondary,656);ui->hints({{"B",tr("action.back")},{"X",tr("storage.refresh")}});return;}
  auto sizes=c.at("sizes");std::vector<std::string> list;for(const auto &e:sizes.items())if(e.key()!="free")list.push_back(e.key());
  std::sort(list.begin(),list.end(),[&](auto a,auto b){return sizes.at(a)==sizes.at(b)?a<b:sizes.at(a)>sizes.at(b);});list.push_back("free");storageItem=std::clamp(storageItem,0,int(list.size())-1);
  // Approved PICO-8 subset: https://lospec.com/palette-list/pico-8
  const std::map<std::string,SDL_Color> palette{{"games",{41,173,255,255}},{"saves",{0,228,54,255}},{"screenshots",{131,118,156,255}},{"bios",{255,236,39,255}},{"backups",{255,163,0,255}},{"logs",{255,0,77,255}},{"cache",{255,204,170,255}},{"system",{255,119,168,255}},{"other",{194,195,199,255}}};
  auto focused=list[storageItem];bool colors=store.data.value("storage_chart_colors",false);
  auto color=[&](const std::string &key){if(key=="free")return ui->bg;if(colors)return palette.at(key);return key==focused?ui->accent:ui->cream;};
  double total=c.at("total").get<double>(),at=0;int x=32;
  border(31,132,658,27,ui->separator);
  for(const auto &key:list){at+=sizes.at(key).get<double>()/total;int next=32+int(std::round(at*656));if(next>x){ui->rect(x,133,next-x,25,color(key));if(key=="free"&&focused=="free")border(x,132,next-x,27,ui->accent);}x=next;}
  auto bytes=sizes.at(focused).get<uintmax_t>();std::ostringstream share;share<<std::fixed<<std::setprecision(1)<<100.0*bytes/total<<"%";
  ui->text(cat(focused)+" · "+amount(bytes)+" · "+share.str(),32,172,21,500,ui->cream,656);
  int start=storageItem<5?0:storageItem-4;
  for(int i=start;i<std::min(int(list.size()),start+5);++i){int y=216+(i-start)*38;ui->row("",y,i==storageItem,672,"",true,36);auto value=amount(sizes.at(list[i]));ui->text(value,680-ui->measure(value,22,500),y+4,22,500,ui->cream);ui->rect(35,y+12,12,12,list[i]=="free"?ui->accent:color(list[i]),2);if(list[i]=="free")ui->rect(37,y+14,8,8,ui->bg,1);ui->text(cat(list[i]),56,y+4,22,500,ui->cream,445);}
  ui->scrollbar(list.size()*38,190,start*38,false,216,406);
  ui->hints({{"B",tr("action.back")},{"L1/R1",tr("library.pages")},{"X",tr("storage.refresh")},{"Y",tr("storage.colors")}});return;
 }
 if(screen==Screen::StorageConflicts) {
  const auto &list=storageReview.at("conflicts");int count=int(list.size());bool ready=true;
  for(const auto &c:list)ready=ready&&c.at("compatible").get<bool>()&&storageChoices.contains(c.at("key").get<std::string>());
  ui->text(tr("storage.game_or_file"),32,93,21,500,ui->secondary);center("TF1",402,93,21,ui->cream);center("TF2",598,93,21,ui->cream);
  int start=std::clamp(storageConflictFocus-3,0,std::max(0,count-4));
  for(int i=start;i<std::min(count,start+4);++i){const auto &c=list.at(i);int y=132+(i-start)*54;bool focused=i==storageConflictFocus;
   ui->row("",y,focused,688,"",c.at("compatible"),52);auto title=c.at("title").get<std::string>();if(title.empty())title=tr("backup.unknown_game");
   ui->text(ui->fit(title,270,22,focused?700:500),32,y+12,22,focused?700:500,c.at("compatible").get<bool>()?ui->cream:ui->secondary,270);
   auto key=c.at("key").get<std::string>();for(int col=0;col<2;++col){int x=col?598:402;auto choice=cardSlot(col)==storageReview.at("source").at("slot")?"source":"target";
    if(focused&&col==storageConflictColumn){ui->rect(x-97,y+1,194,50,ui->accent,5);ui->rect(x-96,y+2,192,48,ui->focus,4);}
    ui->circle(x-10,y+16,20,ui->cream,false);if(storageChoices.value(key,std::string())==choice)ui->circle(x-6,y+20,12,ui->cream,true);
   }
  }
  if(count>4)ui->scrollbar(count*54,216,start*54,false,132,348);
  ui->row(tr("storage.continue"),364,storageConflictFocus==count,672,"",ready,52,"chevron-right");
  std::vector<std::pair<std::string,std::string>> hints;
  if((storageConflictFocus<count&&list.at(storageConflictFocus).at("compatible")==true)||(storageConflictFocus==count&&ready))hints.push_back({"A",tr("action.select")});
  hints.push_back({"B",tr("action.back")});if(storageConflictFocus<count)hints.push_back({"X",tr("storage.compare")});if(count>4)hints.push_back({"L1/R1",tr("library.pages")});ui->hints(hints);return;
 }
 if(screen==Screen::StorageCompare) {
  const auto &c=storageReview.at("conflicts").at(storageConflictFocus);auto title=c.at("title").get<std::string>();if(title.empty())title=tr("backup.unknown_game");
  ui->text(ui->fit(title,656,25,700),32,90,25,700,ui->cream,656);
  bool sourceLeft=storageReview.at("source").at("slot")=="TF1";const auto &left=c.at(sourceLeft?"source":"target"),&right=c.at(sourceLeft?"target":"source");
  center("TF1",402,137,22,ui->cream);center("TF2",598,137,22,ui->cream);
  const std::vector<std::string> fields=storageCategory=="saves"?std::vector<std::string>{"files","bytes","states","latest_utc"}:std::vector<std::string>{"files","bytes"};
  for(size_t i=0;i<fields.size();++i){auto key=fields[i];int y=182+i*40;ui->text(tr("storage.meta."+key),32,y,21,500,ui->secondary,270);
   auto value=[&](const Json &meta){if(key=="bytes")return amount(meta.at(key));if(key=="latest_utc")return meta.at(key).get<long long>()>0?slotDate(meta.at(key)):tr("storage.no_date");return std::to_string(meta.at(key).get<size_t>());};
   center(value(left),402,y,key=="latest_utc"?17:21,ui->cream);center(value(right),598,y,key=="latest_utc"?17:21,ui->cream);
  }
  block(tr(c.at("compatible").get<bool>()?(storageCategory=="saves"?"storage.same_identity":"storage.choose_file_versions"):"storage.unverified_versions"),356,20,ui->secondary);
  ui->hints({{"B",tr("action.back")}});return;
 }
 if(active(screen)) {
  if(screen==Screen::StorageStop){ui->text(tr("storage.stop_question"),32,136,25,600,ui->cream,656);ui->text(tr("storage.original_kept"),32,202,22,500,ui->secondary,656);ui->row(tr("storage.keep_working"),298,row==0);ui->row(tr("storage.stop"),352,row==1);ui->hints({{"A",tr("action.select")},{"B",tr("action.back")}});return;}
  ui->text(cat(storageCategory)+" · "+storageTarget,32,126,25,600,ui->cream,656);
  int phase=storageProgress->phase.load();const char *stages[]{"storage.copying","storage.verifying","storage.switching","storage.finishing"};
  ui->text(tr(storageTask=="discard"?"storage.discarding":storageTask=="finish"?"storage.finishing":stages[std::clamp(phase,0,3)]),32,191,24,500,ui->cream,656);
  auto total=storageProgress->total.load(),done=storageProgress->done.load();ui->rect(32,249,656,8,ui->separator,4);if(total)ui->rect(32,249,int(656*std::min(1.0,double(done)/total)),8,ui->accent,4);
  ui->text(tr("storage.keep_cards"),32,299,22,500,ui->secondary,656);
  ui->hints(phase<2&&storageTask!="finish"&&storageTask!="discard"?std::vector<std::pair<std::string,std::string>>{{"B",tr("storage.stop")}}:std::vector<std::pair<std::string,std::string>>{});return;
 }
 if(screen==Screen::StorageReview||screen==Screen::StorageConfirm) {
  ui->text(cat(storageCategory),32,91,25,700,ui->cream,656);
  if(!storageError.empty()){block(tr(storageError),155,23);ui->text(tr("storage.selected")+": "+savedSlot(options,storageCategory),32,251,22,500,ui->cream,656);block(tr("storage.unchanged"),303,21,ui->secondary);ui->hints({{"B",tr("action.back")}});return;}
  if(storageReview.empty()||storageJob.valid()){ui->text(tr("storage.checking"),32,164,23,500,ui->secondary,656);ui->hints({{"B",tr("action.back")}});return;}
  std::string from=storageReview.at("source").at("slot"),to=storageReview.at("target").at("slot");
  ui->text(locale.tr("storage.route",{{"source",from},{"target",to}})+" · "+amount(storageReview.at("bytes")),32,133,23,500,ui->cream,656);
  if(screen==Screen::StorageReview){ui->text(tr("storage.free")+": "+amount(storageReview.at("free")),32,171,21,500,ui->secondary,360);
   if(!storageReview.at("conflicts").empty())ui->text(locale.tr("storage.differences",{{"count",storageReview.at("conflicts").size()}}),420,171,20,500,ui->secondary,268);
  }
  if(screen==Screen::StorageConfirm) {
   block(locale.tr(storageCollect?"storage.collect_after":"storage.after",{{"slot",to}}),180);
   bool conflicting=!storageReview.at("conflicts").empty();
   block(locale.tr(storageMethod?(conflicting?"storage.move_versions_note":"storage.move_note"):"storage.copy_note",{{"slot",from}}),222,19,ui->secondary);
   if(conflicting)block(tr("storage.replace_versions_note"),257,19,ui->secondary);
   ui->row(tr("action.cancel"),310,row==0);ui->row(locale.tr(storageMethod?"storage.move_to":"storage.copy_to",{{"slot",to}}),364,row==1);
  }else {
   ui->row(locale.tr("storage.copy_to",{{"slot",to}}),223,storageMethod==0,672,"",true,52,"","");
   ui->text(locale.tr("storage.copy_note",{{"slot",from}}),32,275,19,500,ui->secondary,656);
   bool move=!storageReview.value("readonly_source",false);
   ui->row(locale.tr("storage.move_to",{{"slot",to}}),311,storageMethod==1,672,"",move,52,"","");
   ui->text(locale.tr(move?"storage.move_note":"storage.read_only",{{"slot",from}}),32,365,19,500,ui->secondary,656);
  }
  ui->hints({{"A",tr("action.select")},{"B",tr("action.back")}});return;
 }
 if(screen==Screen::StorageRecovery||screen==Screen::StorageDiscard) {
  bool valid=storagePending.is_object()&&!storagePending.contains("error"),verified=valid&&storagePending.value("verified",false);
  bool changedActive=valid&&!verified&&storagePending.value("committed",false);
  ui->text(tr(changedActive?"storage.active_changed":valid?(verified?"storage.verified_copy":"storage.partial_copy"):"storage.journal"),32,134,23,600,ui->cream,656);
  block(tr(valid&&verified&&storagePending.value("move",false)&&screen!=Screen::StorageDiscard?"storage.finish_move_note":valid?"storage.original_kept":"storage.reconnect"),204,22,ui->secondary);
  ui->row(tr("action.back"),310,row==0);
  std::string action=screen==Screen::StorageDiscard?"storage.discard_confirm":verified?"storage.finish_copy":changedActive?"storage.compare_required":"storage.discard";
  ui->row(tr(action),364,row==1,672,"",valid&&!changedActive,52);
  std::vector<std::pair<std::string,std::string>> hints;if(row==0||(valid&&!changedActive))hints.push_back({"A",tr("action.select")});hints.push_back({"B",tr("action.back")});hints.push_back({"X",tr("storage.refresh")});ui->hints(hints);return;
 }
 if(screen==Screen::StorageResult) {
  block(tr(storageError.empty()?"storage.complete":storageError),132,23);
  ui->text(tr(storageCommitted?"storage.location_changed":"storage.unchanged"),32,216,22,500,ui->secondary,656);
  if(!storageError.empty()&&storageCommitted)block(tr("storage.cleanup_note"),272,21,ui->secondary);
  ui->row(tr("storage.title"),352,true,672,"",true,52,"chevron-right");ui->hints({{"A",tr("action.open")},{"B",tr("action.back")}});
 }
}
}
