# Compatibility and verification

Rondo is in early development. The handheld target is Anbernic RG34XXSP with a
720×480 display and BaseOS. The package baseline is BaseOS 1.3.0; BaseOS 1.5.2
has also been tested on the device. Other handheld models have not been tested.

## Runtime environments

| Environment | Verification | Scope |
|---|---|---|
| macOS | Native SDL prototype and automated core, runner, save and UI tests | Does not emulate BaseOS or its hardware drivers |
| Linux | Automated native core, runner, save and UI tests | Dummy video/audio |
| BaseOS 1.3.0 and 1.5.2 | Packaged runtime and synthetic device checks in isolated userspace | Physical drivers are tested separately on the handheld |
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
- Battery level and charging status use vendor readings. Gauge jumps remain under
  investigation; Rondo does not calibrate the battery.
- Wi-Fi connection, reconnect after sleep/reboot and IP display have been tested.
- Network time, localized time-zone selection, offline date/time setting and short
  power-off clock retention have been tested. Long-term RTC retention is unmeasured.
- Shutdown and restart from the menu have been tested on the device.
- Bluetooth remains experimental. The 8BitDo Micro in D mode can pair/connect,
  but controller input is not working in the tested setup. Bluetooth audio and
  PIN entry are not implemented.
- Live TF1/TF2 integration has not been tested; current device checks use TF1.

## State format in 0.9.0

Every core uses the [version 2 binary container](state-format.md). Legacy JSON
states are not loaded or deleted. SRAM/RTC and PSX memory-card files retain their
paths and formats. Writes use checksums, atomic replacement, readback and a
previous automatic generation. Forced power-off and FAT corruption can still
lose data; back up important saves before testing development builds.

Rondo runs independently of NextUI. BaseOS images, commercial games, proprietary
BIOS files and personal saves are not included in the frontend package.
