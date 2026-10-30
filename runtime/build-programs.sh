#!/bin/bash
# Shared objects are private to this /work build; never reused across release runs.
set -euo pipefail
export PATH=/opt/aarch64-nextui-linux-gnu/bin:$PATH
export CC=aarch64-nextui-linux-gnu-gcc CXX=aarch64-nextui-linux-gnu-g++
jobs=$(python3 -c 'import json; print(json.load(open("/src/runtime/build-lock.json"))["jobs"])')
make -C /src -j"$jobs" h700-programs BUILD=/work/programs PKG_CONFIG=true \
 CXX="$CXX" CXXFLAGS='-std=c++17 -O2 -Wall -Wextra' \
 CPPFLAGS='-I/src/src -I/src/vendor -I/work/sdk/include/SDL2' \
 LDLIBS='-L/work/sdk/lib -lSDL2 -lSDL2_ttf -ldl -lpthread -lstdc++fs'

cp -R /src/runtime/test-fixtures /work/programs/test-fixtures
