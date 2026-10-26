// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "libretro.h"
#include <cstring>
static retro_environment_t environment;
static retro_rumble_interface rumble{};
extern "C" {
unsigned retro_api_version(){return RETRO_API_VERSION;}
void retro_set_environment(retro_environment_t fn){environment=fn;}
void retro_set_video_refresh(retro_video_refresh_t){}
void retro_set_audio_sample(retro_audio_sample_t){}
void retro_set_audio_sample_batch(retro_audio_sample_batch_t){}
void retro_set_input_poll(retro_input_poll_t){}
void retro_set_input_state(retro_input_state_t){}
void retro_init(){rumble={};environment(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE,&rumble);}
void retro_deinit(){}
void retro_get_system_info(retro_system_info *info){*info={"Rumble test","1","bin",false,false};}
void retro_get_system_av_info(retro_system_av_info *info){*info={{1,1,1,1,1.0f},{60.0,44100.0}};}
bool retro_load_game(const retro_game_info *){return true;}
void retro_unload_game(){}
void retro_run(){}
void retro_reset(){}
void retro_set_controller_port_device(unsigned,unsigned){}
size_t retro_serialize_size(){return 0;}
bool retro_serialize(void *,size_t){return false;}
bool retro_unserialize(const void *,size_t){return false;}
void *retro_get_memory_data(unsigned){return nullptr;}
size_t retro_get_memory_size(unsigned){return 0;}
bool rondo_test_rumble(unsigned port,unsigned effect,uint16_t strength){
  return rumble.set_rumble_state&&rumble.set_rumble_state(port,static_cast<retro_rumble_effect>(effect),strength);
}
}
