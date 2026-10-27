# Setup wizard

A new installation opens an eight-step setup: language, library layout, display/theme, sound, optional Wi-Fi, date/time, systems/cores, and saving/startup behavior. Settings → Interface → Setup wizard runs it again. An existing configuration without setup metadata is treated as an existing installation and is not reset. Unfinished older setup progress resumes at its corresponding step.

Each step edits the same preferences as ordinary Settings. A acts on the focused row: it changes a value, plays the sound example or opens the selected settings screen. Select the separate Next row to apply the step and save its progress. Skip discards changes on that step and continues; Back discards them and returns to the previous step. Earlier confirmed choices remain saved. Menu asks whether to continue later. The next launch resumes unfinished setup. Completing the summary marks setup complete without reapplying every preference.

The first-run baseline is English, Rondo, 70% brightness, 30% volume, Wi-Fi/Bluetooth off, network time enabled, automatic saves and resume offers enabled, and screen blanking after five idle minutes in menus. An existing valid system time zone is retained when available; otherwise UTC is used. Existing installations keep their volume and other choices when rerunning setup.

Changes to language, theme, brightness and volume can be previewed. Sound plays only after selecting Test sound with A or pressing X. The short original example includes a melody, bass and soft percussion. Wi-Fi connection and manual clock/time-zone changes open the ordinary settings screens and return to setup. Those explicit actions are already confirmed: Skip does not disconnect a network or undo a clock change. A Wi-Fi enable or network-time change waits for the service and settings acknowledgement before continuing. Offline setup remains possible.

No games, BIOS files, alternative cores or themes are downloaded. Core selection uses the installed registry described in [preferences](preferences.md). A settings-write failure keeps the current step open. Recovered or newer read-only configurations are handled before automatic setup.
