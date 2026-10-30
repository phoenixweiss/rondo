// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
#include <iostream>
#include <bitset>
namespace rondo {
static std::string combination(uint16_t mask) {
  std::string text;for(int i=0;i<16;++i)if(mask&(1u<<i)){if(!text.empty())text+=" + ";text+=physicalButton(i);}return text;
}
static const std::vector<std::string> shortcuts{"sticks","triggers","select_r1","custom","off"};
void App::takeScreenshot() {
  if(screen==Screen::ProgressBusy||screen==Screen::ProgressDone)return;
  if(screen==Screen::BackupBusy||screen==Screen::BackupRestart)return;
  if(stop||sleeping||screenBlank||screen==Screen::InputTest||screen==Screen::ScreenshotCapture||screen==Screen::SystemProgress||screen==Screen::SavingAutomatic)return;
  try {
    if(!screenshotWriter.busy()) {
      auto timestamp=screenshotTimestamp(zones,store.data.value("timezone","Etc/UTC"),time(nullptr));
      if(core&&(screen==Screen::Running||screen==Screen::GameMenu)) {
        if(screenshotWriter.submit(options.state/"Screenshots",game,core->frame,core->width,core->height,timestamp,game.system=="PSX"?core->aspectRatio:0))return;
      }else {
        // Render the current interface without the previous screenshot toast.
        auto feedback=screenshotMessageUntil;screenshotMessageUntil=0;
        draw();screenshotMessageUntil=feedback;
        std::vector<uint32_t> pixels(720*480);
        if(SDL_RenderReadPixels(ui->renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,pixels.data(),720*4)!=0)throw std::runtime_error(SDL_GetError());
        if(screenshotWriter.submitInterface(options.state/"Screenshots",pixels,720,480,timestamp))return;
      }
    }
    screenshotMessage=locale.tr("screenshot.busy");
  }catch(const std::exception &e){std::cerr<<"Screenshot: "<<e.what()<<"\n";screenshotMessage=locale.tr("screenshot.failed");}
  screenshotMessageUntil=monotonicMs()+3500;
}
void App::playScreenshotSound() {
  if(stop||sleeping||screen==Screen::SystemProgress||screen==Screen::SavingAutomatic||!store.data.value("screenshot_sound",true))return;
  if(core){core->playScreenshotClick();return;}
  if(previewVolume<=0||soundTest)return; // Do not interrupt the requested sound check.
  if(SDL_InitSubSystem(SDL_INIT_AUDIO))return;
  SDL_AudioSpec spec{};spec.freq=48000;spec.format=AUDIO_S16SYS;spec.channels=2;spec.samples=256;
  auto audio=SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0);if(!audio)return;
  auto samples=makeScreenshotClick(spec.freq,previewVolume);
  if(SDL_QueueAudio(audio,samples.data(),samples.size()*sizeof(int16_t))){SDL_CloseAudioDevice(audio);return;}
  soundTest=audio;soundTestUntil=monotonicMs()+100;SDL_PauseAudioDevice(audio,0);
}
void App::screenshotTick() {
  try {
    if(screenshotWriter.poll()){playScreenshotSound();screenshotMessage=locale.tr("screenshot.saved");screenshotMessageUntil=monotonicMs()+1800;}
  }catch(const std::exception &e){std::cerr<<"Screenshot: "<<e.what()<<"\n";screenshotMessage=locale.tr("screenshot.failed");screenshotMessageUntil=monotonicMs()+4500;}
}
void App::screenshotDraw() {
  if(monotonicMs()>=screenshotMessageUntil)return;
  // A short acknowledgement, not a modal dialog or a permanent percentage.
  int width=std::min(560,ui->measure(screenshotMessage,22)+56);
  int x=696-width;ui->rect(x,76,width,44,ui->focus,8);
  ui->icon("camera",x+12,86,24);ui->text(screenshotMessage,x+44,84,22,500,ui->cream,width-52);
}
void App::screenshotAction(const std::string &a) {
  if(a=="B"){screen=Screen::SettingsSection;category=0;row=4;return;}
  if(a=="UP")row=std::max(0,row-1);if(a=="DOWN")row=std::min(2,row+1);
  if(row==0&&(a=="LEFT"||a=="RIGHT"||a=="A")) {
    auto next=store.data;auto current=next.value("screenshot_shortcut","sticks");
    size_t i=std::find(shortcuts.begin(),shortcuts.end(),current)-shortcuts.begin();
    next["screenshot_shortcut"]=shortcuts[(i+(a=="LEFT"?shortcuts.size()-1:1))%shortcuts.size()];persist(next);screenshotKeys.reset();
  } else if(row==2&&(a=="A"||a=="LEFT"||a=="RIGHT")) {
    auto next=store.data;next["screenshot_sound"]=a=="LEFT"?false:a=="RIGHT"?true:!next.value("screenshot_sound",true);persist(next);
  } else if(row==1&&a=="A") {
    screenshotRecordingArmed=false;screenshotRecording=0;screen=Screen::ScreenshotCapture;notice.clear();
  }
}
void App::screenshotRecord(uint16_t held) {
  if(screen!=Screen::ScreenshotCapture)return;
  if(!screenshotRecordingArmed){if(!held)screenshotRecordingArmed=true;return;}
  if(held){if(std::bitset<16>(held).count()>=std::bitset<16>(screenshotRecording).count())screenshotRecording=held;return;}
  if(!screenshotRecording)return;
  if(!validScreenshotChord(screenshotRecording)){notice=locale.tr("screenshot.invalid");screenshotRecording=0;return;}
  try {
    auto next=store.data;next["screenshot_custom"]=screenshotRecording;next["screenshot_shortcut"]="custom";persist(next);
    screenshotKeys.reset();notice.clear();if(!navigation.empty())navigationBack();else {screen=Screen::ScreenshotSettings;row=1;}
  }catch(...){notice=locale.tr("error.operation");screenshotRecording=0;}
}
void App::screenshotSettingsDraw(const std::string &zone) {
  ui->frame(locale.tr("screenshot.title"),locale.tr("section.interface"),zone);
  if(screen==Screen::ScreenshotCapture) {
    ui->text(locale.tr("screenshot.record_prompt"),32,136,23,500,ui->cream,656);
    ui->text(locale.tr("screenshot.release"),32,180,23,400,ui->secondary,656);
    auto label=combination(screenshotRecording);ui->text(label.empty()?locale.tr("screenshot.waiting"):label,32,250,26,500,ui->cream,656);
    ui->hints({{"MENU",locale.tr("action.cancel")}});return;
  }
  auto mode=store.data.value("screenshot_shortcut","sticks");
  auto label=mode=="off"?locale.tr("screenshot.menu_only"):combination(screenshotChord(store.data));
  ui->valueRow(locale.tr("screenshot.shortcut"),128,row==0,672,label);
  ui->row(locale.tr("screenshot.custom"),182,row==1,672,"",true,52,"chevron-right");
  ui->binaryRow(locale.tr("screenshot.sound"),236,row==2,store.data.value("screenshot_sound",true),locale);
  ui->text(locale.tr(ui->keyboardHints?"screenshot.interface_keyboard":"screenshot.interface_pad"),32,302,22,400,ui->cream,656);
  ui->text(locale.tr("screenshot.folder"),32,346,22,400,ui->secondary,656);
  ui->hints({{"LEFT/RIGHT",locale.tr("footer.value")},{"A",locale.tr("action.select")},{"B",locale.tr("action.back")}});
}
} // namespace rondo
