# System information

Open **Settings → System information** for Device, System, Memory cards and Power. Use Up/Down to scroll, A to open linked details, B to return, X for the full value/source and Y to refresh a group. Navigation and refresh preserve the current selection.

The RG34XXSP manufacturer profile (H700, panel and rated 3300 mAh battery) is separate from observed OS memory, framebuffer geometry and PMIC readings. A framebuffer mode is not a measured physical refresh rate. Rated capacity is not battery health or calibration. Rondo release VERSION and the local source identifier are shown separately; installed core revisions come from the matching installed manifest, never from a planned core.

TF1/TF2 presence comes from block metadata. Whole-card capacity is separate from mounted partitions; available space uses statvfs f_bavail. An absent card, an unmounted card and a failed filesystem reading have different states.

Collection runs in a separate process only on entry or explicit refresh. The UI polls only a nonblocking pipe and never waits on filesystem/hardware I/O. A timeout kills the helper; until it is reaped another request cannot start. Only axp2202-battery and axp2202-usb are inspected; Bluetooth/HID supplies are excluded. No hardware writes, calibration, periodic sensor polling or logs are started by this screen. Existing network-service snapshots provide connection state and IP without passwords.

Voltage and temperature follow the inspected vendor power_supply driver scales (µV and tenths of °C). Temperature can be a fixed fallback when the physical sensor is disabled. Unknown readings are not zero. The inspected vendor charge_full returns configured nominal mAh directly (not standard µAh); it is displayed accordingly and is not a measured full capacity. Other optional readings retain their source and do not imply independently measured battery capacity.

Device testing confirmed the screen on RG34XXSP / BaseOS 1.5.2 with candidate 27c4f81aaeac on 2026-10-22. This confirms the tested interface, not independent sensor accuracy, battery calibration or every missing/error state. Native/headless and isolated BaseOS checks cover additional synthetic cases.
