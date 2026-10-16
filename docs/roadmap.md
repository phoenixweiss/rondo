# Roadmap

Rondo is early-stage, maintainer-led software. These are priorities, not delivery dates or a promise that every planned emulator or device will be supported.

## Current baseline

The first preview, [0.0.1](https://github.com/phoenixweiss/rondo/releases/tag/v0.0.1), is published. Its package, installation and basic NES workflow have been checked, including owner testing on RG34XXSP. This does not establish support for every hardware scenario.

The [core feature review](feature-review.md) compares NextUI, MinUI and muOS with the current source, records partial support and gives stable capability IDs F01–F24. The review distinguishes open work, local implementations and released features.

## First: core functionality and everyday use

- Time-zone picker repair (F24) passed native and ARM64 checks and owner testing on RG34XXSP, including persistence after restart. Its delivery is recorded in the changelog.
- Collect reproducible device issues, unexpected behavior and missing features. Keep observed defects separate from feature ideas and track the checks still needed.
- Wi-Fi controls, network discovery, masked password entry, known-network reconnect/forget and connection errors are implemented for H700 with native tests. Verify the adapter on RG34XXSP, including reboot/sleep, before claiming physical radio support. See [Wi-Fi settings](wifi.md). Keep credentials out of diagnostics.
- Add Bluetooth controls, discovery, pairing, connection management and device removal; verify supported profiles and devices individually.
- Show Wi-Fi and Bluetooth status icons only while the corresponding radio is actually enabled. Distinguish enabled without a connection, connecting and connected states; verify updates after sleep and restart.
- Battery percentage and its visibility toggle (F02) passed owner testing on RG34XXSP. Verify charging/unknown states and low-level highlight separately; native checks passed.
- Wake-on-open remains in the backlog after an unsuccessful exact-kernel helper experiment. The helper is not loaded and the option is unavailable on H700. Owner testing accepted restored sleep-on-close and Power wake; establish the actual hall wake contract and power use before another experiment.
- Investigate vibration support on RG34XXSP, distinguish system feedback from core rumble, and add supported behavior with persistent controls and an off option. Check motor shutdown during sleep, exit and errors.
- Refine library headings, feedback, information screens and settings while preserving the accepted save-slot dates and previews.
- Verify library and gameplay behavior across Power/lid sleep, repeated wake cycles, sound, brightness, input, SRAM, save slots and resume. Add diagnostics and regression coverage as each feature changes.
- Keep installation, rollback, configuration and compatibility documentation aligned with tested behavior. A successful build is separate from physical device acceptance.
- Add F04–F07: configurable idle timers, safe shutdown/autosave and continuation, clock/NTP/RTC controls, and verified user backup/restore and migration.

This list grows with device testing and user feedback; it is not a complete inventory of planned functionality.

## Next: library and gameplay essentials

- F08–F09: recent games, safe game switching, search, collections and large-library navigation.
- F10–F13: control remapping, hotkeys/turbo, fast-forward, measured rewind, user screenshots and scoped core options.
- F14–F16: BIOS/format/disc support as systems are integrated, external controllers/audio routing and image/performance options.
- F17–F18: local playtime tracking, user data transfer and conflict-aware save sync after networking and backup are ready.

## Next: game systems and library support

- Investigate GB/GBC, GBA, SNES, MD and PSX individually, covering licensing, builds, controls, performance, SRAM, save states, resume and sleep on the console. Retain NES regression checks.
- Investigate two-card library behavior, duplicate handling, missing cards and state-storage placement.
- Treat PSP performance and wider H700 hardware support as separate investigations before promising compatibility.

## Optional research

F19–F23 cover RetroAchievements, cheats, measured CPU/power profiles, battery history, supported color/LED controls, and artwork/tools. They need separate core, hardware, licensing or service checks and do not block all other work.

## Later: website and community

The Vue website follows the agreed core functionality and device work. Revisit its start after reviewing that progress; exploratory PSP and additional-device work need not all be finished first.

- A separate website build under website/, with English and Russian content, actual screenshots, installation and compatibility information, and links to canonical releases.
- A considered explanation of the Rondo name.
- Independent website versioning, CI and deployment.
- Revisit the contribution process when the architecture and maintenance capacity are more mature.

The public dev branch receives small, verified changes. README capabilities describe the checked-out code. Releases and website deployments have separate verification and versioning.

Versioned delivery is recorded in [CHANGELOG](../CHANGELOG.md), using [the release contract](release.md). The future website should distinguish this backlog from implemented and released capabilities.
