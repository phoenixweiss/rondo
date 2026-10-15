#!/bin/bash
# Executed inside the pinned cross SDK; sources are independent of NextUI.
set -euo pipefail
export PATH=/opt/aarch64-nextui-linux-gnu/bin:$PATH
export CC=aarch64-nextui-linux-gnu-gcc CXX=aarch64-nextui-linux-gnu-g++ AR=aarch64-nextui-linux-gnu-ar RANLIB=aarch64-nextui-linux-gnu-ranlib
unset PKG_CONFIG_SYSROOT_DIR
export PKG_CONFIG_PATH=/work/sdk/lib/pkgconfig
export PKG_CONFIG_LIBDIR=/work/sdk/lib/pkgconfig
mkdir -p /work/sdk
cd /work/sources/SDL-malifbdev-rot-d4a7d7503524cc469fe775242f3f925d4dd56c88
[ -f configure ] || ./autogen.sh > /work/sdl-autogen.log 2>&1
./configure --host=aarch64-nextui-linux-gnu --prefix=/work/sdk --enable-video-mali --enable-alsa --enable-alsa-shared --enable-video-opengles --disable-video-opengl --disable-video-x11 --disable-video-wayland --disable-video-kmsdrm --disable-pulseaudio --disable-jack --disable-esd --disable-arts --disable-nas --disable-sndio --disable-dbus --disable-ibus --disable-fcitx --disable-hidapi --disable-libudev --disable-power --enable-filesystem --disable-sensor --disable-locale --enable-loadso --enable-shared --disable-static --disable-libsamplerate > /work/sdl-config.log 2>&1
make -j4 > /work/sdl-build.log 2>&1
make install >> /work/sdl-build.log 2>&1
cd /work/sources/SDL2_ttf-2.20.2
export PATH=/work/sdk/bin:$PATH
SDL2_CONFIG=/work/sdk/bin/sdl2-config ./configure --host=aarch64-nextui-linux-gnu --prefix=/work/sdk --disable-harfbuzz --enable-shared --disable-static > /work/ttf-config.log 2>&1
make -j4 > /work/ttf-build.log 2>&1
make install >> /work/ttf-build.log 2>&1
