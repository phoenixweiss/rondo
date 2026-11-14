# Back up and restore progress

Open **Settings → Backups** with the game closed, then choose **Create backup**.
Set **Game progress** to **All games**, **Selected** or **None**. Selected lets you
mark games with A, use Y to select or clear the whole list and L1/R1 to page.
Each entry shows its system and card when available. Marks stay selected while
you browse and switch modes. The list includes games with saved progress,
including entries whose ROM is currently missing.

The creation screen shows the number of games included. Settings, favorites,
recent history and per-game preferences are always included as a whole. None
creates a settings-only copy. If saves are on an unavailable card, choose None
to copy settings to an available backup location.

Confirm **Create backup** on this screen to write a dated folder under
`RondoState/Backups` by default, or in the backup location selected in
Settings → Storage locations. Copy the complete folder to your computer to
protect it against loss of the card.

A copy contains Rondo preferences, favorites/recent history and per-game settings.
For the included games it also contains SRAM/RTC, three manual save slots, the
automatic save and game statistics. Valid previous generations are
included. Logs, screenshots, artwork/cache, BIOS, ROMs, saved Wi-Fi passwords and
Bluetooth pairing records are excluded. This is a progress backup, not an SD image.

The folder contains `manifest.json`, its SHA-256 and the mirrored `data` directory.
It is uncompressed and can be inspected with ordinary file tools. Folder names use
UTC; the interface displays your configured time zone. Rondo does not delete old
backups automatically. Keep the files and folder structure together; do not edit
the manifest or checksums. To import a copy from your computer, put its dated
`Rondo-...` folder inside `RondoState/Backups`.

## Inspect or delete a copy

Open a dated copy to see its size, Rondo version and **Games in backup**. The list
shows titles and systems for archived progress; preferences, favorites and history
are restored as a whole. New copies retain game names. Older copies use their
archived history or the installed library when possible; missing names are shown
as **Unknown game**. A settings-only copy has no progress entries.

**Delete backup** opens a separate confirmation with **Cancel** selected. It removes
only that backup folder; installed games, current progress and other copies remain.
A damaged copy can also be deleted. If removal fails, refresh the list and inspect
the reported result before retrying; some files may already have been removed.

## Restore

Open a copy, choose **Restore backup** and confirm; **Cancel** is selected by default.
Rondo checks the manifest, file sizes/hashes, schemas, supported version and active
save-state core fingerprints before replacing data. The reviewed manifest is
checked again if it changes before confirmation. A copy from a newer Rondo version
or active state from another core build is refused. Older recovery states can be
retained alongside a compatible active state, but cannot bypass core/ROM checks.

Preferences and progress for games included in the copy are replaced as a snapshot;
later slots for those games that are absent from the copy are removed. Other games,
ROMs, BIOS and screenshots stay in place. Rondo then restarts its shell to apply
preferences. Network credentials remain those of the destination card.

Game identities are preserved, not guessed from titles. When moving to a different
card, keep volume identities and ROM paths unchanged. Backup restoration does not reassign moved games automatically. Use
[Relocate progress](progress.md) to check and copy matching progress after a move. Raw SRAM/RTC integrity is
checked against the archived bytes; only loading its core can check the expected
memory size. A checksum is not a signature authenticating a copy from someone else.

## Failed or interrupted restoration

Rondo stages and verifies incoming files, saves the previous destination generation,
and flushes a checksummed rollback journal before replacing active files. Until the
commit marker is confirmed, an error rolls back the replaced files. If rollback
itself cannot write, the journal and original copies are retained and startup
retries recovery before settings or cores can write. A separately flushed prepared
journal is available if the primary journal is incomplete.

After a final commit-barrier failure, a complete new generation may already be
visible. Rondo rereads the transaction and restarts the shell rather than blindly
repeating writes or continuing with old preferences. A missing/corrupt rollback
image or unsupported journal stops startup rather than overwriting progress with
defaults. Preserve `.restore-transaction` for diagnosis in that case; do not delete
it to dismiss an error. An interrupted operation can leave an unpublished
`.creating-...` or `.restore-staging-...` directory; these are not offered as copies.

These steps protect against tested process interruption and write errors. They do
not make FAT immune to loss of SD power or physical media damage. Do not remove the
card or intentionally cut power during the operation. Free space is required for
staging, the previous generation and replacement temporaries. Backups are bounded
to 10,000 data files, 34 MiB per container (core payloads remain limited to 32 MiB),
4 MiB settings documents and 16 KiB statistics documents.
