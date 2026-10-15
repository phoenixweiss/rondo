#!/bin/bash
set -euo pipefail
cp -R /package/System/Rondo /baseos/tmp/app
cp /programs/device-tests /programs/h700-device-tests /baseos/tmp/
mkdir -p /baseos/sys/power /baseos/sys/class/graphics/fb0 /baseos/sys/class/power_supply/axp2202-battery
printf mem > /baseos/sys/power/state
printf 0 > /baseos/sys/class/graphics/fb0/blank
for name in hallkey work_led os_sleep; do
 printf 1 > "/baseos/sys/class/power_supply/axp2202-battery/$name"
done
touch /baseos/dev/disp
printf 'test-only\n' > /baseos/tmp/rondo-synthetic-hardware
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RONDO_INPUT_HINTS=controller
export LD_LIBRARY_PATH=/tmp/app/lib
unset RONDO_DEVICE_INPUT RONDO_BOOTLOGO_TOOL
chroot /baseos /bin/sh /tmp/app/tools/preflight.sh
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/device-tests /tmp/app/assets /tmp/device-state \
 /tmp/app/cores/fceumm_libretro.so '/tmp/app/Demo/card-a/NES/Rondo Test Cartridge/test.nes'
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/h700-device-tests
mkdir /results/captures
cp -R /baseos/tmp/rondo-preflight.* /baseos/tmp/device-state /results/captures/
printf 'PASS: packaged BaseOS userspace and synthetic hardware tests. No physical hardware accessed.\n'
