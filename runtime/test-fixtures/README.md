# GB/GBC technical fixtures

Original Rondo test code, MIT under the repository LICENSE. Regenerate with
`python3 tools/gb_fixture.py`. These tiny programs exercise DMG/CGB video, audio,
MBC3 RAM and RTC persistence through the actual Gambatte ABI. They contain no
commercial game data, Nintendo logo, boot ROM, music or borrowed artwork.
They are automated test inputs, not playable demo games or a user's library.

The original MIT GBA fixture is generated with `python3 tools/gba_fixture.py`. It exercises video, a square wave, cartridge SRAM and A/B/L/R/Start/Select through gpSP. It contains no Nintendo logo or commercial game data and is never installed in the user's library.

The original MIT SNES fixture is generated with `python3 tools/snes_fixture.py`. It exercises NTSC backdrop video, twelve joypad buttons and SRAM; isolated test variants add PAL and copier-header SMC. Audio is silent. No copyrighted cartridge/firmware/logo data is copied and this is never installed in the user’s game library.
