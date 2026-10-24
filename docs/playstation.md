# PlayStation (CHD only)

Rondo routes `.chd` discs and `.m3u` playlists containing only relative CHD paths to pinned PCSX ReARMed. CUE/BIN, ISO, PBP, standalone EXE, physical drives, PSP and other PSX cores are outside this integration. The core uses the ARM64 ari64 dynarec on H700 and its interpreter for quiet native tests. The software NEON renderer keeps the existing framebuffer/audio path. Performance on real games requires separate handheld acceptance.

## BIOS and core settings

Open **Settings → Core settings → Sony PlayStation** without starting a game. Change **BIOS mode** with Left/Right; it is saved for the next launch. The initial modes are:

- **OpenBIOS (default):** the bundled open-source BIOS by the PCSX-Redux authors. This is an actual MIPS BIOS implementation under MIT, not a Sony firmware image and not HLE. The pinned Nugget source, its font credits and reproducible build inputs accompany the distribution. Some games may have compatibility issues; upstream tracks them in [OpenBIOS compatibility](https://github.com/pcsx-redux/nugget/blob/main/openbios/COMPATIBILITY.md).
- **User BIOS:** use your own valid 512 KiB PlayStation BIOS from **RondoState/BIOS/PSX**. `--psx-bios DIRECTORY` changes this path. Names such as `scph5500.bin`, `scph5501.bin`, `scph5502.bin`, `scph1001.bin` and `PSXONPSP660.bin` are supported. The pinned core selects among compatible files. See the [core BIOS documentation](https://docs.libretro.com/library/pcsx_rearmed/#bios) for known identities. Rondo checks the size and the structure accepted by the core; that check does not establish provenance. If the selected BIOS is missing, launch explains the directory and offers Core settings. It does not silently change the selected mode.
- **HLE:** the core's built-in replacement for BIOS functions, without a BIOS file. First launch warns in English/Russian about compatibility and memory-card risks. Continue acknowledges the warning once per settings profile; Back cancels without acknowledging it. The acknowledgement survives restarts. `--psx-test-hle` is only a headless test override.

The selected firmware identity participates in slot/automatic-state compatibility. Changing the firmware or its files invalidates incompatible states, but the per-game memory card remains available. No user BIOS is copied into game directories. Sony firmware and owner games are neither downloaded nor included in public artifacts.

### Rebuilding OpenBIOS

[openbios-source.json](dependencies/openbios-source.json) pins the complete Nugget source archive, compiler description, upstream build arguments and output SHA-256. The asset is `assets/Firmware/OpenBIOS/scphOpenBIOS.bin`. Runtime builds compile it again offline and require byte identity with this asset before packaging. `runtime/openbios-toolchain.json` pins build-only Ubuntu MIPS compiler packages by URL/SHA-256; they are fetched and verified before the offline build and are not installed on the handheld or redistributed in the frontend ZIP. The host SDK image remains pinned by digest.

The unmodified upstream source is built with its C shell and fast CD boot, without a BIOS boot menu. Build paths are normalized and the linker build ID is disabled for reproducibility. OpenBIOS and the source archive remain MIT; the Shinonome font provenance/public-domain credits are retained separately. See the complete notices in `assets/Firmware/OpenBIOS` and the ZIP's `Licenses` directory.

## Discs and persistence

A multidisc game has one folder, one relative M3U and its CHDs. The library displays the playlist once and hides its referenced discs. Blank/comment lines, UTF-8 BOM and CRLF are accepted; duplicate/missing discs, absolute paths and paths escaping the playlist directory are rejected. Other image formats inside M3U are rejected. Maximum playlist length is eight discs, matching the pinned core. Rondo writes a normalized runtime playlist into the game state directory; original playlists and CHDs are never rewritten.

Change the **Disc** row in the paused game settings with Left/Right only when the game asks for the next disc. The frontend opens the virtual tray, selects the image and closes the tray. A state records the disc index; manual/automatic load restores it. Disc order and CHD identity apply to the whole playlist, so the game keeps one set of slots and one 128 KiB frontend-managed memory card. Card 2 is disabled; a shared card across unrelated games is not created. Save states include memory-card contents, so restoring an older state can also restore older card data.

The frontend does not allocate an entire CHD or read every gigabyte on each launch. State identity combines ordered CHD headers (including their declared logical SHA-1 and geometry), file sizes, the BIOS set and the core binary. This detects content changes represented by the CHD identity; it is not a deep payload-integrity audit. Private kit preparation uses complete file checksums separately.

## Controls

Digital pad: B → Cross, A → Circle, Y → Square, X → Triangle; L1/R1 and L2/R2 map directly. D-pad, Start and Select map directly. Menu opens Rondo. Analog-only controls, rumble and external Bluetooth controllers are not part of this acceptance.

## Licenses and tests

PCSX ReARMed retains GPLv2 and its per-file notices; original Rondo remains MIT. The regular package includes its complete pinned source archive, GPL text, libchdr BSD notice, public-domain LZMA notice and extracted third-party notices (including miniz, zstd and libretro-common). Unused dependencies remain only in the unmodified source archive, not a claim that every optional upstream component was linked. Source revision and archive SHA-256 are in [psx-core-source.json](dependencies/psx-core-source.json).

The original MIT fixture generator builds a small MIPS PS-X executable, ISO9660 filesystem and Mode 2 CHD without Sony BIOS/game data. It exercises CHD boot, video/RAM execution, audio callbacks, state/playlist-disc roundtrips, card restart, pause/sleep and startup continuation. Technical CHDs are test inputs and are not installed as demo games. Native and isolated BaseOS checks do not establish compatibility, controls or full-speed execution of the owner's games on the handheld.

## BIOS files

The User BIOS loader accepts structurally valid 512 KiB retail images named `scph*.bin` and `psxonpsp660.bin`. Use lowercase names for predictable lookup on case-sensitive filesystems. [Libretro BIOS documentation](https://docs.libretro.com/library/pcsx_rearmed/#bios) lists common names and known identities. The table below also records the European image verified locally:

| Filename | MD5 | Status in local development checks |
| --- | --- | --- |
| `psxonpsp660.bin` | `c53ca5908936d412331790f4426c6c33` | Crash Bandicoot boot checked natively |
| `scph101.bin` | `6e3735ff4c7dc899ee98981385f6f3d0` | Crash Bandicoot boot checked natively |
| `scph7001.bin` | `1e68c231d0896b7eadcad1d7d8e76129` | Crash Bandicoot boot checked natively |
| `scph7502.bin` | `b9d9a0286c33dc6b7237bb13cd46fdee` | Crash Bandicoot boot checked natively |
| `scph1001.bin` | `924e392ed05558ffdb115408c263dccf` | Supported, not tested in this development pass |
| `scph5500.bin`, `scph5501.bin`, `scph5502.bin` | Depends on the exact image | Supported, not tested in this development pass |

The pinned core first probes `psxonpsp660.bin`, `scph101.bin`, `scph7001.bin` and `scph1001.bin`, then scans other `scph` files as needed. It identifies BIOS versions internally; a filename alone is not proof of version or provenance. No BIOS file is selected by the frontend based solely on the game's region. Do not rename one BIOS to pretend it is another. The MD5 table identifies files; integrity of the local private copies is also recorded using SHA-256. Native execution is not handheld acceptance or a BIOS performance comparison.

Rondo ships **only OpenBIOS**, with its source and notices. The proprietary images above remain owner-managed, outside Git, source archives and distribution ZIPs. Install them separately in `RondoState/BIOS/PSX` and choose User BIOS explicitly. Adding files does not silently replace the selected OpenBIOS or HLE mode.

## Display and controllers

PlayStation gameplay uses the core's display aspect ratio (normally 4:3), including when a game submits 512×240 or changes resolution for video. The renderer must use the calculated destination directly rather than fitting the raw pixel aspect a second time. Integer scaling remains a separate per-game/global choice; frames taller than the panel fall back to aspect-correct fitting when no positive integer scale fits. Black borders authored by the game are retained.

**Settings → Core settings** includes **Left stick as D-pad**, enabled by default for digital systems. It applies to NES, GB/GBC, GBA, SNES, Mega Drive and digital PSX, while keeping the physical D-pad active. The right stick has no synthetic digital binding.

**Core settings → Sony PlayStation → Controller** selects Digital (default, for older games) or DualShock. The in-game settings can override this per game; returning to the global value removes the override. Choose before launching if the game only detects its controller at startup. DualShock forwards both sticks, L3/R3, L2/R2 and face/directional buttons without duplicating analog directions as D-pad input. Some games need analog enabled in their own settings; L1 + R1 + Select toggles the emulated Analog button. This is separate from physical rumble support.

Built-in RG34XXSP input uses ANBERNIC-keys evdev axes 2/3/4/5 and buttons 313–316. Axis ranges are read from evdev with the H700 ±4096 fallback, normalized with a dead zone, and cleared on dropped events/disconnect. Mapped external SDL controllers use the same libretro analog contract. Menu resume waits for neutral buttons **and sticks** before forwarding input.

With diagnostics enabled, the journal records raw submitted frame size/pitch, geometry callbacks, display aspect, exact non-black bounds, destination viewport, controller type and normalized built-in axes. Black bounds are diagnostic observations, never automatic crop instructions. Hardware acceptance and title-specific analog support remain separate from automated checks.

### Explicit file selection

In **Core settings → Sony PlayStation**, User BIOS mode exposes **BIOS file**. A opens a confirmation list containing Automatic and detected valid files. Up/Down browses; L1/R1 moves a page; A saves and B cancels without changing the selection. The selection survives restarts and takes effect at the next game launch. OpenBIOS and HLE ignore the saved user-file choice.

Automatic preserves the existing core lookup. A specific file is validated and staged in a private, content-addressed `RondoState/BIOSSelection` directory containing only that image, with a lowercase filename. The core cannot silently choose a higher-priority file from the user's BIOS directory. Missing or invalid selections stop launch and link back to settings; no automatic substitution occurs. State identity includes the chosen firmware; adding unrelated BIOS files does not invalidate an explicit selection's identity.

## Device verification

Verified on RG34XXSP with BaseOS 1.5.2: PSX startup, save/load, continuation, sleep and volume controls. Native and isolated BaseOS 1.3/1.5.2 tests cover core, runner, state and disc behavior. States use the shared [binary format](state-format.md); previous JSON states are not loaded, while the in-game card format stays unchanged.
