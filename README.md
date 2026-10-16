# Rondo

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/Assets/Identity/rondo-wordmark-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/Assets/Identity/rondo-wordmark-light.svg">
    <img src="assets/Assets/Identity/rondo-wordmark-light.svg" alt="Rondo" width="420">
  </picture>
</p>

<p align="center"><a href="README_RU.md">Русский</a></p>

A personal frontend project for the Anbernic RG34XXSP. I enjoy using this handheld and wanted a frontend that felt right for me, so I started building Rondo.

**Early development.** Interfaces and settings formats may change.

## Components

- Library catalog and persistent preferences, with recovery after a failed write.
- English and Russian localization, with English as the fallback.
- Shared SDL drawing components, typography, icons and keyboard/controller hints.
- Compact volume feedback with confirmed, muted, pending and error states.
- NES execution in a separate process using a pinned FCEUmm core.
- SRAM persistence, manual save slots with game previews and resume from a saved state.
- A library, favorites, settings and game menu.
- Model tests, UI rendering checks and headless launch/save/resume scenarios.
- Localized country/city time-zone selection with saved focus and quick navigation.
- Measured battery fill, charging/unknown states and an optional percentage display.
- Sleep on lid close and Power wake; experimental wake-on-open remains unavailable.
- H700 Wi-Fi controls, separate network discovery, saved-network reconnect, IP and signal status.
- Automatic/on-demand network time and offline date/time editing, with verified system/UTC hardware-clock writes.

## Development

Requires a C++17 compiler, make, pkg-config, SDL2, SDL2_ttf and Python 3.11+.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements.txt
.venv/bin/python tools/build_core.py
.venv/bin/python tools/check.py --work build/native-check
```

The default build produces build/rondo. The core is built separately from source: its download is SHA-256 verified and its revision is pinned in docs/dependencies/core-source.json. The check command builds the application and runs source, model, UI rendering, NES save/resume and device-policy tests with dummy video/audio drivers and an original test cartridge. The build/native-check directory must be new; omit --work to create a temporary directory automatically. Logs, renders and summary.json are saved there. Tests do not access an SD card or validate physical controls, brightness or suspend.

GitHub Actions runs these checks on Ubuntu and macOS for pushes to dev/main and pull requests. Workflow permissions are read-only; external actions are pinned to full commit hashes.

## Desktop demo

```sh
PYTHON=.venv/bin/python sh tools/demo.sh
```

This command opens a window and plays the test cartridge sound. The demo library and state live under build/demo; set RONDO_DEMO_DIR to choose another location.

Menus use arrows, Enter/Z to confirm, Esc/X to go back, C for favorites/info, V for settings, Q/E for sections, Tab for the game menu and =/- for volume. In NES play, Z/X are A/B, Enter is Start and Right Shift is Select.

Commercial ROMs, BIOS files and user saves are not included in the repository.

## H700 development package

Requires Docker with Linux ARM64 container support. Build twice into new directories to compare the source ID, shipped binaries and ZIP hash:

```sh
python3 tools/build_h700_runtime.py --work build/h700-first
python3 tools/build_h700_runtime.py --work build/h700-second
```

The package targets RG34XXSP with BaseOS 1.3.0. It includes the frontend, pinned runtime, original test cartridge, corresponding source and distribution notices. It is a frontend ZIP for an existing BaseOS installation, not a bootable SD image. The builder never accesses a card. The [macOS installer](docs/installation.md) backs up the original boot image and applies Rondo branding before the first frontend boot, unless disabled in existing preferences.

See the [runtime guide](runtime/README.md) for package audit and optional isolated BaseOS verification, [compatibility](docs/compatibility.md) for verification limits and [manual installation](docs/installation.md) for backup and rollback. The combined NES distribution follows GPLv2; Rondo code and original artwork remain MIT. See the [configured shipping audit](docs/shipping-audit.md).

## Documentation

- [Development and controls](docs/development.md)
- [Architecture](docs/architecture.md) and [design provenance](docs/design-source.md)
- [Verification stages](docs/stages.md) and [compatibility](docs/compatibility.md)
- [Installation and rollback](docs/installation.md), [brightness and sleep](docs/device-sleep.md)
- [Roadmap](docs/roadmap.md) and [maintainer release procedure](docs/release.md)

## Contributing

Bug reports with reproduction steps are welcome. Please discuss substantial pull requests first; development currently follows the maintainer's backlog. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Author

Created and maintained by [Pavel Tkachev (@phoenixweiss)](https://github.com/phoenixweiss).

## License

Rondo code and original artwork are MIT-licensed. Fonts, icons, runtime libraries and emulators retain [their own terms](THIRD_PARTY_NOTICES.md); the complete NES package follows GPLv2 distribution requirements.

[MIT](LICENSE) © 2026 PAVEL TKACHEV
