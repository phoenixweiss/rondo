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
make -C /src -j"$jobs" h700-programs BUILD=/work/programs PKG_CONFIG=true \
 CXX="$CXX" CXXFLAGS='-std=c++17 -O2 -Wall -Wextra' \
 CPPFLAGS='-I/src/src -I/src/vendor -I/work/sdk/include/SDL2' \
 LDLIBS='-L/work/sdk/lib -lSDL2 -lSDL2_ttf -ldl -lpthread -lstdc++fs'
