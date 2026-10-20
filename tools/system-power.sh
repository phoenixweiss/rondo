#!/bin/sh
# BaseOS uses BusyBox init, not systemd. Its systemctl shim ignores power verbs.
# Request the native wrappers only after Rondo has exited and flushed its state.
set -eu
case "${1:-}" in
 64) operation=/usr/sbin/baseos-poweroff ;;
 65) operation=/usr/sbin/baseos-reboot ;;
 *) exit 2 ;;
esac
if [ ! -x "$operation" ]; then
 printf 'BaseOS power wrapper unavailable: %s\n' "$operation" >&2
 exit 1
fi
# Do not request a system transition if flushing mounted filesystems fails.
sync
# These wrappers set/clear the poweroff marker consumed by BusyBox init's rcK.
# Never bypass them with forced BusyBox applets or a success-only systemctl shim.
exec "$operation"
