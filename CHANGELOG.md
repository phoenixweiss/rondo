# Changelog

## Unreleased

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
