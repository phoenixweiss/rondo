// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <cmath>
namespace rondo {
// Shared by the interactive event loop and physical-input integration checks.
void App::sampleInput() {
  auto &a=*this;
  bool recording=a.screen==Screen::ScreenshotCapture;
  bool testing=a.screen==Screen::InputTest||recording;
  bool gameFrame=a.core&&(a.screen==Screen::Running||a.screen==Screen::GameMenu);
  if(a.stop||a.sleeping||a.screenBlank)return;
  if(!testing&&!gameFrame) {
    // The shell shortcut needs only the stick clicks, not a second full pad poll.
    bool left=a.ui->handheldKeys->pressed("L3"),right=a.ui->handheldKeys->pressed("R3");
    for(const auto &entry:a.ui->controllers)if(SDL_GameControllerGetAttached(entry.second)) {
      left|=SDL_GameControllerGetButton(entry.second,SDL_CONTROLLER_BUTTON_LEFTSTICK);
      right|=SDL_GameControllerGetButton(entry.second,SDL_CONTROLLER_BUTTON_RIGHTSTICK);
    }
    bool shot=false;a.interfaceScreenshotKeys.sample((left?0x4000:0)|(right?0x8000:0),0xc000,monotonicMs(),shot);
    if(shot)a.takeScreenshot();return;
  }
  const auto *k = SDL_GetKeyboardState(nullptr);
  uint16_t mask = 0;
  auto put = [&](int id, bool on) {
    if (on)
      mask |= uint16_t(1 << id);
  };
  put(RETRO_DEVICE_ID_JOYPAD_A, k[SDL_SCANCODE_Z]);
  put(RETRO_DEVICE_ID_JOYPAD_B, k[SDL_SCANCODE_X]);
  put(RETRO_DEVICE_ID_JOYPAD_START, k[SDL_SCANCODE_RETURN]);
  put(RETRO_DEVICE_ID_JOYPAD_SELECT, k[SDL_SCANCODE_RSHIFT]);
  put(RETRO_DEVICE_ID_JOYPAD_UP, k[SDL_SCANCODE_UP]);
  put(RETRO_DEVICE_ID_JOYPAD_DOWN, k[SDL_SCANCODE_DOWN]);
  put(RETRO_DEVICE_ID_JOYPAD_LEFT, k[SDL_SCANCODE_LEFT]);
  put(RETRO_DEVICE_ID_JOYPAD_RIGHT, k[SDL_SCANCODE_RIGHT]);
  put(RETRO_DEVICE_ID_JOYPAD_X, k[SDL_SCANCODE_S]);
  put(RETRO_DEVICE_ID_JOYPAD_Y, k[SDL_SCANCODE_A]);
  put(RETRO_DEVICE_ID_JOYPAD_L, k[SDL_SCANCODE_Q]);
  put(RETRO_DEVICE_ID_JOYPAD_R, k[SDL_SCANCODE_E]);
  {
    put(RETRO_DEVICE_ID_JOYPAD_L2,k[SDL_SCANCODE_W]);
    put(RETRO_DEVICE_ID_JOYPAD_R2,k[SDL_SCANCODE_R]);
    put(RETRO_DEVICE_ID_JOYPAD_L3,k[SDL_SCANCODE_T]);
    put(RETRO_DEVICE_ID_JOYPAD_R3,k[SDL_SCANCODE_Y]);
  }
  auto &raw = *a.ui->handheldKeys;
  put(RETRO_DEVICE_ID_JOYPAD_A, raw.pressed("A"));
  put(RETRO_DEVICE_ID_JOYPAD_B, raw.pressed("B"));
  put(RETRO_DEVICE_ID_JOYPAD_START, raw.pressed("START"));
  put(RETRO_DEVICE_ID_JOYPAD_SELECT, raw.pressed("SELECT"));
  put(RETRO_DEVICE_ID_JOYPAD_UP, raw.pressed("UP"));
  put(RETRO_DEVICE_ID_JOYPAD_DOWN, raw.pressed("DOWN"));
  put(RETRO_DEVICE_ID_JOYPAD_LEFT, raw.pressed("LEFT"));
  put(RETRO_DEVICE_ID_JOYPAD_RIGHT, raw.pressed("RIGHT"));
  put(RETRO_DEVICE_ID_JOYPAD_X, raw.pressed("X"));
  put(RETRO_DEVICE_ID_JOYPAD_Y, raw.pressed("Y"));
  put(RETRO_DEVICE_ID_JOYPAD_L, raw.pressed("L1"));
  put(RETRO_DEVICE_ID_JOYPAD_R, raw.pressed("R1"));
  put(RETRO_DEVICE_ID_JOYPAD_L2,raw.pressed("L2"));
  put(RETRO_DEVICE_ID_JOYPAD_R2,raw.pressed("R2"));
  put(RETRO_DEVICE_ID_JOYPAD_L3,raw.pressed("L3"));
  put(RETRO_DEVICE_ID_JOYPAD_R3,raw.pressed("R3"));
  mask |= a.ui->controllerButtons(!testing&&!a.psxDualshock() && a.store.data.value("left_stick_dpad",true));
  if(recording){a.screenshotRecord(mask);return;}
  if(testing) {
    std::set<std::string> extras;
    if(raw.pressed("MENU")||k[SDL_SCANCODE_TAB])extras.insert("MENU");
    for(auto &entry:a.ui->controllers)
      if(SDL_GameControllerGetButton(entry.second,SDL_CONTROLLER_BUTTON_GUIDE))extras.insert("MENU");
    if(a.ui->volumeKeys->pressed(true)||k[SDL_SCANCODE_EQUALS])extras.insert("VOL+");
    if(a.ui->volumeKeys->pressed(false)||k[SDL_SCANCODE_MINUS])extras.insert("VOL-");
    if(a.ui->powerKeys->pressed())extras.insert("POWER");
    a.testInput(mask,a.ui->controllerAxes(),extras);return;
  }
  a.gameInput(mask,a.ui->controllerAxes());
}

void App::openInputTest(bool fromCores) {
  testFromCores=fromCores;testButtons=0;testAxes={};testExtras.clear();
  testChordHeld=false;testInputArmed=false;testFeedback.clear();
  screen=Screen::InputTest;notice.clear();
}
void App::testInput(uint16_t buttons,const std::array<int16_t,4> &axes,const std::set<std::string> &extras) {
  if(screen!=Screen::InputTest)return;
  testButtons=buttons;testAxes=axes;testExtras=extras;
  if(buttons||!extras.empty())activity();
  for(auto axis:axes)if(axis){activity();break;}
  if(!testInputArmed) {if(!buttons&&extras.empty())testInputArmed=true;return;}
  const uint16_t exit=(1<<RETRO_DEVICE_ID_JOYPAD_START)|(1<<RETRO_DEVICE_ID_JOYPAD_SELECT);
  if((buttons&exit)==exit) {
    if(!navigation.empty()){if(motor)motor->stop();mappingTest=false;navigationBack();testButtons=0;testAxes={};testExtras.clear();return;}
    if(motor)motor->stop();screen=testFromCores?Screen::CoreSettings:Screen::SettingsSection;
    category=4;row=3;if(mappingTest){mappingTest=false;screen=Screen::Mapping;row=mappingReturnRow;}testButtons=0;testAxes={};testExtras.clear();return;
  }
  const uint16_t shoulders=(1<<RETRO_DEVICE_ID_JOYPAD_L)|(1<<RETRO_DEVICE_ID_JOYPAD_R)|
      (1<<RETRO_DEVICE_ID_JOYPAD_L2)|(1<<RETRO_DEVICE_ID_JOYPAD_R2);
  bool chord=(buttons&shoulders)==shoulders;
  if(chord&&!testChordHeld) {
    testFeedbackUntil=monotonicMs()+2500;
    if(store.data.value("rumble_enabled",true)&&motor&&motor->available()) {
      motor->update(65535,0);testFeedback=locale.tr("input.motor_pulse");
    } else testFeedback=locale.tr("rumble.unavailable");
  }
  testChordHeld=chord;
}
void App::drawInputTest(const std::string &zone) {
  auto tr=[&](const char *key){return locale.tr(key);};
  ui->frame(tr("input.test"),"",zone);
  const SDL_Color white=ui->currentTheme!="rondo"&&(ui->accent.r*299+ui->accent.g*587+ui->accent.b*114>150000)?ui->bg:SDL_Color{255,255,255,255};
  auto key=[&](const std::string &label,int x,int y,int w,int id,bool round=false){
    bool on=id>=0?(testButtons&(1u<<id))!=0:testExtras.count(label)>0;
    auto color=on?ui->accent:ui->separator;
    ui->keycap(x,y,w,round,color,round?w:32,on);
    std::string shown=label;
    if(ui->keyboardHints) {
      static const std::map<std::string,std::string> keys{{"A","Z"},{"B","X"},{"X","S"},{"Y","A"},{"L1","Q"},{"L2","W"},{"R1","E"},{"R2","R"},{"SELECT","RShift"},{"START","Enter"},{"MENU","Tab"},{"VOL-","-"},{"VOL+","+"}};
      auto found=keys.find(label);if(found!=keys.end())shown=found->second;
    }
    ui->keyLabel(shown,x,y,w,18,on?white:ui->cream,round?w:32);
  };
  // The arrangement follows the controls: shoulders, D-pad/face buttons,
  // then both sticks. No pass/fail is inferred from a signal being received.
  key("L2",64,91,66,RETRO_DEVICE_ID_JOYPAD_L2);
  key("L1",144,91,66,RETRO_DEVICE_ID_JOYPAD_L);
  key("R1",510,91,66,RETRO_DEVICE_ID_JOYPAD_R);
  key("R2",590,91,66,RETRO_DEVICE_ID_JOYPAD_R2);
  auto direction=[&](const char *name,int x,int y,int id){
    bool on=testButtons&(1<<id);ui->keycap(x,y,40,false,on?ui->accent:ui->separator,32,on);
    int dx=0,dy=0;std::string direction=name;
    if(direction=="arrow-up")dy=-1;if(direction=="arrow-down")dy=1;
    if(direction=="arrow-left")dx=-1;if(direction=="arrow-right")dx=1;
    auto c=on?white:ui->cream;int cx=x+20,cy=y+16;
    auto stroke=[&](int x1,int y1,int x2,int y2){
      for(int ox=-1;ox<=1;++ox)for(int oy=-1;oy<=1;++oy)ui->line(x1+ox,y1+oy,x2+ox,y2+oy,c);
    };
    stroke(cx-dx*7,cy-dy*7,cx+dx*7,cy+dy*7);
    stroke(cx+dx*7,cy+dy*7,cx+dx*2+dy*5,cy+dy*2-dx*5);
    stroke(cx+dx*7,cy+dy*7,cx+dx*2-dy*5,cy+dy*2+dx*5);
  };
  direction("arrow-up",118,148,RETRO_DEVICE_ID_JOYPAD_UP);
  direction("arrow-left",68,194,RETRO_DEVICE_ID_JOYPAD_LEFT);
  direction("arrow-right",168,194,RETRO_DEVICE_ID_JOYPAD_RIGHT);
  direction("arrow-down",118,240,RETRO_DEVICE_ID_JOYPAD_DOWN);
  key("X",562,148,40,RETRO_DEVICE_ID_JOYPAD_X,true);
  key("Y",512,194,40,RETRO_DEVICE_ID_JOYPAD_Y,true);
  key("A",612,194,40,RETRO_DEVICE_ID_JOYPAD_A,true);
  key("B",562,240,40,RETRO_DEVICE_ID_JOYPAD_B,true);
  key("SELECT",250,158,100,RETRO_DEVICE_ID_JOYPAD_SELECT);
  key("START",370,158,100,RETRO_DEVICE_ID_JOYPAD_START);
  key("MENU",310,206,100,-1);
  key("VOL-",250,254,100,-1);key("VOL+",370,254,100,-1);
  key("POWER",310,302,100,-1);
  for(int stick=0;stick<2;++stick) {
    int x=192+stick*336,y=324;bool on=testButtons&(1<<(stick?RETRO_DEVICE_ID_JOYPAD_R3:RETRO_DEVICE_ID_JOYPAD_L3));
    ui->circle(x-30,y-30,60,on?ui->accent:ui->separator,on);
    ui->line(x-24,y,x+24,y,on?white:ui->separator);ui->line(x,y-24,x,y+24,on?white:ui->separator);
    int dx=int(testAxes[stick*2])*21/32768,dy=int(testAxes[stick*2+1])*21/32768;
    ui->rect(x+dx-5,y+dy-5,10,10,ui->cream,5);
    ui->text(ui->keyboardHints?(stick?"Y":"T"):(stick?"R3":"L3"),x-66,y-11,17,500,on?ui->accent:ui->cream);
    auto percent=[](int16_t axis){int value=int(std::lround(100.0*axis/(axis<0?32768.0:32767.0)));return (value>0?"+":"")+std::to_string(value)+"%";};
    auto coordinates=percent(testAxes[stick*2])+" / "+percent(testAxes[stick*2+1]);
    ui->text(coordinates,x-ui->measure(coordinates,17)/2,360,17,500,ui->secondary);
  }
  auto message=monotonicMs()<testFeedbackUntil?testFeedback:(ui->keyboardHints?std::string("Q + W + E + R: ")+tr("setting.rumble"):tr("input.motor_combo"));
  ui->text(message,(720-ui->measure(message,20))/2,396,20,500,ui->cream,672);
  ui->hints({{"MENU",tr("action.back")}});
}
}
