#!/bin/sh
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 PAVEL TKACHEV
# BaseOS's callback also writes UTC to the RTC. Do not mark unsync as success.
set -eu
case "${1:-}" in
 unsync) rm -f /run/baseos-ntp-synchronized; exit 0 ;;
 step) ;;
 stratum|periodic)
  case "${stratum:-}" in 1|2|3|4|5|6|7|8|9|10|11|12|13|14|15) ;; *) exit 0;; esac ;;
 *) exit 0 ;;
esac
exec /usr/sbin/baseos-ntp-notify "$@"
