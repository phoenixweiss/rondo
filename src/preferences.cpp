// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "preferences.hpp"
#include "screenshots.hpp"
#include "libretro.h"
#include <algorithm>
#include <cctype>
namespace rondo {
std::string themeId(std::string id) {
  if(id=="slate")id="nord";if(id=="moss")id="pine";
  static const std::set<std::string> ids{"rondo","nord","pine","paper","rose","sand","scarlet"};
  return ids.count(id)?id:"rondo";
}
std::string coreId(const std::string &s) {
  static const std::map<std::string,std::string> ids{{"NES","fceumm"},{"GB","gambatte"},{"GBC","gambatte"},{"GBA","gpsp"},{"SNES","supafaust"},{"MD","picodrive"},{"PSX","pcsx_rearmed"}};
  auto it=ids.find(s);return it==ids.end()?"":it->second;
}
bool CoreEntry::available() const {if(!present){std::error_code e;present=!path.empty()&&fs::is_regular_file(path,e);}return *present;}
bool CoreEntry::enabled(const Json &data) const {
  if(!available())return false;
  auto values=data.value("enabled_cores",Json::object());
  return values.contains(id)&&values[id].is_boolean()?values[id].get<bool>():!experimental;
}
std::string physicalButton(int id) {
  static const std::array<const char *,16> names{"B","Y","SELECT","START","UP","DOWN","LEFT","RIGHT","A","X","L1","R1","L2","R2","L3","R3"};
  return id<0?"—":id<16?names[id]:"?";
}
std::vector<std::pair<int,std::string>> coreControls(const std::string &id) {
  std::vector<std::pair<int,std::string>> out{{4,"UP"},{5,"DOWN"},{6,"LEFT"},{7,"RIGHT"},{8,"A"},{0,"B"}};
  if(id=="picodrive") {out[4].second="C";out[5].second="B";out.push_back({1,"A"});out.push_back({10,"X"});out.push_back({9,"Y"});out.push_back({11,"Z"});}
  else if(id=="supafaust"||id=="pcsx_rearmed") {out.push_back({9,id=="pcsx_rearmed"?"Triangle":"X"});out.push_back({1,id=="pcsx_rearmed"?"Square":"Y"});if(id=="pcsx_rearmed"){out[4].second="Circle";out[5].second="Cross";}}
  if(id=="gpsp"||id=="supafaust"||id=="pcsx_rearmed") {out.push_back({10,"L1"});out.push_back({11,"R1"});}
  if(id=="pcsx_rearmed")for(int i=12;i<16;++i)out.push_back({i,physicalButton(i)});
  out.push_back({3,"START"});if(id!="picodrive")out.push_back({2,"SELECT"});else out.push_back({2,"MODE"});
  return out;
}
int mappedSource(const Json &data,const std::string &id,int target) {
  const auto maps=data.find("input_mapping");if(maps==data.end()||!maps->is_object())return target;
  const auto core=maps->find(id);if(core==maps->end()||!core->is_object())return target;
  auto value=core->find(std::to_string(target));return value!=core->end()&&value->is_number_integer()?std::clamp(value->get<int>(),-1,15):target;
}
uint16_t mappedButtons(const Json &data,const std::string &system,uint16_t held) {
  uint16_t result=0;auto id=coreId(system);
  for(int target=0;target<16;++target){int source=mappedSource(data,id,target);if(source>=0&&(held&(1u<<source)))result|=uint16_t(1u<<target);}
  return result;
}
int gamepadColor(const Json &data,const std::string &key) {
  const auto colors=data.find("gamepad_colors");
  if(key.empty() || colors==data.end())return -1;
  const auto id=colors->value(key,"");
  for(size_t i=0;i<GamepadSwatches.size();++i)
    if(GamepadSwatches[i].id==id)return int(i);
  return -1;
}
int chooseGamepadColor(const std::string &key,const std::array<bool,GamepadSwatches.size()> &used) {
  unsigned candidate=0;
  for(char digit:key)
    candidate=(16*candidate+(digit<='9'?digit-'0':digit-'a'+10))%GamepadSwatches.size();
  auto color=candidate;
  while(used[color]) {
    color=(color+1)%GamepadSwatches.size();
    if(color==candidate)break;
  }
  return int(color);
}
static bool gamepadKeyValid(const std::string &key) {
  return key.size()==64 && key.find_first_not_of("0123456789abcdef")==std::string::npos;
}
int gamepadPlayer(const Json &data,const std::string &key) {
  const auto players=data.find("gamepad_players");
  if(key.empty() || players==data.end())return AutomaticGamepadPlayer;
  const int assigned=players->value(key,AutomaticGamepadPlayer);
  return assigned<=GamepadPlayerCount?assigned:AutomaticGamepadPlayer;
}
std::string bluetoothGamepadKey(std::string address) {
  for(auto &c:address)c=char(std::toupper(static_cast<unsigned char>(c)));
  address.insert(0,"bluetooth:");
  return hashBytes({address.begin(),address.end()});
}
void validatePreferences(const Json &data) {
  // Development builds allowed ten saved roles. Keep their configurations
  // readable and resolve obsolete roles as Auto.
  constexpr int legacyPlayerLimit=10;
  if(auto players=data.find("gamepad_players");players!=data.end()) {
    if(!players->is_object())throw std::runtime_error("Invalid gamepad assignments");
    for(const auto &entry:players->items()) {
      if(!gamepadKeyValid(entry.key()) ||
         !entry.value().is_number_integer() || entry.value()<0 || entry.value()>legacyPlayerLimit)
        throw std::runtime_error("Invalid gamepad assignment");
    }
  }
  if(auto colors=data.find("gamepad_colors");colors!=data.end()) {
    if(!colors->is_object())throw std::runtime_error("Invalid gamepad colors");
    for(const auto &entry:colors->items()) {
      if(!gamepadKeyValid(entry.key()) || !entry.value().is_string() || gamepadColor(data,entry.key())<0)
        throw std::runtime_error("Invalid gamepad color");
    }
  }
  if(data.contains("screenshot_sound")&&!data["screenshot_sound"].is_boolean())throw std::runtime_error("Invalid screenshot sound setting");
  if(data.contains("screenshot_shortcut")) {
    const auto &v=data["screenshot_shortcut"];
    if(v!="sticks"&&v!="triggers"&&v!="select_r1"&&v!="custom"&&v!="off")throw std::runtime_error("Invalid screenshot shortcut");
  }
  if(data.contains("screenshot_custom")&&(!data["screenshot_custom"].is_number_integer()||data["screenshot_custom"].get<int64_t>()>65535||data["screenshot_custom"].get<int64_t>()<0||!validScreenshotChord(data["screenshot_custom"].get<int>())))throw std::runtime_error("Invalid screenshot combination");
  for(const auto &entry:std::map<std::string,std::set<std::string>>{{"library_grouping",{"unified","systems"}},{"game_view",{"list","tiles"}},{"system_tiles",{"grid","large","rail"}}})
    if(data.contains(entry.first)&&(!data[entry.first].is_string()||!entry.second.count(data[entry.first].get<std::string>())))throw std::runtime_error("Invalid library preference");
  if(data.contains("theme_id")&&!data["theme_id"].is_string())throw std::runtime_error("Invalid theme");
  if(data.contains("enabled_cores")) {if(!data["enabled_cores"].is_object())throw std::runtime_error("Invalid core selection");for(const auto &v:data["enabled_cores"].items())if(!v.value().is_boolean())throw std::runtime_error("Invalid core switch");}
  if(data.contains("input_mapping")) {
    if(!data["input_mapping"].is_object())throw std::runtime_error("Invalid input mappings");
    for(const auto &c:data["input_mapping"].items()) {
      if(!c.value().is_object())throw std::runtime_error("Invalid core mapping");
      for(const auto &v:c.value().items()) {
        if(v.key().empty()||v.key().size()>2||v.key().find_first_not_of("0123456789")!=std::string::npos||std::stoi(v.key())>15||!v.value().is_number_integer()||v.value().get<int>() < -1||v.value().get<int>()>15)throw std::runtime_error("Invalid button mapping");
      }
    }
  }
  if(data.contains("wizard")) {
    const auto &w=data["wizard"];if(!w.is_object()||(w.value("schema",0)!=1&&w.value("schema",0)!=2)||!w.contains("step")||!w["step"].is_number_integer()||w["step"].get<int>()<0||w["step"].get<int>()>(w.value("schema",0)==2?8:7)||!w.contains("completed")||!w["completed"].is_boolean()||!w.contains("deferred")||!w["deferred"].is_boolean())throw std::runtime_error("Invalid setup progress");
  }
}
} // namespace rondo
