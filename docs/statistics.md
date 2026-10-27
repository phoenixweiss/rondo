# Game statistics

`RondoState/games/<game-id>/statistics.json` stores a small versioned JSON
aggregate with schema 1, stable `game_id`, `launches`, `played_ms`,
`tracking_started_utc` and `historical_time_unknown`. The game identity uses
Rondo's canonical volume/relative-path identity, without an absolute mount path
or localized title. The file can be retained with the game's saves when moving
the same collection to another card. Renamed/unknown identities require an
explicit mapping; totals must not be blindly added when restoring a copy.

A launch is counted after a separate emulator session has loaded its core and,
when requested, its state, and sent its ready acknowledgement. Starting again
or continuing from the library is a new session. Pause/resume, sleep/wake and
loading a slot within that session do not increment it.

Played time uses a monotonic clock only while the game is running. Pause/menu
and suspend time are excluded. One asynchronous writer keeps absolute snapshots,
with at most one job and checkpoints no more often than once per minute.
Pending totals are flushed on orderly exit and before sleep using the same
atomic-write barriers as other state. Failed writes do not stop emulation or
increment counters again on retry. Abrupt power loss can lose time since the
last completed checkpoint. Invalid existing statistics are left untouched.

Tracking starts with the first recorded session. Old sessions are not inferred
from recent entries, timestamps or save slots. Before tracking starts, Game info
shows a dash rather than zero. Zero elapsed time is valid after initialization.
Statistics contain no scraped descriptions, genres or external metadata.
