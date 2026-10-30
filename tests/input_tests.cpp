// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
using namespace rondo;
int main(int argc,char **argv){
  assert(argc==3);Options o;o.assets=argv[1];o.state=fs::path(argv[2])/"state";o.output=argv[2];o.headless=true;
  std::atomic<int> pulses{0};std::atomic<bool> motorOn{false};App a(o);auto before=a.store.data;
  a.motor=std::make_shared<Rumble>([&](bool on){motorOn=on;if(on)++pulses;return true;});
  a.screen=Screen::SettingsSection;a.category=4;a.row=3;a.action("A");assert(a.screen==Screen::InputTest);
  a.testInput(0,{},{});assert(a.testInputArmed);
  for(auto action:{"A","B","X","Y","UP","DOWN","VOLUME_UP","VOLUME_DOWN","SLEEP"})a.action(action);
  assert(a.screen==Screen::InputTest&&a.store.data==before&&!a.infoOpen);
  const uint16_t shoulders=(1<<RETRO_DEVICE_ID_JOYPAD_L)|(1<<RETRO_DEVICE_ID_JOYPAD_R)|(1<<RETRO_DEVICE_ID_JOYPAD_L2)|(1<<RETRO_DEVICE_ID_JOYPAD_R2);
  a.testInput(shoulders,{},{});
  for(int i=0;i<400&&pulses==0;++i)SDL_Delay(5);
  assert(pulses==1);
  for(int i=0;i<100;++i)a.testInput(shoulders,{},{});
  // Wait for the watchdog acknowledgement, not a scheduling-dependent sleep.
  // A second On can be coalesced while the worker has not yet applied Off.
  for(int i=0;i<400&&motorOn;++i)SDL_Delay(5);
  assert(!motorOn&&pulses==1); // No repeating vibration while held.
  a.testInput(0,{},{});a.testInput(shoulders,{},{});
  for(int i=0;i<400&&pulses<2;++i)SDL_Delay(5);assert(pulses==2);
  const uint16_t exit=(1<<RETRO_DEVICE_ID_JOYPAD_START)|(1<<RETRO_DEVICE_ID_JOYPAD_SELECT);
  a.testInput(exit,{},{});assert(a.screen==Screen::SettingsSection&&a.row==3&&a.category==4);
  a.openInputTest(true);a.testInput(exit,{},{});assert(a.screen==Screen::InputTest); // Entry-held chord is ignored.
  a.testInput(0,{},{});a.testInput(exit,{},{});assert(a.screen==Screen::CoreSettings&&a.row==3);
  a.openInputTest(true);a.action("MENU");assert(a.screen==Screen::CoreSettings);
  a.openInputTest(true);a.testInput(0,{},{});a.store.data["rumble_enabled"]=false;
  a.testInput(shoulders,{},{});assert(!a.testFeedback.empty());SDL_Delay(20);assert(pulses==2);
  a.store.data=before;
  for(auto language:{"en","ru"}) {
    a.openInputTest(true);a.locale.select(o.assets,language);a.testInput(0,{},{});a.testFeedbackUntil=0;
    a.capture(std::string("input-neutral-")+language);
    a.testInput(uint16_t((1<<RETRO_DEVICE_ID_JOYPAD_A)|(1<<RETRO_DEVICE_ID_JOYPAD_LEFT)|(1<<RETRO_DEVICE_ID_JOYPAD_L3)),{-25000,16000,20000,-32000},{"MENU","VOL+","POWER"});
    a.capture(std::string("input-held-")+language);
    auto pixel=[&](int x,int y){return reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(a.ui->surface->pixels)+y*a.ui->surface->pitch)[x];};
    auto accent=SDL_MapRGBA(a.ui->surface->format,a.ui->accent.r,a.ui->accent.g,a.ui->accent.b,255);
    auto bg=SDL_MapRGBA(a.ui->surface->format,a.ui->bg.r,a.ui->bg.g,a.ui->bg.b,255);
    auto white=SDL_MapRGBA(a.ui->surface->format,255,255,255,255);
    assert(pixel(632,198)==accent); // Pressed A has a solid accent fill.
    int left=40,right=-1,top=40,bottom=-1,whitePixels=0;
    for(int y=0;y<40;++y)for(int x=0;x<40;++x){auto value=pixel(612+x,194+y);
      if(value!=bg){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
      if(value==white)++whitePixels;
    }
    assert(right-left==bottom-top&&right-left>=37&&whitePixels>10); // Circle, not a capsule; light lettering.
    assert(a.testAxes[3]==-32000&&a.testExtras.count("POWER"));
    a.ui->inputHints(true);a.capture(std::string("input-keyboard-")+language);a.ui->inputHints(false);
    a.testInput(shoulders,{},{});a.capture(std::string("input-motor-")+language);
    a.screen=Screen::CoreSettings;a.row=2;a.capture(std::string("rumble-settings-")+language);
  }
  a.screen=Screen::CoreSettings;a.row=2;a.action("LEFT");assert(!a.store.data.at("rumble_enabled").get<bool>());
  a.requestSettings=[](const Json &){return false;};a.action("RIGHT");assert(!a.store.data.at("rumble_enabled").get<bool>());a.requestSettings={};
  {Store restored(o.state);assert(!restored.data.at("rumble_enabled").get<bool>());}
  auto invalid=a.store.data;invalid["rumble_enabled"]="yes";bool rejected=false;try{a.store.commit(invalid);}catch(...){rejected=true;}assert(rejected);
  std::cout<<"PASS: input diagram, no action side effects, Start+Select exit, four-shoulder motor pulse/release, global rumble persistence and rejected writes.\n";
}
