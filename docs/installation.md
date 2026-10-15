# Manual installation and rollback

Development packages currently target Anbernic RG34XXSP with BaseOS 1.3.0. This is early software with a growing feature set; support for other systems and devices is not implied. A frontend ZIP does not prepare or flash a bootable card. A working BaseOS installation is a prerequisite.

Installation is a separate owner-authorized operation. Neither build nor preflight scripts discover disks, format partitions, repair FAT, write boot sectors or copy files to removable media.

1. Shut down the console before removing its card. After connecting it to the computer, identify the intended card again by capacity, connection type and partition layout. Do not reuse an old disk number.
2. Back up the existing System directory, games, save states, SRAM, preferences and any original boot-logo backup to a separate disk. Check that the backup can be read before proceeding. If the filesystem reports errors, diagnose those separately before updating files.
3. Verify the ZIP against its accompanying SHA-256, extract into a new temporary directory and verify PACKAGE.sha256 there. Preserve archive executable permissions. Review System/Rondo/build-info.json for target, BaseOS version and source ID. The optional userspace preflight is described in the [runtime guide](../runtime/README.md).
4. Keep the previous frontend System tree as a rollback copy outside the card. Replace only System/launch_frontend.sh and System/Rondo with the package's equivalents. Preserve any original boot-logo backup from the old app tree. Leave game folders, RondoState, user save data and other BaseOS files in place. Do not install Sources or Licenses as game folders.
5. Read back the copied frontend files and verify System/Rondo/manifest.sha256 on the destination. Verify games, saves and preferences against the backup inventory. Flush writes, safely eject the card and only then disconnect the reader.
6. On the console, check launch, controls, an existing save/resume, audio, brightness and power/lid behavior. The isolated preflight does not prove these hardware paths.

For rollback, shut down and reconnect the card, identify it again, then restore the backed-up frontend files and verify their hashes before ejecting. Do not restore older user state over new saves as part of a frontend rollback. If boot-logo replacement was enabled, retain its saved original and use the supported logo setting/tool to restore it; do not overwrite firmware partitions manually. State compatibility across future core upgrades must be checked separately.
