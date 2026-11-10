# Compatibility and verification

Rondo targets the Anbernic RG34XXSP with its 720×480 display. The package
baseline is BaseOS 1.3.0. The current handheld test environment is BaseOS 1.5.3,
an upstream prerelease.

## Runtime environments

| Environment | Verification | Scope |
|---|---|---|
| macOS | Native SDL prototype and automated core, runner, save and UI tests | Does not emulate BaseOS or its hardware drivers |
| Linux | Automated native core, runner, save and UI tests | Dummy video/audio |
| BaseOS 1.3.0, 1.4.0 and 1.5.3 | The release ZIP runs through the BaseOS loader in isolated userspace | Launch, saves, library and storage operations, and synthetic device checks |
| RG34XXSP / BaseOS 1.5.3 | On-device checks include library navigation, saves, audio, sleep, NES with two Bluetooth gamepads, and TF2 setup and migration | Current handheld test environment |
| RG34XXSP / BaseOS 1.5.2 | Library, supported systems, saves, continuation, controls, audio and sleep tested on the device | Performance and game compatibility vary by title |

## Systems

| System | Core | Formats and notes |
|---|---|---|
| NES | FCEUmm | See [NES development checks](development.md) |
| Game Boy / Game Boy Color | Gambatte | Cartridge SRAM/RTC, states, continuation and sleep; [details](game-boy.md) |
| Game Boy Advance | gpSP | Saves, continuation and sleep; some games still have minor slowdown or audio stutter; [details](game-boy-advance.md) |
| Super Nintendo | Supafaust | SFC/SMC, SRAM and states; input/volume stutter fixes tested on the device; [details](super-nintendo.md) |
| Mega Drive / Genesis | PicoDrive | GEN/MD cartridges; Sega CD and 32X are not included; [details](mega-drive.md) |
| PlayStation | PCSX ReARMed | CHD and CHD-only M3U, OpenBIOS/user BIOS/HLE, Digital/DualShock; [details](playstation.md) |

## Device features

- Closing the lid sleeps; Power wakes the device. Wake-on-open is unavailable.
- Brightness uses the vendor driver's supported range, including the 254 endpoint.
- Battery level and charging status use vendor readings. Optional smoothing
  reduces visible jumps and briefly holds 100% after a confirmed full charge.
- Wi-Fi connection, reconnect after sleep/reboot and IP display have been tested.
- Network time, localized time-zone selection, offline date/time setting and short
  power-off clock retention have been tested. Long-term RTC retention is unmeasured.
- Shutdown and restart from the menu have been tested on the device.
- Two 8BitDo Micro controllers in D mode were tested together in NES on
  RG34XXSP / BaseOS 1.5.3. Bluetooth audio and PIN entry are unavailable.
- TF2 formatting, booting with both cards and launching games from TF2 have
  been tested on RG34XXSP / BaseOS 1.5.3. See [two-card setup](storage-layout.md).

## Save-state compatibility

Every core uses the [version 2 binary container](state-format.md). Legacy JSON
states are not loaded or deleted. SRAM/RTC and PSX memory-card files retain their
paths and formats. Writes use checksums, atomic replacement, readback and a
previous automatic generation. Forced power-off and FAT corruption can still
lose data. Back up important saves before updating.

Rondo runs independently of NextUI. BaseOS images, commercial games, proprietary
BIOS files and personal saves are not included in the frontend package.
