# Battery display

Settings → Power → Battery percentage changes whether the numeric percentage appears. Left/Right sets Off/On; A toggles. The battery icon remains visible. The default is On for existing and new configurations, preserving the previous numeric display. The boolean setting is show_battery_percent.

The icon uses measured capacity from the RG34XXSP battery sysfs adapter. Fill reflects 0–100%, a large light bolt across the battery marks Charging and a slash marks unavailable/invalid data. Levels at or below 15% use the existing accent color; this is a display threshold, not battery calibration or an automatic shutdown policy. Missing capacity is never rendered as zero. There is no remaining-runtime prediction.

The setting is shared with the runner through the existing settings authority and affects header bars in the library, settings and game menus. Fullscreen gameplay has no new permanent HUD. On macOS the device adapter has no handheld battery reading, so the honest result is the unknown icon, with a dash only when the numeric display is enabled.

Native tests cover level/charging/unknown images, hiding only the percentage, old configurations, invalid setting types, persistence, rejected writes and independence from brightness. The owner confirmed percentage display and hiding on RG34XXSP on 2026-10-16. Charging, low and unknown states still need separate physical acceptance.

The header status group is aligned to the right with consistent gaps. Disabled Wi-Fi and hidden percentages leave no reserved space; the remaining indicators close the gap while the clock stays at the right edge. The order is Wi-Fi, battery (with optional charging mark and percentage), then clock. The charging mark uses the same light foreground as the clock with a dark outline separating it from the battery fill and border.


## Read-only diagnostic journal

Logging is **off by default**. Settings → Diagnostics enables debug mode and
selects Errors, Events or Detailed using Left/Right. Errors records failed
operations; Events adds controls, screen context and lifecycle/sleep markers;
Detailed also samples raw battery/USB changes (five-second polling, otherwise
one record per minute). No setting recalibrates or smooths the gauge.

Ordinary events are short timestamped text lines in `RondoState/Logs/shell.log`
and `runner.log`. Each includes monotonic time, role, event and a compact JSON
context after a tab, so tools can still parse it. Events do not read sysfs, the
device tree, mount tables or the kernel ring. Detailed mode alone adds periodic
battery telemetry to `shell.jsonl`/`runner.jsonl` (at most once per five seconds;
unchanged observations are suppressed for up to a minute). Debug remains off
by default. Filtering happens before constructing controller/video context.
Video context never scans framebuffer pixels; action traces retain at most 256
entries even with debugging disabled.

Both streams rotate independently to `*.previous.log`/`*.previous.jsonl` at
2 MiB per active/previous file. Recovery files are separately bounded to 512 KiB
plus separators. A process has a background writer and a bounded 64-entry queue;
ordinary events never force per-line fsync. The Clear logs action removes both
formats and their previous/recovered generations, including retained older logs.

If SD writes fail, a bounded RAM journal under `/tmp/rondo-diagnostics/Logs`
retains two 256 KiB generations per role. After writing recovers, the data is
copied durably to the card's recovered file before the RAM files are removed.
A runner restart can recover the previous runner's RAM journal. **A power-off
loses RAM data if card writes never recovered.** A full discharge is therefore
not guaranteed to leave a complete journal on a failing filesystem.

Clear logs requires confirmation and removes only fixed Rondo journal names,
RAM copies and legacy battery/launcher journals. Games, saves and settings are
kept. If logging stays enabled, new records appear immediately after clearing.
A deletion failure remains visible; it is not reported as success.
Desktop/headless checks use isolated state directories and no physical adapters.

Rows include UTC seconds, monotonic milliseconds, boot ID, raw `/proc/uptime`,
kernel/model, process role/PID, screen, pause state and brightness/volume settings.
UTC can jump when the clock is corrected; use boot ID, append order, monotonic
and uptime observations when comparing sessions. No game titles, ROM paths,
network names, credentials or device serial numbers are collected.

For each available power supply, an explicit allowlist reads type, status,
capacity, voltage/current/power, temperature, charge/energy now/full/design,
charge limits, cycle count and time-to-full/empty. Values are **raw strings**:
missing fields are null, zero and signed current remain distinct, malformed
values are retained, oversized/unreadable fields have an error marker. The
standard Linux ABI uses microvolts, microamps, microamp-hours/microwatt-hours,
seconds and tenths of a degree Celsius; vendor compliance is not presumed.
See the [Linux power-supply ABI](https://github.com/torvalds/linux/blob/master/Documentation/ABI/testing/sysfs-class-power).
No `uevent` dump, direct PMIC register access, charger writes or voltage-to-percent
curve is used. A reported `Full` with capacity `85` is preserved as a contradiction
for investigation; it is not converted into 100%.

No observations are possible while the CPU is suspended or off. Resume markers
and later boot sessions bracket those gaps; they do not reconstruct a charge
curve during power-off. A partial last line after power loss is separated from
the next record; consumers should skip incomplete JSON lines. Rotation keeps
recent history rather than an unlimited battery archive.

The inspected [NextUI H700 adapter](https://github.com/pvaibhav/NextUI/blob/b4bc9e3eb03de56688f4a88ae321fb6282328752/workspace/h700/platform/platform.c)
uses the same `capacity` attribute and derives its charging flag from USB online
and positive time-to-full. Its [battery monitor](https://github.com/pvaibhav/NextUI/blob/b4bc9e3eb03de56688f4a88ae321fb6282328752/workspace/all/batmon/batmon.c)
retains percentage/charge sessions. Rondo's journal is independently implemented
and additionally retains raw electrical observations where the driver exposes them.
BaseOS retains the vendor kernel; the upstream 4.9.170 sources included for the
inactive research module are not the exact vendor battery driver. The owner's 2026-10-18 readback captured raw capacity jumping from 87 to 100
within five seconds at unchanged 4.069 V when status became Full; a second
97-to-100 correction was captured later. This disproves a permanent 85% cap,
but does not establish the exact vendor gauge policy or a calibration procedure.
The complete discharge was not logged because of write failures and gaps.

Native and isolated BaseOS tests use fake sysfs/proc directories. Real electrical
readings and sample overhead on an SD card still require physical acceptance.

## Comparing the gauge and header

Each diagnostic row also includes `battery_comparison`. `system_percent` is a
fresh, independently parsed read of `axp2202-battery/capacity` from that row's
raw snapshot. `display.source_percent` is the value passed to the battery
renderer; `display.displayed_percent` and `display.label` are the number and
exact text passed to the text renderer. They can differ from the later system
sample; one is never overwritten by the other. Invalid or missing readings are
null, never zero or a made-up 100%.

Display metadata includes `screen`, `header_visible`, `percentage_visible`,
`rendered_monotonic_ms` and `submitted_monotonic_ms`. `display_age_ms` gives the
age of that submission relative to the journal sample in the same process/boot.
Full-screen gameplay explicitly clears header values; disabling the percentage
retains the icon input but clears the displayed number and label. A new UI or a
supervised shell without a UI has no submitted display observation. Lifecycle
and action records may describe the previous submitted frame, whose screen and
age remain explicit.

Detailed-mode periodic samples run after frame submission. They retain the
five-second poll and one-minute unchanged heartbeat. Changes in the display's
number/text/visibility/screen also trigger a row at the next poll; timestamps
alone never do. Explicit existing action/lifecycle/error events remain recorded
according to the selected level. Debug mode is still off by default.

A submitted value is evidence about the rendering path, **not an independent
measurement of physical panel pixels**. When a visual discrepancy occurs, note
the screen and time (a photo is helpful) and compare the matching rows before
changing the battery curve or assuming the kernel gauge is wrong.


## Runtime battery passport

A passport is collected only for the explicit `battery_passport` diagnostic
request, and cached within that logger instance. Starting a game, menu actions,
sleep and ordinary periodic telemetry do not trigger a device-tree/kernel scan.
This low-level request is available to diagnostic tooling; it is not a new user
settings row. The read-only passport collector is also covered by fixture tests.

The passport records kernel release/build/taint and available driver/module
identifiers, then discovers up to four runtime DT battery nodes matching the
AXP2202 battery compatible. It reads only selected capacity, resistance, charge
voltage/current, warning and status properties. Binary properties preserve byte
length, hex and SHA-256; four-byte values are decoded as big-endian u32. The
`param` phandle must resolve uniquely; `select` must name a safe direct child.
The selected `parameter` blob retains exact bytes/hash for comparison with the
packaged BaseOS DTB. Reads are bounded to 4096 bytes per binary property, 8192
scanned tree entries and 20 recursion levels. Missing, ambiguous, oversized or
unreadable inputs are explicit; they do not become default battery settings.

The explicit passport's `power_kernel` and storage snapshot retain up to 64
filtered AXP2202/model/gauge lines (512 bytes each) from a non-clearing read of
the kernel ring. Permission failures and unsupported/isolated hosts are explicit.
The scan does not collect arbitrary DT properties, serial numbers, boot arguments,
network profiles or the full kernel ring. Runtime DT data describes the selected
configuration, **not proof that the PMIC loaded that model**. No PMIC register,
BROM, charger parameter or gauge reset is accessed. Module identifiers do not
prove the exact vendor source revision. Capacity/display behavior is unchanged.

Periodic supply snapshots also retain raw `charge_counter` when exposed. In the
related AXP2202 implementations inspected, that field is calculated from SOC and
nominal capacity; its presence is not proof of independent coulomb counting.
A runtime passport therefore does not by itself establish battery health or a
safe calibration procedure.

### Peripheral battery isolation

Battery diagnostics read only the H700 PMIC supplies `axp2202-battery` and
`axp2202-usb`. They do not enumerate or open HID/peripheral supply attributes,
including `type` and `capacity`. An unknown supply is outside this collector's
scope. The ordinary header already reads the internal battery directly.

Owner testing of candidate b839fc8e9154 exposed a blocking diagnostic path:
after Micro connected, its HID battery capacity returned read errors while UI
work stalled for seconds. Linux HID capacity requests can wait for a remote
report. Earlier native tests did not cover that case. The regression fixture now
uses foreign supply FIFOs with no writer, under an independent child deadline,
and verifies that a snapshot and action journal complete without opening them.
This does not establish the response time of every internal PMIC attribute.
