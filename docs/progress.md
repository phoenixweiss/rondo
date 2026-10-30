# Keep progress when moving a game

Rondo identifies a library entry by volume and relative ROM path. Renaming or
moving that path creates a different entry. **Relocate progress** can copy verified
progress to the new entry without deleting the original save files.

After a successful launch, Rondo records the game's content identity beside its
progress. For cartridge systems it uses the loaded ROM's SHA-256; for PSX it uses
the ordered CHD header identities and file sizes, independently of the selected
firmware. The PSX check avoids rereading entire disc images; it does not verify
every compressed block or treat a recompressed CHD as the same file automatically.

When an entry has no progress and recorded progress belongs to a missing library
entry, launching it checks for matching content in the background. Rondo offers
the matching sources for confirmation; it never chooses among them by title or
silently overwrites the destination. **Play separately** continues with independent
progress. For a manual check, open **About game → Progress** while the game is closed.

Older progress without a content record can be matched by a save state's ROM and
current-core identities. Raw SRAM/RTC alone or a similar title is not enough.
If no verified match exists, keep the original data and restore the original ROM
path, or launch that original entry once to record its identity before moving it.
Entries whose original ROM is still present are excluded: this operation handles
moves, rather than sharing progress between duplicate games.

After selecting a source, confirm **Relocate progress**. **Cancel** is selected by
default. Rondo checks the ROM again and verifies that the reviewed source has not
changed. It copies SRAM/RTC, manual/automatic saves and statistics, transfers
favorites/history, focus/resume references and per-game settings, then restarts
its shell. Statistics get the destination entry's identity; state containers retain
their ROM/core checks and cannot bypass ordinary compatibility checks.

Existing destination progress or conflicting destination game settings are refused.
The source remains untouched, including after a failed operation. A transaction
journal protects the new files and preferences together; interrupted operations
use the same startup rollback as [backup restoration](backups.md). There is no
merge of two save histories, automatic deletion of old progress, or implicit
reassignment of a backup's volume/path identities. Card removal and power loss
remain subject to the storage limitations described in [storage](storage.md).
