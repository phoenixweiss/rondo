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

The package baseline is BaseOS 1.3.0. Rondo has also been tested on RG34XXSP with BaseOS 1.5.2 (upstream prerelease). Time controls detect platform/service dependencies rather than an exact OS version; see [compatibility](docs/compatibility.md).

**Experimental Bluetooth:** pairing and connection controls are available, but controller input is not working on the tested 8BitDo Micro (D mode). Bluetooth audio and PIN confirmation are not supported. See [Bluetooth limitations](docs/bluetooth.md).

See [themes, core switches and button mapping](docs/preferences.md) and the [setup wizard](docs/setup.md) for available settings.

## Components

- Library catalog and persistent preferences, with recovery after a failed write.
- English and Russian localization, with English as the fallback.
- Shared SDL drawing components, typography, icons and keyboard/controller hints.
- Compact volume feedback with confirmed, muted, pending and error states.
- NES (FCEUmm), Game Boy / Game Boy Color (Gambatte), Game Boy Advance (gpSP), experimental Super Nintendo (Supafaust), Mega Drive / Genesis (PicoDrive) and experimental PlayStation (PCSX ReARMed, CHD only) execution in a separate process, using pinned cores.
- Cartridge SRAM and GB/GBC RTC persistence, manual save slots with game previews and resume from a saved state.
- A library, favorites, settings and game menu.
- Model tests, UI rendering checks and headless launch/save/resume scenarios.
- Localized country/city time-zone selection with saved focus and quick navigation.
- Measured battery fill, charging/unknown states and an optional percentage display.
- Sleep on lid close and Power wake; experimental wake-on-open remains unavailable.
- H700 Wi-Fi controls, separate network discovery, saved-network reconnect, IP and signal status.
- Automatic/on-demand network time and offline date/time editing, with verified system/UTC hardware-clock writes.

Recent games keep the last 20 successful starts. A separate automatic exit save
preserves manual slots and offers continuation on startup. Display settings include
global integer scaling, per-game overrides and selectable game borders with a color
sample. Menu screen timeout and quiet in-game battery warnings are configurable.
See [automatic saves](docs/autosave.md), [display settings](docs/display.md) and
[power behavior](docs/power.md).

GB/GBC gameplay, saves, continuation and sleep have been tested on RG34XXSP. See [Game Boy support](docs/game-boy.md).

GBA gameplay, saves, continuation and sleep have been tested on RG34XXSP. See [GBA support](docs/game-boy-advance.md).

SNES gameplay, saves and continuation have been tested on RG34XXSP. Button presses and volume changes no longer interrupt gameplay or audio; holding a volume button repeats the adjustment. See [SNES support](docs/super-nintendo.md).

Mega Drive controls: Y/B/A → Sega A/B/C, L1/X/R1 → X/Y/Z, Select → Mode. Keyboard and scope: [Mega Drive](docs/mega-drive.md). The library is sorted by title across platforms; Recent keeps launch order.

Read-only **Settings → System information** groups device, installed software, memory cards and power; full values and sources are available with X. See [system information](docs/system-information.md).

Mega Drive gameplay, saves, continuation, sleep and System information have been tested on RG34XXSP with BaseOS 1.5.2.

Experimental CHD-only PlayStation with PCSX ReARMed, bundled OpenBIOS, selectable user BIOS/HLE in Core settings, and multidisc playlists is available for development testing. See [PlayStation](docs/playstation.md). Tested on RG34XXSP with BaseOS 1.5.2.

**State format:** version 0.9.0 uses compact binary states for all cores. Previous JSON states are not loaded or deleted; in-game SRAM/RTC and PSX memory cards keep their formats. See [automatic saves](docs/autosave.md).

For **User BIOS**, put your own 512 KiB files in `RondoState/BIOS/PSX`, using the names below. Sony firmware is never included in Rondo packages.

| Filename | BIOS | Verification |
| --- | --- | --- |
| `psxonpsp660.bin` | PS1 firmware extracted from PSP 6.60 | Known MD5; native game boot checked |
| `scph101.bin` | PSone, North America | Known MD5; native game boot checked |
| `scph7001.bin` | PlayStation, North America | Known MD5; native game boot checked |
| `scph7502.bin` | PlayStation, Europe | Native game boot checked |
| `scph1001.bin`, `scph5500.bin`, `scph5501.bin`, `scph5502.bin` | Other retail PlayStation BIOS versions | Supported names; not exercised in these tests |

Use lowercase filenames on the card. Select **BIOS file → Automatic** to let the core choose, or select a specific detected file to use only that image; changing the mode in **Core settings → Sony PlayStation** takes effect at the next game launch. OpenBIOS remains bundled and selected by default. [BIOS identities and selection](docs/playstation.md#bios-files).

## Development

Requires a C++17 compiler, make, pkg-config, SDL2, SDL2_ttf and Python 3.11+.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements.txt
.venv/bin/python tools/build_core.py
.venv/bin/python tools/check.py --work build/native-check
```

The default build produces build/rondo. All five cores are built separately from source, with verified SHA-256 downloads and pinned revisions in docs/dependencies/core-source.json, gb-core-source.json, gba-core-source.json, snes-core-source.json and md-core-source.json. The check command builds the application and runs source, model, UI rendering, NES/GB/GBC/GBA/SNES/MD save/resume and device-policy tests with dummy video/audio drivers and an original test cartridge. The build/native-check directory must be new; omit --work to create a temporary directory automatically. Logs, renders and summary.json are saved there. Tests do not access an SD card or validate physical controls, brightness or suspend.

GitHub Actions runs these checks on Ubuntu and macOS for pushes to dev/main and pull requests. Workflow permissions are read-only; external actions are pinned to full commit hashes.

## Desktop demo

```sh
PYTHON=.venv/bin/python sh tools/demo.sh
```

This command opens a window and plays the test cartridge sound. The demo library and state live under build/demo; set RONDO_DEMO_DIR to choose another location.

Menus use arrows, Enter/Z to confirm, Esc/X to go back, C for favorites/info, V for search, Tab for settings, Q/E for sections, Tab for the in-game menu and =/- for volume. In NES/GB/GBC/GBA/SNES play, Z/X are A/B, Enter is Start and Right Shift is Select; Q/E are L/R for GBA and SNES; S/A are X/Y for SNES.

Commercial ROMs, proprietary BIOS images and user saves are not included in the repository.

## H700 development package

Requires Docker with Linux ARM64 container support. Build twice into new directories to compare the source ID, shipped binaries and ZIP hash:

```sh
python3 tools/build_h700_runtime.py --work build/h700-first
python3 tools/build_h700_runtime.py --work build/h700-second
```

The package targets RG34XXSP with BaseOS 1.3.0. It includes the frontend, pinned runtime, original test cartridge, corresponding source and distribution notices. It is a frontend ZIP for an existing BaseOS installation, not a bootable SD image. The builder never accesses a card. The [macOS installer](docs/installation.md) backs up the original boot image and applies Rondo branding before the first frontend boot, unless disabled in existing preferences.

See the [runtime guide](runtime/README.md) for package audit and optional isolated BaseOS verification, [compatibility](docs/compatibility.md) for verification limits and [manual installation](docs/installation.md) for backup and rollback. GPL cores retain GPLv2 source obligations; PicoDrive retains noncommercial terms. Rondo code and original artwork remain MIT. See the [configured shipping audit](docs/shipping-audit.md).

## Documentation

- [Development and controls](docs/development.md)
- [Architecture](docs/architecture.md) and [design provenance](docs/design-source.md)
- [Verification stages](docs/stages.md) and [compatibility](docs/compatibility.md)
- [Installation and rollback](docs/installation.md), [brightness and sleep](docs/device-sleep.md)
- [Roadmap](docs/roadmap.md) and [maintainer release procedure](docs/release.md)

## Contributing

Bug reports with reproduction steps are welcome. Please discuss substantial pull requests first; development currently follows the maintainer's backlog. See [CONTRIBUTING.md](CONTRIBUTING.md).

Input diagnostics and built-in game vibration are described in [Input test](docs/input-test.md).

## Author

Created and maintained by [Pavel Tkachev (@phoenixweiss)](https://github.com/phoenixweiss).

## License

Rondo code and original artwork are MIT-licensed. Fonts, icons, runtime libraries and emulators retain [their own terms](THIRD_PARTY_NOTICES.md); GPL cores retain their source obligations and PicoDrive retains noncommercial terms. The complete package is not MIT-only or GPL-only.

[MIT](LICENSE) © 2026 PAVEL TKACHEV
