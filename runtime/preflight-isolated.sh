#!/bin/bash
set -euo pipefail
cp -R /package/System/Rondo /baseos/tmp/app
cp /programs/system-info-tests /programs/md-tests /programs/snes-tests /programs/gba-tests /programs/gb-tests /programs/bluetooth-tests /programs/battery-log-tests /programs/clock-tests /programs/h700-clock-tests /programs/battery-tests /programs/timezone-tests /programs/device-tests /programs/h700-device-tests /programs/wifi-tests /programs/h700-wifi-tests /baseos/tmp/
mkdir -p /baseos/sys/power /baseos/sys/class/graphics/fb0 /baseos/sys/class/power_supply/axp2202-battery
printf mem > /baseos/sys/power/state
printf 0 > /baseos/sys/class/graphics/fb0/blank
for name in hallkey work_led os_sleep; do
 printf 1 > "/baseos/sys/class/power_supply/axp2202-battery/$name"
done
mkdir -p /baseos/sys/module/rondo_lid_wake/parameters /baseos/sys/class/input/event42 \
 /baseos/sys/devices/platform/axp2202-pek/input/input42 /baseos/sys/devices/platform/axp2202-pek/power
printf N > /baseos/sys/module/rondo_lid_wake/parameters/wake_on_open
printf 'axp2202-pek\n' > /baseos/sys/devices/platform/axp2202-pek/input/input42/name
printf disabled > /baseos/sys/devices/platform/axp2202-pek/power/wakeup
ln -s /sys/devices/platform/axp2202-pek/input/input42 /baseos/sys/class/input/event42/device
mkdir -p /baseos/sys/class/rfkill/rfkill0 /baseos/sys/class/net/wlan0
printf wlan > /baseos/sys/class/rfkill/rfkill0/type
printf 1 > /baseos/sys/class/rfkill/rfkill0/soft
printf 0 > /baseos/sys/class/rfkill/rfkill0/hard
touch /baseos/dev/disp /baseos/dev/null
printf 'test-only\n' > /baseos/tmp/rondo-synthetic-hardware
printf '{"synthetic":true}\n' > /baseos/tmp/clock-test-guard.json
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RONDO_INPUT_HINTS=controller
export LD_LIBRARY_PATH=/tmp/app/lib
unset RONDO_DEVICE_INPUT RONDO_BOOTLOGO_TOOL
# Exercise the actual BaseOS power wrappers with only their final BusyBox
# operation replaced. No init signal or hardware power operation can run.
for operation in poweroff reboot; do
 grep -F 'BASEOS_BUSYBOX' "/baseos/usr/sbin/baseos-$operation" >/dev/null
done
cat > /baseos/tmp/rondo-power-busybox <<'POWER_STUB'
#!/bin/sh
set -eu
case "$1" in
 poweroff) test -f "$BASEOS_POWEROFF_MARKER" ;;
 reboot) test ! -e "$BASEOS_POWEROFF_MARKER" ;;
 *) exit 1 ;;
esac
printf '%s\n' "$1" >> /tmp/rondo-power-calls
POWER_STUB
chmod 755 /baseos/tmp/rondo-power-busybox
export BASEOS_BUSYBOX=/tmp/rondo-power-busybox BASEOS_POWEROFF_MARKER=/tmp/rondo-power-requested
chroot /baseos /bin/sh /tmp/app/tools/system-power.sh 64
chroot /baseos /bin/sh /tmp/app/tools/system-power.sh 65
unset BASEOS_BUSYBOX BASEOS_POWEROFF_MARKER
chroot /baseos /bin/sh /tmp/app/tools/preflight.sh
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/device-tests /tmp/app/assets /tmp/device-state \
 /tmp/app/cores/fceumm_libretro.so '/tmp/app/Demo/card-a/NES/Rondo Test Cartridge/test.nes'
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/battery-log-tests /tmp/battery-log-state
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/battery-tests /tmp/app/assets /tmp/battery-state
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/timezone-tests /tmp/app/assets /tmp/timezone-state
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/h700-device-tests
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/wifi-tests /tmp/app/assets /tmp/wifi-state
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/h700-wifi-tests
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/bluetooth-tests /tmp/app/assets /tmp/bluetooth-state
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/clock-tests /tmp/app/assets /tmp/clock-state
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/h700-clock-tests
cp -R /programs/test-fixtures /baseos/tmp/test-fixtures
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/gb-tests /tmp/app/assets /tmp/gb-state /tmp/app/cores/fceumm_libretro.so /tmp/app/cores/gambatte_libretro.so /tmp/test-fixtures/gb
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/gba-tests /tmp/app/assets /tmp/gba-state /tmp/app/cores/fceumm_libretro.so /tmp/app/cores/gpsp_libretro.so /tmp/test-fixtures/gba
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/snes-tests /tmp/app/assets /tmp/snes-state /tmp/app/cores/fceumm_libretro.so /tmp/app/cores/mednafen_supafaust_libretro.so /tmp/test-fixtures/snes
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/md-tests /tmp/app/assets /tmp/md-state /tmp/app/cores/fceumm_libretro.so /tmp/app/cores/picodrive_libretro.so /tmp/test-fixtures/md
chroot /baseos /lib/ld-linux-aarch64.so.1 /tmp/system-info-tests /tmp/app/assets /tmp/system-info-state /tmp/app/bin/rondo
mkdir /results/captures
cp /baseos/tmp/rondo-power-calls /results/captures/
cp -R /baseos/tmp/rondo-preflight.* /baseos/tmp/device-state /baseos/tmp/timezone-state /baseos/tmp/battery-state /baseos/tmp/battery-log-state /baseos/tmp/wifi-state /baseos/tmp/clock-state /baseos/tmp/bluetooth-state /baseos/tmp/gb-state /baseos/tmp/gba-state /baseos/tmp/snes-state /baseos/tmp/md-state /baseos/tmp/system-info-state /results/captures/
printf 'PASS: packaged BaseOS userspace and synthetic hardware tests. No physical hardware accessed.\n'
