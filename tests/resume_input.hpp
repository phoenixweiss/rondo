// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "app.hpp"
#include <cassert>
inline void verifyResumeInputFence(rondo::App &app) {
  using namespace rondo;
  assert(app.core&&app.screen==Screen::Running&&!app.core->paused);
  app.action("MENU");assert(app.screen==Screen::GameMenu&&app.core->paused);
  app.row=int(GameMenuItem::Resume);app.action("A");
  assert(app.screen==Screen::Running&&!app.core->paused&&app.gameInputBlocked);
  const uint16_t a=1u<<RETRO_DEVICE_ID_JOYPAD_A;
  app.gameInput(a,{});assert(app.core->buttons==0&&app.gameInputBlocked);
  app.gameInput(0,{});assert(app.core->buttons==0&&!app.gameInputBlocked);
  app.gameInput(a,{});assert(app.core->buttons!=0);
  app.gameInput(0,{});assert(app.core->buttons==0);
}
