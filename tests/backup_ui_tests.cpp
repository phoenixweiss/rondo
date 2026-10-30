// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include "state.hpp"
#include <cassert>
#include <iostream>
using namespace rondo;
static void selectedRowVisible(App &app,int y) {
 app.draw();auto *surface=app.ui->surface;assert(surface);
 auto *pixels=reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(surface->pixels)+(y+26)*surface->pitch);
 Uint8 r,g,b,a;SDL_GetRGBA(pixels[18],surface->format,&r,&g,&b,&a);
 assert(r==app.ui->accent.r&&g==app.ui->accent.g&&b==app.ui->accent.b);
}
static void wait(App &app) {
 for(int i=0;i<6000&&app.screen==Screen::BackupBusy;++i){app.ticks(0);SDL_Delay(5);}
 assert(app.screen!=Screen::BackupBusy);
}
int main(int argc,char **argv) {
 assert(argc==3);Options options;options.assets=argv[1];options.state=fs::path(argv[2])/"state";options.output=fs::path(argv[2])/"renders";options.headless=true;
 options.core=fs::path(argv[2])/"core.bin";atomicWrite(options.core,{1,2,3});
 Store config(options.state);config.commit(config.data);
 const std::string game(64,'a');auto path=options.state/"games"/game;
 StateFile state;state.meta={{"core",hashFile(options.core)},{"rom",std::string(64,'b')},{"utc",10},{"save_sequence",1},{"preview_width",0},{"preview_height",0}};state.payload={1,2,3};writeState(path/"slot-0.state",state);
 App app(options);app.games.push_back({game,"TF1","NES","Battle City",{}});app.screen=Screen::Settings;app.settingsFocus=9;app.row=9;
 app.restoreInputFence=true;app.action("A");assert(app.screen==Screen::Settings);app.sampleInput();assert(!app.restoreInputFence);
 for(const auto *lang:{"en","ru"}){app.locale.select(options.assets,lang);app.capture(std::string("backup-entry-")+lang);}
 app.action("A");assert(app.screen==Screen::Backups);selectedRowVisible(app,128);for(const auto *lang:{"en","ru"}){app.locale.select(options.assets,lang);app.capture(std::string("backup-empty-")+lang);}app.action("X");assert(app.infoOpen);app.capture("backup-info-ru");app.action("B");
 app.btJob=std::async(std::launch::async,[]{return BluetoothState{};});
 app.backupStart(false);assert(app.screen==Screen::Backups&&!app.backupJob.valid());
 app.btJob.wait();app.btTick();
 app.action("A");assert(app.screen==Screen::BackupBusy);
 auto before=app.store.data;app.action("VOLUME_UP");app.action("MENU");app.action("SLEEP");assert(app.store.data==before&&app.screen==Screen::BackupBusy);
 app.capture("backup-progress-ru");wait(app);assert(app.screen==Screen::Backups&&app.backupList.size()==1);
 for(const auto *lang:{"en","ru"}){app.locale.select(options.assets,lang);app.capture(std::string("backup-list-")+lang);}
 app.action("DOWN");app.action("A");assert(app.screen==Screen::BackupDetails&&app.row==0);
 for(const auto *lang:{"en","ru"}){app.locale.select(options.assets,lang);app.capture(std::string("backup-details-")+lang);}
 app.action("A");assert(app.screen==Screen::BackupGames&&app.backupGameList[0]["title"]=="Battle City");for(int i=0;i<12;++i)app.backupGameList.push_back({{"id",std::to_string(i)},{"title",i==0?"An unusually long game title with enough words to need two lines in the list":"Alien 3"},{"system",i%2?"MD":"SNES"}});
 for(const auto *lang:{"en","ru"}){app.locale.select(options.assets,lang);app.capture(std::string("backup-games-")+lang);}
 for(int i=0;i<20;++i)app.action("DOWN");app.capture("backup-games-bottom-ru");assert(app.backupScroll>0);
 app.action("B");assert(app.screen==Screen::BackupDetails&&app.row==0);
 app.action("DOWN");app.action("A");assert(app.screen==Screen::BackupConfirm&&app.row==0);selectedRowVisible(app,304);
 for(const auto *lang:{"en","ru"}){app.locale.select(options.assets,lang);app.capture(std::string("backup-confirm-")+lang);}
 app.action("A");assert(app.screen==Screen::BackupDetails&&app.row==1); // Cancel is the default.
 app.action("A");app.action("DOWN");assert(app.row==1);
 auto next=app.store.data;next["locale"]="ru";app.store.commit(next);
 app.action("A");wait(app);assert(app.screen==Screen::BackupRestart&&app.backupError.empty()&&app.store.data["locale"]=="en");
 app.locale.select(options.assets,"ru");app.capture("backup-restored-ru");app.action("B");assert(!app.stop);app.action("A");assert(app.stop&&app.systemExitCode==66);
 // A damaged copy is rejected before applying; returning to the list needs no restart.
 Options bad=options;bad.state=fs::path(argv[2])/"bad-state";App failed(bad);failed.backupOpen();
 failed.backupSelected=app.backupList[0];auto bytes=readBytes(failed.backupSelected/"data/config.json");atomicWrite(failed.backupSelected/"data/config.json",{0});
 failed.backupStart(true);wait(failed);assert(failed.screen==Screen::Backups&&!failed.backupError.empty()&&!failed.stop);
 assert(Store(bad.state).fresh);
 for(const auto *lang:{"en","ru"}){failed.locale.select(options.assets,lang);failed.notice=failed.locale.tr("backup.invalid");failed.capture(std::string("backup-error-")+lang);}
 atomicWrite(app.backupList[0]/"data/config.json",bytes);
 App manage(options);manage.backupOpen();manage.row=1;manage.action("A");assert(manage.screen==Screen::BackupDetails);
 manage.action("DOWN");manage.action("DOWN");manage.action("A");assert(manage.screen==Screen::BackupDelete&&manage.row==0);
 for(const auto *lang:{"en","ru"}){manage.locale.select(options.assets,lang);manage.capture(std::string("backup-delete-")+lang);}
 manage.action("A");assert(manage.screen==Screen::BackupDetails&&fs::exists(app.backupList[0]));
 manage.action("A");manage.action("DOWN");manage.action("A");wait(manage);assert(manage.screen==Screen::Backups&&manage.backupList.empty());
 assert(fs::exists(options.state/"games"/game/"slot-0.state")&&fs::exists(options.state/"config.json"));
 std::cout<<"PASS: backup detail/game navigation and confirmed isolated deletion; backup navigation, default cancellation, busy-input fencing, async progress, safe reload and localized failure states.\n";
}
