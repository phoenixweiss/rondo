# Input test and game vibration

Open **Settings → Controls → Gamepads** to choose a player's number and
label color. **X → Find gamepad** waits for A on the controller you want to
locate. **Auto** gives an external controller the first free player number
when it connects and keeps that number until it disconnects. Built-in controls
always belong to player 1 and can share that port with an external controller.
Choose **Always player 1–4** to save a fixed number or **Menu only** to use
the controller just in Rondo menus. If all four player numbers are in use,
extra Auto controllers can navigate the menus while they wait for a free number. Manually choosing an occupied number shares
that player's input. Games and cores determine which ports are available.
Paired Bluetooth gamepads stay in the list while disconnected. Label colors
and manual player choices are saved per controller.

Open **Settings → Controls → Input test** to check the input from each device.
The diagram follows the last active device. External gamepads show their saved label color and player role
so two of the same model can be told apart. Buttons light up while held. Both sticks show their positions and normalized
axis deflection from −100% to +100%, with 0% at the centre. The diagram reports input, not whether a button is healthy.
An unmapped external controller needs a mapping before its controls appear.

**Start + Select** returns to the previous menu. **Menu** is tested like the other buttons. **L1 + R1 + L2 + R2** requests one
short motor pulse on the device that supplies all four buttons. Release the
combination before trying again. The motor test
respects **Core settings → Game vibration**, which defaults to On. The screen
reports unavailable vibration for a controller without a motor.
Other buttons do not change volume, open menus or enter sleep on this screen.
Lid-close sleep still follows the power preference.

On a keyboard, Z/X/S/A represent A/B/X/Y; Q/W and E/R represent L1/L2 and R1/R2.
T/Y test stick clicks; arrows test the D-pad. Tab tests Menu, +/− test the volume
buttons, and **Enter + Right Shift** exits. Keyboard labels appear when testing
keyboard input.

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
