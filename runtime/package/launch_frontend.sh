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
# Raw process output stays in RAM. Opt-in, bounded structured journals are
# managed by Rondo itself under RondoState/Logs; no diagnostic SD writes by default.
mkdir -p /tmp/rondo-output
LOG=/tmp/rondo-output/launcher.log
printf '\n--- Rondo startup %s launcher_pid=%s ---\n' "$(date '+%Y-%m-%dT%H:%M:%S%z')" "$$" >> "$LOG"
# Physical testing rejected the experimental hall-wake adapter. Do not load it.
# A cold boot restores the vendor Power-wake path without changing saved settings.
printf 'Lid-wake experiment disabled after device failure; using stock Power wake.\n' >> "$LOG"
VOLUME="RONDO-DEMO=$APP/Demo/card-a"
[ ! -d "$SD/RondoGames/NES" ] || VOLUME="RONDO-TEST=$SD/RondoGames"
# Use the explicit test-library directory when present; otherwise the original demo.
run_frontend() {
 set +e
 "$RONDO_RUNTIME_LOADER" "$APP/bin/rondo" --assets "$APP/assets" \
  --core "$APP/cores/fceumm_libretro.so" --volume "$VOLUME" \
  --state "$SD/RondoState" --output /tmp/rondo-output
 printf '%s\n' "$?" > /tmp/rondo-output/exit-status
}
# Keep raw library/ALSA stderr in two bounded RAM files, never an unbounded SD log.
run_frontend 2>&1 | awk -v root=/tmp/rondo-output '
 BEGIN {slot=0;n=0}
 { path=root "/process-" slot ".log";
   if (n % 512 == 0) { close(path); printf "%s", "" > path; close(path) }
   print substr($0,1,2048) >> path; close(path);
   n++; if (n % 512 == 0) slot=1-slot
 }'
status=$(cat /tmp/rondo-output/exit-status)
case "$status" in
 64|65)
  # The child has exited: SDL/audio, SRAM and diagnostic writers are closed.
  if sh "$APP/tools/system-power.sh" "$status" >> "$LOG" 2>&1; then exit 0; fi
  printf 'System power request failed; inspect launcher.log.\n' >> "$LOG"
  ;;
esac
if [ "$status" = 0 ]; then
 printf 'Rondo exited normally.\n' >> "$LOG"
else
 printf 'Rondo stopped with status %s; device validation is incomplete.\n' "$status" >> "$LOG"
fi
# Avoid init respawn loops on a missing GPU/audio dependency. Use SSH to inspect
# the log, then power off. Deactivation is documented; no other frontend needed.
while :; do sleep 60; done
