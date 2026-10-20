# Game display preferences

Settings → Display contains brightness, the global integer-scaling default and
the game background color. The scaling default is Off, preserving the previous
behavior for games without an explicit preference. Existing per-game On/Off
values retain precedence; no settings migration rewrites those overrides.

Game menu → Game settings cycles Integer scaling through Global, On and Off
with Left/Right or Confirm. Global removes only that game's scaling override,
retains other game options and immediately follows the Display default. These
preferences survive restart and the shell/runner settings-authority exchange.
A rejected write leaves the previous preference active and reports an error.

Background choices are Black (default), Charcoal, Dark blue and Match menus.
They affect only the exposed area around the running game's frame, not the menu
palette or the game pixels. No blur, shaders or new aspect-ratio mode is added.
The choice is global. Integer scaling retains the existing whole-step sizing.

Library headers use All games, Favorite games and the localized {system} games
message, including empty sections. Authored compact system aliases are measured
with the actual font before falling back to a shorter alias. Navigation labels
remain compact; stable system IDs, ROM paths and save identities are unchanged.

Native checks cover existing explicit overrides, inherited defaults, reset to
Global, rejected writes, runner persistence and black-border pixels. They also
render English/Russian Display, game preferences, library titles and power
transitions at 720×480. Physical panel and power behavior require device testing.

The background selector includes a bordered color sample beside its value, including
Match menus. It updates immediately with the selection and current menu palette.
