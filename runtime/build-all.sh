#!/bin/bash
# Fresh, offline compilation inside the pinned SDK. No card/rootfs is mounted.
set -euo pipefail
export LC_ALL=C TZ=UTC
export SOURCE_DATE_EPOCH
SOURCE_DATE_EPOCH=$(python3 -c 'import json; print(json.load(open("/src/runtime/build-lock.json"))["source_date_epoch"])')
umask 022
python3 /src/runtime/inputs.py
/bin/bash /src/runtime/build-libraries.sh
/bin/bash /src/runtime/build-programs.sh
/bin/bash /src/runtime/build-lid-module.sh
prefix=/opt/aarch64-nextui-linux-gnu/bin/aarch64-nextui-linux-gnu
mkdir /work/evidence
for name in rondo fceumm_libretro.so gambatte_libretro.so gpsp_libretro.so; do
  "$prefix-readelf" -h -d --version-info "/work/programs/$name" > "/work/evidence/$name.elf.txt"
done
for name in libSDL2-2.0.so.0 libSDL2_ttf-2.0.so.0; do
  "$prefix-readelf" -h -d --version-info "/work/sdk/lib/$name" > "/work/evidence/$name.elf.txt"
done
python3 /src/runtime/package.py
