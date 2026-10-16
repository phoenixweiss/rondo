# Native development

Rondo is C++17/SDL2 software. The desktop prototype runs natively; it does not boot or emulate BaseOS. The source tree contains no commercial games or device firmware.

## Dependencies

Use a C++17 compiler, make, pkg-config, SDL2, SDL2_ttf and Python 3.11+ supported by tools/requirements.txt. macOS needs Xcode Command Line Tools; its SDL packages may be installed through Homebrew. Linux needs the corresponding SDL2/SDL2_ttf development packages and compiler tools. Linux native execution is a separate verification target from macOS and BaseOS.

asdf can manage Python. Use a project-local virtual environment rather than modifying system Python:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements.txt
make
.venv/bin/python tools/build_core.py
```

The core builder downloads the exact FCEUmm revision from docs/dependencies/core-source.json, verifies the archive SHA-256, retains that archive and builds it below build/cores. It also copies the resulting library to the stable path used by the demo/check entrypoints: build/cores/fceumm_libretro.dylib on macOS or build/cores/fceumm_libretro.so on Linux.

## Quiet checks

```sh
.venv/bin/python tools/check.py
```

This builds the native targets and runs source validation, runtime build-tool and release safety tests, model, compact-volume, real-core/runner and synthetic device-policy checks. SDL video/audio use dummy drivers; hardware/boot adapters are removed from the inherited test environment. It opens no window, produces no test sound and does not access an SD card. Results go to a new temporary directory printed on completion.

The first step checks publication inputs: JSON, local documentation links, locale placeholders, pinned vendor hashes and accidental personal paths/generated directories. Run it independently with `.venv/bin/python tools/check_source.py`.

To retain results at a chosen new location or use a custom core build:

```sh
.venv/bin/python tools/check.py --work build/check-first
.venv/bin/python tools/check.py --core /path/to/native/fceumm_libretro.so
```

The work location must not already exist, which prevents accidental overwrite of earlier results. A core override must be an appropriate native library. These tests are not physical brightness, panel, ALSA or suspend verification.

## Interactive demo

```sh
PYTHON=.venv/bin/python sh tools/demo.sh
```

This explicit manual command opens a window and plays the original test cartridge's test tone. It does not change host system volume. It generates its synthetic library under build/demo/library and keeps settings/saves under build/demo/state, with captures in build/demo/output. Existing user ROM folders and installed frontend state are not used.

Set RONDO_DEMO_DIR to a separate writable location to override this root. The demo preserves an existing fixture library; it does not mirror/delete user directories. Avoid editing active configuration/state files while Rondo is running.

Keyboard menus: arrows, Enter/Z confirm, Esc/X back, C favorite/info, V settings, Q/E section, Tab game menu and =/- volume. During NES play Z/X are A/B, Enter is Start and Right Shift is Select. Input hints follow the active keyboard/controller mode.

## Assets and source boundaries

The compiled program uses checked-in runtime assets, so icon regeneration is optional. tools/raster_icons.py needs CairoSVG and rsvg-convert from librsvg; the latter preserves the wordmark's luminance masks. FontTools/Pillow are used by verification and fixture tools. Preserve third-party notices when changing assets.

build/, Packages/, private test outputs, ROM collections and device state are not product source. Check vendor/sources.json and docs/dependencies for pinned inputs. See THIRD_PARTY_NOTICES.md before distributing a binary.

For country/city labels and picker behavior, see [time-zone selection](timezones.md).

See [battery display](battery.md) for status and settings behavior.

## H700

tools/build_h700.sh is a compile-only gate using the pinned Docker SDK. For a complete source-built SDL/Mali runtime, FCEUmm and frontend ZIP, use `python3 tools/build_h700_runtime.py --work build/h700-first`. See the [runtime build guide](../runtime/README.md) for two-run reproducibility checks and optional isolated BaseOS preflight. This frontend package is not an SD image. Firmware/rootfs files remain external test inputs.
