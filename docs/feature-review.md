# Core feature review

Reviewed 2026-10-16 against Rondo 0.0.1 (f19701f) and the primary sources below. This is a capability backlog, not a claim that these features are implemented or that another frontend's behavior is supported on RG34XXSP. No competing firmware was installed for this review.

Rondo status comes from src/app.cpp, src/core.cpp, src/main.cpp, src/model.cpp, src/platform.cpp, src/ui.cpp and the current compatibility/device-sleep documentation. A menu label, libretro header or model test alone does not prove an end-to-end feature. Priorities and acceptance criteria are our proposed product decisions, inferred from the comparison and owner requests.

## Sources and scope

- [N1: NextUI feature overview](https://nextui.loveretro.games/docs/): rendering, rewind, tracking, connectivity and optional extensions.
- [N2: NextUI controls](https://nextui.loveretro.games/getting-started/controls/): game switcher, controls, core options and screenshots.
- [N3: NextUI saves and migration](https://nextui.loveretro.games/getting-started/managing-saves/): SRAM, RTC, autosave and state compatibility.
- [N4: NextUI power and clock](https://nextui.loveretro.games/customizing/power-leds-clock/): idle timers, NTP, LEDs and battery history.
- [N5: NextUI collections](https://nextui.loveretro.games/customizing/custom-categories/): categories and recents.
- [N6: NextUI RetroAchievements](https://nextui.loveretro.games/getting-started/retroachievements/): online integration and its requirements.
- [M1: MinUI README](https://github.com/shauninman/MinUI#features): automatic sleep, shutdown/resume and disc handling. The repository is archived; use it as a behavioral reference, not an active support promise.
- [U1: muOS power settings](https://muos.dev/tour/modules/muxpower): configurable inactivity behavior and CPU policy.
- [U2: muOS search](https://muos.dev/tour/modules/muxsearch): folder/global content lookup.
- [U3: muOS device backup](https://muos.dev/tour/modules/muxbackup): selective archives of user state.
- [U4: muOS web services](https://muos.dev/tour/modules/muxwebserv): file transfer and Syncthing.
- [U5: muOS advanced settings](https://muos.dev/tour/modules/muxtweakadv): device rumble and activity controls.

NextUI's public overview spans several devices and optional Paks. The existing local H700 reference was also inspected: upstream/h700 209ef02737108cc6bc9151f37125fd4675b72697, local HEAD b4bc9e3eb03de56688f4a88ae321fb6282328752 with local localization work. Its settings and MinArch expose recents, turbo, screenshots and NTP; that is code evidence, not proof of every function on our hardware. The public docs and this H700 snapshot are different revisions. muOS is a complete firmware; Rondo is a frontend on BaseOS, so services/drivers need separate integration.

This review transfers requirements, not source code. Any later reuse of code/assets needs an audit of the exact revision, license and distribution configuration; it is not automatically covered by Rondo's MIT license.

## P0: everyday device behavior and data safety

These extend the already agreed first product stage. The table rows remain open; F24 below passed owner device acceptance; F02 has partial hardware acceptance. Partial means a working piece exists, not that the whole capability is accepted.

| ID | Capability / reference | Rondo baseline | Work and acceptance |
|---|---|---|---|
| F01 | Radio settings (N1, N2) | Local implementation: H700 Wi-Fi controls, network discovery, masked entry, reconnect/forget and conditional telemetry; no Bluetooth management | Verify Wi-Fi radio/credentials/reconnect on RG34XXSP; Bluetooth pairing/profiles remain planned. Actual radio state controls visibility; test sleep/reboot and failures. |
| F02 | Battery status and alerts (N4, U1) | Implemented: measured fill, charging/unknown states, low-level highlight and percentage toggle; native tests passed; owner accepted percentage and hiding | Verify charging/low/unknown states on RG34XXSP, then consider low-charge notifications separately. Do not invent remaining runtime. |
| F03 | Lid wake and vibration (N1, U5 for feedback; lid wake is an owner requirement) | Partial: owner accepted restored lid sleep and Power wake; failed wake-on-open experiment is disabled; no rumble adapter | Research exact BaseOS wake source/motor; separate persistent options, safe motor stop and device tests. No unsupported toggle. |
| F04 | Idle dim/display-off/sleep timers (N4, U1, M1) | Manual Power/lid suspend; no configurable idle policy | Separate timers/off options; distinguish menu, paused game and active play; do not sleep solely because a cutscene has no button input. Test restore and charging policy. |
| F05 | Safe shutdown and automatic continuation (M1, N3) | SRAM flush, manual states/resume and RAM suspend; no durable automatic snapshot/shutdown-resume policy | Separate autosave from manual slots, failure handling and explicit startup policy. Test real power-off/boot, incompatible states and failed writes. Never imply RAM survives shutdown. |
| F06 | Clock setting and NTP/RTC (N4) | Timezone display selection and UTC save dates exist; no clock/NTP settings | Manual date/time, optional NTP after Wi-Fi, confirm RTC behavior across power-off; clock corrections must not confuse save ordering or playtime. |
| F07 | User backup/restore and save migration (U3, N3) | Installer backup/rollback and config fallback exist; no user-facing state export/import | Export settings, SRAM and states with version/core identity; validate before restore. Test failure/no-space recovery; distinguish portable SRAM from core-specific states. |

### F24: time-zone picker repair (owner device acceptance passed)

The country/city picker now has offline CLDR English/Russian labels, localized ordering, saved-selection focus/marks and letter/page navigation. IANA configuration IDs, UTC timestamps and rollback on failed writes are preserved. See [time-zone selection](timezones.md).

Native macOS checks and actual SDL renders passed. Two independent H700 builds matched; ARM64 BaseOS timezone checks passed. The exact package was installed on the dedicated test card, and the owner confirmed the country/city picker and persistence after restart. Versioned delivery is tracked in the changelog. The former five-city translation and continent/technical-label navigation are replaced; normal publication checks remain separate.

## P1: finding games and playing comfortably

Work in small independent slices after the relevant P0 dependencies. Multi-system foundations remain in the existing roadmap; do not defer necessary BIOS/disc support until after shipping PSX.

| ID | Capability / reference | Rondo baseline | Work and acceptance |
|---|---|---|---|
| F08 | Recently played and game switcher (N2, N5) | Favorites and library focus exist; no recent-history list/switcher | Recents with preview; save outgoing progress before switching. Do not promise multiple live cores in RAM. |
| F09 | Search, collections and large libraries (U2, N5) | Recursive scan, system sections and favorites; no search/custom collections | Search and user collections, stable identity, readable long names; test refresh, missing volumes and large libraries. |
| F10 | Remapping, hotkeys and turbo (N2; local NextUI settings) | Fixed desktop/handheld/SDL controller mappings | Per-system/game controls and reset-to-default; safe shortcut conflicts and turbo options; preserve access to the menu. |
| F11 | Fast-forward and optional rewind (N1; local MinArch) | Fixed normal-speed loop; no user speed/rewind controls | Add measured fast-forward; benchmark rewind CPU/RAM/storage implications and state compatibility before enabling it. |
| F12 | User screenshots (N2) | Save-slot previews and test captures exist; no user screenshot action | Game-frame capture, hotkey, timestamped destination and failure feedback; test full storage and avoid unintended UI capture. |
| F13 | Core options and override scope (N2) | Legacy variables read with first/default values; no options editor | Expose supported core settings with global/system/game scope, defaults and compatibility-aware application. |
| F14 | BIOS, archives and multiple discs (M1, N3) | NES runner, raw content loading and internal BIOS directory; no general BIOS/disc/archive workflow | Per-system BIOS diagnostics, supported formats, playlist/disc change and duplicate suppression as each core is integrated; user supplies BIOS. |
| F15 | External controllers and audio routing (N1, N2) | Basic SDL gamepad input exists; no pairing/remap/player assignment/audio selector | USB/Bluetooth profile tests, hotplug/reconnect, player assignment and speaker/headphone behavior; assess USB DAC/HDMI only where hardware supports them. |
| F16 | Image options and performance diagnostics (N1, N2) | Original-aspect and integer scaling exist; no shader/overlay pipeline or frame-time UI | Preserve simple defaults; add supported filters/aspect options, FPS/frame-time diagnostics and measured audio/video timing improvements. Shaders require a separate rendering/runtime check. |
| F17 | Playtime and session history (N1, U5) | Not implemented | Optional local tracking; exclude pauses/sleep, handle crashes and clock changes; provide reset/export behavior. |
| F18 | File transfer and save sync (U4) | Not implemented | After Wi-Fi and backup: explicit opt-in transfer/sync, conflict handling, credentials and network visibility. Do not sync a state being written or silently replace newer progress. |

## P2: optional capabilities and research

These remain visible without becoming conditions for the next preview or for starting the website.

| ID | Capability / reference | Rondo baseline | Decision before implementation |
|---|---|---|---|
| F19 | RetroAchievements (N6) | No integration | After networking/core support: game identification, accounts, offline behavior and state/rewind/cheat interactions. Hardcore is not assumed available. |
| F20 | Cheats (N1) | No UI/runtime integration | Core API and file-format/license review; explicit per-game enable/reset and save/achievement interaction. |
| F21 | CPU profiles and battery history (N1, N4, U1) | No user governor policy/history | Benchmark heat, frame pacing and power on RG34XXSP; never copy another model's frequencies. Remaining-time estimates need enough measured history. |
| F22 | Display color and supported LED controls (N1, N4) | Brightness and sleep work-LED handling only | Verify panel/LED capabilities and useful controls before UI work; TrimUI ambient RGB behavior is not an RG34XXSP requirement. |
| F23 | Artwork acquisition and optional tools (N1, N5) | Local cover/title/screenshot assets exist; no scraper/Pak infrastructure | Consider explicit artwork import first; network sources/licenses and tool distribution need a separate design. A full app store is not a core milestone. |

## Completion and release records

An ID identifies a capability, not one giant commit. Split it into testable tasks; update the baseline, remaining work and evidence after implementation. Keep native tests, package audit, physical acceptance and released availability distinct. A feature may require a core/OS prerequisite or remain unsupported after research.

Use [roadmap](roadmap.md) for priorities and [CHANGELOG](../CHANGELOG.md) for delivered changes. Keep planned capabilities here until implemented; do not turn this comparison into a fictional release history. Version and release records follow [the maintainer contract](release.md).
