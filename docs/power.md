# Lid sleep and wake

## Current device status

The 2026-10-16 hardware run loaded the experimental module, but the actual hall wake attribute could not be found (`Hall wake policy readback mismatch: missing`). Suspend was aborted before writing `mem`; the owner reported that sleep stopped working. The module is no longer loaded by the launcher, and H700 reports wake-on-open as unavailable even if a stale module parameter exists. Sleep on close and Power wake use the stock `os_sleep=16` path. A cold boot is required after installing the recovery package. Existing preferences are preserved, but a saved wake-on-open value cannot enable an unavailable capability. Battery changes are retained. The owner confirmed the recovery candidate 2b1d4eec7f1f on RG34XXSP on 2026-10-16. This is bounded acceptance, not a standby-drain measurement.

The module and build sources remain packaged for research; they are inactive. Wake-on-open is back in the backlog until the actual kernel wake-device contract and standby behavior are established.

## Retained experimental design

Settings → Power has two independent booleans: Sleep on lid close (`lid_sleep`) and Wake on lid open (`wake_on_lid_open`). Both default to On, including when an older configuration has no wake setting. Existing explicit preferences are preserved. Left/Right sets Off/On, A toggles, and Info explains the behavior. The supervised NES runner uses the same settings authority.

Closing only triggers sleep when Sleep on lid close is On. Opening wakes only when Wake on lid open is On and the exact-kernel adapter is available. An already open lid is not an opening transition, so it does not immediately cancel sleep requested by Power. Power remains the fallback wake source. With close-to-sleep enabled, a closed device re-suspends after an accidental Power wake, preserving the existing closed-lid policy. With close-to-sleep disabled, the adapter distinguishes a closing wake edge from a fresh Power release before accepting a closed-lid wake.

Before suspend the game pauses, input is cleared, SRAM is flushed, audio closes and mixer/display state is preserved. SRAM failure aborts suspend. After resume the previous pause state, display, mixer, input and audio are restored; failure leaves the game paused with a visible error. This does not add shutdown autosaves or change slot data.

## Kernel contract

Stock `os_sleep=16` selects full USB host stop and disables hall-device wake capability. Zero enables hall wake but also selects the alternate USB standby path. Rondo retains `os_sleep=16` and then asks `rondo_lid_wake.ko` to call the vendor's exported `set_os_sleep_mode` independently. The helper checks that the USB-policy getter is still one before applying the request; it never writes the USB mode or patches kernel instructions. After resume it restores the stock hall wake policy.

The module is restricted to the audited RG34XXSP vendor kernel recorded in `runtime/lid-wake/profile.json`. Its release/version, function instructions, symbol CRCs, module layout and UTS field offsets are checked. It loads without changing wake policy. Unsupported kernels must be audited separately. A missing or rejected module disables the wake row; Power sleep/wake continues through the existing adapter. No fake On acknowledgement is presented for an unavailable adapter.

Before writing `mem`, userspace reads the hall device's actual `power/wakeup` value back. An enable failure aborts suspend instead of pretending wake-on-open is armed. Diagnostics in the launcher log include requested policy, wake attribute path/readback and closed-lid re-suspend. They contain no game content or network credentials.

## Build and verification

The offline H700 build pins Linux 4.9.170 and GNU BC 1.07.1 sources, the existing SDK, vendor configuration and recovered symbol versions. The BC build-only `ed` formatting step is reproduced in Python; generated manuals are skipped. BC is not installed on the handheld. Both original archives, module source/configuration, build scripts and license texts accompany the package. The kernel Image and boot partition are unchanged.

Native tests cover all four option combinations, defaults, persistence, rejected writes, unsupported adapters, UI/Info in English/Russian and the existing NES suspend lifecycle. ARM64 BaseOS tests use synthetic sysfs and wrapped device calls: they test wake readback, rejection before `mem`, USB-policy retention, restoration and fallback. ELF checks reject incorrect vendor CRCs and kernel identity. They never load a module into the container host kernel.

Any future experiment requires physical acceptance: lid cycles in the library and a real NES game, all four combinations, Power sleep with the lid already open, closing during Power sleep, audio/input/SRAM after resume and comparison of standby drain. Static ABI compatibility is not proof of an actual hall interrupt waking the SoC. If hardware tests fail, disable wake-on-open and retain the original Power path while returning the experiment to the backlog.
