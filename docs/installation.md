# Installation and rollback

Rondo targets Anbernic RG34XXSP with a BaseOS 1.3.0 package baseline; the owner also accepted BaseOS 1.5.2 (upstream prerelease). A frontend ZIP does not prepare a bootable card. On a fresh BaseOS card, complete the first console boot before installing Rondo: BaseOS expands and formats its data partition during that boot. Shut down before removing the card.

## macOS installer

The source tree includes tools/install_h700.py. It installs files on explicitly selected, mounted volumes; it never formats a disk, repairs FAT or writes raw sectors. Obtain the source from the release ZIP's Sources/rondo-source.tar.gz, or check out the matching Git tag. Python 3.11+ is required; no Python packages are needed for the installer.

Identify the physical card again by capacity, connection type and partition layout. Mount both its data partition (p7, usually BASEOS) and boot-resource partition (p2, usually Volumn). The paths below are examples; use the actual paths printed by diskutil. Begin with a read-only plan:

```sh
python3 tools/install_h700.py --package /path/to/Rondo.zip \
  --card /Volumes/BASEOS --boot /Volumes/Volumn
```

The installer verifies the ZIP, complete source/license audit, RG34XXSP/BaseOS target and that both volumes belong to one external physical disk with the expected expanded layout. It prints the data volume UUID. Inspect existing contents and check filesystem errors separately before installing.

After authorizing installation, use that UUID and a new backup directory on the Mac, outside both card volumes:

```sh
python3 tools/install_h700.py --package /path/to/Rondo.zip \
  --card /Volumes/BASEOS --boot /Volumes/Volumn \
  --backup /path/on/mac/rondo-backup --expected-uuid DATA-UUID --apply
```

Before changing the card it copies and verifies existing files, including frontend, games, saves, preferences and boot resources. It rechecks destination identity/content after backup. Only package System files and the explicitly managed boot-logo backup/logo are written. Other BaseOS files, games and saves are preserved. It does not mirror-delete old files. A receipt records pre-install hashes and installed hashes in the external backup directory. If interrupted or a check fails, inspect actual files and the receipt before another operation; no automatic retry or rollback is attempted.

Boot-logo replacement defaults to enabled. The installer preserves the first original as RondoState/BootLogo/original.bmp with original.sha256, never overwrites a valid original backup and writes/verifies the Rondo image in p2 before the console is switched on. An existing false replace_boot_logo preference is respected. Corrupt or incomplete original backups cause refusal. No second application splash or delay is added. The existing console setting still restores the original and controls subsequent launches.

After installation, read back the files and bootlogo, check the FAT filesystem, flush writes and safely eject the whole physical disk. Then test initial boot branding, controls, an existing save/resume, audio, brightness and power/lid behavior on the console. Userspace preflight does not establish these hardware paths. Keep TF2 empty while testing TF1: BaseOS prefers a usable TF2 frontend volume.

## Logo and frontend rollback

To restore the verified first original from macOS with the same volume checks:

```sh
python3 tools/install_h700.py --card /Volumes/BASEOS --boot /Volumes/Volumn \
  --expected-uuid DATA-UUID --restore-logo --apply
```

This also persists replace_boot_logo=false so the application does not immediately reapply Rondo branding. Games, SRAM and manual slots are untouched. Preserve the first original backup for later toggles.

For a full frontend rollback, shut down, reconnect and identify the card, then restore the backed-up frontend files and verify their hashes before ejecting. Do not restore old RondoState or saves over new progress as part of a frontend rollback. Newly introduced managed files should only be removed after reviewing the receipt; do not delete an entire card tree. State compatibility across future core upgrades requires a separate check.

Sources and Licenses in the release ZIP stay with the distribution; they are not game folders. No firmware/rootfs, commercial games or proprietary BIOS are bundled. See [runtime verification](../runtime/README.md) and [compatibility](compatibility.md).
