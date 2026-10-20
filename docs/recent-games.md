# Recent games

The clock tab in the library shows the last 20 successfully started games,
most recent first. It uses the existing Continue / New game choice when a
compatible manual or automatic state is available. History itself does not
write save states or hold multiple cores in memory. See [Automatic saves](autosave.md)
for the separate exit-save and startup-resume options.

The shell updates history only after the runner loads the game (and requested
resume state) and acknowledges its first frame. Opening a game menu, cancelling
a launch and failed core/ROM/resume loads do not change it. A later crash does
not undo a game that did start successfully. A history-write failure is reported
when returning to the library and does not stop gameplay.

History is a bounded MRU array in the existing versioned preferences document,
not a timestamp sort. Clock corrections therefore do not reorder entries. Each
record retains volume ID, relative ROM path, stable game ID, title and system.
A repeated launch moves the existing entry to the front without duplicates.
Malformed entries are dropped without resetting unrelated preferences.

Missing volumes/files retain a disabled recent entry and an unavailable message;
they never become launchable from a stored path. They are excluded from All and
system lists. Rescanning at the next frontend start resolves a returned card or
file against the actual registered library and original identity. This is not
live TF2 hotplug support. Search and favorites use the existing controls.

Native coverage includes the real two-session runner, failed launches,
persistence, ordering/limit/deduplication, different volume identities,
unavailable/reinserted media, malformed relative paths and rejected writes.
English/Russian SDL captures cover empty, populated and unavailable states.
Physical-console acceptance remains separate.
