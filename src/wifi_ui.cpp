// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <algorithm>
#include <iostream>
namespace rondo {
App::~App() {
  if(statistics)try {statistics->flush(monotonicMs(),false);}catch(...) {}
  if(soundTest)SDL_CloseAudioDevice(soundTest);
  if(motor)motor->stop();
  volumeTick(true);
  try {wakeScreen();}catch(const std::exception &error){std::cerr<<error.what()<<"\n";}
  btSleep();
  batterySample("stop");
  if(clockJob.valid())clockJob.wait();
  if(clockState.available && clockDevice && !store.data.value("ntp_auto",true))try{clockDevice->enable(false);}catch(...){}
  clearSecret(wifiPassword);
  if (wifiDevice)
    wifiDevice->cancelled = true;
  if (wifiJob.valid())
    wifiJob.wait();
}
void App::wifiRun(const std::string &task, std::function<WifiState()> action) {
  if (wifiJob.valid()) {
    if (wifiTask == "poll" && !wifiQueued) {
      wifiQueuedTask = task;
      wifiQueued = std::move(action);
    }
    return;
  }
  wifiTask = task;
  wifiDevice->cancelled = false;
  if (task == "toggle" || task == "connect" || task == "scan" || task == "disconnect" ||
      task == "forget") {
    notice.clear();
    wifi.error.clear();
  }
  wifiJob = std::async(std::launch::async, [action = std::move(action)] { return action(); });
}
void App::wifiTick() {
  if (!wifiDevice)
    return;
  if (wifiJob.valid()) {
    if (wifiJob.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
      return;
    auto task = wifiTask;
    const bool toggleAcknowledged = task == "toggle" && wifiToggle.has_value();
    const bool toggleValue = wifiToggle.value_or(false);
    if (task == "toggle")
      wifiToggle.reset();
    try {
      auto previous = wifi.error;
      auto phase = wifi.phase;
      wifi = wifiJob.get();
      if ((task == "poll" || task == "rollback" || task == "recover") && wifi.error.empty()) {
        const bool transient = previous == "transport" || previous == "timeout" ||
                               previous == "unavailable" || previous == "initializing";
        if (!(transient && wifi.ready && (wifi.phase == "connected" || wifi.phase == "off")))
          wifi.error = previous;
      }
      if (toggleAcknowledged) {
        auto old = store.data.value("wifi_enabled", false);
        try {
          auto next = store.data;
          next["wifi_enabled"] = toggleValue;
          persist(next);
        } catch (...) {
          auto adapter = wifiDevice;
          wifiRun("rollback", [adapter, old] { return adapter->enable(old); });
          wifi.error = "save";
        }
      }
      if (task != "poll" || phase != wifi.phase || previous != wifi.error)
        std::cerr << "[wifi] op=" << task << " phase=" << wifi.phase << " error=" << wifi.error
                  << "\n";
    } catch (const WifiError &e) {
      wifi.error = e.what();
      wifiReadback = task != "recover";
      if (wifi.error == "cancelled")
        wifi.error.clear();
      std::cerr << "[wifi] op=" << task << " error=" << wifi.error << "\n";
    } catch (...) {
      wifi.error = "unavailable";
      wifiReadback = task != "recover";
      std::cerr << "[wifi] op=" << task << " error=unavailable\n";
    }
    if (wifiQueued && !sleeping && !wifiJob.valid()) {
      auto next = std::move(wifiQueued);
      wifiQueued = {};
      auto task = wifiQueuedTask;
      wifiQueuedTask.clear();
      wifiRun(task, std::move(next));
    }
    wifiPollAt = monotonicMs() + 1000;
  }
  if (wifiJob.valid() || sleeping || !wifi.available || monotonicMs() < wifiPollAt)
    return;
  wifiPollAt = monotonicMs() + 1000;
  auto adapter = wifiDevice;
  bool wanted = store.data.value("wifi_enabled", false);
  if (wifiReadback) {
    wifiReadback = false;
    wifiRun("recover", [adapter] { return adapter->status(); });
  } else if ((!wifi.ready || wifi.enabled != wanted) && wifi.error != "busy")
    wifiRun("reconcile", [adapter, wanted] { return adapter->enable(wanted); });
  else
    wifiRun("poll", [adapter] { return adapter->status(); });
}
std::vector<std::vector<std::string>> App::wifiKeys() const {
  std::vector<std::vector<std::string>> rows;
  const std::vector<std::string> letters = {"1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm@._"};
  const std::vector<std::string> symbols = {"!\"#$%&'()*", "+,-./:;<=>", "?@[\\]^_`{|",
                                            "}~01234567"};
  for (auto text : wifiSymbols ? symbols : letters) {
    std::vector<std::string> r;
    for (char c : text) {
      if (wifiUpper && !wifiSymbols)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      r.emplace_back(1, c);
    }
    rows.push_back(r);
  }
  rows.push_back({"space", "case", "delete", "join"});
  return rows;
}
void App::wifiText(const std::string &text) {
  if (screen != Screen::WifiPassword)
    return;
  for (unsigned char c : text)
    if (c >= 32 && c <= 126 && wifiPassword.size() < 64)
      wifiPassword += char(c);
  wifi.error.clear();
}
void App::wifiConnect() {
  if (wifiBusy())
    return;
  if (wifiSelected.savedId < 0 && wifiSelected.security == "psk" &&
      !wifiPasswordValid(wifiPassword)) {
    wifi.error = "password_format";
    return;
  }
  auto adapter = wifiDevice;
  auto network = wifiSelected;
  auto secret = wifiPassword;
  clearSecret(wifiPassword);
  SDL_StopTextInput();
  screen = Screen::Wifi;
  row = 0;
  wifi.error.clear();
  wifiRun("connect", [adapter, network, secret = std::move(secret)]() mutable {
    try {
      auto result = adapter->connect(network, secret);
      clearSecret(secret);
      return result;
    } catch (...) {
      clearSecret(secret);
      throw;
    }
  });
}
void App::wifiAction(const std::string &action) {
  auto adapter = wifiDevice;
  if (screen == Screen::WifiPassword) {
    if (action == "B") {
      clearSecret(wifiPassword);
      SDL_StopTextInput();
      screen = Screen::WifiNetwork;
      row = 0;
      wifi.error.clear();
      return;
    }
    if (action == "X" || action == "WIFI_DELETE") {
      if (!wifiPassword.empty())
        wifiPassword.pop_back();
      wifi.error.clear();
      return;
    }
    if (action == "Y") {
      wifiSymbols = !wifiSymbols;
      return;
    }
    if (action == "WIFI_SUBMIT") {
      wifiConnect();
      return;
    }
    auto keys = wifiKeys();
    if (action == "UP")
      wifiKeyRow = std::max(0, wifiKeyRow - 1);
    if (action == "DOWN")
      wifiKeyRow = std::min(4, wifiKeyRow + 1);
    wifiKeyColumn = std::min(wifiKeyColumn, int(keys[wifiKeyRow].size()) - 1);
    if (action == "LEFT")
      wifiKeyColumn = std::max(0, wifiKeyColumn - 1);
    if (action == "RIGHT")
      wifiKeyColumn = std::min(int(keys[wifiKeyRow].size()) - 1, wifiKeyColumn + 1);
    if (action == "A") {
      const auto key = keys[wifiKeyRow][wifiKeyColumn];
      if (key == "case")
        wifiUpper = !wifiUpper;
      else if (key == "delete") {
        if (!wifiPassword.empty())
          wifiPassword.pop_back();
      } else if (key == "join")
        wifiConnect();
      else
        wifiText(key == "space" ? " " : key);
    }
    return;
  }
  if (action == "B") {
    if (screen == Screen::WifiNetwork) {
      screen = Screen::WifiNetworks;
      row = wifiListFocus;
    } else if (screen == Screen::WifiNetworks) {
      screen = Screen::Wifi;
      row = 1;
    } else {
      screen = Screen::SettingsSection;
      category = 5;
      row = 0;
    }
    wifi.error.clear();
    return;
  }
  const bool connected = !wifi.connectedSsid.empty() && !wifi.address.empty();
  const bool connecting = wifi.phase == "connecting" || wifi.phase == "address";
  if (screen == Screen::WifiNetwork) {
    int count = wifiSelected.savedId >= 0 ? 3 : 2;
    if (action == "UP")
      row = std::max(0, row - 1);
    if (action == "DOWN")
      row = std::min(count - 1, row + 1);
    if (action == "A" && wifi.enabled && !wifiBusy()) {
      if (row == 1) {
        if (wifi.connectedSsid == wifiSelected.ssid || connecting)
          wifiRun("disconnect", [adapter] { return adapter->disconnect(); });
      } else if (row == 2) {
        auto network = wifiSelected;
        wifiRun("forget", [adapter, network] { return adapter->forget(network); });
        screen = Screen::WifiNetworks;
        row = 0;
      } else if (connected && wifi.connectedSsid == wifiSelected.ssid) {
        return;
      } else if (wifiSelected.savedId < 0 && wifiSelected.security == "psk") {
        screen = Screen::WifiPassword;
        clearSecret(wifiPassword);
        wifiKeyRow = 0;
        wifiKeyColumn = 0;
        wifiUpper = false;
        wifiSymbols = false;
        SDL_StartTextInput();
        wifi.error.clear();
      } else
        wifiConnect();
    }
    return;
  }
  if (screen == Screen::WifiNetworks) {
    int count = 1 + int(wifi.networks.size());
    if (action == "UP")
      row = std::max(0, row - 1);
    if (action == "DOWN")
      row = std::min(count - 1, row + 1);
    if (action == "A" && wifi.enabled && !wifiBusy()) {
      if (row == 0 && wifi.phase != "scanning")
        wifiRun("scan", [adapter] { return adapter->scan(); });
      else if (row > 0 && row - 1 < int(wifi.networks.size())) {
        wifiSelected = wifi.networks[row - 1];
        if (!wifiSelected.supported) {
          wifi.error = "unsupported";
          return;
        }
        wifiListFocus = row;
        screen = Screen::WifiNetwork;
        row = 0;
        wifi.error.clear();
      }
    }
    return;
  }
  const int count = connected || connecting ? 3 : 2;
  if (action == "UP")
    row = std::max(0, row - 1);
  if (action == "DOWN")
    row = std::min(count - 1, row + 1);
  if ((action == "A" || action == "LEFT" || action == "RIGHT") && row == 0 && wifi.available &&
      !wifiBusy()) {
    bool wanted = action == "LEFT" ? false : action == "RIGHT" ? true : !wifi.enabled;
    if (wanted == wifi.enabled && wanted == store.data.value("wifi_enabled", false))
      return;
    wifiToggle = wanted;
    wifi.error.clear();
    wifiRun("toggle", [adapter, wanted] { return adapter->enable(wanted); });
  } else if (action == "A" && row == 1 && wifi.enabled && !wifiBusy()) {
    screen = Screen::WifiNetworks;
    row = 0;
    wifi.error.clear();
    if (wifi.phase != "scanning")
      wifiRun("scan", [adapter] { return adapter->scan(); });
  } else if (action == "A" && row == 2 && (connected || connecting) && !wifiBusy()) {
    wifiRun("disconnect", [adapter] { return adapter->disconnect(); });
  }
}

void App::wifiDraw() {
  const auto zone = store.data.value("timezone", "Etc/UTC");
  auto tr = [this](const std::string &key) { return locale.tr(key); };
  if (screen == Screen::WifiPassword) {
    ui->frame(tr("wifi.password"), wifiSelected.label, zone);
    std::string hidden(wifiPassword.size(), '*');
    ui->row(hidden.empty() ? tr("wifi.password_empty") : hidden, 128, false, 672, "", true, 48);
    auto keys = wifiKeys();
    for (int y = 0; y < 5; ++y) {
      int count = keys[y].size(), cell = 672 / count;
      for (int x = 0; x < count; ++x) {
        bool focused = y == wifiKeyRow && x == wifiKeyColumn;
        int left = 24 + x * cell;
        ui->rect(left, 190 + y * 43, cell - 5, 38, focused ? ui->focus : ui->bg, 5);
        const auto text = keys[y][x].size() > 1 ? tr("wifi.key." + keys[y][x]) : keys[y][x];
        ui->text(text, left + (cell - 5 - ui->measure(text, 21)) / 2, 196 + y * 43, 21);
      }
    }
    if (ui->keyboardHints)
      ui->hints({{"Enter", tr("wifi.connect")},
                 {"Esc", tr("action.cancel")},
                 {"Backspace", tr("wifi.delete")}});
    else
      ui->hints({{"A", tr("wifi.choose")},
                 {"B", tr("action.cancel")},
                 {"X", tr("wifi.delete")},
                 {"Y", tr("wifi.symbols")}});
  } else {
    const auto phase =
        (wifiJob.valid() && wifiTask == "toggle") || wifiQueuedTask == "toggle" ? "applying"
        : wifiJob.valid() && (wifiTask == "startup" || wifiTask == "reconcile") ? "initializing"
                                                                                : wifi.phase;
    const bool connected = !wifi.connectedSsid.empty() && !wifi.address.empty();
    const bool connecting = wifi.phase == "connecting" || wifi.phase == "address";
    if (screen == Screen::WifiNetwork) {
      const bool current = connected && wifiSelected.ssid == wifi.connectedSsid;
      ui->frame(wifiSelected.label, tr("wifi.networks"), zone);
      ui->row(tr(current ? "wifi.phase.connected" : "wifi.connect"), 128, row == 0, 672, "",
              wifi.enabled && !current && !wifiBusy());
      ui->row(tr("wifi.disconnect"), 182, row == 1, 672, "",
              wifi.enabled && (current || connecting) && !wifiBusy());
      if (wifiSelected.savedId >= 0)
        ui->row(tr("wifi.forget"), 236, row == 2, 672, "", wifi.enabled && !wifiBusy());
    } else if (screen == Screen::WifiNetworks) {
      ui->frame(tr("wifi.networks"), tr("wifi.phase." + phase), zone);
      int count = 1 + int(wifi.networks.size());
      row = std::clamp(row, 0, count - 1);
      int first = std::max(0, row - 3);
      for (int i = first; i < std::min(count, first + 5); ++i) {
        int y = 128 + (i - first) * 54;
        if (i == 0)
          ui->row(tr("wifi.scan"), y, row == 0, 672, "",
                  wifi.enabled && !wifiBusy() && wifi.phase != "scanning");
        else {
          const auto &network = wifi.networks[i - 1];
          auto value = network.ssid == wifi.connectedSsid && connected ? tr("wifi.phase.connected")
                       : network.savedId >= 0                          ? tr("wifi.known")
                       : network.security == "open"                    ? tr("wifi.open")
                       : network.security == "psk"                     ? "WPA/WPA2"
                                                                       : tr("wifi.unsupported");
          ui->row(network.label, y, row == i, 672, value, network.supported && wifi.enabled);
        }
      }
      if (count == 1 && wifi.phase != "scanning")
        ui->text(tr("wifi.empty"), 32, 240, 23, 500, ui->secondary, 650);
    } else {
      ui->frame("Wi-Fi", tr("wifi.phase." + phase), zone);
      row = std::clamp(row, 0, connected || connecting ? 2 : 1);
      ui->binaryRow("Wi-Fi", 128, row == 0,
                    wifi.ready ? std::optional<bool>(wifi.enabled) : std::nullopt, locale,
                    wifi.available);
      ui->row(tr("wifi.networks"), 182, row == 1, 672, "›", wifi.enabled && !wifiBusy());
      if (connected || connecting)
        ui->row(tr("wifi.disconnect"), 236, row == 2, 672, "", !wifiBusy());
      if (wifi.enabled && !wifi.connectedSsid.empty()) {
        ui->text(tr("wifi.network"), 32, 308, 19, 500, ui->secondary);
        ui->text(wifiLabel(wifi.connectedSsid), 170, 304, 23, 500, ui->cream, 510);
      }
      if (wifi.enabled && !wifi.address.empty()) {
        ui->text("IP", 32, 353, 19, 500, ui->secondary);
        ui->text(wifi.address, 170, 349, 23);
      }
    }
    ui->hints({{"A", tr("wifi.choose")}, {"B", tr("action.back")}, {"X", tr("footer.info")}});
  }
  if (!wifi.error.empty())
    ui->notice(tr("wifi.error." + wifi.error), "wifi");
}
} // namespace rondo
