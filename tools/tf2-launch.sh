#!/bin/sh
# Stable TF2 bridge: runtime and configuration remain on the physical TF1.
# Installed on TF2 only by the explicit card-preparation workflow.
set -eu
rondo_mounts() { cat /proc/mounts; }
rondo_block() { [ -b "$1" ]; }
rondo_pause() { sleep 0.2; }
rondo_mount_tf1() {
 mkdir -p /run/rondo/storage/tf1
 mount -t vfat -o rw,utf8,noatime,shortname=mixed /dev/mmcblk0p7 /run/rondo/storage/tf1
}
rondo_card_root() {
 rondo_mounts | awk -v dev="$1" -v writing="${2:-1}" '
 $1 == dev && ($3 == "vfat" || $3 == "exfat") {
  if (writing && ($4 !~ /(^|,)rw(,|$)/ || $4 ~ /(^|,)ro(,|$)/)) next;
  n++; path=$2
 }
 END { if (n != 1) exit 1; gsub(/\\040/," ",path); gsub(/\\011/,"\t",path); gsub(/\\134/,"\\",path); print path }'
}
rondo_tf2_start() {
 tf2=$(rondo_card_root /dev/mmcblk1p1 0) || { echo 'Rondo: TF2 mount not identified' >&2; return 1; }
 [ "$(pwd -P)" = "$tf2" ] || { echo 'Rondo: TF2 launcher has the wrong working directory' >&2; return 1; }
 rondo_block /dev/mmcblk0p7 || { echo 'Rondo: TF1 is missing' >&2; return 1; }
 tf1=
 # rcS may still be releasing its temporary read-only TF1 configuration mount.
 attempt=0
 while [ "$attempt" -lt 10 ]; do
  if tf1=$(rondo_card_root /dev/mmcblk0p7); then break; fi
  if ! rondo_mounts | grep -q '^/dev/mmcblk0p7 '; then
   if rondo_mount_tf1; then tf1=$(rondo_card_root /dev/mmcblk0p7) || return 1; break; fi
  fi
  attempt=$((attempt + 1)); rondo_pause
 done
 [ -n "$tf1" ] && [ "$tf1" != "$tf2" ] || { echo 'Rondo: writable TF1 is unavailable' >&2; return 1; }
 [ -f "$tf1/System/launch_frontend.sh" ] && [ -f "$tf1/System/Rondo/bin/rondo" ] || {
  echo 'Rondo: install the main Rondo runtime on TF1' >&2; return 1;
 }
 export RONDO_TF2_ROOT="$tf2"
 cd "$tf1"
 exec sh "$tf1/System/launch_frontend.sh"
}
rondo_tf2_start
