# Compatibility and verification scope

Rondo is in early development. The initial handheld target is Anbernic RG34XXSP,720x480, using BaseOS1.3.0 and its vendor kernel/runtime. No support claim is made for another H700 model solely because it has the same processor.

| Context | Established evidence | Limit |
|---|---|---|
| Native macOS | Original prototype used interactively; model/core/runner/UI tests | Not a complete handheld emulator; no real panel/brightness/sleep adapter |
| Native Linux ARM64 | Clean source copy built and tested in Ubuntu24.04 Docker, including real FCEUmm/runner/save checks | Dummy video/audio; not a physical Linux desktop test |
| Exact BaseOS ARM64 userspace | Staging package built twice with identical ZIP hashes; BaseOS1.3.0 loader/dependency closure, headless NES save/resume and synthetic brightness/power tests passed | Container/chroot execution cannot confirm physical drivers |
| RG34XXSP | Owner reports working library, NES play/save/resume, scaling, volume and device controls through successive test kits | Bounded owner testing, not a full compatibility or durability campaign |
| Brightness | Vendor255→200 remapping diagnosed from exact kernel and physical log; endpoint254 implemented and exercised in later owner testing | Not a panel luminance calibration |
| Power/lid sleep | Owner accepted recovery candidate 2b1d4eec7f1f on 2026-10-16 after the experimental lid-wake regression was rolled back | Close to sleep; open, then press Power to wake. Wake-on-open is unavailable; the research module is not loaded |
| Battery status | Owner confirmed measured percentage and its visibility toggle on RG34XXSP; level/charging/unknown layouts have native and ARM64 synthetic tests | Charging/low/unknown states have not each received separate physical acceptance |
| Time-zone picker | Owner accepted localized country/city selection and persistence after restart | Time-zone selection changes display, not the UTC clock |
| Wi-Fi | Owner accepted connection, recovery after reboot/sleep, separate discovery, IP and revised status display on RG34XXSP | Not exhaustive testing of networks, authentication failures or RSSI calibration |
| Date and time | Owner accepted automatic/on-demand network time and candidate 1c1ac5e705d3 for offline manual setting, short power-off retention and return to automatic synchronization | Native/BaseOS tests substitute clocks; owner acceptance is bounded and does not establish long-term RTC durability |
| Compact volume | Latest prototype appearance approved by owner; native state/collision checks | Physical legibility on other screens remains unverified |
| Bluetooth (experimental) | Micro D paired/connected on RG34XXSP; the previous multi-second HID-battery stalls were absent in a cold-start test | Buttons do not work; SDL reports no external device. Audio/PIN are not implemented; no working wireless controller claim |
| Shutdown / restart menu | Native confirmation, teardown and mocked launcher/sync tests pass | Physical H700 shutdown/reboot and FAT durability still need acceptance |
| Other emulators | Planned SNES/MD/PSX (CHD only) | Not integrated, benchmarked or cleared for distribution by this source candidate |
| Two-card integration | Model identity tests | Live TF1/TF2 mounting/integration not established |

Saves and preferences are treated conservatively, but a successful atomic write test is not proof of durability under every power-loss or FAT failure. Back up important state before testing development builds.

The independent frontend build does not require NextUI. It relies on the supported installed BaseOS environment. Vendor OS/firmware, ROMs and proprietary BIOS are not bundled here.

## Release verification

The initial 0.0.1 preview is published. For the network/time release, the owner accepted candidate 1c1ac5e705d3 on the dedicated 8 GB test card. The tagged archive is assembled separately: compare all runtime executables, assets and the launcher with that device-tested candidate, and retain any metadata-only differences. Do not describe a byte-identical executable as a physical test of every file in a newly assembled archive.

| Gate | Evidence / release requirement |
|---|---|
| Source/runtime build | Two independent builds must match for the actual release ref; native checks run on Ubuntu and macOS |
| Licenses and corresponding source | Run tools/audit_h700_package.py on the exact downloaded release ZIP |
| RG34XXSP | Retain bounded owner acceptance and compare delivered runtime files with the accepted candidate; the inactive research module must not be loaded |
| Test media | Dedicated 8 GB card passed full F3 write/read; identify it again before any separately authorized write |
| Publication | Verify the exact annotated tag, branch ancestry, hosted CI, public Release and downloaded asset hashes |
| Other cores/devices | Separate integration, license audit and hardware checks; no additional support claim |

The 0.4.0 sprint (Recent games, automatic exit saves/startup continuation, display
preferences, menu blanking and corner feedback) was accepted by the owner on
RG34XXSP before the final color-sample and input-release fixes. Those two fixes
have native and isolated BaseOS coverage; physical recheck is still pending.

GB/GBC: Gambatte is integrated. Native and isolated BaseOS tests cover cartridge SRAM/RTC, manual/automatic states, startup continuation and aspect/integer scaling. The owner accepted GB/GBC play, saves, continuation and sleep on RG34XXSP with candidate 10f1d3f502e6. Slight interface/game stutter remains in the optimization backlog; a contribution from detailed logging is unproven. This bounded acceptance does not establish measured sustained performance or compatibility with every game.

The owner accepted GBA/gpSP play, saves, continuation and sleep on RG34XXSP with candidate 4ce045a0d3fe. The owner also accepted the fix distinguishing explicitly closed games from unfinished startup sessions with candidate a87f004c7129. Mild slowdown in Breath of Fire’s fire intro and slight audio crackling/stutter near the start of play remain in the optimization backlog; the cause is unproven. The built-in open BIOS may not match official-BIOS compatibility. Sustained H700 dynarec performance has not been measured; acceptance is bounded to the owner’s tests.
