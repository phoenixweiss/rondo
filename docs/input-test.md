# Input test and game vibration

Open **Settings → Diagnostics → Input test**, or the shortcut in **Core settings**.
Buttons light up while held. Both sticks show their positions and normalized
axis deflection from −100% to +100%, with 0% at the centre. The diagram reports input, not whether a button is healthy.
An unmapped external controller needs a mapping before its controls appear.

**Start + Select** returns to the previous menu. **L1 + R1 + L2 + R2** requests one
short motor pulse. Release the combination before trying again. The motor test
respects **Core settings → Game vibration**, which defaults to On. The screen
reports an unavailable or disabled motor instead of claiming a successful test.
Other buttons do not change volume, open menus or enter sleep on this screen.
Lid-close sleep still follows the power preference.

On a keyboard, Z/X/S/A represent A/B/X/Y; Q/W and E/R represent L1/L2 and R1/R2.
T/Y test stick clicks; arrows test the D-pad. Tab tests Menu, +/− test the volume
buttons, and **Enter + Right Shift** exits. An attached mapped gamepad can supply
stick movement. Keyboard labels replace button labels on the diagram.

## Built-in game vibration

Rondo accepts the libretro rumble interface on player port 1. Strong and weak
channels are combined for H700's binary motor at
`/sys/class/power_supply/axp2202-battery/moto`. Driver writes run on a separate
worker and only occur on output changes. Repeated frame heartbeats do not write
sysfs. A 250 ms watchdog stops the motor if emulation stops advancing.

Pause, sleep, controller disconnection and exit clear the channels and request
Off. The shell also clears the motor after the game process ends, including a
failed runner. The launcher clears it on startup and normal shell teardown.
The adapter disables further requests after a driver error. External-controller
rumble and artificial event detection for games without rumble are not connected.

| Bundled core | Native signals handled by this integration |
| --- | --- |
| PCSX ReARMed / PSX | DualShock vibration in games that support and enable it |
| Gambatte / GB, GBC | Rumble cartridges supported by the core, such as MBC5 rumble |
| gpSP / GBA | Cartridge GPIO rumble detected by the core |
| FCEUmm / NES | No native rumble callback in the pinned core |
| Supafaust / SNES | No native rumble callback in the pinned core |
| PicoDrive / Mega Drive | No native rumble callback in the pinned core |

PlayStation's default Digital controller remains unchanged. Choose **DualShock**
for compatible rumble games; some games need vibration enabled in their own menu.
The motor test exercises the output adapter independently of any game.

Native tests use a fake libretro core and motor sink. Isolated BaseOS tests use
synthetic sysfs files; neither verifies physical motor strength or game effects.
