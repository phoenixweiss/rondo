#include "ui.hpp"
#include <cassert>
#include <iostream>
using namespace rondo;
int main(int argc,char **argv) {
  assert(argc==3);UI ui(argv[1],true);Locale locale(argv[1]);
  auto spec=readJson(fs::path(argv[1])/"volume-component.json");
  auto panel=spec["panel"];assert(panel["width"].get<int>()<=200 && panel["height"].get<int>()<=40);
  assert(spec["percentage"]==false && spec["normal_label"].is_null());
  std::map<std::string,std::vector<Uint32>> stable;
  for(const auto &code:{"en","ru"}) {
    locale.select(argv[1],code);
    for(const auto &state:{"confirmed","muted","maximum","minimum","applying","applying-muted","error","error-muted","unknown","applying-unknown","unavailable"}) {
      VolumeFeedback feedback;feedback.phase=std::string(state)=="maximum"?"confirmed":std::string(state)=="applying-muted"?"applying":state;
      feedback.confirmed=(std::string(state)=="muted" || std::string(state)=="applying-muted")?0:std::string(state)=="maximum"?100:75;
      if(std::string(state)=="unknown")feedback.confirmed=std::nullopt;
      if(std::string(state)=="minimum")feedback.confirmed=5;
      if(std::string(state)=="error-muted") {feedback.phase="error";feedback.confirmed=0;}
      if(std::string(state)=="applying-unknown") {feedback.phase="applying";feedback.confirmed=std::nullopt;}
      feedback.deadline=monotonicMs()+10000;
      ui.frame(locale.tr("screen.library"),"", "Etc/UTC");
      ui.volumeOverlay(feedback,locale);ui.save(fs::path(argv[2])/(std::string(code)+"-"+state+".png"));
      std::vector<Uint32> pixels;
      for(int y=panel["y"];y<panel["y"].get<int>()+panel["height"].get<int>();++y) {
        auto *row=reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(ui.surface->pixels)+y*ui.surface->pitch);
        int x=panel["x"],width=panel["width"];pixels.insert(pixels.end(),row+x,row+x+width);
      }
      auto name=std::string(state),language=std::string(code);
      if(name=="confirmed" || name=="muted")stable[language+name]=pixels;
      if(name=="applying")assert(pixels==stable.at(language+"confirmed"));
      if(name=="applying-muted")assert(pixels==stable.at(language+"muted"));
      if(name=="error" || name=="unknown")stable[language+name]=pixels;
      if(name=="error-muted")assert(pixels==stable.at(language+"error"));
      if(name=="applying-unknown")assert(pixels==stable.at(language+"unknown"));
      for(const auto *key:{"volume.error","volume.unknown","volume.unavailable"})
        assert(ui.measure(locale.tr(key),spec["state_word"]["size"],spec["state_word"]["weight"])<=spec["state_word"]["max_width"].get<int>());
    }
  }
  VolumeFeedback feedback;feedback.confirmed=50;feedback.phase="confirmed";feedback.deadline=monotonicMs()+10000;
  ui.frame("Library","", "Etc/UTC");ui.notice("Saving state", "save");
  std::vector<Uint32> before(720*64);
  for(int y=76;y<140;++y) {
    auto *row=reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(ui.surface->pixels)+y*ui.surface->pitch);
    std::copy(row,row+720,before.begin()+(y-76)*720);
  }
  ui.volumeOverlay(feedback,locale,true);ui.save(fs::path(argv[2])/"notice-and-volume.png");
  for(int y=76;y<140;++y) {
    auto *row=reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(ui.surface->pixels)+y*ui.surface->pitch);
    assert(std::equal(row,row+720,before.begin()+(y-76)*720));
  }
  std::cout<<"PASS: EN/RU compact icon/bar feedback, minimum/maximum/mute, honest error/unknown/unavailable, stable pending pixels and preserved system notice.\n";
}
