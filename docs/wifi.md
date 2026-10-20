# Wi-Fi settings

Settings → Network → Wi-Fi opens radio control and connection status. The
current network and IP address are shown directly after connection. Open
Wi-Fi networks for discovery and per-network actions; Back returns one level. The
initial scope is RG34XXSP / BaseOS 1.3.0: open and WPA/WPA2-Personal networks.
WEP, enterprise authentication and SAE-only/WPA3-only networks are unavailable.
Hidden-network entry and country/channel controls are not implemented.

Wi-Fi defaults to Off. Enabling waits for BaseOS to finish loading wlan0 and
its WLAN rfkill control, without blocking SDL. Radio readback is checked before
the boolean wifi_enabled is saved. A preference-write failure attempts to
restore the previous radio state; it does not acknowledge a saved preference.
The shell header uses the adapter's confirmed radio state, not a second rfkill
reading or a requested preference. The runner fallback requires an enabled
wlan0 interface as well as unblocked WLAN rfkill. The icon disappears when
Wi-Fi is off and leaves no reserved space. Connected arcs reflect current
RSSI: three at −55 dBm or higher, two at −67, one at −80, zero below that.
Unknown readings use neutral dim arcs, never maximum strength. The shell
requests SIGNAL_POLL; the runner can read negative dBm from proc/net/wireless.
Unavailable or non-dBm measurements remain unknown.

Search lists nearby networks, combines duplicate access points with the same
SSID/security, retains the stronger signal and includes known profiles. Select
a network to connect, disconnect or forget it. Connection is confirmed only
after the selected profile associates and obtains an IPv4 address; this is not
a claim that internet access works. Connection can be cancelled with Disconnect.
Incorrect-key events, command and connection timeouts, blocked radio, another
network owner and save errors remain distinct states.

The handheld keyboard reaches all printable ASCII characters, including case,
symbols and space. X deletes; B cancels and clears the entry. On a desktop
keyboard, type normally; Enter connects, Backspace deletes and Esc cancels.
The entry stays masked. WPA passphrases use 8–63 ASCII characters; a 64-digit
hexadecimal PSK is also accepted. No password is written to trace, shell/runner
reports, screenshots or process arguments. Cancelled or submitted UI entries
are cleared. SSIDs are decoded as bytes and sanitized for display.

## Service ownership and persistence

Rondo uses a private wpa_supplicant control socket and foreground child process,
plus its own BusyBox udhcpc process. It does not run shell command strings,
killall or stop global network services. A conflicting wlan0 owner is reported
as busy. Child processes close inherited game/audio/input/IPC descriptors.
The library process keeps the connection while a supervised NES runner is
active; Wi-Fi management is available from the library settings.

Only the radio preference is in config.json. Network profiles are stored by
wpa_supplicant in RondoState/WiFi/wpa_supplicant.conf after confirmed connection.
Runtime sockets and PID data live in a private temporary directory. The code
requests restrictive permissions; FAT cannot protect credentials from someone
who can physically read the card. No encryption-at-rest claim is made.
Saved enabled state and profiles are restored on startup. A late BaseOS radio
initialization is reconciled with a saved Off preference. The owner confirmed connection and recovery after reboot and
physical sleep for candidate 77896ca7242c. The subsequent menu/header refinements
require separate device acceptance; the lid-wake experiment stays
disabled.

## Verification scope

macOS uses an unavailable adapter for normal runs and a synthetic Unix-domain
socket for tests. It never changes the Mac's Wi-Fi. Native checks exercise
SSID decoding, duplicate access points, unsupported security, IP confirmation,
wrong-key events, timeouts, save rejection, preference rollback, queued input
during polling, keyboard coverage, masked previews and EN/RU UI.
H700-specific checks run inside a disposable BaseOS chroot with synthetic
rfkill/proc files and substituted child processes. These establish the adapter
protocol and ownership behavior, not actual RF, DHCP or wake performance.

The control protocol follows the [official wpa_supplicant control interface](https://w1.fi/wpa_supplicant/devel/ctrl_iface_page.html). No competing frontend source is copied.
