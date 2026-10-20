# Automatic saves and startup continuation

Settings → Power enables Automatic save and Continue on startup by default.
An orderly exit from an NES game writes `autosave.json` in that game's existing
save directory. It never replaces manual slots 1–3. SRAM is flushed before the
state, which includes a preview and the current ROM/core compatibility identity.
Rondo shows Saving… during the write. The previous valid automatic generation
is retained as `autosave.json.previous`; invalid current data cannot replace it.
Atomic replacement and readback detect write failures. State and SRAM are separate
files, not a multi-file transaction or a power-loss guarantee.

Continue chooses the newest compatible manual or automatic state using save
sequence, then UTC time. A corrupt automatic file falls back to its compatible
previous generation. New game still starts from reset. On a fresh frontend start,
the most recent available game with a compatible state offers Continue / Library;
it never launches without confirmation. B opens the library. Sleep/resume does
not reopen this prompt or serialize the already running core.

If automatic saving fails, the game stays open and paused. Retry or cancel is
available, with an explicit Exit without automatic save choice after the failure.
That choice still requires a successful SRAM flush. Turning Automatic save Off
retains SRAM flushing; existing automatic states remain available for Continue.
Turning Continue on startup Off hides only the startup offer.

Forced Power holds, removal of the SD card, crashes and sudden battery loss cannot
guarantee an exit save. Always leave the game normally, then use the system
shutdown menu and wait until the console is off before removing its card.
Native and BaseOS checks do not replace real-console acceptance of this lifecycle.
