# Reproducible H700 runtime and package

Run from the repository root with Python 3.11+ and Docker running Linux containers:

```sh
python3 tools/build_h700_runtime.py --fresh --work build/h700-first
python3 tools/build_h700_runtime.py --fresh --work build/h700-second
```

Each work directory must be new. Downloads are retained in build/h700-cache by their SHA-256; use --cache to choose another directory. Archives are verified on every run, including cache hits. A corrupt cache entry fails the build. The pinned SDK image must be available to Docker; Docker can fetch it when needed. Dependency acquisition needs a network connection only for missing inputs. Compilation itself runs with networking disabled.

The builder freezes the allowlisted source tree and records every file hash before starting. With `--fresh`, it builds SDL/Mali, SDL_ttf with its bundled FreeType 2.12.1, all six emulator cores, OpenBIOS, the research module, Rondo and test executables from source in independent directories. It does not reuse installed frontend binaries or a previous SDK prefix. The compiler image and fixed archive timestamp are pinned in build-lock.json. Source revisions and archive hashes are in [dependency metadata](../docs/dependencies/runtime-sources.json) and [core metadata](../docs/dependencies/core-source.json).

The result is output/Rondo-BaseOS-RG34XXSP-development-*.zip plus its SHA-256 and build-result.json. Compare the two build-result.json files: the source ID, shipped ELF hashes (frontend, emulator cores, SDL libraries and the inactive research module) and final ZIP hash must match. Determinism uses stable container paths, SOURCE_DATE_EPOCH, normalized source archive metadata and sorted ZIP entries. The fixed epoch is packaging metadata, not a release/publication date. Reproducibility is scoped to identical source inputs and the pinned Linux ARM64 SDK, not arbitrary host toolchains.

The exact-kernel lid-wake research module is included with source/build material but is not loaded by the launcher; H700 wake-on-open remains unavailable after a failed hardware experiment.

The ZIP contains System/, corresponding source archives and build definitions, license texts, build provenance and two checksum manifests. Its only cartridge is an original synthetic test fixture. It contains no BaseOS firmware, commercial games, saves or user preferences. The SDK is a build tool, not a NextUI frontend dependency. The compiler-only tools/build_h700.sh remains a separate, quicker gate.

## Fast development builds

For ordinary development, omit `--fresh`:

```sh
python3 tools/build_h700_runtime.py --work build/h700-candidate
```

The first build records compiled dependencies in `build/h700-dependencies`; choose
another location with `--compiled-cache`. The key includes the pinned compiler
image, source/dependency metadata, dependency build flags/scripts and kernel-module
contract. Application code and ordinary documentation do not invalidate it.
The cache retains at most two entries containing only installed SDK files, core
binaries and dependency verification material, without source/object trees.
Each reuse validates file hashes, modes and internal symlinks before restoration.
Corrupt or incomplete entries fail; remove the affected disposable cache entry and
rebuild. Rondo, tests, ELF inspection, source/license archives and package checks
still run every time. `cache-result.json` records hit/miss/bypass and elapsed time
outside the distributed package.

`--fresh` bypasses both reads and writes to this compiled cache. Release bundling
requires two separate `--fresh` builds and checks their source inputs and outputs.
The download cache remains available in either mode and is always hash-checked.

## Optional BaseOS verification

Provide your own extracted BaseOS 1.5.2 RG34XXSP rootfs outside the repository:

```sh
python3 tools/check_h700_runtime.py --build build/h700-first \
  --rootfs /path/to/extracted-baseos-rootfs --work build/h700-preflight \
  --baseos-version 1.5.2
```

The verifier checks target/version, ZIP integrity and complete checksums before extracting. It mounts the rootfs read-only, overlays tmpfs for /tmp, /sys and /dev, and substitutes a mixer stub only inside the container. No privileged container, host devices or real sysfs are exposed. The actual packaged preflight runs through the BaseOS loader, checks ELF dependencies (including dynamically opened EGL/GLES/ALSA), reports SDL capabilities and exercises NES save/resume and GB/GBC SRAM/RTC and GBA/SNES cartridge persistence, manual/automatic states and startup continuation. Freshly compiled tests also check inline settings, slot previews, power/lid policy and all 21 brightness steps against synthetic ioctl/sysfs fixtures. Captures and logs are retained under the chosen work directory. No rootfs content is included in the ZIP.

Pass the selected BaseOS version explicitly as above. The extracted rootfs must carry that exact target/version and include `/usr/lib` from its `usr-lib.sqfs`, as mounted during a real boot. The report records the userspace version and package baseline separately. Boot and migration are checked on the device.

This is userspace/ABI and synthetic policy verification. Physical graphics, audio, controls, brightness and suspend still require an explicitly authorized device test. Building or verifying never installs to an SD card. See [installation and rollback](../docs/installation.md) for the separate manual operation.

## Distribution audit

Run `python3 tools/audit_h700_package.py build/h700-first/output/Rondo-BaseOS-RG34XXSP-development-SOURCEID.zip` using the actual filename printed by the builder. It checks the ZIP against its own frozen source inputs and required distribution notices. GPL-core runners use FreeType's GPLv2 alternative; the alternative MIT/PicoDrive runner uses FTL with its attribution. PicoDrive retains its noncommercial terms in the same ZIP. See [shipping audit](../docs/shipping-audit.md) and [asset policy](../assets/LICENSE.md).

Frontend and test compilation share the root Makefile object graph within each
`/work/programs` directory. Header dependencies and command configuration changes
trigger rebuilds; no compiled objects are reused between independent full builds.
