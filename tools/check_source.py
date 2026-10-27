"""Validate publication inputs without building or opening a device."""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parent.parent
trees = ["src", "assets", "vendor", "tests", "tools", "runtime", "docs", "licenses", ".github", ".bumpster"]
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
system_icons = ROOT / "assets/Systems"
system_sources = json.loads((system_icons / "sources.json").read_text())
assert system_sources["license"] == "MIT"
assert (system_icons / "LICENSE.txt").is_file() and (system_icons / "UPSTREAM-README.txt").is_file()
for name, record in system_sources["icons"].items():
    assert hashlib.sha256((system_icons / (name + ".svg")).read_bytes()).hexdigest() == record["svg_sha256"]
    assert hashlib.sha256((ROOT / "assets/IconsPNG" / ("system-" + name + ".png")).read_bytes()).hexdigest() == record["png_sha256"]
    assert record["scale_x"] == record["scale_y"]
    assert max(record["rendered_viewbox"][0] / 320, record["rendered_viewbox"][1] / 200) <= 1.000001
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

zone_names = ROOT / "assets/Timezones/names.json"
zone_inputs = json.loads((zone_names.parent / "sources.json").read_text())
assert hashlib.sha256(zone_names.read_bytes()).hexdigest() == zone_inputs["output_sha256"]
assert (zone_names.parent / "LICENSE").read_text().startswith("UNICODE LICENSE V3")

mapping_root = ROOT / "assets/ControllerMappings"
mapping_inputs = json.loads((mapping_root / "sources.json").read_text())
for name, expected in [(mapping_inputs["output"], mapping_inputs["output_sha256"]),
                       (mapping_inputs["license_file"], mapping_inputs["license_sha256"])]:
    assert hashlib.sha256((mapping_root / name).read_bytes()).hexdigest() == expected, name
mapping_rows = [row for row in (mapping_root / mapping_inputs["output"]).read_text().splitlines()
                if row and not row.startswith("#")]
assert len(mapping_rows) == 4 and all(",8BitDo Micro," in row for row in mapping_rows)
assert sum("platform:Linux," in row for row in mapping_rows) == 2
assert re.fullmatch(r"[0-9a-f]{40}", mapping_inputs["commit"])

ob=json.loads((ROOT/'docs/dependencies/openbios-source.json').read_text())
assert json.loads((ROOT/'assets/Firmware/OpenBIOS/sources.json').read_text())==ob
assert hashlib.sha256((ROOT/'assets/Firmware/OpenBIOS'/ob['output']).read_bytes()).hexdigest()==ob['output_sha256']
assert (ROOT/'assets/Firmware/OpenBIOS/LICENSE').read_text().startswith('MIT License')

print(f"PASS: {len(files)} publication input files, JSON, local links, vendor hashes and locale placeholders")

settings_icons = ROOT / "assets/Assets/Icons"
icon_inputs = json.loads((settings_icons / "settings-sources.json").read_text())
assert re.fullmatch(r"[0-9a-f]{40}", icon_inputs["revision"])
assert hashlib.sha256((settings_icons / icon_inputs["license"]).read_bytes()).hexdigest() == icon_inputs["license_sha256"]
for name, record in icon_inputs["icons"].items():
    assert hashlib.sha256((settings_icons / (name + ".svg")).read_bytes()).hexdigest() == record["svg_sha256"], name
    assert hashlib.sha256((ROOT / "assets/IconsPNG" / (name + ".png")).read_bytes()).hexdigest() == record["png_sha256"], name

nav_inputs=json.loads((settings_icons/"navigation-sources.json").read_text())
assert re.fullmatch(r"[0-9a-f]{40}",nav_inputs["revision"])
assert (settings_icons/nav_inputs["license"]).is_file()
for name,record in nav_inputs["icons"].items():
    assert hashlib.sha256((settings_icons/(name+".svg")).read_bytes()).hexdigest()==record["svg_sha256"]
    assert hashlib.sha256((ROOT/"assets/IconsPNG"/(name+".png")).read_bytes()).hexdigest()==record["png_sha256"]

icon_update=json.loads((settings_icons/"icon-update-sources.json").read_text())
assert re.fullmatch(r"[0-9a-f]{40}",icon_update["revision"])
assert (settings_icons/icon_update["license"]).is_file()
for record in icon_update["icons"]:
    name=Path(record["file"]).stem
    assert hashlib.sha256((settings_icons/record["file"]).read_bytes()).hexdigest()==record["sha256"]
    assert hashlib.sha256((ROOT/"assets/IconsPNG"/(name+".png")).read_bytes()).hexdigest()==record["png_sha256"]
