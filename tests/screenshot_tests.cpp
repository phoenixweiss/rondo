// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include "stb_image.h"
#include "stb_image_write.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace rondo;
static void finish(App &a){for(int i=0;i<5000&&a.screenshotWriter.busy();++i){a.screenshotTick();SDL_Delay(1);}assert(!a.screenshotWriter.busy());}
static std::vector<fs::path> images(const fs::path &root){std::vector<fs::path> out;if(fs::exists(root))for(const auto &p:fs::recursive_directory_iterator(root))if(p.path().extension()==".png")out.push_back(p.path());return out;}
static void galleryChecks(const Options &base,const fs::path &work) {
  auto options=base;
  options.state=work/"gallery-state";
  options.output=work/"gallery-renders";
  Store config(options.state);config.commit(config.data);
  auto root=options.state/"Screenshots";
  auto emptyRoot=work/"no-gallery";
  assert(screenshotGroups(StatePaths(emptyRoot)).empty()&&!fs::exists(emptyRoot));
  ScreenshotWriter writer;
  std::vector<uint32_t> pixels(900*600,0xff547789);
  Game game{"gallery-one","TF1","NES","Battle City",{}};
  auto write=[&](bool interface) {
    bool submitted=interface?writer.submitInterface(root,pixels,900,600,"2026-11-14_20-30-00"):
                             writer.submit(root,game,pixels,900,600,"2026-11-14_20-31-00");
    assert(submitted);
    std::optional<fs::path> result;
    for(int i=0;i<5000&&!result;++i){result=writer.poll();if(!result)SDL_Delay(1);}
    assert(result);return *result;
  };
  write(true);auto original=write(false);
  for(int i=0;i<8;++i)
    fs::copy_file(original,original.parent_path()/("2026-11-14_21-0"+std::to_string(i)+"-00.png"));
  auto legacy=root/"Battle City-legacy/capture.PNG";
  fs::create_directories(legacy.parent_path());fs::copy_file(original,legacy);
  auto outside=work/"outside.png";fs::copy_file(original,outside);
  fs::create_symlink(outside,root/"linked.png");
  fs::create_directory_symlink(legacy.parent_path(),root/"linked-folder");
  atomicWrite(original.parent_path()/".keep",{7});
  auto groups=screenshotGroups(options.paths());
  assert(groups.size()==3);
  auto image=loadGalleryImage(options.paths(),groups[0].files[0]);
  assert(image.width==900&&image.height==600&&image.rgba.size()==900*600*4);
  auto candidate=groups[0].files[0];
  auto escape=candidate;escape.relative="../../outside.png";
  bool escaped=false;
  try{deleteGalleryImage(options.paths(),escape);}catch(const std::exception &){escaped=true;}
  assert(escaped&&fs::exists(outside));
  auto linked=candidate;linked.relative="linked.png";
  escaped=false;
  try{deleteGalleryImage(options.paths(),linked);}catch(const std::exception &){escaped=true;}
  assert(escaped&&fs::exists(outside));
  atomicWrite(candidate.root/candidate.relative,{0});
  bool rejected=false;
  try{deleteGalleryImage(options.paths(),candidate);}catch(const GalleryError &e){rejected=std::string(e.what())=="gallery.changed";}
  assert(rejected&&fs::exists(candidate.root/candidate.relative));
  groups=screenshotGroups(options.paths());
  auto damaged=groups[0].files[0];
  rejected=false;
  try{loadGalleryImage(options.paths(),damaged);}catch(const GalleryError &e){rejected=std::string(e.what())=="gallery.image_error";}
  assert(rejected);deleteGalleryImage(options.paths(),damaged);
  assert(fs::exists(original.parent_path()/".keep")&&fs::exists(outside));

  // Split-card guards recheck the current destination before deletion.
  groups=screenshotGroups(options.paths());candidate=groups[0].files[0];
  auto routed=options.paths();bool readOnly=false,moved=false;
  routed.resolve=[&](const std::string &category,bool writing) {
    if(category=="screenshots") {
      if(writing&&readOnly)throw StorageError("storage.readonly");
      return moved?work/"other-card/Screenshots":root;
    }
    return options.state;
  };
  readOnly=true;rejected=false;
  try{deleteGalleryImage(routed,candidate);}catch(const StorageError &){rejected=true;}
  assert(rejected&&fs::exists(candidate.root/candidate.relative));
  readOnly=false;moved=true;rejected=false;
  try{deleteGalleryImage(routed,candidate);}catch(const GalleryError &){rejected=true;}
  assert(rejected&&fs::exists(candidate.root/candidate.relative));

  App app(options);
  auto wait=[&] {
    for(int i=0;i<5000&&(app.galleryScan.valid()||app.galleryLoad.valid()||app.galleryRemoval.valid());++i) {
      app.ticks(0);SDL_Delay(1);
    }
    assert(!app.galleryScan.valid()&&!app.galleryLoad.valid()&&!app.galleryRemoval.valid());
  };
  // Enter through the public settings path, without substituting the route or row.
  app.action("MENU");assert(app.screen==Screen::Settings);
  app.action("A");assert(app.screen==Screen::SettingsSection);
  for(int i=0;i<4;++i)app.action("DOWN");
  app.action("A");assert(app.screen==Screen::ScreenshotSettings);
  for(int i=0;i<3;++i)app.action("DOWN");
  assert(app.screenshotWriter.submitInterface(root,{0xff111111},1,1,"busy-probe"));
  app.action("A");assert(app.screen==Screen::ScreenshotSettings&&!app.notice.empty());
  finish(app);
  app.action("A");assert(app.screen==Screen::GalleryGroups);wait();
  assert(app.galleryGroups.size()==3);
  for(const auto *language:{"en","ru"}) {
    app.locale.select(options.assets,language);app.capture(std::string("gallery-groups-")+language);
  }
  while(app.galleryGroups[app.galleryGroup].title!="Battle City")app.action("DOWN");
  app.action("A");assert(app.screen==Screen::GalleryImages);
  for(int i=0;i<80;++i){app.draw();SDL_Delay(5);}
  for(const auto *language:{"en","ru"}) {
    app.locale.select(options.assets,language);app.capture(std::string("gallery-images-")+language);
  }
  app.action("R1");assert(app.galleryIndex==4);app.action("L1");assert(app.galleryIndex==0);
  app.action("UP");assert(app.screen==Screen::GalleryImages&&!app.navigationActions);
  app.action("A");wait();assert(app.screen==Screen::GalleryView&&app.galleryTexture);
  app.capture("gallery-fit-ru");
  app.action("A");assert(app.galleryActualSize);
  app.action("RIGHT");app.action("DOWN");assert(app.galleryPanX>0&&app.galleryPanY>0);
  app.capture("gallery-actual-ru");
  app.action("R1");assert(app.galleryLoad.valid());
  app.action("X");assert(app.screen==Screen::GalleryDelete&&app.row==0);
  wait();assert(app.galleryIndex==1&&app.galleryActualSize&&app.galleryTexture);
  auto chosen=app.galleryDeletingFile;
  for(const auto *language:{"en","ru"}) {
    app.locale.select(options.assets,language);app.capture(std::string("gallery-delete-")+language);
  }
  app.action("A");assert(app.screen==Screen::GalleryView&&fs::exists(chosen.root/chosen.relative));
  app.action("X");app.action("DOWN");app.action("A");assert(app.screen==Screen::GalleryDeleting);
  app.action("B");assert(app.screen==Screen::GalleryDeleting);
  wait();assert(app.screen==Screen::GalleryImages&&!fs::exists(chosen.root/chosen.relative));
  assert(fs::exists(original.parent_path()/".keep")&&fs::exists(original.parent_path()/".game-id"));
  app.action("B");assert(app.screen==Screen::GalleryGroups);
  app.action("B");assert(app.screen==Screen::ScreenshotSettings&&app.row==3);
  assert(fs::exists(outside));
  // Existing empty directories and damaged PNGs remain manageable.
  for(const auto &group:screenshotGroups(options.paths()))
    for(const auto &file:group.files)deleteGalleryImage(options.paths(),file);
  assert(fs::is_directory(legacy.parent_path()));
  app.action("A");assert(app.screen==Screen::GalleryGroups);wait();
  assert(app.galleryGroups.empty());
  for(const auto *language:{"en","ru"}) {
    app.locale.select(options.assets,language);app.capture(std::string("gallery-empty-")+language);
  }
  auto saved=root.parent_path()/"saved-screenshots";
  fs::rename(root,saved);fs::create_directory_symlink(saved,root);
  app.action("Y");wait();assert(!app.galleryError.empty());
  app.capture("gallery-unavailable-ru");
  fs::remove(root);fs::rename(saved,root);
  app.action("Y");wait();assert(app.galleryError.empty()&&app.galleryGroups.empty());
  app.action("B");assert(app.screen==Screen::ScreenshotSettings);
  std::cout<<"PASS: gallery legacy groups, no linked files, corrupt/replaced/read-only/card guards, public settings navigation, thumbnails, paging, 1:1 pan, cancellation and confirmed single-file deletion.\n";
}
int main(int argc,char **argv) {
  assert(argc==4||argc==5);SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);
  fs::path work=argv[2];fs::create_directories(work);auto root=work/"state";
  {Store old(root);auto legacy=old.data;legacy.erase("screenshot_shortcut");legacy.erase("screenshot_custom");old.commit(legacy);}
  Options o;o.assets=argv[1];o.state=root;o.core=argv[3];o.headless=true;o.output=work/"captures";
  Game game{"screenshot-game","TF1","NES","Test / Кадр",work/"test.bin"};atomicWrite(game.path,{0});
  App a(o,true,game);assert(screenshotChord(a.store.data)==0xc000);
  a.core->width=3;a.core->height=2;a.core->frame={0xffff0000,0xff00ff00,0xff0000ff,0xff112233,0xffffffff,0xff000000};
  const auto raw=a.core->frame;auto config=a.store.data;
  a.gameInput(0,{});assert(!a.gameInputBlocked);
  a.notice="OVERLAY MUST NOT BE CAPTURED";
  a.gameInput(0xc000,{});assert(a.core->buttons==0);finish(a);
  auto files=images(root/"Screenshots");assert(files.size()==1);
  int w=0,h=0,n=0;auto *pixels=stbi_load(files[0].c_str(),&w,&h,&n,4);assert(pixels&&w==3&&h==2);
  for(size_t i=0;i<raw.size();++i){assert(pixels[i*4]==((raw[i]>>16)&255));assert(pixels[i*4+1]==((raw[i]>>8)&255));assert(pixels[i*4+2]==(raw[i]&255));assert(pixels[i*4+3]==255);}stbi_image_free(pixels);
  for(int i=0;i<20;++i){a.gameInput(0xc000,{});a.ticks(0);}assert(images(root/"Screenshots").size()==1);
  a.gameInput(0x4000,{});assert(a.core->buttons==0);a.gameInput(0,{});a.gameInput(0xc000,{});finish(a);assert(images(root/"Screenshots").size()==2);
  a.action("MENU");assert(a.core->paused);a.row=int(GameMenuItem::Screenshot);a.action("A");finish(a);assert(a.screen==Screen::GameMenu&&a.core->paused&&a.row==int(GameMenuItem::Screenshot));assert(images(root/"Screenshots").size()==3);
  a.action("B");a.gameInput(0xc000,{});assert(a.core->buttons==0);assert(!a.screenshotWriter.busy());a.gameInput(0,{});
  assert(a.store.data==config);
  // Deterministic chord timing: quick taps survive, a held chord fires once,
  // unrelated buttons survive, and late single presses remain usable.
  ScreenshotChord keys;bool shot=false;
  assert(keys.sample(0x4000,0xc000,100,shot)==0&&!shot);
  assert(keys.sample(0,0xc000,120,shot)==0x4000&&!shot);
  keys.sample(0x4000,0xc000,200,shot);assert(keys.sample(0xc100,0xc000,216,shot)==0x100&&shot);
  assert(keys.sample(0xc000,0xc000,300,shot)==0&&!shot);keys.sample(0,0xc000,310,shot);
  keys.sample(0x4000,0xc000,400,shot);assert(keys.sample(0x4000,0xc000,480,shot)==0x4000&&!shot);
  assert(keys.sample(0xc000,0xc000,500,shot)==0&&shot);
  a.screen=Screen::SettingsSection;a.category=0;a.row=4;a.action("A");assert(a.screen==Screen::ScreenshotSettings);
  a.row=0;a.action("RIGHT");assert(screenshotChord(a.store.data)==0x3000);
  a.action("RIGHT");assert(screenshotChord(a.store.data)==0x804);
  a.action("LEFT");a.row=1;a.action("A");assert(a.screen==Screen::ScreenshotCapture);
  a.screenshotRecord(0x100);a.screenshotRecord(0); // Ignore the entry-held A.
  a.screenshotRecord(0x300);a.screenshotRecord(0x100);a.screenshotRecord(0);
  assert(a.screen==Screen::ScreenshotSettings&&screenshotChord(a.store.data)==0x300);
  a.row=1;a.action("A");a.screenshotRecord(0);a.screenshotRecord(12);a.screenshotRecord(0);assert(a.screen==Screen::ScreenshotCapture&&!a.notice.empty());a.action("MENU");
  for(auto lang:{"en","ru"}){
    a.locale.select(o.assets,lang);a.notice.clear();a.screenshotMessageUntil=0;
    a.screen=Screen::ScreenshotSettings;a.row=0;a.capture(std::string("screenshot-settings-")+lang);
    a.row=1;a.action("A");a.capture(std::string("screenshot-record-")+lang);a.action("MENU");
    a.screen=Screen::GameMenu;a.row=int(GameMenuItem::Screenshot);a.core->paused=true;a.capture(std::string("screenshot-menu-")+lang);
    a.takeScreenshot();finish(a);a.screen=Screen::Running;a.core->paused=false;a.capture(std::string("screenshot-feedback-")+lang);
  }
  a.screen=Screen::ScreenshotSettings;a.row=0;a.store.data["screenshot_shortcut"]="off";assert(screenshotChord(a.store.data)==0);
  a.requestSettings=[](const Json &){return false;};a.action("RIGHT");assert(a.store.data["screenshot_shortcut"]=="off");a.requestSettings={};
  auto bad=a.store.data;bad["screenshot_custom"]=12;bool rejected=false;try{a.store.commit(bad);}catch(...){rejected=true;}assert(rejected);
  // Exercise the generated click on a dummy device, never host audio.
  assert(a.store.data.value("screenshot_sound",false));
  a.core.reset();a.core=std::make_unique<Core>(o.core,game,work/"sound-core",false);
  a.core->width=3;a.core->height=2;a.core->frame=raw;
  a.core->paused=true;assert(a.core->playScreenshotClick());a.core->volume=0;assert(!a.core->playScreenshotClick());a.core->volume=80;
  a.screen=Screen::GameMenu;a.core->paused=true;a.core->silence();a.takeScreenshot();finish(a);assert(a.screenshotMessage==a.locale.tr("screenshot.saved"));
  a.core->silence();
  a.screen=Screen::ScreenshotSettings;a.row=2;a.action("LEFT");assert(!a.store.data["screenshot_sound"].get<bool>());
  assert(!Store(root).data["screenshot_sound"].get<bool>());
  a.screen=Screen::GameMenu;a.takeScreenshot();finish(a);
  a.screen=Screen::ScreenshotSettings;a.row=2;a.action("RIGHT");assert(a.store.data["screenshot_sound"].get<bool>());
  auto wrong=a.store.data;wrong["screenshot_sound"]="on";rejected=false;try{a.store.commit(wrong);}catch(...){rejected=true;}assert(rejected);
  // The runner loads cover artwork only when opening Game info, even when
  // the library currently displays a different artwork type.
  assert(!a.ui->artworkCache);std::vector<uint32_t> cover(80*100,0xff2444cc);assert(stbi_write_png((game.path.parent_path()/"cover.png").c_str(),80,100,4,cover.data(),80*4));
  a.artwork="title";a.screen=Screen::GameMenu;a.row=0;a.action("Y");assert(a.ui->artworkCache&&a.screen==Screen::GameDetails&&a.core->paused);a.capture("runner-game-info-cover");
  auto *poster=reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(a.ui->surface->pixels)+240*a.ui->surface->pitch+496*4);Uint8 pr,pg,pb,pa;SDL_GetRGBA(*poster,a.ui->surface->format,&pr,&pg,&pb,&pa);assert(pr>150&&pg<100&&pb<100);
  a.action("B");assert(a.screen==Screen::GameMenu);a.row=0;for(int i=0;i<6;++i)a.action("DOWN");assert(a.row==int(GameMenuItem::Exit));a.capture("game-menu-exit-last");a.action("A");assert(a.screen==Screen::ExitConfirm);a.action("B");assert(a.screen==Screen::GameMenu&&a.row==int(GameMenuItem::Exit));
  a.row=int(GameMenuItem::Settings);a.action("A");assert(a.screen==Screen::GameSettings);a.action("B");assert(a.screen==Screen::GameMenu&&a.row==int(GameMenuItem::Settings));
  // A failing destination reports an error and retains the game/pause state.
  auto obstruction=work/"blocked";atomicWrite(obstruction,{1});a.options.state=obstruction;a.screen=Screen::GameMenu;a.core->paused=true;a.takeScreenshot();finish(a);assert(a.screenshotMessage==a.locale.tr("screenshot.failed")&&a.core->paused&&a.screen==Screen::GameMenu);
  a.options.state=root;a.store.data["screenshot_shortcut"]="sticks";a.store.data["left_stick_dpad"]=true;
  // Exercise the same physical sampling function used by the interactive loop,
  // with a real SDL virtual controller instead of directly injecting a bitmask.
  int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);assert(index>=0);
  auto *joystick=SDL_JoystickOpen(index);assert(joystick);
  char guid[64];SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick),guid,sizeof(guid));
  std::string mapping=std::string(guid)+",Screenshot physical test,a:b0,b:b1,back:b4,start:b6,leftstick:b7,rightstick:b8,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
  assert(SDL_GameControllerAddMapping(mapping.c_str())>=0);SDL_PumpEvents();a.ui->refreshControllers();a.syncGamepadPlayers();assert(a.ui->controllers.size()==1);
  auto sample=[&](int l,int r){SDL_JoystickSetVirtualButton(joystick,7,l);SDL_JoystickSetVirtualButton(joystick,8,r);SDL_GameControllerUpdate();a.sampleInput();};
  a.screen=Screen::Running;a.core->paused=false;a.gameInputBlocked=true;
  sample(0,0);assert(!a.gameInputBlocked);
  auto verifyMenuCapture=[&] {
    auto all=images(root/"Screenshots/Interface");assert(!all.empty());
    auto latest=*std::max_element(all.begin(),all.end());
    auto *png=stbi_load(latest.c_str(),&w,&h,&n,4);assert(png&&w==720&&h==480);
    // The menu header and rows must be present, not just the game frame.
    assert(png[0]!=255||png[1]!=0||png[2]!=0);
    stbi_image_free(png);
  };
  auto count=images(root/"Screenshots").size();sample(1,0);sample(1,1);finish(a);assert(images(root/"Screenshots").size()==count+1&&a.core->buttons==0);
  for(int i=0;i<10;++i)sample(1,1);finish(a);assert(images(root/"Screenshots").size()==count+1);
  sample(0,0);a.action("MENU");assert(a.screen==Screen::GameMenu&&a.core->paused);
  sample(1,1);finish(a);assert(images(root/"Screenshots").size()==count+2&&a.screen==Screen::GameMenu&&a.core->paused&&a.core->buttons==0);
  verifyMenuCapture();
  // The explicit menu action still exports the game, not the interface.
  auto gameBefore=images(root/"Screenshots/NES").size();
  a.row=int(GameMenuItem::Screenshot);a.action("A");finish(a);
  assert(images(root/"Screenshots/NES").size()==gameBefore+1);
  sample(0,0);a.core.reset();a.options.state=root/"interface-test";a.screen=Screen::Library;a.section="All";a.games.clear();a.navigation.clear();a.navigationActions=false;
  auto unchanged=a.store.data;a.screenshotMessageUntil=0;
  sample(1,1);finish(a);auto uiFiles=images(a.options.state/"Screenshots");assert(uiFiles.size()==1&&uiFiles[0].parent_path().filename()=="Interface");
  pixels=stbi_load(uiFiles[0].c_str(),&w,&h,&n,4);assert(pixels&&w==720&&h==480);
  for(int y=0;y<h;++y){auto *row=reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(a.ui->surface->pixels)+y*a.ui->surface->pitch);for(int x=0;x<w;++x){auto i=size_t(y*w+x)*4;assert(pixels[i]==((row[x]>>16)&255)&&pixels[i+1]==((row[x]>>8)&255)&&pixels[i+2]==(row[x]&255));}}stbi_image_free(pixels);
  assert(a.store.data==unchanged&&a.soundTest); // Dummy audio only; shell shutter follows the setting.
  for(int i=0;i<10;++i)sample(1,1);finish(a);assert(images(a.options.state/"Screenshots").size()==1);
  sample(0,0);a.screen=Screen::Settings;sample(1,1);finish(a);assert(images(a.options.state/"Screenshots").size()==2&&a.screen==Screen::Settings);
  sample(0,0);SDL_Delay(110);a.wizardTick();assert(!a.soundTest);
  a.store.data["screenshot_sound"]=false;a.store.data["screenshot_shortcut"]="off";a.action("SCREENSHOT");finish(a);assert(!a.soundTest&&images(a.options.state/"Screenshots").size()==3);
  a.screen=Screen::InputTest;sample(1,1);a.action("SCREENSHOT");finish(a);assert(images(a.options.state/"Screenshots").size()==3);sample(0,0);
  a.screen=Screen::ScreenshotCapture;sample(1,1);a.action("SCREENSHOT");assert(images(a.options.state/"Screenshots").size()==3);
  a.screen=Screen::Library;a.screenshotMessageUntil=0;
  sample(0,0);SDL_JoystickClose(joystick);assert(SDL_JoystickDetachVirtual(index)==0);SDL_PumpEvents();a.ui->refreshControllers();
  auto previousZone=getenv("TZ")?std::string(getenv("TZ")):"";
  assert(screenshotTimestamp(a.zones,"Etc/UTC",0)=="1970-01-01_00-00-00");assert(screenshotTimestamp(a.zones,"Europe/Moscow",0)=="1970-01-01_03-00-00");assert((getenv("TZ")?std::string(getenv("TZ")):"")==previousZone);
  // Human-readable grouping and same-title collision isolation.
  ScreenshotWriter writer;auto folders=work/"folder-format";Game first{"game-one","TF1","NES","Battle City",game.path};
  auto capture=[&](const Game &g){assert(writer.submit(folders,g,raw,3,2,"2026-10-27_17-21-36"));std::optional<fs::path> file;for(int i=0;i<5000&&!file;++i){file=writer.poll();if(!file)SDL_Delay(1);}assert(file);return *file;};
  fs::create_directories(folders/"Battle City-legacy");atomicWrite(folders/"Battle City-legacy/keep.txt",{7});
  auto one=capture(first);assert(one.filename().string().rfind("2026-10-27_17-21-36-",0)==0);assert(one.parent_path()==folders/"NES/Battle City");auto again=capture(first);assert(again.parent_path()==one.parent_path()&&again!=one);
  auto other=first;other.id="game-two";auto two=capture(other);assert(two.parent_path()==folders/"NES/Battle City (2)");assert(capture(other).parent_path()==two.parent_path());
  other.system="GB";assert(capture(other).parent_path()==folders/"GB/Battle City");assert(readBytes(folders/"Battle City-legacy/keep.txt")==std::vector<uint8_t>({7}));
  first.title="../Bad / Name";auto safe=capture(first);assert(safe.lexically_relative(folders).string().find("../")==std::string::npos);
  a.options.state=root;a.screenshotMessageUntil=0;
  // PSX uses display geometry, preserving every native source pixel by only
  // expanding an axis. High-resolution 512/640-wide modes retain their detail.
  a.core=std::make_unique<Core>(o.core,game,work/"aspect-core",true);a.game=game;
  auto oldGame=a.game;int oldWidth=a.core->width,oldHeight=a.core->height;auto oldFrame=a.core->frame;auto oldAspect=a.core->aspectRatio;
  a.game.system="PSX";a.screen=Screen::Running;a.store.data["screenshot_sound"]=false;
  for(const auto &size:std::vector<std::pair<int,int>>{{368,480},{256,240},{320,240},{512,240},{640,240},{640,480},{384,288}}) {
    a.core->width=size.first;a.core->height=size.second;a.core->aspectRatio=4.0/3;
    a.core->frame.assign(size_t(size.first)*size.second,0xff123456);a.core->frame.front()=0xffff0000;a.core->frame.back()=0xff00ff00;auto unchangedFrame=a.core->frame;
    auto beforeShots=images(root/"Screenshots");a.takeScreenshot();finish(a);auto afterShots=images(root/"Screenshots");fs::path created;
    for(const auto &p:afterShots)if(std::find(beforeShots.begin(),beforeShots.end(),p)==beforeShots.end())created=p;assert(!created.empty());
    pixels=stbi_load(created.c_str(),&w,&h,&n,4);assert(pixels&&w>=size.first&&h>=size.second&&std::abs(double(w)/h-4.0/3)<0.005);
    assert(pixels[0]==255&&pixels[1]==0&&pixels[2]==0);auto last=(size_t(w)*h-1)*4;assert(pixels[last]==0&&pixels[last+1]==255&&pixels[last+2]==0);stbi_image_free(pixels);assert(a.core->frame==unchangedFrame);
    if(size.first==368&&size.second==480)assert(w==640&&h==480);
    if(size.first==512&&size.second==240)assert(w==512&&h==384);
    if(size.first==640&&size.second==240)assert(w==640&&h==480);
  }
  a.core->width=368;a.core->height=480;a.core->aspectRatio=4.0/3;
  a.core->frame.assign(368*480,0xffff0000);a.screen=Screen::GameMenu;a.core->paused=true;
  a.notice.clear();a.screenshotMessageUntil=0;a.draw();
  auto at=[&](int x,int y){return *reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(a.ui->surface->pixels)+y*a.ui->surface->pitch+x*4)&0xffffffu;};
  assert(at(594,242)==0xff0000u);assert(at(495,242)==0xff0000u);
  assert(at(594,141)!=0xff0000u&&at(594,343)!=0xff0000u);
  a.capture("psx-pause-preview-4-3");
  a.game=oldGame;a.core->width=oldWidth;a.core->height=oldHeight;a.core->frame=std::move(oldFrame);a.core->aspectRatio=oldAspect;
  // Optional private raw PSX capture: exercise the actual export pipeline without
  // adding game artwork or user screenshots to the repository.
  if(argc==5) {
    pixels=stbi_load(argv[4],&w,&h,&n,4);assert(pixels);std::vector<uint32_t> input(size_t(w)*h);
    for(size_t i=0;i<input.size();++i)input[i]=0xff000000u|(uint32_t(pixels[4*i])<<16)|(uint32_t(pixels[4*i+1])<<8)|pixels[4*i+2];stbi_image_free(pixels);
    Game psx{"private-psx-capture","TF1","PSX","Display aspect check",{}};ScreenshotWriter exportWriter;assert(exportWriter.submit(work/"corrected-psx",psx,input,w,h,"test",4.0/3));
    std::optional<fs::path> file;for(int i=0;i<5000&&!file;++i){file=exportWriter.poll();if(!file)SDL_Delay(1);}assert(file);fs::copy_file(*file,work/"psx-corrected.png");
  }
  galleryChecks(o,work);
  std::cout<<"PASS: raw PNG colors/dimensions, unique paths, no repeat/input leak, menu pause, presets/custom/persistence, rejected writes, EN/RU renders, asynchronous errors.\n";
}
