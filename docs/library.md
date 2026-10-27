# Library layouts

The existing unified library and list presentation remain the defaults.
**Settings → Interface** offers three independent preferences:

| Preference | Choices |
| --- | --- |
| Game grouping | Unified list or By system |
| Game view | List or Tiles |
| System tiles | 3 × 2, 2 × 2 or Horizontal row |

The editor uses a draft: change rows, select Apply to save, or Back to cancel.
The second setup step offers the same preferences after language selection.
Skipping retains the current choices. Existing installations keep their current
unified list; unfinished older setup resumes at its corresponding step.

By system opens a console selection screen. All, Favorites and Recent above the
consoles open shared collections. Move up from the first console row of the collection to these
shortcuts; Down returns to the consoles. L1/R1 changes pages. Back from the games
returns to the same console and page. Disabled cores retain their existing
launch restriction without hiding games or deleting progress.

The visible action row provides Back, Search, View and Settings where
applicable. Up from the first item of a list focuses these actions; scrolling upward
from a later item stays in the list, including at the top of the viewport.
For console grids, Up enters the actions from the first row of the collection;
for single-row game tiles, Up enters them from any page. Left/Right
selects one, A activates it, and Down returns to the selected game or console.
The root also offers Favorites and Recent, plus All games in system grouping.
Back at the root focuses these actions and never exits Rondo.

**Select** toggles between the visible actions and the current game, console or
search keyboard from any position, without changing selection, query or scroll.
The footer names the destination: Actions from content, Games / Systems /
Keyboard from the action row. On a keyboard, use **F1**; Shift remains available
for typing. Menu still opens Settings directly, and L2/R2 changes the game view.

**Menu** opens Settings directly. Back from settings and child editors restores
the invoking screen, selected game and position. Search is a visible action; results have their own screen. Back from results returns to the same query input;
Back from input restores the original library route. Returning from a game
launched through search restores the results and selection.

**View** opens an Apply/Cancel editor for grouping, game view, game image and
system layout. Left/Right no longer changes artwork while browsing a list.
**L1/R1** changes pages in both lists and tiles. **L2/R2** additionally switches
List/Tiles while retaining the selected game and image type. **X** changes
Favorites. **Y** opens Game info for the selected game; it is also available from the pause menu. The showcase displays local artwork, title, system and recorded statistics. Missing statistics remain unknown.

Search includes known recent games on an unavailable card while retaining their
unavailable status. Queries are session-only. Confirmed view/theme settings
remain applied when returning to a parent; cancel only discards an editor draft.

Both views use the same game identifiers, alphabetical catalog, favorites,
recent history, save files and asynchronous artwork cache. Switching layouts
does not copy or rename games. Four visible tile images share one bounded cache
request, with one neighbouring image prefetched. Images retain their original
proportions and colors; a missing image is shown explicitly.

Console artwork is from Automatic by baxysquare / BAXY Themes: NES front loader,
SNES PAL (SNSP-001), Game Boy, Game Boy Color, Game Boy Advance, Mega Drive
Model 2 and PlayStation. See [asset licenses](../assets/LICENSE.md).

The artwork type is shown in View, without a permanent caption in the library. The position counter and scrollbar remain visible. See [game statistics](statistics.md) for the portable data format and tracking rules.

A visible **Systems** action is available when the immediate parent is not the
console catalog. It opens the catalog without changing the preferred startup
grouping. Back from an open platform with no saved parent also returns to its
console tile. Changing the game view retains this route and game selection.

Game info loads cover artwork when opened in a running game's menu; this is
independent of the library's cover/title/gameplay image preference. The artwork
worker starts on demand while paused.

The game menu is ordered: Resume, Save state, Load state, Screenshot, Game
settings, Game info, Exit game. Exit is always the last row; the existing save
and confirmation behavior is preserved.

In list view, the system code (NES, GB, GBC, GBA, SNES, MD or PSX) and counter
sit together below the artwork on the right, for example `MD   1500/3000`.
Tile view shows the full system name on the left and the counter on the right.
Both views share the same vertical position and text baseline. In search
results, the query appears on the left, after the full system name in tile
view. Text is truncated before the counter. These labels also appear in
All games, Favorites and Recent.

Game info uses the full system name, wrapping to two lines when needed.
