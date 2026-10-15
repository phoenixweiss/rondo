"""Validate publication inputs without building or opening a device."""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parent.parent
trees = ["src", "assets", "vendor", "tests", "tools", "runtime", "docs", "licenses", ".github"]
files = [p for p in ROOT.iterdir() if p.is_file()]
for tree in trees:
    if (ROOT / tree).is_dir():
        files.extend(p for p in (ROOT / tree).rglob("*")
                     if p.is_file() and "__pycache__" not in p.parts)
text_types = {".md", ".py", ".sh", ".cpp", ".hpp", ".h", ".json", ".yml", ".yaml"}
personal_path = re.compile(r"/(?:Users|home)/[^/\s\"']+/")
for path in files:
    if path.suffix not in text_types:
        continue
    source = path.read_text()
    assert not personal_path.search(source), f"Personal home path: {path.relative_to(ROOT)}"
    if path.suffix == ".json":
        json.loads(source)
    if path.suffix == ".md":
        for target in re.findall(r"\]\(([^)]+)\)", source):
            if target.startswith(("https:", "http:", "mailto:", "#")):
                continue
            local = path.parent / target.split("#", 1)[0]
            assert local.exists(), f"Broken link: {path.relative_to(ROOT)} -> {target}"

vendor = json.loads((ROOT / "vendor/sources.json").read_text())
for name, record in vendor.items():
    actual = hashlib.sha256((ROOT / "vendor" / name).read_bytes()).hexdigest()
    assert actual == record["sha256"], f"Vendor hash mismatch: {name}"

locales = ROOT / "assets/Locales"
english = json.loads((locales / "en.json").read_text())
russian = json.loads((locales / "ru.json").read_text())
placeholder = re.compile(r"\{([A-Za-z_][A-Za-z_0-9]*)\}")
for key in english.keys() & russian.keys():
    if isinstance(english[key], str) and isinstance(russian[key], str):
        assert set(placeholder.findall(english[key])) == set(placeholder.findall(russian[key])), key
for forbidden in ["Packages", "Diagnostics", "Builds", "RondoGames", "RondoState", "RondoRollback"]:
    assert not (ROOT / forbidden).exists(), f"Private/generated directory: {forbidden}"
assert (ROOT / "LICENSE").read_text().startswith("MIT License\n")
assert (ROOT / "THIRD_PARTY_NOTICES.md").is_file()
print(f"PASS: {len(files)} publication input files, JSON, local links, vendor hashes and locale placeholders")
