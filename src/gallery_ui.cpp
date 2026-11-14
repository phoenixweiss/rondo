// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
#include <iostream>
namespace rondo {
namespace {
bool ready(const std::future_status status) {return status==std::future_status::ready;}
std::string captureLabel(const fs::path &path) {
  auto name=path.stem().string();
  if(name.size()>=19&&name[4]=='-'&&name[7]=='-'&&name[10]=='_'&&name[13]=='-')
    return name.substr(8,2)+"."+name.substr(5,2)+" "+name.substr(11,2)+":"+name.substr(14,2);
  return name;
}
}
bool App::galleryRoute() const {
  return screen==Screen::GalleryGroups||screen==Screen::GalleryImages||
         screen==Screen::GalleryView||screen==Screen::GalleryDelete||screen==Screen::GalleryDeleting;
}
void App::galleryOpen() {
  if(screenshotWriter.busy()){notice=locale.tr("screenshot.busy");return;}
  notice.clear();screenshotMessageUntil=0;
  screen=Screen::GalleryGroups;galleryGroup=galleryIndex=0;galleryScroll=0;
  galleryRefresh();
}
void App::galleryRefresh() {
  if(galleryScan.valid())return;
  galleryGroups.clear();galleryTexture.reset();galleryError.clear();
  auto paths=options.paths();
  galleryScan=std::async(std::launch::async,[paths]{return screenshotGroups(paths);});
}
void App::galleryShow() {
  galleryTexture.reset();galleryError.clear();galleryPanX=galleryPanY=0;
  galleryWidth=galleryHeight=0;
  if(galleryLoad.valid())return;
  const auto &files=galleryGroups.at(galleryGroup).files;
  galleryLoadingFile=files.at(galleryIndex);
  auto file=galleryLoadingFile;auto paths=options.paths();
  galleryLoad=std::async(std::launch::async,[paths,file]{return loadGalleryImage(paths,file);});
}
void App::galleryTick() {
  if(galleryScan.valid()&&ready(galleryScan.wait_for(std::chrono::milliseconds(0)))) {
    try {
      auto groups=galleryScan.get();
      if(screen==Screen::GalleryGroups) {
        galleryGroups=std::move(groups);
        galleryGroup=std::clamp(galleryGroup,0,std::max(0,int(galleryGroups.size())-1));
      }
    } catch(const StorageError &error){if(galleryRoute())galleryError=error.code;}
      catch(const std::exception &){if(galleryRoute())galleryError="gallery.unavailable";}
  }
  if(galleryLoad.valid()&&ready(galleryLoad.wait_for(std::chrono::milliseconds(0)))) {
    bool viewing=screen==Screen::GalleryView||
      (screen==Screen::GalleryDelete&&galleryDeleteReturn==Screen::GalleryView);
    bool current=viewing&&!galleryGroups.empty()&&
      galleryGroups[galleryGroup].files[galleryIndex].relative==galleryLoadingFile.relative&&
      galleryGroups[galleryGroup].files[galleryIndex].root==galleryLoadingFile.root;
    try {
      auto image=galleryLoad.get();
      if(current) {
        auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(image.rgba.data(),image.width,image.height,
                                                        32,image.width*4,SDL_PIXELFORMAT_RGBA32);
        if(!surface)throw GalleryError("gallery.image_error");
        galleryTexture.reset(SDL_CreateTextureFromSurface(ui->renderer,surface));
        SDL_FreeSurface(surface);
        if(!galleryTexture)throw GalleryError("gallery.image_error");
        SDL_SetTextureScaleMode(galleryTexture.get(),SDL_ScaleModeNearest);
        galleryWidth=image.width;galleryHeight=image.height;
      }
    } catch(const GalleryError &error){if(current)galleryError=error.what();}
      catch(const std::exception &){if(current)galleryError="gallery.unavailable";}
    if(screen==Screen::GalleryView&&!current)galleryShow();
  }
  if(galleryRemoval.valid()&&ready(galleryRemoval.wait_for(std::chrono::milliseconds(0)))) {
    try {
      galleryRemoval.get();
      auto &files=galleryGroups.at(galleryGroup).files;
      files.erase(files.begin()+galleryIndex);
      galleryTexture.reset();
      if(files.empty()) {
        galleryGroups.erase(galleryGroups.begin()+galleryGroup);
        galleryGroup=std::max(0,std::min(galleryGroup,int(galleryGroups.size())-1));
        screen=Screen::GalleryGroups;
      } else {
        galleryIndex=std::min(galleryIndex,int(files.size())-1);
        screen=Screen::GalleryImages;
      }
    } catch(const GalleryError &error){galleryError=error.what();screen=galleryDeleteReturn;}
      catch(const StorageError &error){galleryError=error.code;screen=galleryDeleteReturn;}
      catch(const std::exception &){galleryError="gallery.delete_failed";screen=galleryDeleteReturn;}
  }
}
void App::galleryAction(const std::string &a) {
  if(screen==Screen::GalleryDeleting)return;
  if(screen==Screen::GalleryDelete) {
    if(a=="B"||(a=="A"&&row==0)){screen=galleryDeleteReturn;return;}
    if(a=="UP")row=0;
    if(a=="DOWN")row=1;
    if(a=="A"&&row==1) {
      auto paths=options.paths();auto file=galleryDeletingFile;
      galleryError.clear();
      galleryRemoval=std::async(std::launch::async,[paths,file]{deleteGalleryImage(paths,file);});
      screen=Screen::GalleryDeleting;
    }
    return;
  }
  if(a=="B") {
    if(screen==Screen::GalleryGroups) {
      if(!navigation.empty())navigationBack();
      else {screen=Screen::ScreenshotSettings;row=3;}
    } else screen=screen==Screen::GalleryView?Screen::GalleryImages:Screen::GalleryGroups;
    return;
  }
  if(screen==Screen::GalleryGroups) {
    if(a=="Y"){galleryRefresh();return;}
    if(galleryScan.valid()||galleryGroups.empty())return;
    if(a=="UP")galleryGroup=std::max(0,galleryGroup-1);
    if(a=="DOWN")galleryGroup=std::min(int(galleryGroups.size())-1,galleryGroup+1);
    if(a=="L1")galleryGroup=std::max(0,galleryGroup-5);
    if(a=="R1")galleryGroup=std::min(int(galleryGroups.size())-1,galleryGroup+5);
    if(a=="A"){screen=Screen::GalleryImages;galleryIndex=0;galleryError.clear();}
    return;
  }
  if(galleryGroups.empty())return;
  auto &files=galleryGroups.at(galleryGroup).files;
  if(a=="X") {
    galleryDeletingFile=files.at(galleryIndex);galleryDeleteReturn=screen;
    galleryError.clear();screen=Screen::GalleryDelete;row=0;return;
  }
  int last=int(files.size())-1,previous=galleryIndex;
  if(a=="L1")galleryIndex=std::max(0,galleryIndex-(screen==Screen::GalleryView?1:4));
  if(a=="R1")galleryIndex=std::min(last,galleryIndex+(screen==Screen::GalleryView?1:4));
  if(screen==Screen::GalleryView) {
    if(a=="A"){galleryActualSize=!galleryActualSize;galleryPanX=galleryPanY=0;}
    if(galleryActualSize) {
      if(a=="LEFT")galleryPanX-=32;
      if(a=="RIGHT")galleryPanX+=32;
      if(a=="UP")galleryPanY-=32;
      if(a=="DOWN")galleryPanY+=32;
      int horizontal=std::max(0,galleryWidth-720),vertical=std::max(0,galleryHeight-432);
      galleryPanX=std::clamp(galleryPanX,-horizontal/2,(horizontal+1)/2);
      galleryPanY=std::clamp(galleryPanY,-vertical/2,(vertical+1)/2);
    } else {
      if(a=="LEFT")galleryIndex=std::max(0,galleryIndex-1);
      if(a=="RIGHT")galleryIndex=std::min(last,galleryIndex+1);
    }
    if(previous!=galleryIndex)galleryShow();
    return;
  }
  if(a=="LEFT")galleryIndex=std::max(0,galleryIndex-1);
  if(a=="RIGHT")galleryIndex=std::min(last,galleryIndex+1);
  if(a=="UP")galleryIndex=std::max(0,galleryIndex-2);
  if(a=="DOWN")galleryIndex=std::min(last,galleryIndex+2);
  if(a=="A"){screen=Screen::GalleryView;galleryShow();}
}
void App::galleryDraw(const std::string &zone) {
  auto tr=[this](const char *key){return locale.tr(key);};
  if(screen==Screen::GalleryView) {
    ui->rect(0,0,720,480,{0,0,0,255});
    if(galleryTexture) {
      double scale=galleryActualSize?1.0:std::min(720.0/galleryWidth,432.0/galleryHeight);
      int width=int(galleryWidth*scale),height=int(galleryHeight*scale);
      SDL_Rect target{(720-width)/2-galleryPanX,(432-height)/2-galleryPanY,width,height};
      SDL_Rect clip{0,0,720,432};SDL_RenderSetClipRect(ui->renderer,&clip);
      SDL_RenderCopy(ui->renderer,galleryTexture.get(),nullptr,&target);
      SDL_RenderSetClipRect(ui->renderer,nullptr);
    } else {
      ui->text(tr(galleryLoad.valid()?"gallery.loading":"gallery.image_error"),24,198,24,500,ui->cream,672);
    }
    ui->rect(0,432,720,48,ui->bg);
    std::vector<std::pair<std::string,std::string>> hints{{"A",galleryActualSize?tr("gallery.fit"):"1:1"}};
    if(galleryActualSize)hints.push_back({"UP/DOWN",tr("gallery.pan")});
    hints.insert(hints.end(),{{"B",tr("action.back")},{"X",tr("gallery.delete")},{"L1/R1",tr("gallery.frame")}});
    ui->hints(hints);
  } else {
    auto title=screen==Screen::GalleryGroups?tr("gallery.title"):
               galleryGroups.empty()?tr("gallery.title"):galleryGroups[galleryGroup].title;
    if(title=="Interface"&&!galleryGroups.empty()&&galleryGroups[galleryGroup].system.empty())title=tr("gallery.interface");
    if(title.empty())title=tr("gallery.other");
    ui->frame(title,tr("screenshot.title"),zone);
    if(screen==Screen::GalleryDeleting) {
      ui->text(tr("gallery.deleting"),32,164,24,500,ui->cream,656);ui->hints({});
    } else if(screen==Screen::GalleryDelete) {
      ui->text(tr("gallery.delete_question"),32,144,26,600,ui->cream,656);
      ui->text(galleryDeletingFile.relative.filename().string(),32,202,22,400,ui->secondary,656);
      ui->row(tr("action.cancel"),304,row==0);
      ui->row(tr("gallery.delete"),358,row==1,672,"",true,52,"","trash");
      ui->hints({{"A",tr("action.select")},{"B",tr("action.back")}});
    } else if(screen==Screen::GalleryGroups) {
      int count=int(galleryGroups.size()),height=54,viewport=270;
      if(galleryGroup*height<galleryScroll)galleryScroll=galleryGroup*height;
      if(galleryGroup*height+height>galleryScroll+viewport)galleryScroll=galleryGroup*height+height-viewport;
      galleryScroll=std::clamp(galleryScroll,0.0,double(std::max(0,count*height-viewport)));
      SDL_Rect clip{16,128,688,viewport};SDL_RenderSetClipRect(ui->renderer,&clip);
      for(int i=0;i<count;++i) {
        int y=128+i*height-int(galleryScroll);if(y+height<128||y>=398)continue;
        auto label=galleryGroups[i].title;
        if(label=="Interface"&&galleryGroups[i].system.empty())label=tr("gallery.interface");
        if(label.empty())label=tr("gallery.other");
        auto detail=galleryGroups[i].system;
        if(!detail.empty())detail+=" · ";
        detail+=std::to_string(galleryGroups[i].files.size());
        ui->row(label,y,galleryGroup==i,672,detail,true,52,"chevron-right","camera");
      }
      SDL_RenderSetClipRect(ui->renderer,nullptr);
      ui->scrollbar(count*height,viewport,galleryScroll,false,128,398);
      if(!count) {
        auto message=galleryError.empty()?tr(galleryScan.valid()?"gallery.loading":"gallery.empty"):locale.tr(galleryError);
        ui->text(message,32,164,23,500,ui->secondary,656);
        ui->hints({{"B",tr("action.back")},{"Y",tr("gallery.refresh")}});
      } else ui->hints({{"A",tr("action.open")},{"B",tr("action.back")},{"Y",tr("gallery.refresh")},{"L1/R1",tr("library.pages")}});
    } else {
      const auto &files=galleryGroups[galleryGroup].files;
      int first=galleryIndex/4*4,last=std::min(first+4,int(files.size()));
      std::vector<ArtworkRequest> requests;
      requests.push_back({files[galleryIndex].root/files[galleryIndex].relative,312,96,false});
      for(int i=first;i<last;++i)if(i!=galleryIndex)
        requests.push_back({files[i].root/files[i].relative,312,96,false});
      if(ui->artworkCache)ui->artworkCache->select(requests);
      for(int i=first;i<last;++i) {
        int x=24+(i-first)%2*344,y=128+(i-first)/2*132;
        bool selected=i==galleryIndex;
        ui->rect(x,y,328,126,selected?ui->accent:ui->separator,6);
        ui->rect(x+1,y+1,326,124,selected?ui->focus:ui->bg,5);
        ArtworkRequest image{files[i].root/files[i].relative,312,96,false};
        if(!ui->artworkImage({image},x+8,y+4,312,96,false))
          ui->artworkPlaceholder(x+8,y+4,312,96,"",false);
        ui->text(captureLabel(files[i].relative),x+10,y+101,19,500,ui->cream,308);
      }
      ui->text(std::to_string(galleryIndex+1)+" / "+std::to_string(files.size()),32,402,20,400,ui->secondary,656);
      ui->hints({{"A",tr("action.open")},{"B",tr("action.back")},{"X",tr("gallery.delete")},{"L1/R1",tr("library.pages")}});
    }
  }
  if(!galleryError.empty()&&(screen!=Screen::GalleryGroups||!galleryGroups.empty()))ui->notice(locale.tr(galleryError),"camera",false,true);
}
}
