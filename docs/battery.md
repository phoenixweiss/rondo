# Battery display

Settings → Display → Battery percentage changes whether the numeric percentage appears. Left/Right sets Off/On; A toggles. The battery icon remains visible. The default is On for existing and new configurations, preserving the previous numeric display. The boolean setting is show_battery_percent.

The icon uses measured capacity from the RG34XXSP battery sysfs adapter. Fill reflects 0–100%, a large light bolt across the battery marks Charging and a slash marks unavailable/invalid data. Levels at or below 15% use the existing accent color; this is a display threshold, not battery calibration or an automatic shutdown policy. Missing capacity is never rendered as zero. There is no remaining-runtime prediction.

The setting is shared with the runner through the existing settings authority and affects header bars in the library, settings and game menus. Fullscreen gameplay has no new permanent HUD. On macOS the device adapter has no handheld battery reading, so the honest result is the unknown icon, with a dash only when the numeric display is enabled.

Native tests cover level/charging/unknown images, hiding only the percentage, old configurations, invalid setting types, persistence, rejected writes and independence from brightness. The owner confirmed percentage display and hiding on RG34XXSP on 2026-10-16. Charging, low and unknown states still need separate physical acceptance.

The header status group is aligned to the right with consistent gaps. Disabled Wi-Fi and hidden percentages leave no reserved space; the remaining indicators close the gap while the clock stays at the right edge. The order is Wi-Fi, battery (with optional charging mark and percentage), then clock. The charging mark uses the same light foreground as the clock with a dark outline separating it from the battery fill and border.
