# State container version 2

All supported cores use `slot-0.state` through `slot-2.state`, `autosave.state`
and `.previous` generations. JSON states are not supported by this development
build; they remain untouched. Persistent SRAM/RTC and PSX card data are unchanged.

The format has an 84-byte header: eight magic/version bytes `RONDOST` + byte 2,
three little-endian uint32 lengths (metadata, preview, core payload), then the
64 ASCII hex characters of the metadata SHA-256. The following sections are
bounded UTF-8 JSON metadata (at most 16 KiB), little-endian 0xAARRGGBB uint32 pixels
(at most 192×144), and raw libretro serialization bytes (at most 32 MiB).
There is no compression or third-party codec dependency.

Metadata contains schema 2, core/ROM identity, positive save sequence, UTC,
preview width/height, and independent SHA-256 hashes of preview and payload.
PSX also records its disc index. PSX previews use 4:3 pixel aspect correction;
other systems retain frame proportions. The original framebuffer is unchanged.

Metadata-only reads validate the header, exact file length and metadata hash.
Preview reads additionally verify just the preview. A matching menu entry means
its metadata is compatible, not that its unread payload has been verified.
Load verifies both sections and ROM/core identity before calling unserialize.
Automatic loading falls back to the previous generation on corruption.

Writes keep a valid previous generation, replace through a temporary file with
file and directory fsync, and verify binary payload, preview and sequence on
readback. They do not remove durability barriers. Persistent memory is written
only when bytes have changed or its file needs recreating; a successful write
updates the in-memory comparison cache. This is not a transaction across files.

`make build/state-benchmark` builds an optional performance harness. Arguments:
assets, fresh work directory, NES core, PSX core, technical PSX fixture directory,
result JSON path. Use dummy SDL video/audio. Three runs measure init, slot
selection, previews, manual overwrite, autosave and load. The fixed 512×240
preview makes both versions comparable; the core payload is produced by the
pinned PCSX ReARMed on the original technical fixture. Host times do not predict
H700 or SD latency. No commercial game or BIOS is needed.
