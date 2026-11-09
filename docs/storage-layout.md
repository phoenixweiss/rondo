# Storage foundation

Rondo separates six writable data categories: saves (SRAM, RTC, manual states,
automatic continuation and game progress files), screenshots, user BIOS,
backups, diagnostic logs and artwork cache. The policy and transfer API live in
`src/storage.hpp`; no disk formatting or raw-device writing is performed there.

`StorageLayout` takes the TF1 configuration directory and a fresh physical-card
probe. Card roots and stable card identities come from the caller. Volume labels
are not identities. `storageMounts` parses BaseOS mount records for TF1
(`/dev/mmcblk0p7`) and TF2 (`/dev/mmcblk1p1`), checks filesystem type and read-only
flags, decodes escaped mount paths and rejects ambiguous duplicate mounts.
It does not mount, repair, label or provision cards.

Without a placement profile every category resolves to its existing directory
under TF1's `RondoState`. Reading a default profile creates no files. Explicit
placements pin the slot, card CID and filesystem type and volume serial.
A new volume serial on the same CID invalidates placements and interrupted
transfers. Copying a filesystem image to a different CID also changes the
identity. A legacy profile containing only CIDs requires an explicit migration
and is kept intact. Its bindings cannot establish the original volume serial.
Missing, replaced and read-only cards are reported separately. No other card is
silently selected. Paths containing symbolic links are rejected.

The profile is `RondoState/storage.json` on TF1. Game libraries remain distinct
from the placement profile: both cards can contain ROMs regardless of where
saves or screenshots are written.

## Copy and switch

`copyCategory` requires the controller to quiesce the category's readers and
writers first. It rejects an occupied destination, checks available space and
pins both cards throughout the operation. Files stream into a temporary tree,
then their sizes and SHA-256 checksums are checked. Source changes invalidate
the copy. Files and directories are flushed before publishing the destination;
the placement profile is atomically switched last. Copy mode preserves source data.
Move is an explicit option: after commit it checks both cards and the source
inventory again, then removes only verified transferred files. Changed source
files or a read-only card block cleanup; the active destination and remaining
originals are reported separately. The journal allows interrupted cleanup to
resume without deleting new or changed source files.

A journal on TF1 records interrupted transfers. An incomplete temporary tree
remains inactive. `finishPending` can complete a fully published and verified
copy or acknowledge a profile that was already committed before interruption.
It does not choose between differing versions or erase incomplete trees.
Explicit Move cleanup removes only verified transferred files.
The controller must expose inspection and explicit disposition of incomplete
copies; subsequent transfers are blocked while a journal is unresolved.
The transfer lock is released by the OS if the worker process dies.

Hooks permit isolated tests to inject cancellation, short copies, no free
space, card replacement and interruption. Runtime code uses the default
streaming copier and filesystem space checks.

## TF2 boot bridge

BaseOS 1.5.3 selects the frontend on TF2 when that card is mounted. A prepared
TF2 can contain a stable `System/launch_frontend.sh` bridge based on
`tools/tf2-launch.sh`. It identifies TF2 and TF1 by their device mounts, mounts
TF1 only when absent, and launches TF1's installed Rondo. It does not remount
an existing read-only TF1 or run filesystem repair. The main launcher keeps
its application and configuration root on TF1 and adds TF2's `RondoGames` as
a separate library. A missing TF1 or missing runtime is an error.

The bridge is bundled as a preparation asset; ordinary updates do not install
it onto TF2. The card-preparation controller must do that explicitly.
An unprepared TF2 without a frontend retains BaseOS's existing boot behaviour.

## Application routing

`StatePaths` keeps the TF1 configuration root separate from category directories.
Store, progress discovery/relinking, backup creation, deletion and restore use
these paths. Config, favorites, recent history and per-game preferences stay on
TF1; SRAM, RTC, states, game identity and statistics follow the saves category.
Portable backups still use `config.json` and `games/<id>/...` logical paths;
restoring a copy never imports the source card's placement profile.

The BaseOS launcher enables physical storage discovery. Both shell and runner
read the same profile; the shell passes its expected binding to the child, which
rejects a changed profile. The read-only physical probe matches each mounted device and reads its MMC CID,
access flags and first 512 bytes. It derives the identity from the FAT12/16/32
or exFAT volume serial, independently of labels and mutable filesystem fields.
An unreadable or zero serial makes only that card unavailable. Desktop checks
use file-backed boot sectors and private mount and sysfs fixtures.
A byte-identical image restored on the same CID has the same identity. A
formatter must generate a fresh volume serial for the new filesystem.

Restore stages and rollback originals stay on TF1. Active files are replaced
atomically within their destination filesystem. Available space is checked on
TF1 and every affected target; the rollback journal records the placement
binding. Recovery validates all required destinations before changing any file.
A missing or changed card keeps the transaction pending; it cannot redirect a
rollback to another card. Older journals without a binding are recovered only
when saves still use the original TF1 directory.

Core save/load/delete operations and asynchronous statistics, screenshots,
artwork-cache and journal writes validate their destination before file I/O.
Missing saves prevent game launch. Missing artwork storage allows an in-memory
cache; unavailable log storage retains bounded RAM diagnostics. Neither writes
to an alternate card. Raw launcher output also remains in bounded RAM logs.
Shared user BIOS directories are `BIOS/<system>`; PSX additionally supports the
bundled OpenBIOS, HLE and explicit BIOS-directory override.

## Storage screens

Settings → Storage locations shows six independent rows with TF1 and TF2 columns.
The filled circle is the saved location; the outlined cell is focus. Left/right
moves focus, A reviews a change and X opens Files on cards for that category.
Select enters/leaves the action panel. Up at the first row stays in the table;
Back restores the parent screen and its selection.

The review names the source, destination, free space and copy/move consequences.
Cancellation is initially focused in the final confirmation. Readers and writers
are drained before taking the review snapshot, including detailed logs and the
artwork cache. They stay stopped through confirmation and transfer; cancelling
review resumes them. A worker uses its own profile instance, and the application
reopens settings and services only after that worker finishes. An active game or
another write operation blocks transfer. No setting changes on cursor movement.

Incomplete copies have explicit recovery or temporary-copy deletion screens.
Deleting temporary data never deletes the source. A verified published copy can
be completed through the journal. Back from a result returns to current storage;
it never reopens an obsolete progress screen. Copy cancellation is available
before publication/commit; finishing an already published operation is not a
second blind copy.

Files on cards reports existing category data independently of the placement
choice. Cards shows filesystem capacity and a proportional usage bar pinned
above the scrolling list. The worker counts allocated blocks without opening
ROM contents, does not follow directory symlinks and avoids double-counting hard
links. A changed filesystem total or inconsistent allocation produces a stale
result, not invented Other data. Missing, scanning, stale and unavailable data
have separate states. Results are held in memory and can be refreshed explicitly.

Y Colors toggles the diagram's PICO-8 category colors without changing the theme,
card or focused row. The default is theme foreground/background with only the
selected category accented. The shared preference is stored on TF1, so inspecting
a read-only TF2 does not prevent changing diagram colors. Charts describe the
mounted data filesystem; BaseOS partitions are not counted as Rondo files.

## Version choice and collection

An occupied destination is reviewed before any replacement. Unique groups from
both cards are retained and identical groups are deduplicated. Differing groups
require an explicit source or destination choice; no version is selected by
default or inferred from its timestamp. Saves are compared as coherent per-game
bundles, including SRAM, RTC, states and progress files. Manual states from one
version are never silently combined with memory from the other. The comparison
shows size, file and state counts, and the last recorded state time.

A differing save bundle requires matching verified game identity and ROM
fingerprint, readable state metadata and compatible core identities. Missing
identity or a ROM/core mismatch blocks replacement; a matching title alone is
insufficient. Other categories use file or folder groups appropriate to their
layout. Back retains explicit choices and the Copy/Move method.

`resolveCategory` stages a verified union on the destination filesystem and
keeps the original destination in a retired tree. The `.storage-merge.json`
journal on TF1 records both inventories and the profile before and after the
operation. Directory renames publish the complete replacement, then placement
is committed. Retired originals are removed only after the new tree is verified.
Move removes only the source groups actually copied; a source version rejected
in favor of the target remains intact. Unrelated games and files are preserved.

Recovery validates card identities and inventories again. Before commit it can
restore the original destination and discard only the temporary replacement.
After commit it can finish verified cleanup; it cannot discard active data as
though it were a temporary copy. Changed files or cards stop recovery for
inspection. Ordinary cancellation is available while staging, before retirement
and publication begin.

Files on cards also offers copying inactive data from the other card into the
already selected location. This uses the same explicit conflict choices and
transactional verification, with no change to the placement profile.

Byte counts use decimal units (1000 bytes per KB): B/KB/MB/GB/TB in English and
Б/КБ/МБ/ГБ/ТБ with a decimal comma in Russian. Capacity, backups and storage
screens use the same formatter.

## Format TF2

In **Storage locations → Cards**, highlight **TF2** and press **Y**. The
confirmation shows its capacity, starts on **Cancel** and enables **Erase TF2**
after three seconds. Confirm with a new press of **A**.

Formatting requires a mounted, writable TF2 with one recognized data partition,
a verified TF1 running Rondo and at least 30% battery charge. Move any active
storage locations to TF1 and finish any pending transfer or restore first.
Formatting erases the contents of the TF2 partition. Rondo creates a new FAT32
volume with a new volume serial, an empty `RondoGames` directory and the
launcher that hands off to the Rondo installation on TF1.

On BaseOS 1.5.3, a TF2 without a frontend takes priority over TF1 at startup.
The formatting screen is reached through a running two-card setup. If setup
fails after erasing TF2 and Rondo cannot start, power off, remove TF2 and boot
with TF1. System-card provisioning and migration are separate operations.
Cards are not supported for hot-swapping during a game.
