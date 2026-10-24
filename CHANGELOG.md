# Changelog

## Unreleased

## [0.9.0] - 2026-10-25

### Added

- PlayStation support through PCSX ReARMed: CHD discs, CHD-only M3U playlists, disc switching, a per-game memory card, save states and continuation.
- Bundled OpenBIOS, plus selectable user BIOS or HLE. Core settings include a picker for a specific BIOS file.
- Built-in RG34XXSP sticks and L2/R2/L3/R3, Digital/DualShock selection and optional left-stick D-pad mapping for digital systems.
- Corresponding PCSX ReARMed, OpenBIOS and dependency sources and licenses in the H700 package.

### Fixed

- Correct PSX aspect ratio for frames such as 512×240; fit oversized interlaced frames within the screen.

### Performance

- Compact binary states and separate previews for all cores. Slot lists and previews no longer read the entire state payload.
- Skip unchanged SRAM/RTC writes and unnecessary state serialization on exit.
- Lighter event logs, periodic detailed telemetry and bounded action history.

### Save-state format

- Legacy JSON slots and automatic states are no longer loaded. Existing files are left untouched; SRAM, RTC and PSX memory-card files keep their formats and paths.

### Verification

- Verified on RG34XXSP with BaseOS 1.5.2: PSX startup, saving and loading, continuation, sleep and volume controls.
- Native checks, BaseOS 1.3/1.5.2 regression checks, state corruption/fallback tests, out-of-space preservation and source/license audits passed.

## [0.8.0] - 2026-10-22

- Add read-only Settings → System information in English and Russian: device profile and OS memory, installed Rondo/BaseOS/kernel/core revisions, physical cards and mounted partition space, and named handheld battery readings. X reveals full values and sources; Y refreshes through a single bounded helper process without hardware reads on the UI thread. Unknown, absent and failed readings remain explicit.

- Add Mega Drive / Genesis cartridge support through pinned PicoDrive, with `.gen`/`.md` routing, six-button controls, SRAM, slots, automatic continuation and the existing sleep policy. Cartridge play does not require a Sega BIOS. Sega CD, 32X and Master System are outside this integration.
- Ship PicoDrive and its required pinned dependencies in the regular H700 ZIP, with full source and component notices. PicoDrive retains its noncommercial terms; original Rondo code/artwork remain MIT. The complete package must not be described as MIT-only or GPL-only.
- Sort the library by title across systems, ignoring ASCII/Cyrillic case and sorting Russian Ё with Е. Keep stable duplicate ordering and preserve Recent launch order and game/save identities.

### Verification and known limitations

- The owner accepted Mega Drive gameplay, save states, continuation, sleep and System information on RG34XXSP / BaseOS 1.5.2 using candidate 27c4f81aaeac on 2026-10-22. This is bounded device acceptance, not a full compatibility or sustained-performance claim for every cartridge.
- All 264 owner-provided MD ROMs passed short automated native launch, state and process-restart continuation probes. The combined candidate passed native checks, isolated BaseOS 1.3.0 and 1.5.2 regression checks, and the corresponding-source/license audit. Owner ROMs and artwork are excluded from the public ZIP.
- System information separates manufacturer specifications from observed data. Battery readings remain vendor estimates, not a health measurement or calibration; unavailable values remain explicit.
- Bluetooth remains experimental: Micro D pairs/connects, but controller input does not work. Battery-gauge jumps, recurring FAT inconsistencies, wake-on-lid-open and broader performance optimization remain in the backlog.

## [0.7.1] - 2026-10-22

- Enable the RG34XXSP time adapter by BaseOS/H700 identity, its callback and executable time-service dependencies instead of an exact BaseOS version. Compatible system updates no longer hide network synchronization and manual date/time controls; other devices/OSes and missing services stay unavailable.
- Add an explicit BaseOS-version selector to isolated runtime checks, reporting the tested version separately from the unchanged 1.3.0 package baseline.

### Verification and known limitations

- The same candidate passed native macOS checks and complete isolated BaseOS 1.3.0 and 1.5.2 userspace/synthetic checks, including NTP, manual time, UTC RTC acknowledgement and failure handling. Regression checks cover platform rejection and version-independent detection.
- The owner accepted the BaseOS 1.5.2 update and candidate 847da83a5e4b on RG34XXSP after the requested boot, time and gameplay/sleep checks. BaseOS 1.5.2 is an upstream prerelease; this is bounded device acceptance, not a compatibility guarantee for every hardware variant or future service interface.
- Bluetooth remains experimental: Micro D pairs/connects, but controller input does not work. Battery-gauge jumps, recurring FAT inconsistencies, wake-on-lid-open and broader performance optimization remain in the backlog.

## [0.7.0] - 2026-10-22

- Repeat handheld volume changes while a button is held: one immediate 5% step, then a 350 ms delay and 100 ms repeats. Ignore duplicate/kernel repeats and cancel held state on release or lost input.

- Apply in-game volume without clearing queued audio or waiting for SD settings writes. Coalesce rapid gain changes and persist the final value through a background shell-authority request; finish pending changes before other settings, sleep or exit.

- Keep gameplay button presses out of shell action diagnostics. Collect battery/system diagnostics and append SD journals on a background worker with a bounded queue, including detailed logging. Drain pending records while paused before suspend and before clearing logs.

- Add experimental SNES / Super Famicom through pinned GPLv2 Supafaust, with SFC/SMC routing, all twelve joypad buttons, cartridge SRAM, manual/automatic save states, continuation and sleep policy. The owner accepted SNES play on RG34XXSP; sustained performance and full-game compatibility remain unmeasured.
- Preserve SNES display aspect ratio from core geometry and support centred integer scaling, PAL timing and copier-header SMC input. Keep previous systems and save identities unchanged.
- Include complete Supafaust source/notices and reproducible H700 build flags; use the existing condition-variable semaphore implementation for native macOS checks.

### Verification and known limitations

- All 304 owner-provided SNES ROMs passed automated native launch, save/load and process-restart continuation probes. Commercial ROMs and artwork are excluded from the public package; short probes do not establish full-game compatibility.
- The owner accepted SNES play and the gameplay-button/volume stutter fixes on RG34XXSP, most recently with candidate 228786831d10. The owner also accepted hold-to-repeat volume on the physical console with candidate 31da354d2018. Native timing/regression checks, H700/BaseOS checks and the package source/license audit passed.
- Bluetooth remains experimental: the tested Micro D pairs/connects, but controller input does not work. Battery-gauge jumps, wake-on-lid-open, recurring FAT inconsistencies and broader frame/audio pacing optimization remain in the backlog. The input-related fixes do not establish sustained performance across games.

## [0.6.0] - 2026-10-21

- Add GBA through pinned gpSP, with A/B/L/R/Start/Select input, cartridge SRAM, manual/automatic states, startup continuation and aspect/integer scaling. The owner accepted GBA play, saves, continuation and sleep on RG34XXSP.
- Use gpSP's bundled open BIOS replacement when no user BIOS is supplied; preserve its GPLv2 source and attribution. Select system-clock RTC, 32768 Hz audio and disabled link cable. H700 uses the ARM64 dynarec; macOS checks use the interpreter.
- Swap library shortcuts: Menu/Tab opens settings; Y/V opens search. In-game Menu/Tab continues to open the game menu. Show updated footer hints, including filtered/empty lists.
- Offer startup continuation only for an unfinished game session. Explicitly closing a game returns the next boot to the library while retaining recent history, manual slots and automatic saves. Older configurations without a session marker start in the library.

### Verification and known limitations

- All 115 owner-provided GBA ROMs passed automated native launch, save/load and process-restart continuation checks; commercial ROMs and artwork are excluded from the public package. The owner accepted GBA play, saves, continuation and sleep with candidate 4ce045a0d3fe and the startup-continuation fix with candidate a87f004c7129.
- The owner reports mild GBA slowdown in Breath of Fire’s fire intro and slight audio crackling/stutter near the start of play. Performance and audio pacing remain in the optimization backlog; the cause is not established.
- Bluetooth remains experimental. Battery-gauge jumps, wake-on-lid-open, recurring FAT inconsistencies and slight interface/game stutter remain in the backlog; this release does not establish full-game compatibility or measured sustained performance.

## [0.5.0] - 2026-10-21

- Add Game Boy and Game Boy Color through a pinned, source-built Gambatte core, with independent cartridge SRAM/RTC, manual slots, automatic exit saves and startup continuation.
- Preserve the GB/GBC image aspect ratio; integer scaling uses 3× at 720×480. Existing NES scaling and save identities remain unchanged.
- Include Gambatte source, license and provenance in reproducible H700 packages; add isolated native/BaseOS GB/GBC save/resume coverage. The owner accepted GB/GBC play, saves, continuation and sleep on RG34XXSP.

### Verification and known limitations

- All 32 owner-provided GB/GBC ROMs passed automated native launch, manual-save and automatic-resume checks; commercial ROMs and artwork are not part of this release.
- The owner reports slight stutter in the interface and games. Performance optimization is in the backlog; detailed logging is a suspected contributor, not an established cause.
- Bluetooth remains experimental: the tested 8BitDo Micro in D mode pairs/connects but does not provide working gamepad input. Battery-gauge jumps and wake-on-lid-open remain unresolved.

## [0.4.0] - 2026-10-21

### Library and display

- Add Recent games with the last 20 acknowledged starts, stable ordering across clock changes, deduplication and disabled entries for missing media.
- Add a global integer-scaling default and per-game Global / On / Off overrides, preserving existing choices.
- Default game borders to black and offer Charcoal, Dark blue and Match menus, with a live color sample.
- Use localized All games / Favorite games / {system} games headers without changing game or save identities.

### Game continuity

- Add a separate automatic NES exit save with a valid previous generation, preserving all three manual slots. A failed save leaves the game paused, with retry, cancel and an explicit exit-without-autosave choice.
- Offer continuation of the last available game on startup, only after confirmation. Automatic saving and the startup offer can be disabled independently.
- Consume held gameplay input until release after leaving menus, loading a state, starting a runner or waking from sleep, preventing confirm/back presses from becoming game actions.

### Power and feedback

- Show shutdown/restart progress before flushing and exiting; ignore duplicate requests.
- Add a default five-minute menu screen timeout with first-input wake consumption and raw backlight restoration. Active games are excluded; diagnostic logging continues with the display blanked.
- Add optional three-second in-game battery indicators at 10% and 5%, once per observed charge cycle across game changes and restarts. Menus and read failures remain silent; vendor capacity and shutdown policy are unchanged.
- Move volume feedback to a smaller monochrome icon/bar in the upper-left corner, aligned with the upper-right game battery indicator. Preserve mute, pending and error behavior.

### Verification and limitations

- The main sprint was accepted on RG34XXSP. The final color sample and gameplay input-release guard have automated native and BaseOS coverage; their physical-console recheck remains pending.
- Bluetooth remains experimental: 8BitDo Micro in D mode pairs/connects, but controller input does not work on the tested device. Battery-gauge jumps and wake-on-lid-open remain unresolved.

## [0.3.1] - 2026-10-20

- Fix the 0.3.0 shutdown/restart requests to use the native BaseOS BusyBox-init wrappers instead of its no-op systemctl compatibility shim. Preserve filesystem sync and the native poweroff marker; shutdown and restart were confirmed on RG34XXSP. Bluetooth controller input remains experimental and is unchanged by this patch.

## [0.3.0] - 2026-10-20


### Library and interface

- Add title search within All, Favorites or a system, with English/Russian on-screen entry, desktop typing and case-insensitive filtering.
- Add confirmed save-slot deletion without changing other slots or in-game SRAM; use filled/outlined favorite stars consistently.
- Organize settings into seven scrollable sections, retain focus, move battery percentage to Power and time zone to Date & time, and hide unavailable lid-open wake controls.
- Add consistent section icons, a settings-header icon, and separate confirmed Restart / Shut down actions at the bottom of Settings. The launcher waits for application teardown and filesystem sync before the system request; physical shutdown/restart acceptance is still pending.

### Performance and diagnostics

- Share frontend/test object files within each native or H700 build while retaining independent reproducibility builds. Track header and compiler/link flag changes.
- Remove per-frame controller enumeration and time-zone database loading; record bounded frame timing summaries in detailed diagnostics.
- Keep volume controls usable after sleep and when settings cannot be saved, preserve the live level across shell/runner transitions and report persistence errors separately.
- Add opt-in Diagnostics (Errors, Events, Detailed) and confirmed log clearing, bounded SD journals and temporary RAM fallback during storage failures.
- Record a read-only H700 battery passport, system/display percentage comparison, PMIC/USB observations and sleep/resume markers. Battery calibration and charge-estimation jumps remain unresolved; no corrected gauge is claimed.
- Exclude external HID batteries before opening any attributes, avoiding blocking peripheral battery reads. A cold-start Micro test no longer showed the previous 5–19-second stalls; an isolated 0.7-second frame was still recorded.

### Experimental Bluetooth — controller input does not work yet

- Add BaseOS Bluetooth enable/disable, discovery, pairing, connection/removal, reconnect preference and a radio-dependent status icon. Discovery keeps one agent/client alive and batches state readback.
- Load pinned, attributed Micro profiles and add bounded controller identity, binding and raw/mapped activity diagnostics; virtual SDL tests cover hotplug, buttons and release.
- **Bluetooth remains experimental. On RG34XXSP, 8BitDo Micro in D mode pairs/connects, but buttons do not work: SDL reports no external input device. A successful connection is not working gamepad support.** PIN/numeric confirmation and Bluetooth audio are not implemented or verified. Do not rely on Bluetooth controllers for normal use.
- Retain the built-in controls as the supported input path. Further Linux HID/input discovery and physical controller tests are required.

### Release maintenance

- Prepare changelog entries before Bumpster runs; keep pre-bump validation read-only for compatibility with Bumpster 1.4.

## [0.2.0] - 2026-10-16

- Use matching, centered, font-independent arrow pairs, center key labels by visible bounds and share a softer outline weight/contrast across circular and rectangular footer keys.

- Add offline date/time editing in the selected time zone, calendar/DST validation, explicit automatic-sync shutdown and separate system/UTC hardware-clock readback. Owner testing accepted offline setting, short power-off retention and return to automatic synchronization on RG34XXSP; this is not a long-term RTC durability test.
- Add automatic and on-demand BaseOS network-time synchronization, confirmed status and last-sync history. Filter time-service events and preserve save ordering across backward clock corrections. Owner testing accepted the network-time candidate on RG34XXSP; the final manual-time candidate also passed owner acceptance.
- Add H700 Wi-Fi controls, network discovery on a separate screen, masked password entry, known-network reconnect/forget and IP-confirmed connection state. Show confirmed radio visibility and measured signal strength. Owner testing accepted connection, reboot/sleep recovery and the revised menu/status display; exhaustive failure modes and signal calibration remain separate checks. Credentials stay out of diagnostics.

## [0.1.0] - 2026-10-16

- Add centered Rondo logos with light/dark theme variants to both READMEs.

- Add independent power-setting preferences, but keep wake-on-open unavailable on H700 after the device experiment failed hall-policy readback. Stop loading the experimental module and restore stock sleep-on-close / Power wake without changing saved preferences. Battery changes remain included.

- Show measured battery level in the status icon, distinguish charging and unavailable data, and add a persistent battery-percentage toggle under Display without changing brightness. Arrange visible status items as Wi-Fi, battery and clock with no empty slots, and use a larger monochrome charging mark across the battery with a contrasting cutout.

- Replace the mixed-language time-zone picker with localized country/city lists, current-selection focus and marks, letter/page navigation, and complete offline English/Russian CLDR labels. Preserve IANA settings, UTC timestamps and rollback on failed writes.

- Expand the core-functionality roadmap with a source-based NextUI, MinUI and muOS comparison, record the time-zone picker repair as a near-term priority, and document ongoing version/changelog records for the future website.

## [0.0.1] - 2026-10-15

- Add the catalog and persistent settings foundation.
- Add shared SDL interface components, adaptive input hints and compact volume feedback.
- Integrate the NES frontend and isolated runner, with SRAM persistence, save/load/resume and state previews.
- Add a quiet native check command and Ubuntu/macOS CI for source validation, model/UI tests and NES save/resume scenarios.
- Add source-built H700 runtime packaging, deterministic archives, distribution audit and isolated BaseOS verification.
- Document architecture, development, verification stages and roadmap; configure the maintainer Bumpster branch flow.
- Add README author/copyright footers, Bumpster release checks and a verified H700 candidate/draft workflow for the first 0.0.1 preview.
- Add a guarded macOS installer with first-boot Rondo branding, verified original-logo backup and restoration, preserving games, saves and preferences.
