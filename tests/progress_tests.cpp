// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include "state.hpp"
#include <cassert>
#include <iostream>
using namespace rondo;
static void wait(App &app) {
 for(int i=0;i<6000&&app.screen==Screen::ProgressBusy;++i){app.ticks(0);SDL_Delay(5);}
 assert(app.screen!=Screen::ProgressBusy);
}
int main(int argc,char **argv) {
 assert(argc==3);fs::path root=argv[2],card=root/"card";std::vector<Volume> volumes{{"TF1",card}};
 auto rom=card/"NES/Original/test.nes";atomicWrite(rom,{1,2,3,4});auto old=scan(volumes).at(0);
 auto core=root/"core.bin";atomicWrite(core,{4,5,6});auto coreHash=hashFile(core),romHash=hashFile(rom),fingerprint=progressFingerprint(old,romHash);
 Store settings(root/"state");auto config=settings.data;config["favorites"]={old.id};config["game_options"][old.id]={{"integer_scaling",true}};config["recent_games"]=recentAfterLaunch(Json::array(),old,volumes);config["resume_game"]=old.id;config["focus"]=old.id;settings.commit(config);
 auto source=settings.gameDir(old);atomicWrite(source/"sram.bin",{7,8});rememberProgress(settings.root,old,volumes,fingerprint);
 StateFile state;state.meta={{"core",coreHash},{"rom",romHash},{"utc",10},{"save_sequence",1},{"preview_width",0},{"preview_height",0}};state.payload={1,2};writeState(source/"slot-0.state",state);
 {GameStatistics stats(source/"statistics.json",old.id,0);assert(stats.flush(0));}
 auto originalSram=readBytes(source/"sram.bin"),originalState=readBytes(source/"slot-0.state");
 auto newPath=card/"NES/Renamed/new.nes";fs::create_directories(newPath.parent_path());fs::rename(rom,newPath);auto games=scan(volumes);auto target=games.at(0);assert(target.id!=old.id&&progressFingerprint(target)==fingerprint);
 auto candidates=findProgress(settings.root,target,games,fingerprint,romHash,coreHash);assert(candidates.size()==1&&candidates[0]["id"]==old.id);
 auto withLive=games;withLive.push_back(old);assert(findProgress(settings.root,target,withLive,fingerprint,romHash,coreHash).empty());
 // Duplicate missing identities require an explicit source choice.
 auto secondPath=card/"NES/Second/test.nes";atomicWrite(secondPath,{1,2,3,4});Game second;
 for(const auto &g:scan(volumes))if(g.path==secondPath)second=g;
 assert(!second.id.empty());rememberProgress(settings.root,second,volumes,fingerprint);atomicWrite(settings.gameDir(second)/"sram.bin",{9});fs::remove(secondPath);
 assert(findProgress(settings.root,target,games,fingerprint,romHash,coreHash).size()==2);
 auto forged=candidates[0];forged["files"][0]["path"]="games/"+second.id+"/sram.bin";bool refused=false;try{relocateProgress(settings.root,forged,target,volumes,games,fingerprint,romHash,coreHash);}catch(...){refused=true;}assert(refused&&!hasProgress(settings.root,target.id));
 // Source modification since review refuses the operation without changing settings.
 auto before=readBytes(settings.root/"config.json");atomicWrite(source/"sram.bin",{1});bool rejected=false;
 try{relocateProgress(settings.root,candidates[0],target,volumes,games,fingerprint,romHash,coreHash);}catch(...){rejected=true;}assert(rejected&&readBytes(settings.root/"config.json")==before&&!hasProgress(settings.root,target.id));atomicWrite(source/"sram.bin",originalSram);
 // Legacy copies can use a matching state, but never just a name or raw SRAM.
 auto record=readBytes(source/"identity.json");fs::remove(source/"identity.json");auto legacy=findProgress(settings.root,target,games,fingerprint,romHash,coreHash);assert(legacy.size()==2);
 auto wrong=findProgress(settings.root,target,games,fingerprint,romHash,std::string(64,'e'));assert(wrong.size()==1&&wrong[0]["id"]==second.id);
 atomicWrite(source/"identity.json",record);
 // UI uses an isolated state copy and the same transaction implementation.
 Options opts;opts.assets=argv[1];opts.state=root/"ui-state";opts.output=root/"renders";opts.volumes=volumes;opts.headless=true;opts.core=core;
 fs::copy(settings.root,opts.state,fs::copy_options::recursive);App app(opts);app.prepareGame(target);assert(app.screen==Screen::ProgressBusy);wait(app);assert(app.screen==Screen::ProgressSources&&app.progressSources.size()==2);
 for(const auto *language:{"en","ru"}){app.locale.select(opts.assets,language);app.capture(std::string("progress-sources-")+language);}
 int index=0;for(size_t i=0;i<app.progressSources.size();++i)if(app.progressSources[i]["id"]==old.id)index=int(i);app.row=index;
 app.action("A");assert(app.screen==Screen::ProgressConfirm&&app.row==0);
 for(const auto *language:{"en","ru"}){app.locale.select(opts.assets,language);app.capture(std::string("progress-confirm-")+language);}
 app.action("A");assert(app.screen==Screen::ProgressSources&&!hasProgress(opts.state,target.id));app.action("A");app.action("DOWN");app.action("A");assert(app.screen==Screen::ProgressBusy);
 app.action("MENU");app.action("SLEEP");assert(app.screen==Screen::ProgressBusy);wait(app);assert(app.screen==Screen::ProgressDone&&app.progressError.empty());app.capture("progress-done-ru");app.action("A");assert(app.stop&&app.systemExitCode==66);
 assert(readBytes(opts.state/"games"/old.id/"sram.bin")==originalSram&&readBytes(opts.state/"games"/target.id/"sram.bin")==originalSram);
 assert(GameStatistics::readExact(opts.state/"games"/target.id/"statistics.json",target.id).at("launches")==1);
 assert(app.store.data["favorites"]==Json::array({target.id})&&app.store.data["game_options"][target.id]["integer_scaling"]==true&&app.store.data["recent_games"][0]["id"]==target.id&&app.store.data["resume_game"]==target.id);
 App blocked(opts);blocked.game=target;blocked.screen=Screen::GameDetails;blocked.action("Y");wait(blocked);assert(blocked.screen==Screen::ProgressSources&&blocked.progressError=="progress.target_exists");
 for(const auto *lang:{"en","ru"}){blocked.locale.select(opts.assets,lang);blocked.capture(std::string("progress-protected-")+lang);}
 // A subsequent migration cannot replace target progress or use a changed ROM.
 rejected=false;try{relocateProgress(opts.state,candidates[0],target,volumes,games,fingerprint,romHash,coreHash);}catch(...){rejected=true;}assert(rejected);
 relocateProgress(settings.root,candidates[0],target,volumes,games,fingerprint,romHash,coreHash);
 assert(readBytes(settings.gameDir(target)/"slot-0.state")==originalState&&readBytes(source/"slot-0.state")==originalState);
 auto copy=createBackup(settings.root,{"0.12.0",{coreHash}});auto manifest=backupManifest(copy);bool saved=false;for(const auto &file:manifest["files"])if(file["path"]=="games/"+target.id+"/identity.json")saved=true;assert(saved);
 // Same title / different bytes never matches a passport.
 atomicWrite(newPath,{5,6});auto changed=progressFingerprint(target);Store empty(root/"empty");fs::create_directories(empty.root/"games");fs::copy(source,empty.root/"games"/old.id,fs::copy_options::recursive);auto sameTitle=target;sameTitle.title=old.title;assert(findProgress(empty.root,sameTitle,games,changed,hashFile(newPath),coreHash).empty());
 Store raw(root/"raw");atomicWrite(raw.root/"games"/old.id/"sram.bin",originalSram);assert(findProgress(raw.root,target,games,fingerprint,romHash,coreHash).empty());
 std::cout<<"PASS: renamed ROM identity, ambiguous sources, legacy state/core evidence, changed-source refusal, protected target, preserved originals, transactional settings/SRAM/stats/slots, backup passport and EN/RU UI.\n";
}
