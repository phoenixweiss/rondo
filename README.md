# Rondo

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/Assets/Identity/rondo-wordmark-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/Assets/Identity/rondo-wordmark-light.svg">
    <img src="assets/Assets/Identity/rondo-wordmark-light.svg" alt="Rondo" width="420">
  </picture>
</p>

<p align="center"><a href="README_RU.md">Русский</a></p>

Rondo is a retro gaming frontend for the Anbernic RG34XXSP. I liked this handheld enough to use it every day and wanted a frontend that felt right on it, so I built Rondo.

## Download and install

[Download Rondo for RG34XXSP](https://github.com/phoenixweiss/rondo/releases/latest) · [Installation, updates and rollback](docs/installation.md)

Install Rondo on a card that already boots BaseOS on your RG34XXSP. On a freshly flashed card, let BaseOS finish its first boot before installing Rondo. The release ZIP includes the frontend, emulator cores, corresponding source and licenses. Bring your own games; the ZIP contains no BaseOS image, commercial games or proprietary BIOS.

BaseOS 1.3.0, 1.4.0 and 1.5.3 are covered by the automated compatibility matrix. Bluetooth gamepads and two-card storage were tested on RG34XXSP with BaseOS 1.5.3, which is an upstream prerelease. See [compatibility](docs/compatibility.md) for the scope of these checks.

## What you can do

- Browse one library or group games by system. Switch between lists and cover tiles, search across systems, and keep favorites and recent games. [Library controls](docs/library.md)
- Keep games on TF1, TF2 or both. Choose where saves, screenshots, BIOS files, backups, logs and artwork cache are stored. [Set up TF2 and move data](docs/storage-layout.md#format-tf2)
- Resume from automatic saves, manage manual slots with previews and back up progress. [Saves](docs/autosave.md) · [Backups and restore](docs/backups.md)
- Pair Bluetooth gamepads and assign up to four player ports in supported games. The built-in controls stay on player 1. Two 8BitDo Micro controllers in D mode were tested together in NES. [Bluetooth](docs/bluetooth.md) · [Controls and vibration](docs/input-test.md)
- Take screenshots with L3 + R3 or from the game menu. [Shortcuts and file locations](docs/screenshots.md)
- Use the setup wizard in English or Russian. Adjust themes, display, power, Wi-Fi and network or manual time in Settings. [Setup](docs/setup.md) · [Preferences](docs/preferences.md) · [Display](docs/display.md) · [Power](docs/power.md) · [Wi-Fi](docs/wifi.md)

## Game systems

| System | Core | Notes |
| --- | --- | --- |
| NES | FCEUmm | Cartridge saves and states |
| Game Boy / Game Boy Color | Gambatte | [Cartridge saves, RTC and states](docs/game-boy.md) |
| Game Boy Advance | gpSP | [Core and compatibility notes](docs/game-boy-advance.md) |
| Super Nintendo | Supafaust | [Core and compatibility notes](docs/super-nintendo.md) |
| Mega Drive / Genesis | PicoDrive | [Six-button layout and core limits](docs/mega-drive.md) |
| PlayStation | PCSX ReARMed | [CHD, multidisc playlists, BIOS and controllers](docs/playstation.md) |

PlayStation uses the bundled OpenBIOS by default. You can select your own supported BIOS or HLE under **Settings → Core settings → Sony PlayStation**. Game compatibility and performance vary by title.

## Development

On macOS or Linux, install a C++17 compiler, make, pkg-config, SDL2, SDL2_ttf and Python 3.11+. See [development setup](docs/development.md) for platform details.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements.txt
.venv/bin/python tools/build_core.py
.venv/bin/python tools/check.py
```

Run `PYTHON=.venv/bin/python sh tools/demo.sh` to open the desktop demo with sound and a technical test cartridge. The demo keeps its library and state under `build/demo` by default. See [keyboard controls](docs/development.md) and [H700 builds and audits](runtime/README.md).

[Changelog](CHANGELOG.md) · [Roadmap](docs/roadmap.md) · [CI](docs/ci.md) · [Maintainer releases](docs/release.md)

## Contributing

Bug reports with reproduction steps are welcome. Please discuss substantial pull requests first while I work through the project's backlog. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Author and license

Created and maintained by [Pavel Tkachev (@phoenixweiss)](https://github.com/phoenixweiss).

Rondo code and original artwork use MIT. Bundled components retain [their own licenses](THIRD_PARTY_NOTICES.md), including GPLv2 for several cores and noncommercial terms for PicoDrive.

[MIT](LICENSE) © 2026 PAVEL TKACHEV
