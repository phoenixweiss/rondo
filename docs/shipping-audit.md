# Configured emulator shipping audit

Reviewed 2026-10-15. Scope: the source-built RG34XXSP/BaseOS 1.3.0 package, pinned SDL fork d4a7d750, SDL2_ttf 2.20.2 with FreeType 2.12.1, FCEUmm ac0d00a7 and GCC 8.3.0 from the digest-pinned ARM64 SDK. This is a component/source/distribution review of this configuration, not certification of other cores, firmware or future versions.

## Findings and disposition

| Item | Disposition | Evidence/action |
|---|---|---|
| Original code and independently authored fixtures | Cleared under MIT | Root LICENSE, SPDX headers, full source snapshot |
| Original wordmark/symbol/boot artwork | Cleared under MIT by owner decision on 2026-10-15 | assets/LICENSE.md; copyright permission distinguished from trademark/endorsement |
| FCEUmm/core interface | GPLv2-compliant combined distribution required | Full pinned core source, Rondo source, headers, build scripts and GPL text included; do not market the full ZIP as MIT-only |
| FreeType license choice | Corrected | Previous FTL-only notice did not fit GPLv2. Select the upstream GPLv2 alternative, preserve both texts and the upstream explanation |
| SDL, yuv2rgb, SDL_ttf, FreeType helpers, vendored headers | Notices retained | Standalone GPL/zlib/BSD/MIT terms plus per-file header notices and complete unmodified archives |
| GCC template/static filesystem code | Covered runtime exception recorded | Pinned SDK bits/fs_path.h contains GPLv3 + GCC Runtime Library Exception 3.1; normal GCC build with -lstdc++fs, no proprietary compiler IR plugin; exact SDK license texts copied |
| Ubuntu font software | Separate licensed data | Three unmodified Ubuntu 0.83 files independently matched upstream bytes; UFL and Canonical metadata retained |
| Lucide/Feather SVG and raster derivatives | Separate ISC/MIT data | All SVG source hashes and complete upstream ISC/Feather MIT notice retained; derivative generator included |
| Corresponding source and build control | Included | Complete Rondo snapshot with every input hash; exact original SDL/TTF/core archives and hash pins; build definitions/configuration; compiler image pinned by digest |
| Firmware, OS libraries, BIOS, user data | Excluded | No BaseOS image/rootfs, proprietary BIOS, commercial ROMs, user saves or private state installed by the builder |

## Configured runtime boundary

The four installed ELF files are rondo, fceumm_libretro.so, libSDL2-2.0.so.0 and libSDL2_ttf-2.0.so.0. All are AArch64. Direct dependencies:

- Rondo: SDL2, SDL2_ttf, libdl, libpthread, libstdc++, libm, libgcc_s and libc.
- FCEUmm: libm and libc.
- SDL2 and SDL2_ttf: libm, libdl, libpthread and libc; SDL2_ttf also links the packaged SDL2. FreeType is compiled into SDL2_ttf.
- Dynamic platform loads: installed EGL/GLES/Mali and ALSA. These remain part of the owner's installed OS, not package payload. Exact BaseOS loader checks pass, but dummy-driver execution does not validate proprietary GPU/audio operation.

The GPLv2 distribution analysis treats the installed OS's normal compiler/OS libraries as the system-library boundary. It grants no redistribution rights to vendor firmware or drivers. Bundling a BaseOS image, vendor driver or a different platform library changes this scope and requires a new review. The use of the libretro API/dlopen alone is not treated as an exemption from the core's copyleft.

FCEUmm is built with platform=unix, ARCH=arm64, HAVE_HDPACK=0 and HAVE_NTSC=0. SDL enables Mali-fbdev/OpenGLES and ALSA; desktop window systems, libsamplerate and hardware discovery services are disabled. HarfBuzz is disabled; bundled FreeType uses internal zlib. External bzip2/PNG/Brotli are disabled by the bundled FreeType configuration. Source archives retain optional uncompiled HarfBuzz and platform files. In particular, the original SDL_ttf source tar contains three Apple SDL framework binaries; their upstream license files remain in that archive and they are not part of System/ or the H700 build.

The ZIP distributes complete corresponding source directly with the binary instead of relying only on a source URL or a future written offer. This implements the source-accompaniment path in GPLv2 section 3(a); build commands and exact source/compiler inputs are recorded. It does not imply redistribution of compiler binaries. The GCC runtime exception permits eligible generated target code while retaining the other components' license obligations.

## Checks and remaining gates

Run tools/audit_h700_package.py against the built ZIP. It checks complete manifests, four expected AArch64 ELFs, the exact source inventory and upstream archive hashes, selected FreeType alternative, compiler-license hashes, font/icon source hashes and required notices. Run the isolated BaseOS preflight separately. Keep build and audit reports with the exact release candidate.

No unresolved license/asset-policy decision remains for the documented NES configuration after the listed corrections. This is not a claim that all future upstream code is automatically cleared. Changed revisions/options, new libraries/cores, added firmware or altered fonts require another review. The final public artifact must pass these gates again after its actual version/ref is selected. Physical installation/acceptance of that final package and actual hosted CI/release verification remain release gates, not completed by this audit.

Primary references: [GPLv2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html), [FSF plugin guidance](https://www.gnu.org/licenses/gpl-faq.html#GPLPlugins), [FreeType license choice](https://freetype.org/license.html), [GCC runtime exception](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html), [Ubuntu Font Licence](https://canonical.com/legal/font-licence), [Lucide's full ISC/MIT notice](https://github.com/lucide-icons/lucide/blob/main/LICENSE). The exact local source and license hashes are authoritative for this reviewed build.

## GB/GBC candidate (2026-10-21)

Gambatte d9d6cd06382d1ced30de34d56d3609452323dab1 is pinned by revision and archive SHA-256 in docs/dependencies/gb-core-source.json. The package carries its complete unmodified source archive and COPYING as Licenses/Gambatte-GPLv2.txt; per-file notices are retained. This GPLv2 core follows the same combined-distribution policy as FCEUmm. It is built with platform=unix, HAVE_NETWORK=0 and an empty GIT_VERSION; link-cable networking is outside this configuration. No BIOS or commercial ROM is supplied. Original MIT GB/GBC technical fixtures are source/CI material and are not installed in the user's game library. Package auditing checks the exact core binary, archive and license against the frozen source metadata. Physical hardware remains a separate acceptance gate.

## GBA candidate (2026-10-21)

gpSP 5819380c2ffb0900219d700a382ee68c464ebb99 is revision/SHA-256 pinned in gba-core-source.json. Full unmodified source, COPYING, per-file attributions and the open BIOS README are included. The GPLv2 BIOS replacement credits Normmatt and VBA/VBA-M, with sources/build Makefile retained; its bundled prebuilt image is embedded as supplied upstream, not rebuilt using the H700 toolchain. No Nintendo firmware is supplied. H700 uses platform=arm64 (dynarec), Mac uses osx/HAVE_DYNAREC=0; GIT_VERSION is empty for deterministic builds. The combined GPLv2 distribution and existing FreeType license choice apply. Package auditing verifies the exact binary set, source archive checksum and notices against frozen metadata. Physical acceptance is separate.

## SNES candidate (2026-10-21)

Supafaust 642d1d1b6684aa7e306a02a89885f3f5456a5157 is revision/archive-SHA pinned in snes-core-source.json. GPLv2 and per-file attribution are retained; COPYING and the full unmodified archive accompany the frontend. This uses the existing GPLv2 combined-package policy, retaining MIT for original Rondo code/artwork. Native macOS selects the existing condition-variable semaphore implementation through a compiler define. H700 builds use platform=unix and Cortex-A53 tuning, with fixed GIT_VERSION and no source patch. No proprietary SNES firmware or commercial games are supplied. The audit checks the exact core ELF, source archive and license bytes against frozen source metadata. Hardware acceptance remains separate.
