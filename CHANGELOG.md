# Changelog

## Unreleased

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
