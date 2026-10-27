# Storage durability

Settings, save states and persistent game memory are replaced through a unique
same-directory temporary file. Rondo completes the write, flushes and closes
that file, renames it, then flushes the directory. On Linux it also calls
`syncfs` for the owning filesystem, including vendor FAT allocation metadata.
Interrupted write/flush calls are retried; other errors are reported.

An error before rename leaves the active generation intact. An error after
rename can leave the complete new generation visible even though durability
was not confirmed. Callers must inspect actual state before retrying. A valid
previous settings/state generation is retained where applicable. These writes
are not a transaction across several files. Process termination may leave an
incomplete temporary file; Rondo does not blindly delete other writers' files.

Slot deletion and log clearing flush their directory changes before returning
success. Screenshot creation flushes both the complete PNG and its directory,
on the background writer. If the final directory barrier fails, the complete
PNG is retained and the operation reports failure.

Diagnostic appends remain bounded and asynchronous, without per-record fsync.
A partial last record is separated from the next record; readers can skip the
incomplete row. Disposable artwork thumbnails can be regenerated after a failed
write and do not use the save-state durability barriers.

Before sleep, Rondo finishes pending screenshot and volume writes, flushes game
memory and drains diagnostics; the device adapter then calls `sync`. Menu
shutdown/restart finishes application teardown before the launcher calls
`sync` and the native BaseOS power wrapper. Forced power loss can interrupt
these steps. A successful file readback alone does not establish FAT health.

## Checks

`state-tests` exercises short writes, process interruption and failed settings
replacement using per-child file-size limits in private test directories.
`battery-log-tests` checks recovery after a real partial append. These checks
leave the host filesystem and its caches running; they do not simulate removal
of SD power.

On Linux, `python3 tests/storage_sync_tests.py` injects short writes, ENOSPC,
write/fsync/rename/syncfs errors, interrupted syscalls and unsupported directory
fsync into the actual writer through linker adapters. It checks operation
ordering, old/new generation outcomes and temporary-file cleanup. It also runs
as part of the native check suite. Device FAT behavior requires a separate
controlled card test.
