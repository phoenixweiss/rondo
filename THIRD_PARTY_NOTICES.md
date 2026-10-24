# Third-party notices

Original Rondo code, scripts, synthetic test fixtures and original artwork are MIT. See LICENSE and [asset policy](assets/LICENSE.md). This does not relicense third-party code or data. The regular ZIP includes alternative GPLv2 FCEUmm, Gambatte, gpSP and Supafaust cores and noncommercial PicoDrive. Each runner loads one core; no PicoDrive code is linked into those GPL cores, and no GPL core is linked into PicoDrive. Preserve the GPL source obligations and PicoDrive noncommercial terms. This download has component-specific terms, not a single MIT or GPL grant. Original Rondo files keep their MIT grant.

| Component | Exact input | Terms retained |
|---|---|---|
| Unicode CLDR localized countries/cities | assets/Timezones/sources.json | Unicode License v3, assets/Timezones/LICENSE; offline derived labels, pinned generator |
| nlohmann/json 3.12.0, including Hoehrmann/Loitsch code | vendor/sources.json | MIT; licenses/nlohmann-json-MIT.txt and preserved header attribution |
| libretro API header | vendor/sources.json | MIT grant in header; not a license for all cores |
| PicoSHA2 1.0.1 | vendor/sources.json | MIT grant in header |
| stb image/read-write | vendor/sources.json | MIT option retained in vendor/stb-LICENSE and headers |
| PicoDrive, including FAME/CZ80 and embedded helpers | docs/dependencies/md-core-source.json | Noncommercial and per-file notices; PicoDrive-Noncommercial.txt, FAME-License.html, complete original archive |
| PicoDrive submodules: libpicofe, emu2413, dr_libs | same pinned metadata | Upstream README/license files and complete archives; emu2413 MIT, dr_libs alternative permissive/public-domain notices, libpicofe per-file terms |
| Supafaust (Mednafen snes_faust/libretro) | docs/dependencies/snes-core-source.json | GPLv2 and retained per-file notices; shipped Supafaust-GPLv2.txt and full unmodified source archive |
| gpSP and its bundled open BIOS replacement | docs/dependencies/gba-core-source.json | GPLv2; shipped Licenses/gpSP-GPLv2.txt and gpSP-Open-BIOS.txt; complete source archive including bios/source and BIOS build Makefile |
| Gambatte, including libretro-common helpers | docs/dependencies/gb-core-source.json | GPLv2 baseline and exact per-file notices; shipped Licenses/Gambatte-GPLv2.txt from pinned COPYING |
| FCEUmm, including libretro-common helpers | docs/dependencies/core-source.json | GPLv2 baseline and exact per-file MIT/GPL notices; licenses/FCEUmm-GPLv2.txt |
| SDL_GameControllerDB Micro profiles | assets/ControllerMappings/sources.json | zlib-style terms and upstream copyright; assets/ControllerMappings/LICENSE |
| SDL2 Mali-fbdev fork | docs/dependencies/runtime-sources.json | zlib-style source terms; licenses/SDL-LICENSE.txt |
| SDL yuv2rgb | same SDL source archive | BSD-3-Clause; licenses/SDL-yuv2rgb-BSD-3-Clause.txt |
| SDL2_ttf 2.20.2 | docs/dependencies/runtime-sources.json | zlib-style source terms; licenses/SDL_ttf-LICENSE.txt |
| bundled FreeType 2.12.1 | complete SDL2_ttf archive | GPLv2 alternative for GPL-core runners; FTL alternative for the MIT/PicoDrive runner; licenses/FreeType-GPLv2.txt and FreeType-License-Choice.txt |
| FreeType BDF/PCF/hash helpers and bundled zlib 1.2.12 | same complete SDL2_ttf archive | MIT-style and zlib notices retained beside binaries and in source |
| GCC 8.3.0 template/static filesystem support | pinned toolchain image | GPLv3 with GCC Runtime Library Exception 3.1; licenses/GCC-GPLv3.txt and GCC-Runtime-Exception-3.1.txt |
| Ubuntu 0.83 font files | docs/dependencies/asset-inputs.json | Ubuntu Font Licence 1.0, Canonical copyright in font metadata and assets/Fonts/Ubuntu-UFL.txt |
| Lucide SVG/raster icons, including Feather-derived icons | assets/Assets/Icons/sources.json | Complete ISC/MIT notices in assets/Assets/Icons/LICENSE |

FreeType is dual-licensed: GPLv2 is selected for runners using GPLv2 cores, while the MIT/PicoDrive runtime uses FTL and retains its required attribution: this software is based in part on the work of the FreeType Team. FTL is incompatible with GPLv2, so it is not used as the grant for the GPL-core runners. Both upstream license texts and the license-choice explanation accompany the libraries. See [FreeType's license guidance](https://freetype.org/license.html).

The packaged Licenses directory includes standalone terms, asset notices and Nested-Source-Notices.txt with preserved per-file notices from the accompanying source trees. Those archives also contain optional, uncompiled components; retaining their notices does not mean every component is linked into the H700 runtime. The unmodified SDL_ttf archive includes three unused Apple SDL framework binaries with their upstream notices; none is installed or loaded by Rondo on H700.

System libraries are supplied by the owner's BaseOS installation and are not copied into this ZIP: loader, glibc, libstdc++, libgcc_s, ALSA and vendor EGL/GLES/Mali. The SDK itself is a build tool, not a bundled executable. GCC's exception applies to covered runtime/header code generated by an eligible normal GCC compilation; it does not relicense emulator cores or remove their source obligations. Firmware/driver redistribution requires a separate review if it is ever added.

NES/FCEUmm, GB/GBC/Gambatte and GBA/gpSP and the experimental SNES/Supafaust candidate are integrated. Mega Drive/PicoDrive is experimental; PSX/PCSX ReARMed (CHD only) is an experimental integration requiring a user-supplied BIOS; full source and GPL/component notices are included. Review the exact revision and build configuration of each new core before shipping it. Snes9x/PicoDrive have noncommercial conditions that cannot be removed by Rondo's MIT license.

See the [configured shipping audit](docs/shipping-audit.md) for the scope, runtime dependency boundary and remaining release acceptance steps.

The experimental RG34XXSP lid-wake module uses Linux 4.9.170 headers and the audited vendor symbol ABI. The module source is MIT and its kernel module declares Dual MIT/GPL compatibility; the binary package retains GPLv2-compatible distribution terms. Linux corresponding source, configuration and module build scripts accompany H700 packages. GNU BC 1.07.1 (GPLv3 or later) is a build-only tool; its executable is not installed on the handheld.

## OpenBIOS

The bundled OpenBIOS is an open-source PlayStation BIOS implementation by the PCSX-Redux authors, from [Nugget](https://github.com/pcsx-redux/nugget). It is not Sony firmware. Its MIT license, Shinonome public-domain font attribution, pinned source revision and output checksum are retained in `assets/Firmware/OpenBIOS` and `docs/dependencies/openbios-source.json`. Runtime packages include the complete source archive and notices; a pinned offline MIPS rebuild verifies the binary asset. Build-only compiler packages are not part of the installed or distributed runtime.
