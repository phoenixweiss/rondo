# Themes, cores and controls

## Color themes

Settings → Interface → Color theme offers Rondo, Nord, Pine, Paper, Rose, Sand and Scarlet. Rondo is the default. Up/Down previews a palette; the check marks the saved choice. Apply saves it, while Back restores the saved palette. A failed settings write also restores the saved palette.

The same palette is used by the library, settings, game menus, slot lists and overlays. Game frames, cover art and saved previews retain their original colors. The game-background setting remains independent: Black, Charcoal and Navy have fixed colors; Theme follows the palette.

When Rondo boot-logo replacement is enabled, applying a theme also updates the boot logo for the next startup. The original BaseOS logo backup is preserved. Turning replacement off restores that original. A boot-file error is reported separately; no second splash or startup delay is added. Old `slate` and `moss` identifiers resolve to Nord and Pine; unknown identifiers use Rondo.

Nord uses colors from the [Nord palette](https://www.nordtheme.com/docs/colors-and-palettes/). Rondo's theme definitions are in `assets/themes.json`.

## Systems and cores

Settings → Emulator cores → Systems and cores lists the registered modules in the installed package. Game Boy and Game Boy Color share Gambatte and one switch. Available ordinary modules are enabled by default; modules explicitly marked experimental in the registry default to disabled. A saved choice takes precedence after an update. Missing modules cannot be enabled. Presence is checked when the shell starts.

Disabling a module prevents launching its games. It does not delete the core, games, favorites, recent history, SRAM or save states, and does not hide games from the library. If all modules are disabled, enable them again in this menu. This is not a downloader or a choice between alternative emulators.

## Button mapping

Settings → Emulator cores → Controls by core selects the core to edit. Each row names a game control on the left and its physical input on the right. Left/Right changes the physical button; `—` disables that control. Changes are saved immediately. Duplicate physical assignments are allowed. Reset asks for confirmation and restores only the selected core's default layout.

Check opens the live input diagram; Start + Select returns to the same editor row. The diagram shows physical inputs before game remapping. Mapping affects gameplay only: menu navigation, power, volume and the menu shortcut stay available. PSX analog axes and the left-stick D-pad option keep their existing behavior.

GB and GBC use one Gambatte mapping. Mega Drive shows A/B/C/X/Y/Z and Mode; PlayStation shows Circle/Cross/Triangle/Square. The default mappings are unchanged.
