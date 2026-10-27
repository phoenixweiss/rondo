# Screenshots

In the Rondo interface, **L3 + R3** captures the complete current screen at
720 × 480. On a desktop keyboard, use **F12**. Interface captures are saved in
**RondoState/Screenshots/Interface**. The input test and combination recorder
keep their own button handling and do not trigger screenshots.


During a game, press **L3 + R3** together or open the game menu and select
**Screenshot**. The shortcut also works in the game menu and keeps the game
paused, just like the menu action. Captures contain the
original game frame at the core's output resolution, without Rondo menus,
notifications, display scaling or the surrounding background.

Open **Settings → Interface → Screenshots** to choose the **game shortcut**:

- L3 + R3 (default);
- L2 + R2;
- Select + R1;
- a recorded custom combination;
- Menu only, which disables the shortcut but keeps the game-menu action.

**Record combination** waits for the entry buttons to be released. Hold two to
four buttons together, then release them to save. Menu cancels. The D-pad and
Start + Select are reserved and cannot be used in a custom combination.

Shortcuts use physical buttons before per-core remapping. Rondo briefly defers
their initial presses (up to 70 ms) to recognise a combination and consumes the
completed combination until release. A single press still reaches the game;
if the second button is pressed late, the first may already have reached it.
Choose Menu only for games where reserving a combination is inconvenient.
Holding a combination takes one picture, with no automatic repeat.

Game PNG files are stored under **RondoState/Screenshots/<system>/<game>**, for
example `Screenshots/NES/Battle City/`. Different same-title versions within one
system receive numbered folders such as `Battle City (2)`. Filenames include
date/time in Rondo's selected time zone and a sequence suffix; existing captures are never overwritten.
Earlier title/hash directories are retained without moving or deleting files.
On a keyboard, T + Y maps to the default in-game stick-click shortcut; W + R
maps to L2 + R2, and Right Shift + E to Select + R1.

Encoding and file writes run in the background with one capture in progress
at a time. A brief message confirms completion or reports failure. Wait for
confirmation before removing the card. Sleep and normal shutdown finish a
pending capture before handing control back to the system.

The interface shortcut stays L3 + R3 independently of the game shortcut. F12
also captures the raw game frame during gameplay or in the pause menu.

Screenshot sound is enabled by default and can be disabled in screenshot settings. A short shutter click plays after a successful PNG write, at the current game volume. During gameplay it is mixed into the existing audio stream; it does not open another audio device. Failed writes do not play the click. Interface captures follow the same sound and volume settings.
