#!/bin/sh
# BaseOS standalone entry. Does not install or update BaseOS.
set -eu
SD=$(pwd)
APP="$SD/System/Rondo"
export LD_LIBRARY_PATH="$APP/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export RONDO_RUNTIME_LOADER=/lib/ld-linux-aarch64.so.1
export SDL_VIDEODRIVER=mali SDL_AUDIODRIVER=alsa
export RONDO_DEVICE_INPUT=h700 RONDO_INPUT_HINTS=controller
export RONDO_BOOTLOGO_TOOL="$APP/tools/boot-logo.sh"
mkdir -p "$SD/RondoState"
LOG="$SD/RondoState/launcher.log"
printf '\n--- Rondo startup %s launcher_pid=%s ---\n' "$(date '+%Y-%m-%dT%H:%M:%S%z')" "$$" >> "$LOG"
# Physical testing rejected the experimental hall-wake adapter. Do not load it.
# A cold boot restores the vendor Power-wake path without changing saved settings.
printf 'Lid-wake experiment disabled after device failure; using stock Power wake.\n' >> "$LOG"
VOLUME="RONDO-DEMO=$APP/Demo/card-a"
[ ! -d "$SD/RondoGames/NES" ] || VOLUME="RONDO-TEST=$SD/RondoGames"
# Use the explicit test-library directory when present; otherwise the original demo.
if "$RONDO_RUNTIME_LOADER" "$APP/bin/rondo" --assets "$APP/assets" \
 --core "$APP/cores/fceumm_libretro.so" --volume "$VOLUME" \
 --state "$SD/RondoState" --output /tmp/rondo-output >> "$LOG" 2>&1; then
 printf 'Rondo exited normally.\n' >> "$LOG"
else
 status=$?
 printf 'Rondo stopped with status %s; device validation is incomplete.\n' "$status" >> "$LOG"
fi
# Avoid init respawn loops on a missing GPU/audio dependency. Use SSH to inspect
# the log, then power off. Deactivation is documented; no other frontend needed.
while :; do sleep 60; done
