# Rondo

[Русский](README_RU.md)

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

## Development

Requires a C++17 compiler, make, pkg-config, SDL2, SDL2_ttf and Python 3.11+.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements.txt
make -j4 all build/model-tests build/core-tests build/device-tests build/volume-ui-tests
.venv/bin/python tools/build_core.py
make test
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/volume-ui-tests assets build/volume-renders
.venv/bin/python tools/check_source.py
rondo_core=build/cores/fceumm_libretro.dylib
[ "$(uname -s)" != Linux ] || rondo_core=build/cores/fceumm_libretro.so
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy .venv/bin/python tests/run_checks.py --core "$rondo_core" --work build/native-check
```

The default build produces build/rondo. The core is built separately from source: its download is SHA-256 verified and its revision is pinned in docs/dependencies/core-source.json. Checks use dummy video/audio drivers and an original test cartridge. The build/native-check directory must be new. Tests do not access an SD card or validate physical controls, brightness or suspend.

## Desktop demo

```sh
PYTHON=.venv/bin/python sh tools/demo.sh
```

This command opens a window and plays the test cartridge sound. The demo library and state live under build/demo; set RONDO_DEMO_DIR to choose another location.

Menus use arrows, Enter/Z to confirm, Esc/X to go back, C for favorites/info, V for settings, Q/E for sections, Tab for the game menu and =/- for volume. In NES play, Z/X are A/B, Enter is Start and Right Shift is Select.

Commercial ROMs, BIOS files and user saves are not included in the repository.

## License and contributions

Rondo code and original artwork are [MIT-licensed](LICENSE). Fonts, icons and other dependencies retain [their own terms](THIRD_PARTY_NOTICES.md).

Bug reports with reproduction steps are welcome. Please discuss substantial pull requests first; development currently follows the maintainer's backlog. See [CONTRIBUTING.md](CONTRIBUTING.md).
