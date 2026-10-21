"""Retain component and nested permissive notices beside the binary package."""
from pathlib import Path
import re
import shutil


def bundle(root, sources, licenses):
    for path in sorted((root / "licenses").iterdir()):
        if path.is_file():
            shutil.copyfile(path, licenses / path.name)
    # Complete source headers accompany binaries as well as the source archives.
    # This preserves per-file attribution in MIT/libretro-common and SDL helpers.
    sections = []
    for name, tree in sorted(sources.items()):
        if name not in {"sdl", "ttf", "fceumm", "gambatte", "gpsp", "supafaust"}:
            continue  # Kernel/BC retain complete source archives and COPYING files.
        blocks = {}
        for path in sorted(tree.rglob("*")):
            if path.suffix not in {".c", ".h", ".cpp", ".hpp"}:
                continue
            text = path.read_text(errors="replace")
            for block in re.findall(r"/\*.*?\*/", text, re.S):
                lower = block.lower()
                if ("copyright" in lower and
                    any(word in lower for word in ["permission", "redistribution", "license", "licence"])):
                    blocks.setdefault(block, []).append(path.relative_to(tree).as_posix())
        for block, paths in sorted(blocks.items(), key=lambda pair: pair[1][0]):
            sections.append(name + "/" + ", ".join(paths) + "\n" + block + "\n")
    for name in ["libretro.h", "picosha2.h"]:
        text = (root / "vendor" / name).read_text()
        sections.append("Rondo/vendor/" + name + "\n" + re.search(r"/\*.*?\*/", text, re.S)[0] + "\n")
    sections.append("Rondo/vendor/stb-LICENSE\n" + (root / "vendor/stb-LICENSE").read_text())
    json_header = (root / "vendor/json.hpp").read_text()
    copyrights = sorted(set(re.findall(r"^// SPDX-FileCopyrightText: (.+)$", json_header, re.M)))
    sections.append("Rondo/vendor/json.hpp — MIT copyright notices\n" + "\n".join(copyrights) +
                    "\n\n" + (root / "licenses/nlohmann-json-MIT.txt").read_text())
    (licenses / "Nested-Source-Notices.txt").write_text("\n".join(sections))
    ttf = sources["ttf"]
    for source, name in [
        (ttf / "external/freetype/src/bdf/README", "FreeType-BDF-Notices.txt"),
        (ttf / "external/freetype/src/pcf/README", "FreeType-PCF-Notices.txt"),
        (ttf / "external/freetype/src/gzip/zlib.h", "FreeType-zlib-Notice.txt"),
        (root / "assets/Fonts/Ubuntu-UFL.txt", "Ubuntu-Font-Licence.txt"),
        (root / "assets/Assets/Icons/LICENSE", "Lucide-Feather-Licenses.txt"),
        (root / "assets/LICENSE.md", "Rondo-Asset-Policy.md")
    ]:
        shutil.copyfile(source, licenses / name)
