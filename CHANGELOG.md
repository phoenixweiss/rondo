# Changelog

## Unreleased

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
