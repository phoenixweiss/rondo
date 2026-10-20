#!/bin/sh
# Compile only. Does not package an SDL runtime or install on a card.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
image=ghcr.io/loveretro/h700-toolchain@sha256:c9d160c407570a771683e6a471757b44f6e155a7df7d1804c70f901ee3890da3
mkdir -p "$root/build/h700"
docker run --rm -v "$root:/src:ro" -v "$root/build/h700:/out" --entrypoint /bin/bash "$image" -lc '
 /opt/aarch64-nextui-linux-gnu/bin/aarch64-nextui-linux-gnu-g++ -std=c++17 -O2 -Wall -Wextra \
 -I/src/src -I/src/vendor -I/opt/nextui/include/SDL2 \
 -I/opt/aarch64-nextui-linux-gnu/aarch64-nextui-linux-gnu/libc/usr/include/SDL2 \
 /src/src/model.cpp /src/src/platform.cpp /src/src/ui.cpp /src/src/core.cpp /src/src/app.cpp /src/src/clock.cpp /src/src/clock_ui.cpp /src/src/wifi.cpp /src/src/wifi_ui.cpp /src/src/battery_log.cpp /src/src/bluetooth.cpp /src/src/bluetooth_ui.cpp /src/src/main.cpp \
 -L/opt/nextui/lib -L/opt/aarch64-nextui-linux-gnu/aarch64-nextui-linux-gnu/libc/usr/lib \
 -lSDL2 -lSDL2_ttf -ldl -lpthread -lstdc++fs -o /out/rondo
 /opt/aarch64-nextui-linux-gnu/bin/aarch64-nextui-linux-gnu-readelf -d /out/rondo > /out/dependencies.txt
 /opt/aarch64-nextui-linux-gnu/bin/aarch64-nextui-linux-gnu-readelf --version-info /out/rondo > /out/symbol-versions.txt
'
