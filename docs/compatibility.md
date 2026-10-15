# Compatibility and verification scope

Rondo is in early development. The initial handheld target is Anbernic RG34XXSP,720x480, using BaseOS1.3.0 and its vendor kernel/runtime. No support claim is made for another H700 model solely because it has the same processor.

| Context | Established evidence | Limit |
|---|---|---|
| Native macOS | Original prototype used interactively; model/core/runner/UI tests | Not a complete handheld emulator; no real panel/brightness/sleep adapter |
| Native Linux ARM64 | Clean source copy built and tested in Ubuntu24.04 Docker, including real FCEUmm/runner/save checks | Dummy video/audio; not a physical Linux desktop test |
| Exact BaseOS ARM64 userspace | Staging package built twice with identical ZIP hashes; BaseOS1.3.0 loader/dependency closure, headless NES save/resume and synthetic brightness/power tests passed | Container/chroot execution cannot confirm physical drivers |
| RG34XXSP | Owner reports working library, NES play/save/resume, scaling, volume and device controls through successive test kits | Bounded owner testing, not a full compatibility or durability campaign |
| Brightness | Vendor255→200 remapping diagnosed from exact kernel and physical log; endpoint254 implemented and exercised in later owner testing | Not a panel luminance calibration |
| Power/lid sleep | Device adapter and failure-path tests; owner reports successful use | Opening the lid alone does not wake the tested BaseOS configuration; use Power |
| Compact volume | Latest prototype appearance approved by owner; native state/collision checks | Physical legibility on other screens remains unverified |
| Other emulators | Planned GB/GBC/GBA/SNES/MD/PSX; possible PSP | Not integrated, benchmarked or cleared for distribution by this source candidate |
| Two-card integration | Model identity tests | Live TF1/TF2 mounting/integration not established |

Saves and preferences are treated conservatively, but a successful atomic write test is not proof of durability under every power-loss or FAT failure. Back up important state before testing development builds.

The independent frontend build does not require NextUI. It relies on the supported installed BaseOS environment. Vendor OS/firmware, ROMs and proprietary BIOS are not bundled here.

## Release acceptance matrix

| Gate | Current evidence | Required before first public binary release |
|---|---|---|
| Source/runtime build | Two clean ARM64 builds matched byte for byte; four binaries matched installed kit10 | Repeat for the actual final release ref |
| Licenses and corresponding source | Configured NES package audited; GPLv2 FreeType alternative, compiler exception and nested notices included | Run tools/audit_h700_package.py on the exact release ZIP |
| Real RG34XXSP | Owner accepted installed kit10; newly assembled packaging not installed | Check the actual release package on the target |
| Test media | Dedicated 8 GB card passed full F3 write/read after reconnect on 2026-10-15 with zero lost/corrupt/overwritten sectors | Identify the card again before any separately authorized write; this is a media test, not boot validation |
| Publication | Public dev branch; native Ubuntu24.04/macOS15 CI passed for 9cac15b; no binary release | Verify the release ref, hosted CI, tag and downloaded public artifact |
| Other cores/devices | Not integrated or cleared by this audit | Separate revision/configuration and device checks |
