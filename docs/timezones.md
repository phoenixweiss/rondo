# Time-zone selection

Settings → Date & time → Time zone opens a country list at the saved time zone's country. UTC is pinned first. Choose a country, then a city; the check marks the saved choice. Countries and cities are ordered by their localized display labels, with case/diacritic folding in the checked-in sort keys.

Up/Down selects; Left/Right jumps to another initial letter; L1/R1 pages (Q/E on a keyboard). Enter/A applies and Esc/B returns without saving. Info shows the technical IANA identifier. Choosing a city returns to Date & time, where its localized name is shown.

Country/city names come from pinned Unicode CLDR data in assets/Timezones. The runtime is offline and has no additional localization-library dependency. tools/build_timezone_names.py regenerates the labels and records exact input/output hashes; retain the accompanying Unicode License v3. New zones absent from that data fall back to a readable IANA city name and country code until the catalog is updated. Incomplete third-party UI locales use the English name set.

The installed system zone.tab determines which zones are selectable; CLDR labels do not replace timezone rules. Offsets come from the installed tzdata for the current date, including fractional offsets and daylight saving. Missing tzdata produces a localized notice. Unknown saved IDs are not silently rewritten merely by browsing.

Configuration still stores the IANA ID, and save timestamps remain UTC. Applying changes Rondo's process-local display zone, not macOS system settings, the hardware clock or NTP. A write failure restores the previous process zone and leaves the previous durable setting authoritative.

Native tests cover full name coverage for the host zone.tab, localized ordering, initial focus, navigation, cancellation, persistence and failed writes, nested zones, unavailable tzdata and DST/fractional offsets. SDL captures use the actual 720x480 UI in a quiet native test process. The ARM64 suite also passed in isolated BaseOS. The owner accepted the installed picker on RG34XXSP, including persistence after restart; this is bounded device feedback, not a claim of testing every zone.
