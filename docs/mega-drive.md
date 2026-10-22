# Mega Drive / Genesis

The development build routes `.gen` and `.md` cartridges to PicoDrive. `--md-core` can override the sibling `picodrive_libretro` library. A missing core or rejected ROM opens the recoverable error screen. Existing library/save identities are unchanged. The owner accepted gameplay, save states, continuation and sleep on RG34XXSP / BaseOS 1.5.2 with candidate 27c4f81aaeac on 2026-10-22. Native and isolated userspace checks supplement this bounded device test; none establishes full-game compatibility or sustained performance across all cartridges.

## Default controls

| Sega controller | RG34XXSP button | Mac keyboard |
|---|---|---|
| D-pad | D-pad | Arrow keys |
| A | Y | A |
| B | B | X |
| C | A | Z |
| X | L1 | Q |
| Y | X | S |
| Z | R1 | E |
| Start | Start | Enter |
| Mode | Select | Right Shift |
| Rondo game menu | Menu | Tab |

Port 1 uses a six-button pad. The mapping follows the pinned core's standard libretro descriptors. Per-core remapping and a controller diagram are planned separately; three-button-only game quirks need individual checks.

## Scope and data

Cartridge SRAM/EEPROM uses the existing per-game persistent-memory path. Manual and automatic states include the ROM/core fingerprints; incompatible versions are not silently loaded. Resume, game-menu input release, automatic saving and sleep use the same policies as previous systems. States from other emulators are not promised compatible.

The output uses the core's aspect ratio, or centred integer scaling when enabled. NTSC and PAL timing follow the cartridge/core auto region policy. No Sega boot ROM is required or supplied. Only Mega Drive / Genesis cartridge loading is exposed here: Sega CD/CHD, 32X, Master System, extra pads and other upstream capabilities are outside this integration.

`tools/md_fixture.py` generates an original MIT technical ROM for repeatable video/input/work-RAM state, SRAM and timing checks. It is test material, not a game installed in the owner's library. Commercial ROMs/artwork are never included in public source or the frontend ZIP. The private owner-game kit is separate from that redistributable ZIP.

## Sources and licenses

Exact revisions and archive checksums, including the required libpicofe, emu2413 and dr_libs submodules, are recorded in [md-core-source.json](dependencies/md-core-source.json). Builds disable unused SH2/SVP dynamic recompilers and CHD support; the exposed system is cartridge MD only. Complete source archives accompany the standard package. The original upstream trees and licenses are not rewritten.

[PicoDrive](https://github.com/libretro/picodrive) and its FAME components retain noncommercial conditions. emu2413, dr_libs, libretro-common and other embedded helpers retain their own notices. See [third-party notices](../THIRD_PARTY_NOTICES.md) and [shipping audit](shipping-audit.md). Rondo's MIT license does not remove these conditions. Packaging the components in one download does not give the whole download one license.
