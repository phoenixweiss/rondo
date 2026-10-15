#!/bin/sh
# BaseOS RG34XXSP only. Keep the first original forever; never reflash a disk.
set -eu
case "${1:-}" in on|off|status) MODE=$1 ;; *) echo 'Expected on, off or status' >&2; exit 2;; esac
grep -q '^ID=baseos$' /etc/os-release
grep -q '^BASEOS_TARGET=rg34xxsp$' /etc/baseos-release
[ -b /dev/mmcblk0p2 ]
APP=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
SD=$(CDPATH= cd -- "$APP/../.." && pwd)
BACKUP="$SD/RondoState/BootLogo"
LOGO="$APP/assets/Boot/bootlogo.bmp"
LOCK=/tmp/rondo-bootlogo.lock
mkdir "$LOCK" || { echo 'Boot logo update already running' >&2; exit 1; }
MOUNT="$LOCK/boot"
cleanup() {
  if mountpoint -q "$MOUNT"; then
    sync
    umount "$MOUNT" || return 1
  fi
  rmdir "$MOUNT" 2>/dev/null || :
  rmdir "$LOCK" 2>/dev/null || :
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
mkdir "$MOUNT"
# Do not take ownership of somebody else's mounted boot partition.
if grep -q '^/dev/mmcblk0p2 ' /proc/mounts; then
  echo 'Boot partition already mounted elsewhere' >&2; exit 1
fi
OPTIONS=rw,utf8
[ "$MODE" != status ] || OPTIONS=ro,utf8
mount -t vfat -o "$OPTIONS" /dev/mmcblk0p2 "$MOUNT"
[ -s "$MOUNT/bootlogo.bmp" ]
if [ "$MODE" = status ]; then
  cmp -s "$LOGO" "$MOUNT/bootlogo.bmp" && exit 10
  if [ -f "$BACKUP/original.sha256" ]; then
    (cd "$BACKUP" && sha256sum -c original.sha256)
    cmp -s "$BACKUP/original.bmp" "$MOUNT/bootlogo.bmp" && exit 11
  fi
  exit 12
fi
mkdir -p "$BACKUP"
if [ ! -e "$BACKUP/original.sha256" ]; then
  [ ! -e "$BACKUP/original.bmp" ] || { echo 'Incomplete original backup' >&2; exit 1; }
  [ "$MODE" = on ] || { echo 'No original backup to restore' >&2; exit 1; }
  cp "$MOUNT/bootlogo.bmp" "$BACKUP/original.bmp.tmp"
  cmp "$MOUNT/bootlogo.bmp" "$BACKUP/original.bmp.tmp"
  mv "$BACKUP/original.bmp.tmp" "$BACKUP/original.bmp"
  (cd "$BACKUP" && sha256sum original.bmp > original.sha256.tmp && mv original.sha256.tmp original.sha256)
  sync
fi
(cd "$BACKUP" && sha256sum -c original.sha256)
SOURCE="$BACKUP/original.bmp"
[ "$MODE" != on ] || SOURCE="$LOGO"
[ -s "$SOURCE" ]
if ! cmp -s "$SOURCE" "$MOUNT/bootlogo.bmp"; then
  cp "$SOURCE" "$MOUNT/rondo-new.bmp"
  cmp "$SOURCE" "$MOUNT/rondo-new.bmp"
  sync
  mv "$MOUNT/rondo-new.bmp" "$MOUNT/bootlogo.bmp"
  sync
  cmp "$SOURCE" "$MOUNT/bootlogo.bmp"
fi
cleanup
trap - EXIT
printf 'Boot logo %s verified; effective next startup.\n' "$MODE"
