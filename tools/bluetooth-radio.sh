#!/bin/sh
# BaseOS owns the UART and BlueZ state; do not replace firmware or pairing files.
set -eu
rfkill unblock bluetooth
/mnt/vendor/ctrl/setBluetooth.sh all >/dev/null 2>&1
systemctl start bluetooth >/dev/null 2>&1
n=0
while [ "$n" -lt 5 ]; do
 if [ -d /sys/class/bluetooth/hci0 ]; then exit 0; fi
 n=$((n+1))
 sleep 1
done
exit 1
