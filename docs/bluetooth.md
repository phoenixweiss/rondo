# Bluetooth (experimental)

**Controller input is not working on the tested RG34XXSP / 8BitDo Micro D combination.**
The cold-start test paired/connected successfully, loaded both Linux profiles,
and recorded no SDL external input device in menus or the game. The earlier
5–19-second stalls were absent after excluding external HID battery reads,
but one 0.7-second frame remained. Linux HID/input creation and SDL discovery
need further investigation. Connection alone is not usable controller support.

Settings → Network → Bluetooth controls the BaseOS BlueZ adapter. Bluetooth is
off by default. The header symbol appears only after a powered-on readback and
is placed to the left of Wi-Fi. The radio uses BaseOS's existing UART attach and
service scripts; Rondo does not replace Bluetooth firmware or pairing storage.

Devices lists named, paired or HID-advertising devices. Unresolved anonymous
advertisements are omitted; an unnamed HID target has a visible address suffix.
Search keeps a single `bluetoothctl` session alive for up to 30 seconds, updating
the list about once a second while remote names resolve. The same session owns
the pairing agent. Leaving discovery, completing a connection or suspending
stops discovery. Cached-list polling does not disable interactive actions.
Select a device to pair/connect, disconnect or forget it. Forget requires
confirmation and removal readback. Device addresses are validated before entering
argv. Remote names remain display text and never enter commands. Commands use a persistent `bluetoothctl` with a fixed executable, C locale and
bounded replies, without a shell. State is fetched in one `dbus-send`
GetManagedObjects request rather than starting a client per device. No periodic
Bluetooth queries run while the radio is off. Success requires rereading
Powered/Paired/Trusted/Connected properties.

The pairing agent uses NoInputNoOutput. PIN entry and numeric confirmation are
not implemented. Audio routing and game-controller input require separate device
acceptance; a BlueZ connection alone is not proof of either profile working in
Rondo. The last explicitly selected paired device is reconnected after restart
or suspend. An explicit disconnect clears that reconnect target. Pairing records
remain in BaseOS's `/data/bluetooth` storage; Rondo stores only the enabled
preference and reconnect address. Device names and addresses are not journaled.

SDL-mapped gamepads are kept open across menu and game sessions, including
hotplug and disconnect. D-pad and the left stick navigate; the stick uses a
12000-unit dead zone. Guide or Start+Back opens the game menu; Back alone
remains NES Select. Unsupported SDL mappings require a separate mapping entry.

Native and isolated BaseOS tests use a simulated BlueZ protocol, structured
D-Bus snapshots and real child-process transport fixtures (persistent ownership,
EOF, cancellation, timeout and response limits);
virtual SDL gamepads also cover hotplug, buttons, dead zone and release after
disconnect. They never access host Bluetooth. Real pairing, profile availability and repeated
sleep/reconnect cycles still need testing on RG34XXSP.

Protocol reference: [BlueZ bluetoothctl documentation](https://github.com/bluez/bluez/blob/master/doc/bluetoothctl.rst).

With Diagnostics enabled, operation type, elapsed time, state and bounded error
codes are journaled without device names or addresses. The initial device test
failed to pair an 8BitDo Micro in D mode and showed severe shell slowdown. The
cold-start retest paired/connected, but controller input failed as described above.
D is the Android gamepad mode in the [official Micro manual](https://download.8bitdo.com/Manual/Controller/Micro/Micro-Bluetooth-gamepad-8.pdf).

### Micro mapping and input evidence

Rondo loads the four selected Micro profiles in
`assets/ControllerMappings/micro.txt` before opening controllers. The Linux USB
and Bluetooth rows supplement the older SDL fork, whose Linux automatic mapping
routine is disabled. Existing controller mappings remain available. Exact
provenance and retained terms are in that directory; no network download occurs
on the console. A differing firmware GUID or report layout still needs device
acceptance, rather than a guessed mapping for an arbitrary controller.

Opt-in diagnostic records include a bounded `controller_input` snapshot: SDL
platform, loaded profile count, GUID, VID/PID/version, axes/buttons/hats, mapped
status, active button binding indices and aggregate raw/mapped activity counts.
Unknown joysticks are kept open for observation, up to eight external devices;
adding a mapping promotes the same device to game-controller input. Joystick
and controller hotplug/remap events refresh the inventory, not every frame.
Snapshots refresh at most once per second and use the existing bounded journal;
no extra per-button disk writes are introduced. Arbitrary device names, MACs,
serial numbers and individual button histories are not included in this input
snapshot. The GUID identifies the SDL device format, not its Bluetooth address.
