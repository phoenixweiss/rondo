# Core feature review

Rondo status below is current for released **0.12.0** (2026-10-27). The reference
comparison originated on 2026-10-16; it is background for the capability IDs, not
an updated audit of other products. Follow [the roadmap](roadmap.md) for the ordered
stable-release scope. TF2 is required before 1.0; network transfer follows 1.0.

A row can contain both released functionality and a remaining extension. The
remaining column does not mean the whole feature is missing. Current status is
based on the source, feature documentation and dated changelog; specific hardware
checks are retained where implementation alone is insufficient.

## Sources and scope

- [N1: NextUI feature overview](https://nextui.loveretro.games/docs/): rendering, rewind, tracking, connectivity and optional extensions.
- [N2: NextUI controls](https://nextui.loveretro.games/getting-started/controls/): game switcher, controls, core options and screenshots.
- [N3: NextUI saves and migration](https://nextui.loveretro.games/getting-started/managing-saves/): SRAM, RTC, autosave and state compatibility.
- [N4: NextUI power and clock](https://nextui.loveretro.games/customizing/power-leds-clock/): idle timers, NTP, LEDs and battery history.
- [N5: NextUI collections](https://nextui.loveretro.games/customizing/custom-categories/): categories and recents.
- [N6: NextUI RetroAchievements](https://nextui.loveretro.games/getting-started/retroachievements/): online integration and its requirements.
- [M1: MinUI README](https://github.com/shauninman/MinUI#features): automatic sleep, shutdown/resume and disc handling. Use the historical comparison as a behavioral reference, not a current platform-support promise.
- [U1: muOS power settings](https://muos.dev/tour/modules/muxpower): configurable inactivity behavior and CPU policy.
- [U2: muOS search](https://muos.dev/tour/modules/muxsearch): folder/global content lookup.
- [U3: muOS device backup](https://muos.dev/tour/modules/muxbackup): selective archives of user state.
- [U4: muOS web services](https://muos.dev/tour/modules/muxwebserv): file transfer and Syncthing.
- [U5: muOS advanced settings](https://muos.dev/tour/modules/muxtweakadv): device rumble and activity controls.

NextUI's public overview spans several devices and optional Paks. The existing local H700 reference was also inspected: upstream/h700 209ef02737108cc6bc9151f37125fd4675b72697, local HEAD b4bc9e3eb03de56688f4a88ae321fb6282328752 with local localization work. Its settings and MinArch expose recents, turbo, screenshots and NTP; that is code evidence, not proof of every function on our hardware. The public docs and this H700 snapshot are different revisions. muOS is a complete firmware; Rondo is a frontend on BaseOS, so services/drivers need separate integration.

This review transfers requirements, not source code. Any later reuse of code/assets needs an audit of the exact revision, license and distribution configuration; it is not automatically covered by Rondo's MIT license.

## P0: everyday device behavior and data safety

Separate released everyday functions from remaining reliability work. Stable requirements are defined by the roadmap, not by treating every item below as an unfinished prerequisite.

| ID | Capability / reference | Rondo 0.12.0 | Work and acceptance |
|---|---|---|---|
| F01 | Radio settings (N1, N2) | Wi-Fi settings, IP/status, reconnect and sleep/reboot recovery; experimental Bluetooth settings/pairing | Extend Wi-Fi failure regression. Micro D pairs but has no working input; fix or retain explicit experimental status. Other BT profiles are separate. |
| F02 | Battery status and alerts (N4, U1) | Battery fill, charging/unknown states, percentage toggle and low-charge warning | Verify charging/low/unknown states in menus/games and after sleep. System gauge research is separate; do not invent remaining runtime. |
| F03 | Lid wake and vibration (N1, U5; lid wake requested separately) | Lid sleep and Power wake; rumble adapter/switch/input test with safe motor stop | Check actual effects in compatible games. Wake-on-open remains disabled research, not a working option. |
| F04 | Idle dim/display-off/sleep timers (N4, U1, M1) | Configurable menu display-off timeout and manual Power/lid suspend | Verify restore and activity handling. Additional dim/automatic-suspend policies are extensions; active cutscenes must not count as idle menus. |
| F05 | Safe shutdown and continuation (M1, N3) | Automatic states, startup Continue policy, SRAM flushing, shutdown/restart/progress UI; explicit game close clears the startup offer | Extend normal power/failure/FAT regression and recovery. Forced power loss does not preserve RAM or guarantee an exit save. |
| F06 | Clock setting and NTP/RTC (N4) | Manual/network time and country/city time zones; saved ordering survives backward clock corrections | Retain RTC durability/readback/network-failure regression; do not rewrite existing save UTC values. |
| F07 | User backup/restore and migration (U3, N3) | Config fallback and installation preservation exist; no complete user-facing export/restore | Before 1.0, export settings, SRAM/RTC, states and statistics with identities/validation; handle no space and incompatible states before replacement. |

### F24: time-zone picker — delivered

The country/city picker has offline English/Russian CLDR labels, localized ordering,
saved-selection focus and letter/page navigation. IANA IDs and UTC save timestamps
are retained. See [time-zone selection](timezones.md); broader clock recovery checks
remain under F06.

## P1: finding games and playing comfortably

The seven-system foundation is delivered. Keep performance/regression and TF2 work in the stable plan; optional additions are not prerequisites just because they share a capability row.

| ID | Capability / reference | Rondo 0.12.0 | Work and acceptance |
|---|---|---|---|
| F08 | Recently played and game switcher (N2, N5) | Recent history, artwork, missing-game state and normal save/exit/launch lifecycle | Dedicated game switcher is a later extension; preserve outgoing progress without promising multiple live cores. |
| F09 | Search, collections and large libraries (U2, N5) | Search, favorites/recents, optional systems grouping, list/tile views and selection-preserving navigation | Before 1.0, measure large libraries and add TF2/copy identity/migration. Custom collections follow later. |
| F10 | Remapping, hotkeys and turbo (N2; local NextUI settings) | Persistent per-core mappings/reset, physical input test and configurable in-game screenshot chord | Verify physical controls and conflicts. Guided controller diagrams, broader game overrides, turbo and more hotkeys are extensions. |
| F11 | Fast-forward and optional rewind (N1; local MinArch) | No user speed/rewind control | Later: measured fast-forward/turbo; assess rewind CPU/RAM and state compatibility before enabling it. |
| F12 | User screenshots (N2) | Game/interface PNG, game-menu action, preset/custom in-game chords, optional shutter, asynchronous writes and error feedback | Retain input/full-storage regression; browsing/deleting captures in a gallery is later work. |
| F13 | Core options and override scope (N2) | Core enable switches, PSX firmware/controller options, scaling defaults and selected game overrides | A universal options editor and wider scope are later extensions; expose only supported options with clear application rules. |
| F14 | BIOS, archives and multiple discs (M1, N3) | Existing seven-system formats; PSX CHD/playlists/disc selection, OpenBIOS default and optional user BIOS/HLE | Broaden game/firmware checks. Archive-based library import is part of later file-transfer work, not support for arbitrary PSX formats. |
| F15 | External controllers and audio routing (N1, N2) | SDL input/hotplug and experimental BT management; Micro D input remains broken | Verify Micro D and reconnect/sleep first. Additional devices/profiles/audio routes require separate hardware checks. |
| F16 | Image options and performance diagnostics (N1, N2) | Aspect/integer defaults and overrides, PSX aspect-fit, configurable game background and diagnostic journals | Profile real frame/audio/UI bottlenecks before 1.0. New shaders/effects and a live frame-time UI are extensions. |
| F17 | Playtime and session history (N1, U5) | Local versioned launch/active-playtime statistics in Game info; pause/sleep excluded | Include statistics in backup/TF2 migration. Broader reset/export UI and profile-aware sync are separate work; past untracked time stays unknown. |
| F18 | File transfer and save sync (U4) | Not implemented | After 1.0: opt-in folder/ZIP transfer, then conflict-aware sync with backup and write coordination. |

## P2: optional capabilities and research

These are optional research and do not block 1.0 unless a concrete data-safety or power defect is found.

| ID | Capability / reference | Rondo 0.12.0 | Decision before implementation |
|---|---|---|---|
| F19 | RetroAchievements (N6) | No integration | After networking/core support: game identification, accounts, offline behavior and state/rewind/cheat interactions. Hardcore is not assumed available. |
| F20 | Cheats (N1) | No UI/runtime integration | Core API and file-format/license review; explicit per-game enable/reset and save/achievement interaction. |
| F21 | CPU profiles and battery history (N1, N4, U1) | Raw battery diagnostics/passport; no user CPU policy/history screen | Separate measured power/gauge research from UI percentages; use actual device frequencies and sufficient history for any estimates. |
| F22 | Display color and supported LED controls (N1, N4) | Brightness and sleep work-LED handling only | Verify panel/LED capabilities and useful controls before UI work; TrimUI ambient RGB behavior is not an RG34XXSP requirement. |
| F23 | Artwork acquisition and optional tools (N1, N5) | Local artwork, bounded asynchronous thumbnail cache/prefetch; no scraper/Pak infrastructure | Later: explicit artwork import, sources/licenses and optional tools. An app store is not a stable-release requirement. |

## Completion and release records

An ID identifies a capability, not one giant commit. Split it into testable tasks; update the baseline, remaining work and evidence after implementation. Keep native tests, package audit, physical acceptance and released availability distinct. A feature may require a core/OS prerequisite or remain unsupported after research.

Use [roadmap](roadmap.md) for priorities and [CHANGELOG](../CHANGELOG.md) for delivered changes. Keep planned capabilities here until implemented; do not turn this comparison into a fictional release history. Version and release records follow [the maintainer contract](release.md).
