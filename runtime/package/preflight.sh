#!/bin/sh
# Quiet userspace check; writes test state only under /tmp. No hardware UI/audio.
set -eu
APP=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
case "$(uname -m)" in aarch64|arm64) ;; *) echo 'Requires AArch64 Linux.' >&2; exit 1;; esac
[ -f /etc/os-release ] && grep -q '^ID=baseos$' /etc/os-release || { echo 'Requires BaseOS.' >&2; exit 1; }
export LD_LIBRARY_PATH="$APP/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export RONDO_RUNTIME_LOADER=/lib/ld-linux-aarch64.so.1
unset RONDO_DEVICE_INPUT RONDO_BOOTLOGO_TOOL
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RONDO_INPUT_HINTS=controller
cd "$APP"
sha256sum -c manifest.sha256
"$RONDO_RUNTIME_LOADER" --list "$APP/bin/rondo"
"$RONDO_RUNTIME_LOADER" --list "$APP/cores/fceumm_libretro.so"
"$RONDO_RUNTIME_LOADER" --list "$APP/cores/gambatte_libretro.so"
"$RONDO_RUNTIME_LOADER" --list "$APP/cores/gpsp_libretro.so"
"$RONDO_RUNTIME_LOADER" --list "$APP/cores/mednafen_supafaust_libretro.so"
"$RONDO_RUNTIME_LOADER" --list "$APP/cores/picodrive_libretro.so"
# SDL opens these dynamically; check their ELF closure as well.
for library in /usr/lib/libEGL.so.1 /usr/lib/libGLESv2.so.2 /usr/lib/aarch64-linux-gnu/libasound.so.2; do
 [ -f "$library" ] || { echo "Missing platform library: $library" >&2; exit 1; }
 "$RONDO_RUNTIME_LOADER" --list "$library"
done
"$RONDO_RUNTIME_LOADER" "$APP/bin/rondo" --runtime-info
WORK=$(mktemp -d /tmp/rondo-preflight.XXXXXX)
"$RONDO_RUNTIME_LOADER" "$APP/bin/rondo" --assets "$APP/assets" \
 --core "$APP/cores/fceumm_libretro.so" --volume "RONDO-DEMO=$APP/Demo/card-a" \
 --state "$WORK/state" --headless --script "$APP/tools/smoke.json" --output "$WORK/output"
printf 'Userspace preflight passed. Results: %s\nPhysical display/audio/input/power remain unverified.\n' "$WORK"
