# Nintendo Entertainment System / Famicom

Rondo runs `.nes` cartridge images with FCEUmm. Put your games under
`RondoGames/NES` on TF1 or a prepared TF2. Subfolders are supported; the library
recognizes the system from the file extension. See [two-card storage](storage-layout.md)
for card preparation and data locations.

## Controls

The D-pad, A/B, Start and Select control the game. Menu opens the game menu.
On a keyboard, use the arrows, Z/X for A/B, Enter for Start, Right Shift for
Select and Tab for the game menu. Core button mappings and the option to use
the left stick as a D-pad are available in Settings → Core settings.

External gamepads use the player assignments in Settings → Controls. Two
8BitDo Micro controllers in D mode were tested together in NES on RG34XXSP
with BaseOS 1.5.3. See [Bluetooth setup](bluetooth.md) and
[input testing](input-test.md).

## Display and saves

Global integer scaling, per-game scaling overrides and the selected border
color apply to NES. See [display settings](display.md).

Cartridge save RAM is persisted when the game exposes it. Three manual slots
with previews and a separate automatic save provide save states and
continuation. States are checked against the ROM and core identities before
loading. See [automatic saves](autosave.md), [backups](backups.md) and
[moving games while keeping progress](progress.md).

## Core and verification

The FCEUmm revision and source archive checksum are pinned in
[core-source.json](dependencies/core-source.json). Its GPLv2 license and complete
corresponding source accompany the release package.

Automated checks use the original Rondo Test Cartridge to exercise game
launch, video/audio output, save-state roundtrips, separate game processes and
continuation. On-device testing covered NES gameplay, saves, continuation,
sleep and two-controller play. See [compatibility](compatibility.md) for the
tested environments.
