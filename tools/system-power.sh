#!/bin/sh
# Invoked by the launcher after Rondo has exited, never from the UI process.
set -eu
case "${1:-}" in
 64) operation=poweroff ;;
 65) operation=reboot ;;
 *) exit 2 ;;
esac
# Do not request a system transition if flushing mounted filesystems fails.
sync
exec systemctl "$operation"
