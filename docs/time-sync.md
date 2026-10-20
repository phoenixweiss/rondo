# Date and time

Settings → Date & time provides Automatic sync, Synchronize now,
the current date/time and time-zone selection. Info shows the last successful
synchronization. Automatic sync
defaults to On; it does not enable Wi-Fi. Without a connection it waits. Time-zone
selection is on the same Date & time page and does not change the UTC clock.

Rondo uses BaseOS 1.3.0's timedatectl/baseos-ntp service and BusyBox ntpd, with
BaseOS's configured time servers. It starts asynchronously and restarts when
Wi-Fi obtains an address, including after sleep. Enabling the preference is not
reported as a successful synchronization. A filtered callback event and a
synchronized kernel clock are required. Unsync events and stratum 16 are not
successes; unavailable servers or blocked traffic can prevent synchronization.

Synchronize now requests a fresh update, including when Automatic sync is off.
That one-shot request stops the service after confirmed success or a 90-second
monotonic timeout, without changing the saved automatic preference. During
sleep/exit, a temporary one-shot service is stopped. Automatic mode is managed
by BaseOS and can continue outside the frontend. No global process-name kill
is used. Changing the preference requires service readback; a preference-save
failure attempts to restore the previous service setting.

The callback is installed in writable RAM because FAT may mask executable bits.
It filters BusyBox events before invoking BaseOS's own notifier. That notifier
also attempts hwclock -u -w. This is not acknowledgement of a successful RTC
write: retention after complete power-off must be checked on RG34XXSP.

Only ntp_auto is in config.json. Time/last-sync.json stores a confirmed UTC
history timestamp. It is displayed in the selected time zone and remains
available when automatic mode is off. Existing save timestamps are not rewritten.
New save states add an optional, increasing save_sequence field so Continue
can select the latest write even after time moves backwards. Legacy states
without it retain UTC/mtime ordering; their payload and compatibility checks
are unchanged. Offline manual date/time entry is also available; physical RTC retention remains a separate device check.

## Offline manual setting

Set date & time edits year, month, day, hour and minute in the selected IANA
zone. One line shows DD.MM.YYYY HH:MM. Left/right selects a part, up/down changes its value, Apply confirms the entire draft, and Cancel
discards the draft. Changing month/year clamps the day to the valid month.
The supported local years are 2020–2099, seconds start at zero. Invalid,
nonexistent and ambiguous DST times are rejected rather than silently normalized.

Apply first saves Automatic sync = Off; if persistence fails, neither clocks nor
the time service are changed. BaseOS NTP is then stopped and its exit confirmed,
CLOCK_REALTIME is set and read back, followed by a UTC RTC_SET_TIME/RTC_RD_TIME
check through /dev/rtc0 (or /dev/rtc). A failed RTC write/readback is reported
separately: system time may already be set, but retention is not confirmed.
An unsuccessful system-clock readback is also explicit. Automatic sync can be
re-enabled separately; it is not silently restored over a manual choice.
Manual setting does not create a successful NTP-history entry or rewrite old saves.

The normal macOS adapter remains unavailable and never sets the host clock.
See the [Linux RTC interface](https://docs.kernel.org/admin-guide/rtc.html) and
[clock_settime](https://man7.org/linux/man-pages/man2/clock_settime.2.html).

## Verification

Normal macOS runs use an unavailable adapter. Native tests substitute the time
service and never change macOS clocks or contact time servers. BaseOS checks
use its real service scripts in a read-only chroot, private RAM-backed data/run,
a fake ntpd, a substituted RTC command and a wrapped modes=0 kernel query.
They cannot establish real NTP reachability, clock adjustment or RTC retention.
Owner testing on RG34XXSP accepted network time and candidate 1c1ac5e705d3 for offline manual setting, short full-power-off retention and return to automatic synchronization. This bounded acceptance does not establish long-term RTC durability or all server/network failure cases.

The callback event contract was checked against [BusyBox 1.36.1 ntpd](https://raw.githubusercontent.com/mirror/busybox/1_36_1/networking/ntpd.c).
Kernel state is read with [adjtimex modes=0](https://man7.org/linux/man-pages/man2/adjtimex.2.html).
No BusyBox or BaseOS implementation is copied into Rondo's MIT code.
