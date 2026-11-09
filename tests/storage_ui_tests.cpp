// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include "state.hpp"
#include <cassert>
#include <iostream>
using namespace rondo;
static void wait(App &app){for(int i=0;i<10000&&app.storageJob.valid();++i){app.ticks(0);SDL_Delay(2);}assert(!app.storageJob.valid());}
static void formatUI(Options opts,const fs::path &root) {
 fs::create_directories(root/"one/RondoState");
 fs::create_directories(root/"two");
 std::vector<StorageCard> cards{{"TF1","format-one",root/"one",false},{"TF2","format-two",root/"two",false}};
 opts.state=root/"one/RondoState";opts.output=root/"Previews";
 opts.storage=std::make_shared<StorageLayout>(opts.state,[&]{return cards;});
 opts.tf2FormatProbe=[&] {
  if(cards.size()<2)throw StorageError("storage.missing","TF2");
  if(cards[1].readOnly)throw StorageError("storage.readonly","TF2");
  return Tf2FormatTarget{cards[1].id,cards[1].root,128000000000,2};
 };
 int calls=0;bool unmountFails=true;
 opts.tf2Formatter=[&](const Tf2FormatTarget &target,const fs::path &tf1,const std::function<void(int)> &phase) {
  assert(target.id==cards[1].id&&target.root==root/"two"&&tf1==root/"one");
  ++calls;phase(0);
  if(unmountFails)throw StorageError("storage.format_unmount");
  phase(1);phase(2);phase(3);
  cards[1].id="formatted-two";
 };
 atomicWrite(opts.state/"games/preserved.bin",{1});
 App app(opts);
 app.action("MENU");
 assert(app.screen==Screen::Settings);
 for(int i=0;i<10;++i)app.action("DOWN");
 app.action("A");wait(app);assert(app.screen==Screen::Storage);
 app.action("SELECT");app.action("RIGHT");app.action("RIGHT");app.action("A");
 assert(app.screen==Screen::StorageCards);
 app.action("Y");assert(app.screen==Screen::StorageCards&&calls==0);
 app.action("DOWN");app.action("Y");
 assert(app.screen==Screen::StorageFormat&&app.row==0);
 app.action("A");assert(app.screen==Screen::StorageCards&&calls==0);
 app.action("Y");app.action("DOWN");app.action("A");
 assert(app.screen==Screen::StorageFormat&&calls==0);
 app.capture("en-format-countdown");
 app.locale.select(opts.assets,"ru");app.capture("ru-format-countdown");
 SDL_Delay(3050);app.ticks(0);
 assert(app.screen==Screen::StorageFormat&&calls==0);
 app.capture("ru-format-ready");
 cards[1].id="replacement";app.action("A");
 assert(app.storageError=="storage.changed"&&calls==0);
 app.capture("ru-format-replacement");
 app.action("B");cards[1].readOnly=true;app.action("Y");
 assert(app.storageError=="storage.readonly");app.action("B");cards[1].readOnly=false;
 const auto original=cards[1];cards.pop_back();app.action("Y");
 assert(app.storageError=="storage.missing");app.action("B");cards.push_back(original);
 app.action("Y");app.action("DOWN");app.action("B");assert(calls==0);
 app.action("Y");app.action("DOWN");SDL_Delay(3050);app.action("A");wait(app);
 assert(app.screen==Screen::StorageFormatResult&&app.storageError=="storage.format_unmount"&&calls==1);
 app.capture("ru-format-error");
 app.action("B");wait(app);unmountFails=false;
 app.action("Y");app.action("DOWN");SDL_Delay(3050);app.action("A");wait(app);
 assert(app.screen==Screen::StorageFormatResult&&app.storageError.empty()&&calls==2);
 assert(cards[1].id=="formatted-two"&&readBytes(opts.state/"games/preserved.bin")==std::vector<uint8_t>{1});
 app.capture("ru-format-success");
 std::cout<<"PASS: TF2 format via Settings, TF1 excluded, countdown, fresh confirmation, Back, replacement, missing/read-only cards, unmount failure, retry and success.\n";
}
static void conflictUI(Options opts,const fs::path &root) {
 fs::create_directories(root/"one/RondoState");fs::create_directories(root/"two/RondoState");std::vector<StorageCard> slots{{"TF1","versions-one",root/"one",false},{"TF2","versions-two",root/"two",false}};
 opts.state=root/"one/RondoState";opts.output=root/"Previews";opts.storage=std::make_shared<StorageLayout>(opts.state,[&]{return slots;});
 const std::string id(64,'a'),unique(64,'b'),other(64,'c');
 auto seed=[&](const fs::path &parent,const std::string &key,uint8_t value,const std::string &fingerprint=std::string(64,'f')) {
  auto identity="NES\n"+fingerprint;auto folder=parent/key;Json body={{"schema",1},{"game_id",key},{"title","Battle City"},{"system","NES"},{"fingerprint",hashBytes({identity.begin(),identity.end()})}};auto text=body.dump();auto wrapped=Json{{"payload",body},{"sha256",hashBytes({text.begin(),text.end()})}};text=wrapped.dump();atomicWrite(folder/"identity.json",{text.begin(),text.end()});atomicWrite(folder/"sram.bin",{value});
  StateFile state;state.payload={value};state.meta={{"rom",fingerprint},{"core",std::string(64,'d')},{"utc",value==1?100:200},{"save_sequence",1},{"preview_width",0},{"preview_height",0}};writeState(folder/"slot-0.state",state);
 };
 auto source=root/"one/RondoState/games",target=root/"two/RondoState/games";seed(source,id,1);seed(target,id,2);seed(source,unique,3);seed(target,other,4);
 Store settings(opts.paths());settings.commit(settings.data);App app(opts);app.storageOpen();wait(app);app.storageColumn=1;app.action("A");wait(app);assert(app.storageReview.at("conflicts").size()==1);
 app.action("A");assert(app.screen==Screen::StorageConflicts&&app.storageChoices.empty());app.action("DOWN");app.action("A");assert(app.screen==Screen::StorageConflicts);app.action("UP");app.action("X");assert(app.screen==Screen::StorageCompare);app.action("B");
 auto themes=readJson(opts.assets/"themes.json").at("themes");for(const auto *lang:{"en","ru"}){app.locale.select(opts.assets,lang);for(const auto &t:themes){auto theme=t.at("id").get<std::string>();app.ui->setTheme(theme);app.screen=Screen::StorageConflicts;app.capture(std::string(lang)+"-"+theme+"-versions");app.action("X");app.capture(std::string(lang)+"-"+theme+"-compare");app.action("B");}}
 app.locale.select(opts.assets,"ru");app.ui->setTheme("rondo");app.action("RIGHT");app.action("A");assert(app.storageChoices.at(id)=="target");app.action("B");app.action("DOWN");assert(app.storageMethod==1);app.action("A");assert(app.storageChoices.at(id)=="target"&&app.storageMethod==1);
 app.action("DOWN");app.action("A");assert(app.screen==Screen::StorageConfirm);app.capture("ru-confirm-versions-move");app.action("B");assert(app.screen==Screen::StorageConflicts&&app.storageChoices.at(id)=="target");app.action("A");app.action("DOWN");app.action("A");wait(app);
 assert(app.storageError.empty()&&app.options.storage->path("saves")==target);assert(fs::exists(source/id/"slot-0.state")&&!fs::exists(source/unique/"sram.bin")&&fs::exists(target/unique)&&fs::exists(target/other));assert(readBytes(target/id/"sram.bin")==std::vector<uint8_t>{2});
 auto profile=app.options.storage->profile();app.action("B");app.action("X");app.action("A");assert(app.screen==Screen::StorageDetails);app.action("DOWN");assert(app.storageDetailsAction==1);app.capture("ru-collect-entry");app.action("A");wait(app);assert(app.storageCollect&&app.storageReview.at("intent")=="collect");app.action("A");app.action("A");assert(app.storageChoices.at(id)=="source");app.action("DOWN");app.action("A");app.capture("ru-collect-confirm");app.action("DOWN");app.action("A");wait(app);
 assert(app.storageError.empty()&&app.options.storage->profile()==profile&&readBytes(target/id/"sram.bin")==std::vector<uint8_t>{1}&&fs::exists(source/id));
 app.action("B");seed(source,id,1,std::string(64,'e'));app.action("X");app.action("A");app.action("DOWN");app.action("A");wait(app);app.action("A");assert(app.screen==Screen::StorageConflicts);app.action("A");assert(app.storageChoices.empty());app.action("X");app.capture("ru-unverified-version");app.action("B");app.action("DOWN");app.action("A");assert(app.screen==Screen::StorageConflicts&&app.options.storage->profile()==profile);
 // More conflicts than fit on one screen: page through them without losing choices.
 auto sample=app.storageReview.at("conflicts")[0];sample["compatible"]=true;app.storageReview["conflicts"]=Json::array();app.storageChoices=Json::object();for(int i=0;i<9;++i){auto item=sample;item["key"]=std::to_string(i);item["title"]="Battle City "+std::to_string(i);app.storageReview["conflicts"].push_back(item);}app.storageConflictFocus=0;
 for(int i=0;i<9;++i){app.action("A");app.action("DOWN");}assert(app.storageChoices.size()==9&&app.storageConflictFocus==9);app.capture("ru-versions-bottom");app.action("L1");assert(app.storageConflictFocus==5);app.capture("ru-versions-middle");app.action("R1");app.action("A");assert(app.screen==Screen::StorageConfirm);app.action("B");assert(app.storageConflictFocus==9&&app.storageChoices.size()==9);
 std::cout<<"PASS: native conflict choices, comparison EN/RU/seven themes, no default, Continue guard, Copy/Move/Back preserve selection, unchosen source bundle retained, collection without placement change, incompatible saves blocked.\n";
}
int main(int argc,char **argv){
 assert(storageBytes(1000000000,"ru")=="1,0 ГБ");assert(storageBytes(1000000000,"en")=="1.0 GB");assert(storageBytes(0,"ru")=="0 Б");
 assert(argc==3);fs::path root=fs::absolute(argv[2]);fs::create_directories(root/"one/RondoState");fs::create_directories(root/"two");
 std::vector<StorageCard> cards{{"TF1","one",root/"one",false},{"TF2","two",root/"two",false}};
 auto probe=[&]{return cards;};Options opts;opts.assets=fs::absolute(argv[1]);opts.state=root/"one/RondoState";opts.headless=true;opts.output=root/"previews";opts.storage=std::make_shared<StorageLayout>(opts.state,probe);
 Store initial(opts.paths());initial.commit(initial.data);atomicWrite(opts.state/"games/game/sram.bin",{1,2,3});atomicWrite(opts.state/"Screenshots/Interface/test.png",{1,2,3});
 App app(opts);app.screen=Screen::Settings;app.settingsFocus=8;app.action("DOWN");assert(app.settingsFocus==13);app.action("A");assert(app.screen==Screen::Storage);wait(app);
 auto before=app.options.storage->profile();app.storageColumn=0;app.action("RIGHT");assert(app.options.storage->profile()==before);
 auto themes=readJson(opts.assets/"themes.json").at("themes");
 for(const auto *locale:{"en","ru"}) {
  app.locale.select(opts.assets,locale);
  auto catalog=readJson(opts.assets/(std::string("Locales/")+locale+".json"));
  for(const auto &entry:catalog.items())if(entry.key().find("storage.")==0){auto text=entry.value().get<std::string>();
   for(size_t i=0;i<text.size();){uint32_t code=uint8_t(text[i++]);int extra=0;if(code>=0xf0){extra=3;code&=7;}else if(code>=0xe0){extra=2;code&=15;}else if(code>=0xc0){extra=1;code&=31;}while(extra--){assert(i<text.size());code=(code<<6)|(uint8_t(text[i++])&63);}if(code>=32){if(!TTF_GlyphIsProvided32(app.ui->font(23,500),code)){std::cerr<<entry.key()<<" missing glyph "<<code<<"\n";assert(false);}}}
  }
  for(const auto &theme:themes) {
   auto id=theme.at("id").get<std::string>();app.ui->setTheme(id);app.screen=Screen::Storage;app.storagePanel=false;app.capture(std::string(locale)+"-"+id+"-locations");
   app.screen=Screen::StorageFiles;app.storageItem=7;app.capture(std::string(locale)+"-"+id+"-files-bottom");
   if(app.storageUsage.at("TF1").value("usage_state","")=="ready"){
    app.screen=Screen::StorageUsage;app.storageCard=0;app.storageItem=0;
    app.store.data["storage_chart_colors"]=false;app.capture(std::string(locale)+"-"+id+"-usage-mono");
    app.store.data["storage_chart_colors"]=true;app.capture(std::string(locale)+"-"+id+"-usage-colors");app.store.data["storage_chart_colors"]=false;
   }
  }
 }
 app.ui->setTheme("rondo");app.locale.select(opts.assets,"ru");app.screen=Screen::Storage;app.storageFocus=5;app.storageColumn=1;
 app.action("X");assert(app.screen==Screen::StorageFiles&&app.storageItem==6);app.action("A");assert(app.screen==Screen::StorageDetails);app.capture("ru-category-details");app.action("B");assert(app.screen==Screen::StorageFiles&&app.storageItem==6);app.action("B");assert(app.screen==Screen::Storage&&app.storageFocus==5&&app.storageColumn==1);
 app.action("SELECT");app.storageNav=2;app.action("A");assert(app.screen==Screen::StorageCards);app.capture("ru-cards");app.action("A");assert(app.screen==Screen::StorageUsage);
 if(app.storageUsage.at("TF1").value("usage_state","")=="ready") {
  int focus=app.storageItem;auto current=app.store.data.value("storage_chart_colors",false);app.action("Y");assert(app.store.data.value("storage_chart_colors",false)!=current&&app.storageItem==focus);
  for(int i=0;i<9;++i)app.action("DOWN");app.capture("ru-usage-bottom");app.action("Y");app.capture("ru-usage-mono-bottom");
 }
 app.action("B");app.action("B");app.storagePanel=false;app.storageFocus=0;app.storageColumn=1;app.action("A");wait(app);assert(app.screen==Screen::StorageReview&&!app.storageReview.empty());app.capture("ru-review-copy");
 app.action("A");assert(app.screen==Screen::StorageConfirm);app.capture("ru-confirm-copy");app.action("B");assert(app.options.storage->profile()==before);
 app.action("A");app.action("DOWN");app.action("A");wait(app);assert(app.screen==Screen::StorageResult&&app.storageError.empty());app.capture("ru-result-copy");
 assert(app.options.storage->path("saves")==root/"two/RondoState/games");assert(fs::exists(root/"one/RondoState/games/game/sram.bin"));
 app.action("B");assert(app.screen==Screen::Storage);app.storageFocus=1;app.storageColumn=1;app.action("A");wait(app);app.action("DOWN");assert(app.storageMethod==1);app.capture("ru-review-move");app.action("A");app.action("DOWN");app.action("A");wait(app);
 assert(app.storageError.empty()&&app.options.storage->path("screenshots")==root/"two/RondoState/Screenshots");assert(!fs::exists(root/"one/RondoState/Screenshots/Interface/test.png"));
 app.action("B");app.storageFocus=0;app.storageColumn=0;app.action("A");wait(app);assert(app.storageReview.at("conflict")==true);app.capture("ru-existing-data");app.action("A");assert(app.screen==Screen::StorageConfirm);app.action("B");app.action("B");
 // Missing/readonly target preserves focus and selection; neither starts a transfer.
 auto profile=app.options.storage->profile();auto saved=cards.back();cards.pop_back();app.storageFocus=2;app.storageColumn=1;app.action("A");wait(app);assert(app.storageError=="storage.missing");app.capture("ru-missing-target");app.action("B");cards.push_back(saved);cards.back().readOnly=true;
 app.action("A");wait(app);assert(app.storageError=="storage.readonly");app.action("B");cards.back().readOnly=false;assert(app.options.storage->profile()==profile);
 // Replacement after review is rejected before destination files or profile change.
 app.action("A");wait(app);app.action("A");app.action("DOWN");cards.back().id="replacement";app.action("A");wait(app);assert(app.screen==Screen::StorageResult&&!app.storageError.empty());cards.back()=saved;
 assert(app.options.storage->profile()==profile&&!fs::exists(root/"two/RondoState/BIOS"));
 // Interrupted unpublished data can be discarded explicitly, without source loss.
 auto l=app.options.storage->reopen();StorageTransferHooks hooks;hooks.checkpoint=[](const std::string &p){if(p=="copied")throw StorageError("injected");};
 atomicWrite(l->path("bios")/"test.bin",{9});try{l->copyCategory("bios","TF2",true,hooks);assert(false);}catch(const StorageError &){}
 assert(l->pendingStatus().at("verified")==false);app.action("B");app.storageScan();wait(app);app.storagePanel=true;app.storageNav=3;app.action("A");assert(app.screen==Screen::StorageRecovery);app.action("DOWN");app.action("A");assert(app.screen==Screen::StorageDiscard);app.capture("ru-discard-temporary");app.action("DOWN");app.action("A");wait(app);assert(app.storageError.empty()&&app.options.storage->pending().is_null());assert(fs::exists(root/"one/RondoState/BIOS/test.bin"));
 // A verified published copy can be finished instead of copied again.
 app.action("B");l=app.options.storage->reopen();hooks.checkpoint=[](const std::string &p){if(p=="published")throw StorageError("injected");};
 try{l->copyCategory("bios","TF2",true,hooks);assert(false);}catch(const StorageError &){}
 app.storageScan();wait(app);assert(app.storagePending.at("verified")==true);app.storagePanel=true;app.storageNav=3;app.action("A");app.capture("ru-finish-verified");app.action("DOWN");app.action("A");wait(app);assert(app.storageError.empty()&&app.options.storage->path("bios")==root/"two/RondoState/BIOS");
 // Opt-in live logs are drained before their review, not after the snapshot.
 app.action("B");auto config=app.store.data;config["debug_enabled"]=true;config["debug_level"]="detailed";app.persist(config);app.configureDiagnostics();assert(app.batteryLog);
 app.storageFocus=4;app.storageColumn=1;app.storagePanel=false;app.action("A");wait(app);assert(app.screen==Screen::StorageReview&&app.storageQuiesced&&!app.batteryLog&&!app.ui->artworkCache);
 app.action("A");app.action("DOWN");app.action("A");wait(app);assert(app.storageError.empty()&&app.options.storage->path("logs")==root/"two/RondoState/Logs"&&app.batteryLog);
 app.batteryLog.reset();config=app.store.data;config["debug_enabled"]=false;app.persist(config);
 // Ready read-only inventory may change TF1 UI colors, not TF2 files.
 app.action("B");cards.back().readOnly=true;app.storageScan();wait(app);app.storageCard=1;app.screen=Screen::StorageUsage;app.capture("ru-readonly-usage");
 auto colors=app.store.data.value("storage_chart_colors",false);if(app.storageUsage.at("TF2").value("usage_state","")=="ready"){app.action("Y");assert(app.store.data.value("storage_chart_colors",false)!=colors);}
 // A changed active destination must never be offered as a disposable staging copy.
 cards.back().readOnly=false;app.ui->artworkCache.reset();l=app.options.storage->reopen();atomicWrite(l->path("cache")/"state.bin",{4});
 hooks.checkpoint=[&](const std::string &p){if(p=="committed"){atomicWrite(root/"two/RondoState/Cache/state.bin",{5});throw StorageError("injected");}};
 try{l->copyCategory("cache","TF2",true,hooks,true);assert(false);}catch(const StorageError &){}
 app.options.storage=l->reopen();app.screen=Screen::Storage;app.storageScan();wait(app);assert(app.storagePending.at("committed")==true&&app.storagePending.at("verified")==false);
 app.storagePanel=true;app.storageNav=3;app.action("A");assert(app.screen==Screen::StorageRecovery);app.action("DOWN");app.action("A");assert(app.screen==Screen::StorageRecovery);app.capture("ru-changed-active-copy");
 assert(readBytes(root/"one/RondoState/Cache/state.bin")==std::vector<uint8_t>{4}&&readBytes(root/"two/RondoState/Cache/state.bin")==std::vector<uint8_t>{5});
 conflictUI(opts,root/"versions-ui");
 opts.volumes.clear();formatUI(opts,root/"format-ui");
 std::cout<<"PASS: native storage matrix EN/RU/seven themes, focus/selection, exact Back, inventory, usage colors, copy/move, occupied/missing/readonly/replaced cards, interrupted-copy disposal and source preservation. No physical media accessed.\n";
}
