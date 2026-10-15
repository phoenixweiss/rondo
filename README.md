# Rondo

[Русский](README_RU.md)

A personal project by an Anbernic RG34XXSP owner who enjoyed the handheld and wanted a frontend that felt right.

**Early development.** An existing working prototype is being published in coherent source imports. This README describes this import only, not an invented earlier development date or a stable firmware release.

Catalog identity, conservative preference storage/recovery, EN/RU fallback and the complete model/platform policies with synthetic tests. No frontend window or NES execution is included yet.

Requires C++17, make and Python 3.11+. SDL components/application additionally require pkg-config, SDL2 and SDL2_ttf.

```sh
make -j4
make test
python3 tools/check_source.py
```

Checks do not install to SD or prove physical controls, brightness or suspend. English is primary; Russian is maintained. Original code is [MIT](LICENSE); dependencies retain [their terms](THIRD_PARTY_NOTICES.md). No commercial ROMs, BIOS or user saves belong here. Discuss substantial PRs first; the maintainer is focusing on their own backlog.
