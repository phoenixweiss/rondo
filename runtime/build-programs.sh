#!/bin/bash
set -euo pipefail
export PATH=/opt/aarch64-nextui-linux-gnu/bin:$PATH
export CC=aarch64-nextui-linux-gnu-gcc CXX=aarch64-nextui-linux-gnu-g++
cd /work/sources/libretro-fceumm-ac0d00a7fc01166f84c7bac01d1084ebfd2b865e
make -j4 platform=unix ARCH=arm64 CC="$CC" CXX="$CXX" HAVE_HDPACK=0 HAVE_NTSC=0 > /work/core-build.log 2>&1
mkdir -p /work/programs
cp fceumm_libretro.so /work/programs/
$CXX -std=c++17 -O2 -Wall -Wextra -I/src/src -I/src/vendor -I/work/sdk/include/SDL2 \
 /src/src/model.cpp /src/src/platform.cpp /src/src/ui.cpp /src/src/core.cpp /src/src/app.cpp /src/src/clock.cpp /src/src/clock_ui.cpp /src/src/wifi.cpp /src/src/wifi_ui.cpp /src/src/main.cpp \
 -L/work/sdk/lib -lSDL2 -lSDL2_ttf -ldl -lpthread -lstdc++fs -o /work/programs/rondo
$CXX -std=c++17 -O2 -Wall -Wextra -I/src/src -I/src/vendor \
 /src/tests/h700_device_tests.cpp /src/src/model.cpp /src/src/platform.cpp \
 -ldl -lpthread -lstdc++fs -Wl,--wrap=ioctl,--wrap=write -o /work/programs/h700-device-tests
$CXX -std=c++17 -O2 -Wall -Wextra -I/src/src -I/src/vendor -I/work/sdk/include/SDL2 \
 /src/tests/device_tests.cpp /src/src/model.cpp /src/src/platform.cpp /src/src/ui.cpp /src/src/core.cpp /src/src/app.cpp /src/src/clock.cpp /src/src/clock_ui.cpp /src/src/wifi.cpp /src/src/wifi_ui.cpp \
 -L/work/sdk/lib -lSDL2 -lSDL2_ttf -ldl -lpthread -lstdc++fs -o /work/programs/device-tests

$CXX -std=c++17 -O2 -Wall -Wextra -I/src/src -I/src/vendor -I/work/sdk/include/SDL2 \
 /src/tests/timezone_tests.cpp /src/src/model.cpp /src/src/platform.cpp /src/src/ui.cpp /src/src/core.cpp /src/src/app.cpp /src/src/clock.cpp /src/src/clock_ui.cpp /src/src/wifi.cpp /src/src/wifi_ui.cpp \
 -L/work/sdk/lib -lSDL2 -lSDL2_ttf -ldl -lpthread -lstdc++fs -o /work/programs/timezone-tests

$CXX -std=c++17 -O2 -Wall -Wextra -I/src/src -I/src/vendor -I/work/sdk/include/SDL2 \
 /src/tests/battery_tests.cpp /src/src/model.cpp /src/src/platform.cpp /src/src/ui.cpp \
 -L/work/sdk/lib -lSDL2 -lSDL2_ttf -ldl -lpthread -lstdc++fs -o /work/programs/battery-tests

$CXX -std=c++17 -O2 -Wall -Wextra -I/src/src -I/src/vendor -I/work/sdk/include/SDL2 \
 /src/tests/wifi_tests.cpp /src/src/model.cpp /src/src/platform.cpp /src/src/ui.cpp /src/src/core.cpp /src/src/app.cpp /src/src/clock.cpp /src/src/clock_ui.cpp /src/src/wifi.cpp /src/src/wifi_ui.cpp \
 -L/work/sdk/lib -lSDL2 -lSDL2_ttf -ldl -lpthread -lstdc++fs -o /work/programs/wifi-tests
$CXX -std=c++17 -O2 -Wall -Wextra -I/src/src -I/src/vendor \
 /src/tests/h700_wifi_tests.cpp /src/src/model.cpp /src/src/platform.cpp /src/src/wifi.cpp \
 -ldl -lpthread -lstdc++fs -Wl,--wrap=posix_spawn -o /work/programs/h700-wifi-tests

$CXX -std=c++17 -O2 -Wall -Wextra -I/src/src -I/src/vendor -I/work/sdk/include/SDL2 \
 /src/tests/clock_tests.cpp /src/src/model.cpp /src/src/platform.cpp /src/src/ui.cpp /src/src/core.cpp /src/src/app.cpp /src/src/clock.cpp /src/src/clock_ui.cpp /src/src/wifi.cpp /src/src/wifi_ui.cpp \
 -L/work/sdk/lib -lSDL2 -lSDL2_ttf -ldl -lpthread -lstdc++fs -o /work/programs/clock-tests
$CXX -std=c++17 -O2 -Wall -Wextra -I/src/src -I/src/vendor \
 /src/tests/h700_clock_tests.cpp /src/src/model.cpp /src/src/platform.cpp /src/src/clock.cpp \
 -ldl -lpthread -lstdc++fs -Wl,--wrap=adjtimex,--wrap=clock_settime,--wrap=clock_gettime,--wrap=ioctl -o /work/programs/h700-clock-tests
