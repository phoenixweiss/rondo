# Roadmap to the first stable release

[0.12.0](https://github.com/phoenixweiss/rondo/releases/tag/v0.12.0) is a public
early-development preview. The next product milestone is stable 1.0. This is a
bounded scope and an order of work, not a delivery date or a promise to implement
every backlog feature before 1.0. Actual deliveries belong in [CHANGELOG](../CHANGELOG.md).

## Stable scope

- Anbernic RG34XXSP, with BaseOS 1.5.2 as the current development/test base.
- NES, GB/GBC, GBA, SNES, Mega Drive and CHD-only PlayStation. Record specific
  core/game limitations rather than promising every game runs at full speed.
- Both TF1 and TF2 are required before 1.0. Safe single-card use remains supported.
- Browser file transfer, network sync and online updates follow 1.0.
- Bluetooth is experimental until controller input is verified. Unsupported
  wake-on-lid-open stays unavailable. PSP is outside the project scope.
- Other devices, Android and a separate website do not block the stable release.

## What is already available

The released baseline includes list/tile game views, optional system grouping,
search, favorites and recents, artwork caching, game information and local playtime,
PNG screenshots, save-slot deletion/previews, automatic saves/continuation, per-core
button mappings and input testing, the rumble adapter, themes and first-run setup.
Wi-Fi, manual/network time, diagnostics, volume/brightness and normal Power/lid
sleep are implemented. Remaining device checks and defects do not mean these
features need to be implemented again. See the current [capability review](feature-review.md).

## Ordered milestones

### 1. A shorter, verifiable development and release cycle

Measure queueing, dependency setup, compilation and checks separately. Remove
identical native runs caused only by main/dev pushes; reuse validated dependency
and object caches without accepting stale inputs. Separate candidate packaging,
reproducibility checks and publication. Review the cadence of independent full
rebuilds explicitly; retain source/license audits, exact-ref checks and downloaded
artifact verification. Existing release gates remain in force until changed.

Complete when a representative before/after comparison shows less duplicate work,
cache invalidation is tested, and finished runs retain compact evidence and only
the needed packages. Routine development must not require compiling unchanged cores.

### 2. Data safety, recovery and normal power operations

Identify FAT inconsistencies across host/card and console workflows; separate
host-side trash changes from game/state writes. Verify saves, logging, shutdown,
restart, suspend and interrupted/no-space writes. Add versioned user export/restore
for settings, SRAM/RTC, states and statistics, validated before replacing data.
Define safe progress relinking after game moves or renames without relying on titles.

Complete when controlled normal-use cycles preserve data, failures leave a usable
last valid copy, and backup/restore is exercised on a clean copy. Include the final
library status placement and the Settings Sleep action in this hardware regression.
Forced power loss on FAT cannot be advertised as universally lossless.

### 3. Measured performance and audio fixes

Profile heavy PSX, GBA and SNES scenes and a 1000+ game library. Measure frame timing,
audio underruns, input/volume latency, CPU/RAM/storage, and cold/warm artwork caches.
Compare diagnostic levels and relevant BIOS/core modes under matched conditions;
optimize demonstrated bottlenecks rather than assuming logging or firmware is the cause.

Complete when comparable device measurements show the improvement, ordinary input
and volume changes do not stall emulation, and fixes retain saves/sleep/input across
all existing cores. Record concrete unsupported game modes where needed.

### 4. Controls and everyday device behavior

Validate existing mappings/reset, PSX controller modes/sticks, menu transitions and
actual rumble in compatible games. Fix the 8BitDo Micro D input path and reconnect/
sleep behavior without blocking UI or gameplay. If it is not ready at RC, Bluetooth
remains clearly experimental and outside the stable support claim; pairing alone
is insufficient. Additional profiles and Bluetooth audio are separate work.

Complete a device matrix covering representative games, SRAM/states/continuation,
Power/lid sleep, screenshots, sound/brightness, English/Russian UI, Wi-Fi failures,
clock recovery and battery/charging status. Existing features need regression,
not another wholesale interface redesign.

### 5. Two-card library support

Agree the TF1/TF2 data contract and UX first: game/copy identity, locations for
progress/configuration/BIOS/cache, duplicate versions and when to show the source
card. Implement safe migration, missing/returning TF2, read-only/full storage and
consistent search/favorites/recents/statistics. Do not silently merge progress or
promise that removing a working card during play is a supported hot-swap operation.

Complete one hardware pass covering TF1 alone, both cards, duplicates, migration,
missing media and an update without losing progress. Manual card-based game import
remains available; network file transfer is not a dependency.

### 6. Installation, beta/RC and stable 1.0

Provide one clear clean-install/update path from 0.12.0, backup/recovery instructions,
English/Russian setup material, real screenshots and usable diagnostics. Align
package baseline metadata with the declared BaseOS contract. Select and test the
advertised BaseOS matrix before 1.0 instead of testing every historical OS routinely.
Retain component licenses and clarify the technical test-cartridge artwork notice.
Any optional user demo games need verified redistribution rights and explicit credits;
their absence does not block 1.0. A graphical installer is not required.

Freeze features for RC. Exercise clean installation and upgrade with TF2 in two
complete passes. Fix blocking boot/data/power/control/library defects before release;
document actual core/game and experimental-feature limits. Then follow the
[release contract](release.md): Bumpster, CI, draft, asset verification, authorized
publication and public download readback. A green build alone is not the stable gate.

## After 1.0

- Browser transfer of folders/ZIP, then SMB and separately researched USB exchange;
  conflict-aware backup/sync, online updates and artwork acquisition.
- Screenshot gallery, fast-forward/turbo/hotkeys, collections/game switching,
  broader core-option overrides and measured rewind.
- PICO-8 after runtime/content-license work; profiles/PIN, localized game metadata,
  achievements/cheats, CPU/display/LED options and restrained blurred backgrounds.
- Battery-gauge/offline-charging research, wake-on-open and additional controllers/
  rumble profiles. New data-safety or power faults override the ordinary queue.
- A separate Vue website in English/Russian, based on real features, screenshots,
  installation, credits and release history, with its own version/CI/deployment.
- Additional H700 devices and Android tablet/TV interaction as separate projects.

## Working rhythm

Use complete, meaningful batches: quiet native checks, one BaseOS preflight and
one combined hardware test. Do not make every cosmetic edit a separate SD update.
Add new ideas to the later backlog rather than silently enlarging the stable gate.
