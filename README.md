# Rondo

[Русский](README_RU.md)

A personal frontend project for the Anbernic RG34XXSP. I enjoy using this handheld and wanted a frontend that felt right for me, so I started building Rondo.

**Early development.** Interfaces and settings formats may change.

## Components

- Library catalog and persistent preferences, with recovery after a failed write.
- English and Russian localization, with English as the fallback.
- Shared SDL drawing components, typography, icons and keyboard/controller hints.
- Compact volume feedback with confirmed, muted, pending and error states.
- Model tests and headless UI rendering checks.

## Development

Requires a C++17 compiler, make, pkg-config, SDL2, SDL2_ttf and Python 3.11+.

```sh
make -j4
make test
make ui-test
python3 tools/check_source.py
```

The default build produces model and UI test executables. UI checks use dummy video/audio drivers, open no window and save their renders under build/volume-renders. Tests do not access an SD card or validate physical controls, brightness or suspend.

## License and contributions

Rondo code and original artwork are [MIT-licensed](LICENSE). Fonts, icons and other dependencies retain [their own terms](THIRD_PARTY_NOTICES.md).

Bug reports with reproduction steps are welcome. Please discuss substantial pull requests first; development currently follows the maintainer's backlog. See [CONTRIBUTING.md](CONTRIBUTING.md).
