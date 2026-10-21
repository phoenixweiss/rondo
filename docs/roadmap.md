# Roadmap

Rondo is early-stage, maintainer-led software. These are priorities, not delivery dates or a promise that every planned emulator or device will be supported.

## Current baseline

The first preview, [0.0.1](https://github.com/phoenixweiss/rondo/releases/tag/v0.0.1), is published. Its package, installation and basic NES workflow have been checked, including owner testing on RG34XXSP. This does not establish support for every hardware scenario.

The [core feature review](feature-review.md) compares NextUI, MinUI and muOS with the current source, records partial support and gives stable capability IDs F01–F24. The review distinguishes open work, local implementations and released features.

## Agreed next steps

1. Continue game-system integration with GBA, then SNES, MD and PSX (CHD only), retaining NES/GB/GBC regression checks and individual license/performance/device acceptance gates.
2. Design a console-first library list with full system names before replacing the accepted handheld layout.
3. Keep slight interface/game stutter in the optimization backlog. Compare disabled, normal and detailed diagnostic logging under matched conditions; logging is a hypothesis, not a demonstrated cause.
4. Browser file transfer, network backup/save synchronization, online updates and artwork retrieval remain deferred; the website follows core functionality.

## First: core functionality and everyday use

- Time-zone picker repair (F24) passed native and ARM64 checks and owner testing on RG34XXSP, including persistence after restart. Its delivery is recorded in the changelog.
- Collect reproducible device issues, unexpected behavior and missing features. Keep observed defects separate from feature ideas and track the checks still needed.
- Wi-Fi controls, network discovery, masked password entry, known-network reconnect/forget and connection errors are implemented for H700 with native tests. Owner testing accepted connection, reboot/sleep recovery and the revised menu/status display; exhaustive failures and signal calibration remain separate checks. See [Wi-Fi settings](wifi.md). Keep credentials out of diagnostics.
- Add Bluetooth controls, discovery, pairing, connection management and device removal; verify supported profiles and devices individually.
- Show Wi-Fi and Bluetooth status icons only while the corresponding radio is actually enabled. Distinguish enabled without a connection, connecting and connected states; verify updates after sleep and restart.
- Battery percentage and its visibility toggle (F02) passed owner testing on RG34XXSP. Verify charging/unknown states and low-level highlight separately; native checks passed.
- Wake-on-open remains in the backlog after an unsuccessful exact-kernel helper experiment. The helper is not loaded and the option is unavailable on H700. Owner testing accepted restored sleep-on-close and Power wake; establish the actual hall wake contract and power use before another experiment.
- Investigate vibration support on RG34XXSP, distinguish system feedback from core rumble, and add supported behavior with persistent controls and an off option. Check motor shutdown during sleep, exit and errors.
- Refine library headings, feedback, information screens and settings while preserving the accepted save-slot dates and previews.
- Verify library and gameplay behavior across Power/lid sleep, repeated wake cycles, sound, brightness, input, SRAM, save slots and resume. Add diagnostics and regression coverage as each feature changes.
- Keep installation, rollback, configuration and compatibility documentation aligned with tested behavior. A successful build is separate from physical device acceptance.
- Add F04–F05/F07: configurable idle timers, safe shutdown/autosave and continuation, and verified user backup/restore and migration. F06 network/manual clock controls passed bounded owner acceptance; retain broader RTC and error regression work.

This list grows with device testing and user feedback; it is not a complete inventory of planned functionality.

## Next: library and gameplay essentials

- F08–F09: recent games, safe game switching, search, collections and large-library navigation.
- Design a console-first library list for handhelds, inspired by NextUI, with
  full system names and navigation into each system's games. Agree the design
  before replacing the current layout.
- F10–F13: control remapping, hotkeys/turbo, fast-forward, measured rewind, user screenshots and scoped core options.
- F14–F16: BIOS/format/disc support as systems are integrated, external controllers/audio routing and image/performance options.
- F17–F18: local playtime tracking, user data transfer and conflict-aware save sync after networking and backup are ready.

## Next: game systems and library support

- GB/GBC play, saves, continuation and sleep passed bounded owner testing on RG34XXSP. GBA play, saves, continuation and sleep were also accepted with candidate 4ce045a0d3fe. Investigate SNES, MD and PSX (CHD only) individually, covering licensing, builds, controls, performance, SRAM, save states, resume and sleep on the console. Retain NES regression checks.
- Investigate two-card library behavior, duplicate handling, missing cards and state-storage placement.
- Investigate wider H700 hardware support separately before promising compatibility. PSP is outside the project scope.

## Optional research

F19–F23 cover RetroAchievements, cheats, measured CPU/power profiles, battery history, supported color/LED controls, and artwork/tools. They need separate core, hardware, licensing or service checks and do not block all other work.

## Later: website and community

The Vue website follows the agreed core functionality and device work. Revisit its start after reviewing that progress; additional-device investigations need not all be finished first.

- A separate website build under website/, with English and Russian content, actual screenshots, installation and compatibility information, and links to canonical releases.
- A considered explanation of the Rondo name.
- Independent website versioning, CI and deployment.
- Revisit the contribution process when the architecture and maintenance capacity are more mature.

The public dev branch receives small, verified changes. README capabilities describe the checked-out code. Releases and website deployments have separate verification and versioning.

Versioned delivery is recorded in [CHANGELOG](../CHANGELOG.md), using [the release contract](release.md). The future website should distinguish this backlog from implemented and released capabilities.

### Distant platform ideas

Explore Android tablets and Android TV later, using the original Rondo layout
as a possible large-screen direction. Touch, remote and gamepad interaction need
separate design and implementation; these platforms are not currently supported.
