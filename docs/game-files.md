# Game files

Open a game’s information with Y in the library, then press MENU for **Files**.
The Files menu offers **Removal preview** and **Find progress**. Progress lookup
is also available directly with Y on the game information screen.

## Review and delete

This screen inventories files before deletion. A opens a
category, then opens the full path of an entry. The D-pad scrolls each list;
L1/R1 changes pages in file lists. B returns one level. Y refreshes the inventory.
X includes or excludes an optional category in the preview total. Choices are
local to this visit and are not saved as preferences.

The ROM or playlist is included by default. Artwork, SRAM/RTC, manual and automatic
save slots (including previous copies), screenshots, statistics and library
records are separate choices, initially excluded. Library records are individual
favorites, recent-game, per-game-preference and resume entries; they do not mean
removing the shared configuration file. The total counts both files and records;
only file sizes contribute to the byte total.

The game card and system appear above the inventory. File views show full paths
and their source categories. Screenshots are associated only through Rondo’s
`.game-id` marker; old unmarked folders are kept. Saves and screenshots are read
from their configured TF1/TF2 locations. An unavailable location makes the
inventory incomplete, with details under **Files to keep**.

CHD discs referenced by another M3U are protected. An unreadable playlist prevents
claiming exclusive ownership of discs on that game volume. Artwork shared by
multiple launchable files in a directory is protected. Unknown files, BIOS,
backup copies and the shared artwork cache stay outside the selected operation.
The inventory never turns a game folder into a recursive deletion target and
skips symbolic links. It does not hash ROM contents or decode artwork.

Press START to review the confirmation. Cancel is selected initially. A on
Cancel or B returns to the inventory; selecting Delete game and pressing A
starts the operation. The game cannot be running. Screenshots, sleep and other
input are blocked while removal is in progress.

Before deletion, Rondo scans the library again and checks the inventory, settings,
card bindings and filesystem identities. Changed files or storage require a fresh
review. Each file is checked again through its parent directory before removal.
Deletion is permanent and stops at the first error. The result shows the actual
removed count; Details lists removed paths, the failure and the operation log.
Returning to the library reloads games and preferences. If history was kept,
a recent entry for the removed game can remain marked unavailable.

A compact operation log is written under `RondoState/Operations/` on TF1 before
the first deletion. It contains a planned-file list and one JSONL event per
completed deletion, plus completion or failure. It does not copy ROM contents.
An interrupted operation can leave a started log without a completion event;
files already removed are not rolled back. Inspect the result before retrying.

