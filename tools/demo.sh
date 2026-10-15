#!/bin/sh
# Explicit manual launch. Opens a window and plays the original NES test tone.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
rondo_demo_dir=${RONDO_DEMO_DIR:-"$root/build/demo"}
core="$root/build/cores/fceumm_libretro.dylib"
[ "$(uname)" != Linux ] || core="$root/build/cores/fceumm_libretro.so"
[ -f "$core" ] || { echo 'Build the pinned core with tools/build_core.py first.' >&2; exit 1; }
if [ ! -d "$rondo_demo_dir/library/card-a" ]; then
 "${PYTHON:-python3}" "$root/tools/fixtures.py" "$rondo_demo_dir/library"
fi
exec "$root/build/rondo" --assets "$root/assets" --core "$core" --state "$rondo_demo_dir/state" \
 --volume "DEMO-A=$rondo_demo_dir/library/card-a" --volume "DEMO-B=$rondo_demo_dir/library/card-b" \
 --output "$rondo_demo_dir/output"
