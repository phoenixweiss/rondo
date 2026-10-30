#!/bin/bash
# Shared objects are private to this /work build; never reused across release runs.
set -euo pipefail
export PATH=/opt/aarch64-nextui-linux-gnu/bin:$PATH
export CC=aarch64-nextui-linux-gnu-gcc CXX=aarch64-nextui-linux-gnu-g++
jobs=$(python3 -c 'import json; print(json.load(open("/src/runtime/build-lock.json"))["jobs"])')
cd /work/sources/libretro-fceumm-ac0d00a7fc01166f84c7bac01d1084ebfd2b865e
make -j"$jobs" platform=unix ARCH=arm64 CC="$CC" CXX="$CXX" HAVE_HDPACK=0 HAVE_NTSC=0 > /work/core-build.log 2>&1
mkdir -p /work/programs
cp fceumm_libretro.so /work/programs/
cd "/work/sources/$(python3 -c 'import json; print("gambatte-libretro-"+json.load(open("/src/docs/dependencies/gb-core-source.json"))["revision"])')"
make -f Makefile.libretro -j"$jobs" platform=unix CC="$CC" CXX="$CXX" HAVE_NETWORK=0 GIT_VERSION= > /work/gb-core-build.log 2>&1
cp gambatte_libretro.so /work/programs/
cd "/work/sources/$(python3 -c 'import json; print("gpsp-"+json.load(open("/src/docs/dependencies/gba-core-source.json"))["revision"])')"
make -j"$jobs" platform=arm64 CC="$CC" CXX="$CXX" GIT_VERSION= > /work/gba-core-build.log 2>&1
cp gpsp_libretro.so /work/programs/
cd "/work/sources/$(python3 -c 'import json; print("supafaust-"+json.load(open("/src/docs/dependencies/snes-core-source.json"))["revision"])')"
CXXFLAGS='-mcpu=cortex-a53 -mtune=cortex-a53' make -j"$jobs" platform=unix CC="$CC" CXX="$CXX" GIT_VERSION= > /work/snes-core-build.log 2>&1
cp mednafen_supafaust_libretro.so /work/programs/
cd "/work/sources/$(python3 -c 'import json; print("picodrive-"+json.load(open("/src/docs/dependencies/md-core-source.json"))["revision"])')"
make -f Makefile.libretro -j"$jobs" platform=aarch64 CC="$CC" CXX="$CXX" use_libchdr=0 use_sh2drc=0 use_svpdrc=0 GIT_REVISION= PLATFORM_TREMOR=1 > /work/md-core-build.log 2>&1
cp picodrive_libretro.so /work/programs/
cd "/work/sources/$(python3 -c 'import json; print("pcsx_rearmed-"+json.load(open("/src/docs/dependencies/psx-core-source.json"))["revision"])')"
CFLAGS='-mcpu=cortex-a53 -mtune=cortex-a53' make -f Makefile.libretro -j"$jobs" platform=unix ARCH=aarch64 DYNAREC=ari64 HAVE_CHD=1 HAVE_PHYSICAL_CDROM=0 GIT_VERSION= > /work/psx-core-build.log 2>&1
cp pcsx_rearmed_libretro.so /work/programs/
