# Reproducible H700 runtime and package

Run from the repository root with Python 3.11+ and Docker running Linux containers:

```sh
python3 tools/build_h700_runtime.py --work build/h700-first
python3 tools/build_h700_runtime.py --work build/h700-second
```

Each work directory must be new. Downloads are retained in build/h700-cache by their SHA-256; use --cache to choose another directory. Archives are verified on every run, including cache hits. A corrupt cache entry fails the build. The pinned SDK image must be available to Docker; Docker can fetch it when needed. Dependency acquisition needs a network connection only for missing inputs. Compilation itself runs with networking disabled.

The builder freezes the allowlisted source tree and records every file hash before starting. It builds SDL/Mali, SDL_ttf with its bundled FreeType 2.12.1, FCEUmm, Rondo and test executables from source in independent directories. It does not reuse installed frontend binaries or a previous SDK prefix. The compiler image and fixed archive timestamp are pinned in build-lock.json. Source revisions and archive hashes are in [dependency metadata](../docs/dependencies/runtime-sources.json) and [core metadata](../docs/dependencies/core-source.json).

The result is output/Rondo-BaseOS-RG34XXSP-development-*.zip plus its SHA-256 and build-result.json. Compare the two build-result.json files: the source ID, shipped ELF hashes (four active runtime binaries and the inactive research module) and final ZIP hash must match. Determinism uses stable container paths, SOURCE_DATE_EPOCH, normalized source archive metadata and sorted ZIP entries. The fixed epoch is packaging metadata, not a release/publication date. Reproducibility is scoped to identical source inputs and the pinned Linux ARM64 SDK, not arbitrary host toolchains.

The exact-kernel lid-wake research module is included with source/build material but is not loaded by the launcher; H700 wake-on-open remains unavailable after a failed hardware experiment.

The ZIP contains System/, corresponding source archives and build definitions, license texts, build provenance and two checksum manifests. Its only cartridge is an original synthetic test fixture. It contains no BaseOS firmware, commercial games, saves or user preferences. The SDK is a build tool, not a NextUI frontend dependency. The compiler-only tools/build_h700.sh remains a separate, quicker gate.

## Optional BaseOS verification

Provide your own extracted BaseOS 1.3.0 RG34XXSP rootfs outside the repository:

```sh
python3 tools/check_h700_runtime.py --build build/h700-first \
  --rootfs /path/to/extracted-baseos-rootfs --work build/h700-preflight
```

The verifier checks target/version, ZIP integrity and complete checksums before extracting. It mounts the rootfs read-only, overlays tmpfs for /tmp, /sys and /dev, and substitutes a mixer stub only inside the container. No privileged container, host devices or real sysfs are exposed. The actual packaged preflight runs through the BaseOS loader, checks ELF dependencies (including dynamically opened EGL/GLES/ALSA), reports SDL capabilities and exercises NES save/resume and GB/GBC SRAM/RTC and GBA/SNES cartridge persistence, manual/automatic states and startup continuation. Freshly compiled tests also check inline settings, slot previews, power/lid policy and all 21 brightness steps against synthetic ioctl/sysfs fixtures. Captures and logs are retained under the chosen work directory. No rootfs content is included in the ZIP.

For an explicit BaseOS 1.5.2 compatibility probe, add `--baseos-version 1.5.2`. The extracted rootfs must carry that exact target/version and include `/usr/lib` from its `usr-lib.sqfs`, as mounted during a real boot. The report records the tested version separately from the package baseline; this does not relabel a package or certify physical boot/migration.

This is userspace/ABI and synthetic policy verification. Physical graphics, audio, controls, brightness and suspend still require an explicitly authorized device test. Building or verifying never installs to an SD card. See [installation and rollback](../docs/installation.md) for the separate manual operation.

## Distribution audit

Run `python3 tools/audit_h700_package.py build/h700-first/output/Rondo-BaseOS-RG34XXSP-development-SOURCEID.zip` using the actual filename printed by the builder. It checks the ZIP against its own frozen source inputs and required distribution notices. The combined NES/GB/GBC/GBA/SNES package selects FreeType's GPLv2 alternative. See [shipping audit](../docs/shipping-audit.md) and [asset policy](../assets/LICENSE.md).

Frontend and test compilation share the root Makefile object graph within each
`/work/programs` directory. Header dependencies and command configuration changes
trigger rebuilds; no compiled objects are reused between independent full builds.
