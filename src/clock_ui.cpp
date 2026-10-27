// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
namespace rondo {
void App::clockRun(const std::string &task, std::function<ClockState()> action) {
  if (clockJob.valid()) {
    if (clockTask == "poll" && !clockQueued) {
      clockQueuedTask = task;
      clockQueued = std::move(action);
    }
    return;
  }
  clockTask = task;
  clockJob = std::async(std::launch::async, std::move(action));
}
void App::clockTick() {
  if (!clockDevice || !clockState.available)
    return;
  if (clockJob.valid()) {
    if (clockJob.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
      return;
    auto task = clockTask;
    const bool ack = task == "toggle" && clockToggle.has_value();
    const bool value = clockToggle.value_or(false);
    if (task == "toggle")
      clockToggle.reset();
    try {
      clockState = clockJob.get();
      if (task == "set") {
        clockLocalUpdated = clockState.manualUtc > 0;
        if (!clockLocalUpdated)
          clockError = "clock_readback";
        else if (!clockState.rtcSaved)
          clockError = "rtc";
        if (clockLocalUpdated) {
          screen = Screen::Clock;
          row = 2;
          if(!navigation.empty()&&navigation.back().route==Screen::Clock)navigationBack();
        }
      }
      if (ack) {
        bool old = store.data.value("ntp_auto", true);
        try {
          auto next = store.data;
          next["ntp_auto"] = value;
          persist(next);
        } catch (...) {
          auto d = clockDevice;
          clockRun("rollback", [d, old] { return old ? d->restart() : d->enable(false); });
          clockError = "save";
        }
      }
      if (clockState.synchronized && clockState.syncUtc > 0) {
        if (clockLastSync != clockState.syncUtc) {
          auto text = Json{{"utc", clockState.syncUtc}}.dump();
          try {
            atomicWrite(options.state / "Time/last-sync.json", {text.begin(), text.end()});
            clockLastSync = clockState.syncUtc;
          } catch (...) {
            clockError = "save";
          }
        }
        if (clockError == "timeout" || clockError == "service" || clockError == "readback")
          clockError.clear();
        clockJustUpdated = true;
        clockDeadline = 0;
        if (clockManual) {
          clockManual = false;
          clockDeadline = 0;
          if (!store.data.value("ntp_auto", true)) {
            auto d = clockDevice;
            clockRun("finish", [d] { return d->enable(false); });
          }
        }
      }
      if (task != "poll")
        std::cerr << "[clock] op=" << task << " enabled=" << clockState.enabled
                  << " synchronized=" << clockState.synchronized << "\n";
    } catch (const ClockError &e) {
      clockError = e.what();
      clockReadback = true;
      if (task == "manual") {
        clockManual = false;
        clockDeadline = 0;
      }
      std::cerr << "[clock] op=" << task << " error=" << clockError << "\n";
    } catch (...) {
      clockError = "save";
      clockReadback = true;
      if (task == "manual") {
        clockManual = false;
        clockDeadline = 0;
      }
      std::cerr << "[clock] op=" << task << " error=save\n";
    }
    if (clockQueued && !sleeping && !clockJob.valid()) {
      auto action = std::move(clockQueued);
      clockQueued = {};
      auto task = clockQueuedTask;
      clockQueuedTask.clear();
      clockRun(task, std::move(action));
    }
    clockPollAt = monotonicMs() + 1000;
  }
  if (clockJob.valid() || sleeping || monotonicMs() < clockPollAt)
    return;
  clockPollAt = monotonicMs() + 1000;
  auto d = clockDevice;
  bool automatic = store.data.value("ntp_auto", true);
  bool online = wifi.enabled && !wifi.address.empty();
  if (clockDeadline && monotonicMs() >= clockDeadline) {
    clockManual = false;
    clockDeadline = 0;
    clockError = "timeout";
    if (!automatic) {
      clockRun("finish", [d] { return d->enable(false); });
      return;
    }
  }
  if (clockReadback) {
    clockReadback = false;
    clockRun("recover", [d] { return d->status(); });
  } else if (!clockManual && clockState.enabled != automatic)
    clockRun("reconcile", [d, automatic] { return automatic ? d->restart() : d->enable(false); });
  else if (automatic && online && !clockNetworkWasOnline) {
    clockDeadline = monotonicMs() + 90000;
    clockRun("network", [d] { return d->restart(); });
  } else
    clockRun("poll", [d] { return d->status(); });
  clockNetworkWasOnline = online;
}
void App::clockSleep() {
  clockQueued = {};
  clockQueuedTask.clear();
  clockToggle.reset();
  while (clockJob.valid()) {
    clockJob.wait();
    clockTick();
  }
  if (clockManual && !store.data.value("ntp_auto", true))
    try {
      clockState = clockDevice->enable(false);
    } catch (...) {
      clockError = "service";
    }
  clockManual = false;
  clockDeadline = 0;
  clockNetworkWasOnline = false;
  clockPollAt = 0;
}
std::string App::clockPhase() const {
  if (!clockState.available)
    return "unavailable";
  if (clockBusy())
    return "applying";
  if (clockLocalUpdated && !clockState.enabled)
    return "local";
  if (clockManual)
    return "syncing";
  if (clockState.synchronized)
    return "synced";
  if (clockJustUpdated && !clockState.enabled)
    return "updated";
  if (!clockState.enabled)
    return "off";
  return wifi.enabled && !wifi.address.empty() ? "syncing" : "waiting";
}
void App::clockAction(const std::string &action) {
  if (screen == Screen::ClockEdit) {
    if (clockBusy()) return;
    if (action == "B") {
      screen = Screen::Clock; row = 2; clockError.clear(); return;
    }
    if (action == "LEFT") row = std::max(0, row - 1);
    if (action == "RIGHT") row = std::min(4, row + 1);
    if (action == "UP" || action == "DOWN") {
      int *fields[] = {&clockFields.day, &clockFields.month, &clockFields.year,
                      &clockFields.hour, &clockFields.minute};
      int low[] = {1,1,2020,0,0}, high[] = {clockDays(clockFields.year,clockFields.month),12,2099,23,59};
      int &value = *fields[row];
      value += action == "UP" ? 1 : -1;
      if (value < low[row]) value = high[row];
      if (value > high[row]) value = low[row];
      clockFields.day = std::min(clockFields.day, clockDays(clockFields.year,clockFields.month));
      clockError.clear();
    }
    if (action != "A") return;
    try {
      zones.applyHost(store.data.value("timezone", "Etc/UTC"));
      auto utc = clockUtc(clockFields);
      // A failed settings write must not change the clock or the time service.
      auto next = store.data;
      next["ntp_auto"] = false;
      persist(next);
      clockError.clear(); notice.clear();
      clockLocalUpdated = false; clockJustUpdated = false;
      clockManual = false; clockDeadline = 0; clockToggle.reset();
      auto d = clockDevice;
      clockRun("set", [d, utc] { return d->setTime(utc); });
    } catch (const ClockError &e) { clockError = e.what(); }
      catch (...) { clockError = "save"; }
    return;
  }
  if (action == "B") {
    returnToSettings();
    return;
  }
  if (action == "UP")
    row = std::max(0, row - 1);
  if (action == "DOWN")
    row = std::min(3, row + 1);
  if (action == "A" && row == 3) {
    if (zones.zones.empty()) notice = locale.tr("timezone.unavailable");
    else openTimezones();
    return;
  }
  if (!clockState.available || clockBusy())
    return;
  auto d = clockDevice;
  if (row == 0 && (action == "A" || action == "LEFT" || action == "RIGHT")) {
    bool old = store.data.value("ntp_auto", true), wanted = action == "LEFT"    ? false
                                                            : action == "RIGHT" ? true
                                                                                : !old;
    if (wanted == old)
      return;
    notice.clear();
    clockError.clear();
    clockJustUpdated = false;
    clockLocalUpdated = false;
    clockManual = false;
    clockDeadline = 0;
    clockToggle = wanted;
    clockRun("toggle", [d, wanted] { return wanted ? d->restart() : d->enable(false); });
  } else if (row == 1 && action == "A") {
    if (!wifi.enabled || wifi.address.empty()) {
      clockError = "network";
      return;
    }
    notice.clear();
    clockError.clear();
    clockJustUpdated = false;
    clockLocalUpdated = false;
    clockManual = true;
    clockDeadline = monotonicMs() + 90000;
    clockRun("manual", [d] { return d->restart(); });
  } else if (row == 2 && action == "A") {
    zones.applyHost(store.data.value("timezone", "Etc/UTC"));
    time_t now = time(nullptr);
    tm local{};
    localtime_r(&now, &local);
    clockFields = {std::clamp(local.tm_year + 1900, 2020, 2099), local.tm_mon + 1,
                   local.tm_mday, local.tm_hour, local.tm_min};
    clockFields.day = std::min(clockFields.day, clockDays(clockFields.year, clockFields.month));
    screen = Screen::ClockEdit;
    row = 0;
    clockError.clear(); notice.clear();
  }
}
void App::clockDraw() {
  auto tr = [&](const std::string &key) { return locale.tr(key); };
  if (screen == Screen::ClockEdit) {
    ui->frame(tr("clock.edit"), zoneLabel(store.data.value("timezone", "Etc/UTC")),
              store.data.value("timezone", "Etc/UTC"));
    const int values[] = {clockFields.day,clockFields.month,clockFields.year,clockFields.hour,clockFields.minute};
    std::vector<std::string> parts;
    int width=0;
    for (int i=0;i<5;++i) {
      std::ostringstream text;
      text << std::setw(i==2?4:2) << std::setfill('0') << values[i];
      parts.push_back(text.str());
      width += ui->measure(parts.back(),46,700);
      if(i<4) width += ui->measure(i<2?".":i==2?"   ":":",46,700);
    }
    int x=(720-width)/2;
    for(int i=0;i<5;++i) {
      int w=ui->measure(parts[i],46,700);
      if(row==i) {
        ui->rect(x-7,207,w+14,68,ui->focus,9);
        ui->rect(x,272,w,3,ui->accent,1);
      }
      ui->text(parts[i],x,214,46,700,clockBusy()?ui->secondary:ui->cream);
      x+=w;
      if(i<4) {
        std::string separator=i<2?".":i==2?"   ":":";
        ui->text(separator,x,214,46,700,ui->secondary);
        x+=ui->measure(separator,46,700);
      }
    }
    ui->hints({{"LEFT/RIGHT",tr("clock.field")},{"UP/DOWN",tr("footer.value")},
               {"A",tr("action.apply")},{"B",tr("action.cancel")},{"X",tr("footer.info")}});
    if (!clockError.empty()) ui->notice(tr("clock.error."+clockError), "clock");
    return;
  }
  ui->frame(tr("clock.title"), tr("clock.phase." + clockPhase()),
            store.data.value("timezone", "Etc/UTC"));
  ui->binaryRow(tr("clock.auto"), 128, row == 0, store.data.value("ntp_auto", true), locale,
                clockState.available);
  ui->row(tr("clock.now"), 182, row == 1, 672, "",
          clockState.available && !clockBusy() && wifi.enabled && !wifi.address.empty());
  ui->row(tr("clock.edit"),236,row==2,672,"",clockState.available&&!clockBusy(),52,"chevron-right");
  ui->row(tr("setting.timezone"),290,row==3,672,zoneLabel(store.data.value("timezone","Etc/UTC")),true,52,"chevron-right");
  ui->text(zones.valid(store.data.value("timezone","Etc/UTC")) ? slotDate(time(nullptr)) : tr("timezone.unavailable"),32,365,27,700,ui->cream,650);
  std::vector<std::pair<std::string,std::string>> hints;
  if (row == 0) hints.push_back({"LEFT/RIGHT",tr("footer.value")});
  hints.push_back({"A",tr(row==0?"footer.toggle":row==1?"clock.now_short":"action.change")});
  hints.push_back({"B",tr("action.back")});
  hints.push_back({"X",tr("footer.info")});
  ui->hints(hints);
  if (!clockError.empty())
    ui->notice(tr("clock.error." + clockError), "clock");
}
} // namespace rondo
