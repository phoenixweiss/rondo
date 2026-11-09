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
  auto testHandheld=[&](uint16_t buttons,std::array<int16_t,4> axes,std::set<std::string> extras) {
    a.testInput({{App::TestHandheld,{buttons,axes,std::move(extras)}}});
  };
  a.action("MENU");assert(a.screen==Screen::Settings);
  for(int i=0;i<6;++i)a.action("DOWN");
  a.action("A");assert(a.screen==Screen::SettingsSection&&a.category==4);
  for(int i=0;i<3;++i)a.action("DOWN");
  a.action("A");assert(a.screen==Screen::InputTest);
  testHandheld(0,{},{});assert(a.testInputArmed);
  for(auto action:{"A","B","X","Y","UP","DOWN","VOLUME_UP","VOLUME_DOWN","SLEEP","MENU"})a.action(action);
  assert(a.screen==Screen::InputTest&&a.store.data==before&&!a.infoOpen);
  const uint16_t shoulders=(1<<RETRO_DEVICE_ID_JOYPAD_L)|(1<<RETRO_DEVICE_ID_JOYPAD_R)|(1<<RETRO_DEVICE_ID_JOYPAD_L2)|(1<<RETRO_DEVICE_ID_JOYPAD_R2);
  testHandheld(shoulders,{},{});
  for(int i=0;i<400&&pulses==0;++i)SDL_Delay(5);
  assert(pulses==1);
  for(int i=0;i<100;++i)testHandheld(shoulders,{},{});
  // Wait for the watchdog acknowledgement, not a scheduling-dependent sleep.
  // A second On can be coalesced while the worker has not yet applied Off.
  for(int i=0;i<400&&motorOn;++i)SDL_Delay(5);
  assert(!motorOn&&pulses==1); // No repeating vibration while held.
  testHandheld(0,{},{});testHandheld(shoulders,{},{});
  for(int i=0;i<400&&pulses<2;++i)SDL_Delay(5);assert(pulses==2);
  const uint16_t exit=(1<<RETRO_DEVICE_ID_JOYPAD_START)|(1<<RETRO_DEVICE_ID_JOYPAD_SELECT);
  testHandheld(exit,{},{});assert(a.screen==Screen::SettingsSection&&a.row==3&&a.category==4);
  a.action("B");assert(a.screen==Screen::Settings);
  a.action("B");assert(a.screen==Screen::Library||a.screen==Screen::Systems);
  a.openInputTest(true);testHandheld(exit,{},{});assert(a.screen==Screen::InputTest); // Entry-held chord is ignored.
  testHandheld(0,{},{});testHandheld(exit,{},{});assert(a.screen==Screen::CoreSettings&&a.row==3);
  a.openInputTest(true);testHandheld(0,{},{});a.action("MENU");
  testHandheld(0,{},{"MENU"});assert(a.screen==Screen::InputTest&&a.testExtras.count("MENU"));
  a.action("MENU");assert(a.screen==Screen::InputTest);testHandheld(0,{},{});
  testHandheld(exit,{},{});assert(a.screen==Screen::CoreSettings);
  a.openInputTest(true);testHandheld(0,{},{});a.store.data["rumble_enabled"]=false;
  testHandheld(shoulders,{},{});assert(!a.testFeedback.empty());SDL_Delay(20);assert(pulses==2);
  a.store.data=before;
  // A real SDL Guide event must remain observable, never become navigation.
  int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);assert(index>=0);
  auto *pad=SDL_JoystickOpen(index);assert(pad);char guid[64];
  SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(pad),guid,sizeof(guid));
  auto mapping=std::string(guid)+",Input test virtual pad,back:b4,guide:b5,start:b6,leftx:a0,lefty:a1,rightx:a2,righty:a3,";
  assert(SDL_GameControllerAddMapping(mapping.c_str())>=0);SDL_PumpEvents();a.ui->refreshControllers();
  a.openInputTest(true);a.sampleInput();assert(a.testInputArmed);
  SDL_JoystickSetVirtualButton(pad,5,1);SDL_GameControllerUpdate();a.sampleInput();a.action("MENU");
  assert(a.screen==Screen::InputTest&&a.testExtras.count("MENU")&&a.store.data==before);
  SDL_JoystickSetVirtualButton(pad,5,0);SDL_GameControllerUpdate();a.sampleInput();assert(a.testExtras.empty());
  SDL_JoystickSetVirtualButton(pad,4,1);SDL_JoystickSetVirtualButton(pad,6,1);SDL_GameControllerUpdate();a.sampleInput();
  assert(a.screen==Screen::CoreSettings);
  SDL_JoystickSetVirtualButton(pad,4,0);SDL_JoystickSetVirtualButton(pad,6,0);

  int externalPulses=0;
  SDL_VirtualJoystickDesc desc{};
  desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;
  desc.name="Input test virtual pad";desc.nbuttons=15;desc.naxes=6;desc.userdata=&externalPulses;
  desc.Rumble=[](void *userdata,Uint16 strong,Uint16 weak) {
    if(strong||weak)++*static_cast<int *>(userdata);
    return 0;
  };
  int second=SDL_JoystickAttachVirtualEx(&desc);assert(second>=0);
  auto *other=SDL_JoystickOpen(second);assert(other);
  for(auto *joystick:{pad,other}) {
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick),guid,sizeof(guid));
    auto shouldersMapping=std::string(guid)+",Input test virtual pad,leftshoulder:b0,rightshoulder:b1,lefttrigger:b2,righttrigger:b3,";
    assert(SDL_GameControllerAddMapping(shouldersMapping.c_str())>=0);
  }
  auto poll=[&]{SDL_GameControllerUpdate();a.sampleInput();};
  auto press=[](SDL_Joystick *joystick,std::initializer_list<int> buttons,int value) {
    for(int button:buttons)assert(SDL_JoystickSetVirtualButton(joystick,button,value)==0);
  };
  SDL_PumpEvents();a.ui->refreshControllers();
  a.openInputTest(true);poll();assert(a.testInputArmed);
  press(pad,{0,2},1);poll();assert(a.testSource==SDL_JoystickInstanceID(pad));
  press(other,{1,3},1);poll();
  assert(a.testSource==SDL_JoystickInstanceID(other)&&a.testButtons!=shoulders&&externalPulses==0);
  press(pad,{1,3},1);poll();
  assert(a.testSource==SDL_JoystickInstanceID(pad)&&a.testFeedback==a.locale.tr("input.motor_unsupported"));
  assert(pulses==2&&externalPulses==0);
  a.capture("input-external-no-vibration");
  press(other,{0,2},1);poll();poll();
  assert(a.testSource==SDL_JoystickInstanceID(other)&&externalPulses==1&&pulses==2);
  a.capture("input-external-vibration");
  SDL_JoystickClose(other);
  assert(SDL_JoystickDetachVirtual(second)==0);SDL_PumpEvents();a.ui->refreshControllers();poll();
  assert(!a.testSource&&a.testButtons==0);
  SDL_JoystickClose(pad);
  assert(SDL_JoystickDetachVirtual(index)==0);SDL_PumpEvents();a.ui->refreshControllers();
  for(auto language:{"en","ru"}) {
    a.openInputTest(true);a.locale.select(o.assets,language);testHandheld(0,{},{});a.testFeedbackUntil=0;
    a.capture(std::string("input-neutral-")+language);
    testHandheld(uint16_t((1<<RETRO_DEVICE_ID_JOYPAD_A)|(1<<RETRO_DEVICE_ID_JOYPAD_LEFT)|(1<<RETRO_DEVICE_ID_JOYPAD_L3)),{-25000,16000,20000,-32000},{"MENU","VOL+","POWER"});
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
    a.testInput({{App::TestKeyboard,{uint16_t(1<<RETRO_DEVICE_ID_JOYPAD_A),{},{}}}});
    a.capture(std::string("input-keyboard-")+language);
    testHandheld(shoulders,{},{});a.capture(std::string("input-motor-")+language);
    a.screen=Screen::CoreSettings;a.row=2;a.capture(std::string("rumble-settings-")+language);
  }
  a.screen=Screen::CoreSettings;a.row=2;a.action("LEFT");assert(!a.store.data.at("rumble_enabled").get<bool>());
  a.requestSettings=[](const Json &){return false;};a.action("RIGHT");assert(!a.store.data.at("rumble_enabled").get<bool>());a.requestSettings={};
  {Store restored(o.state);assert(!restored.data.at("rumble_enabled").get<bool>());}
  auto invalid=a.store.data;invalid["rumble_enabled"]="yes";bool rejected=false;try{a.store.commit(invalid);}catch(...){rejected=true;}assert(rejected);
  std::cout<<"PASS: input diagram, no action side effects, Start+Select exit, four-shoulder motor pulse/release, global rumble persistence and rejected writes.\n";
}
