// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include <SDL.h>
#include <vector>
#include <cstdint>
namespace rondo {
// Scale a reusable native-resolution texture on the display renderer.
class GameFrame {
  SDL_Renderer *renderer;
  SDL_Texture *texture=nullptr;
  int width=0,height=0;
  SDL_Rect target{};
  SDL_Color background{};
public:
  explicit GameFrame(SDL_Renderer *r):renderer(r){}
  ~GameFrame(){SDL_DestroyTexture(texture);}
  GameFrame(const GameFrame &)=delete;
  GameFrame &operator=(const GameFrame &)=delete;
  bool draw(const std::vector<uint32_t> &pixels,int w,int h,const SDL_Rect &target,SDL_Color background) {
    if(w<=0||h<=0||pixels.size()<size_t(w)*h)return false;
    if(!texture||w!=width||h!=height) {
      SDL_DestroyTexture(texture);
      texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);
      width=w;height=h;
      if(!texture)return false;
      SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_NONE);
      SDL_SetTextureScaleMode(texture,SDL_ScaleModeNearest);
    }
    if(SDL_UpdateTexture(texture,nullptr,pixels.data(),w*4))return false;
    this->target=target;this->background=background;
    return true;
  }
  bool redraw() {
    if(!texture)return false;
    SDL_SetRenderDrawColor(renderer,background.r,background.g,background.b,255);
    if(SDL_RenderClear(renderer))return false;
    return SDL_RenderCopy(renderer,texture,nullptr,&target)==0;
  }
};
}
