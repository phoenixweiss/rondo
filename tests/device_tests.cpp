#include "app.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace rondo;
struct FakeDevice : DeviceControl {
  int level=60,sleeps=0;bool open=true,fail=false,wakeAvailable=true;
  std::vector<std::pair<bool,bool>> policies;
  std::function<void()> before;
  std::optional<int> brightness() override { return level; }
  void brightness(int n) override { level=n; }
  bool canSleep() const override { return true; }
  bool canWakeOnLid() const override { return wakeAvailable; }
  std::optional<bool> lidOpen() override { return open; }
  void sleep(bool wake, bool close) override { policies.emplace_back(wake,close);++sleeps;if(before)before();if(fail)throw std::runtime_error("Injected suspend failure");open=true; }
};
int main(int argc,char **argv) {
  assert(argc==5);Options o;o.assets=argv[1];o.state=argv[2];o.core=argv[3];o.headless=true;o.output=o.state/"renders";
  auto root=o.state;
  PowerInputState key;
  assert(key.event(1,116,0).empty());
  assert(key.event(1,116,1).empty());assert(key.event(1,116,2).empty());
  assert(key.event(1,116,0)=="SLEEP");assert(key.event(1,116,0).empty());
  key.event(1,116,1);key.event(0,3,0);assert(key.event(1,116,0).empty());
  {
    App a(o);auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);a.brightness=60;
    a.action("Y");a.action("R1");a.action("RIGHT");assert(d->level==65 && a.store.data["brightness"]==65);a.capture("brightness-inline");
    a.action("DOWN");a.action("LEFT");assert(a.row==1 && !a.store.data["show_battery_percent"].get<bool>() && d->level==65);
    a.capture("battery-percent-off");
    a.requestSettings=[](Json){return false;};a.action("RIGHT");assert(!a.store.data["show_battery_percent"].get<bool>() && d->level==65);
    a.requestSettings={};a.action("RIGHT");assert(a.store.data["show_battery_percent"].get<bool>());a.action("UP");
    a.requestSettings=[](Json){return false;};a.action("LEFT");assert(d->level==65 && a.brightness==65 && a.store.data["brightness"]==65);a.requestSettings={};
    a.action("R1");a.action("RIGHT");assert(a.screen==Screen::Settings && a.store.data["volume"]==85);
    a.action("A");assert(a.screen==Screen::Settings);a.capture("sound-inline");a.action("LEFT");assert(a.store.data["volume"]==80);
    a.action("R1");a.action("LEFT");assert(a.store.data["lid_sleep"]==false);
    d->open=false;a.lidCheckAt=0;a.ticks(0);assert(d->sleeps==0);
    a.action("RIGHT");d->open=true;a.lidCheckAt=0;a.ticks(0);d->open=false;a.lidCheckAt=0;a.ticks(0);assert(d->sleeps==1);
    a.action("SLEEP");assert(d->sleeps==2);
    // A supervised shell without SDL must not independently suspend its child.
    a.ui.reset();d->open=false;a.lastLidOpen=true;a.lidCheckAt=0;a.ticks(0);assert(d->sleeps==2);
  }
  {
    o.state=root/"lid-options";App a(o);auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);
    assert(a.store.data["lid_sleep"]==true && a.store.data["wake_on_lid_open"]==true);
    a.action("Y");for(int i=0;i<3;++i)a.action("R1");
    a.action("DOWN");assert(a.row==1);a.action("LEFT");assert(a.store.data["wake_on_lid_open"]==false && a.store.data["lid_sleep"]==true);
    a.requestSettings=[](Json){return false;};a.action("RIGHT");assert(a.store.data["wake_on_lid_open"]==false);a.requestSettings={};a.notice.clear();
    for(const auto *language:{"en","ru"}) { a.locale.select(o.assets,language);a.capture(std::string("lid-options-")+language);a.action("X");a.capture(std::string("lid-wake-info-")+language);a.action("B"); }
    for(bool close:{false,true})for(bool wake:{false,true}) {
      auto next=a.store.data;next["lid_sleep"]=close;next["wake_on_lid_open"]=wake;a.persist(next);
      int before=d->sleeps;d->open=false;a.lastLidOpen=true;a.lidCheckAt=0;a.ticks(0);
      assert(d->sleeps==before+int(close));
      a.action("SLEEP");assert(d->policies.back()==std::make_pair(wake,close));
    }
    Store restart(o.state);assert(restart.data["wake_on_lid_open"]==true && restart.data["lid_sleep"]==true);
    auto bad=Json{{"schema",1},{"wake_on_lid_open","true"}}.dump();
    atomicWrite(root/"invalid-lid/config.json",{bad.begin(),bad.end()});
    Store invalid(root/"invalid-lid");assert(!invalid.warning.empty() && invalid.data["wake_on_lid_open"].is_boolean());
    d->wakeAvailable=false;a.row=1;a.action("LEFT");assert(a.store.data["wake_on_lid_open"]==true);
    a.capture("lid-adapter-unavailable");a.action("SLEEP");assert(d->policies.back()==std::make_pair(false,true));
  }
  {
    o.state=root/"runner";Game game{"device-game","TEST","NES","Fixture",argv[4]};App a(o,true,game);
    auto fake=std::make_unique<FakeDevice>();auto *d=fake.get();a.device=std::move(fake);
    a.ticks(90);auto hash=a.core->stateHash();auto frames=a.core->frames;
    d->before=[&]{assert(a.core->paused && a.core->buttons==0);};
    a.core->buttons=1;a.action("SLEEP");assert(d->sleeps==1 && !a.core->paused && a.screen==Screen::Running);
    assert(a.core->frames==frames && a.core->stateHash()==hash);
    a.action("MENU");a.row=3;a.action("X");assert(!a.infoOpen); // game menu X has no hidden mutation.
    a.action("SLEEP");assert(a.core->paused && a.row==3 && a.screen==Screen::GameMenu);
    a.action("B");d->fail=true;a.action("SLEEP");assert(a.core->paused && a.screen==Screen::GameMenu && !a.notice.empty());
    d->fail=false;
    auto dir=a.store.gameDir(game);fs::rename(dir,dir.string()+".saved");std::ofstream(dir)<<"blocked";
    int count=d->sleeps;a.action("SLEEP");assert(d->sleeps==count && a.core->paused); // SRAM failure aborts sleep.
    fs::remove(dir);fs::rename(dir.string()+".saved",dir);
    a.core->save(0);a.screen=Screen::LoadSlots;a.row=0;a.capture("slot-preview");auto meta=readJson(dir/"slot-0.json");assert(meta.contains("preview"));
    auto &view=a.slotView(0);assert(view.compatible && view.pixels==a.core->frame && view.width==a.core->width && view.utc>0);
    meta.erase("preview");auto text=meta.dump();atomicWrite(dir/"slot-1.json",{text.begin(),text.end()});
    auto &old=a.slotView(1);assert(old.compatible && old.pixels.empty() && old.utc==view.utc);
    meta["core"]="wrong-core";text=meta.dump();atomicWrite(dir/"slot-2.json",{text.begin(),text.end()});assert(!a.slotView(2).compatible);a.row=1;a.capture("slot-legacy");a.row=2;a.capture("slot-incompatible");a.action("X");assert(a.infoOpen);a.capture("slots-info");
    // Updating time backwards must not make Continue choose the older slot.
    auto legacy=readJson(dir/"slot-0.json");legacy.erase("save_sequence");legacy["utc"]=static_cast<long long>(time(nullptr))+86400;
    auto encoded=legacy.dump();atomicWrite(dir/"slot-0.json",{encoded.begin(),encoded.end()});
    a.core->save(1);auto recent=readJson(dir/"slot-1.json");
    assert(recent["utc"]<legacy["utc"] && saveSequence(recent)>0 && a.latestResumeSlot()==1);
    a.core->save(0);assert(a.latestResumeSlot()==0 && saveSequence(readJson(dir/"slot-0.json"))>saveSequence(recent));
    Store reload(o.state);assert(reload.data.value("lid_sleep",true));
  }
  {
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);UI hidden(o.assets,true);
    Game game{"audio-wake","TEST","NES","Fixture",argv[4]};Core c(argv[3],game,root/"audio-wake",false);
    for(int i=0;i<60;++i)c.tick();
    auto hash=c.stateHash();auto audio=c.audioFrames;
    c.closeAudio();c.openAudio();assert(c.stateHash()==hash);
    c.tick();assert(c.audioFrames>audio);
  }
  std::cout<<"PASS: fresh Power release, independent lid defaults/four combinations/persistence/failure/unavailable fallback, lid policy, inline volume/brightness with rollback, active-process ownership, paused core continuity, suspend failure, SRAM failure abort, preview integrity and legacy states.\n";
}
