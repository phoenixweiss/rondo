// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "app.hpp"
#include <cassert>
#include <iostream>
using namespace rondo;
int main(int argc, char **argv) {
  assert(argc == 3);
  fs::path root = argv[2];
  fs::create_directories(root);
  for (const auto &language : {"en", "ru"}) {
    Options options;
    options.assets = argv[1]; options.state = root / language; options.output = root / language / "renders"; options.headless = true;
    fs::create_directories(options.output);
    fs::create_directories(options.state);
    auto config = Json{{"schema", 1}, {"locale", language}, {"timezone", "Europe/Moscow"}};
    auto bytes = config.dump(); atomicWrite(options.state / "config.json", {bytes.begin(), bytes.end()});
    App app(options);
    const auto configBefore = readBytes(options.state / "config.json");
    assert(app.countryLabel("RU") == (std::string(language) == "ru" ? "Россия" : "Russia"));
    assert(app.zoneLabel("America/Argentina/Buenos_Aires") == (std::string(language) == "ru" ? "Буэнос-Айрес" : "Buenos Aires"));
    for (const auto &zone : app.zones.zones) {
      if (zone.id == "Etc/UTC") continue;
      assert(app.timezoneNames[language]["countries"].contains(zone.country));
      assert(app.timezoneNames[language]["cities"].contains(zone.id));
    }
    app.action("MENU"); for(int i=0;i<3;++i) { app.action("DOWN"); }
    app.action("A");
    for(int i=0;i<3;++i) { app.action("DOWN"); }
    app.action("A");
    auto countries = app.regions(); assert(countries.front() == "UTC");
    assert(countries[app.row] == "RU");
    app.capture("countries");
    app.action("RIGHT"); assert(countries[app.row] != "RU");
    app.openTimezones(); app.action("A"); assert(app.screen == Screen::Zones);
    auto cities = app.locations(); assert(cities[app.row].id == "Europe/Moscow");
    assert(cities.size() > 10);
    assert(std::is_sorted(cities.begin(), cities.end(), [&](const auto &a, const auto &b) {
      return app.timezoneSortKey(app.zoneLabel(a.id)) < app.timezoneSortKey(app.zoneLabel(b.id));
    }));
    app.capture("russia"); app.action("X"); app.capture("info"); app.action("B");
    app.action("R1"); app.capture("page"); app.action("B"); app.action("B");
    assert(readBytes(options.state / "config.json") == configBefore);
    app.openTimezones(); app.region = "AR"; app.screen = Screen::Zones; app.row = 0; app.scroll = 0;
    app.capture("argentina");
    app.openTimezones(); app.region = "NP"; app.screen = Screen::Zones; app.row = 0;
    const auto nepal = app.locations().front().id;
    assert(app.zones.offset(nepal, 1767225600) == "UTC+05:45");
    app.capture("fractional");
    app.requestSettings = [](Json) { return false; };
    app.action("A"); assert(app.screen == Screen::Zones && !app.notice.empty());
    assert(app.store.data["timezone"] == "Europe/Moscow");
    assert(readBytes(options.state / "config.json") == configBefore); app.capture("write-failure");
    app.requestSettings = {}; app.action("A");
    assert(app.screen == Screen::Clock && app.row == 3 && app.store.data["timezone"] == nepal);
    app.capture("applied");
    app.openTimezones(); assert(app.regions()[app.row] == "NP");
    app.row = 0; app.action("A"); assert(app.store.data["timezone"] == "Etc/UTC");
    assert(app.zones.offset("Europe/London", 1767225600) == "UTC+00:00");
    assert(app.zones.offset("Europe/London", 1782864000) == "UTC+01:00");
    app.zones = Timezones(root / "missing-tzdata");
    app.action("A");
    assert(app.screen == Screen::Clock && app.row == 3 && app.notice == app.locale.tr("timezone.unavailable"));
    app.capture("missing-data");
  }
  Options options; options.assets = argv[1]; options.state = root / "restart"; options.headless = true;
  Store saved(root / "ru"); assert(saved.data["timezone"] == "Etc/UTC");
  std::cout << "PASS: complete EN/RU labels, localized ordering, country/current selection, letter/page navigation, cancellation, persistence, failed writes, nested zones and DST/fractional offsets.\n";
}
