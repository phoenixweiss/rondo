# Architecture

Rondo is an independent C++17/SDL2 frontend with a separate libretro runner process. It targets a720x480 interface and keeps a native desktop build for development. The initial hardware adapter is deliberately limited to RG34XXSP on the supported BaseOS environment.

## Components

- model.cpp/model.hpp: library identity and scanning, persistent preferences, locale data and related shared models.
- ui.cpp/ui.hpp: shared SDL surface rendering, fonts/artwork, input hints, settings rows, game menu and feedback components.
- app.cpp/app.hpp: navigation and return context, settings acknowledgements, save-slot presentation and device-policy ownership.
- core.cpp/core.hpp: dynamically loaded libretro callbacks, video/audio, state envelopes, SRAM and screenshot association.
- platform.cpp/platform.hpp: host/device capabilities, telemetry, volume feedback sequencing, early boot branding, brightness and suspend adapters.
- main.cpp: CLI, shell/runner startup, process/socket communication and active input ownership.

The same UI implementation is used in the library and paused game menu. English is the source/fallback language; Russian is selected explicitly and persisted. Source identifiers, paths and emulator IDs are not translated.

## Shell and runner

The shell starts a runner for the selected game and transfers control through an explicit protocol. The active process owns the relevant input/device actions. Returning from a game restores the library context rather than guessing from translated titles. Runner failure has a recoverable route back to the shell.

Separating processes provides failure containment and explicit ownership. It does not automatically settle software-license boundaries: the runner dlopens the core and exchanges libretro function calls/data, so linked-core distribution requirements remain applicable.

## Persistence and feedback

Preferences update only after the applicable authority acknowledges them, with documented session-only exceptions. Save-state envelopes associate state bytes with preview metadata/pixels and a checksum; older states can remain usable without a preview. SRAM and manual save states are separate concepts. Suspend flushes SRAM and retains runtime state in memory; it does not promise persistence through power loss.

Volume feedback holds the last acknowledged level while a change is pending. Missing/error states are explicit and do not pretend to be mute. The compact display is an icon plus bar, with no percentage or pending ellipsis.

## Device boundary

Desktop execution does not perform a handheld boot or expose real brightness/suspend capabilities. The supported BaseOS adapter verifies its environment and checks device operation results. Brightness uses the observed vendor remapping workaround at raw254 for100%, with5% steps and strict readback. Early boot branding preserves the original image and does not add a second application splash.

See compatibility.md for the distinction between native checks, isolated BaseOS userspace and physical device tests. See THIRD_PARTY_NOTICES.md for the licensing boundary; support for additional emulators requires independent build, licensing and device work.
