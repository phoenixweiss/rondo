"""Run quiet native checks in a new isolated directory, without device adapters."""
from pathlib import Path
import argparse
import json
import os
import platform
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--core", type=Path)
parser.add_argument("--work", type=Path)
args = parser.parse_args()
extension = "dylib" if platform.system() == "Darwin" else "so"
core = (args.core or ROOT / "build/cores" / f"fceumm_libretro.{extension}").resolve()
if not core.is_file():
    parser.error("Build the pinned core first: python3 tools/build_core.py")
if args.work:
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
else:
    work = Path(tempfile.mkdtemp(prefix="rondo-check-"))
environment = {k: v for k, v in os.environ.items() if not k.startswith("RONDO_")}
environment.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                   RONDO_INPUT_HINTS="controller")


def run(name, command, timeout=120):
    with (work / f"{name}.log").open("w") as log:
        subprocess.run(command, cwd=ROOT, env=environment, stdout=log,
                       stderr=subprocess.STDOUT, check=True, timeout=timeout)
    print(f"PASS: {name}", flush=True)


run("source", [sys.executable, str(ROOT / "tools/check_source.py")])
run("build-graph", [sys.executable, str(ROOT / "tests/build_graph_tests.py")])
run("runtime-build-tools", [sys.executable, str(ROOT / "tests/runtime_build_tests.py")])
run("release-tools", [sys.executable, str(ROOT / "tests/release_tests.py")])
run("system-power", [sys.executable, str(ROOT / "tests/system_power_tests.py")])
run("installer", [sys.executable, str(ROOT / "tests/installer_tests.py")])
run("build", ["make", "-j4", "all", "build/battery-log-tests", "build/model-tests", "build/core-tests",
              "build/device-tests", "build/snes-tests", "build/gba-tests", "build/gb-tests", "build/bluetooth-tests", "build/volume-ui-tests", "build/timezone-tests", "build/battery-tests", "build/wifi-tests", "build/clock-tests"], timeout=300)
run("snes", [str(ROOT / "build/snes-tests"), str(ROOT / "assets"), str(work / "snes"), str(core), str(core.with_name("mednafen_supafaust_libretro."+extension)), str(ROOT / "runtime/test-fixtures/snes")])
run("snes-runner", [sys.executable, str(ROOT / "tests/snes_runner_checks.py"), str(work / "snes-runner"), str(core), str(core.with_name("mednafen_supafaust_libretro."+extension))])
run("gba", [str(ROOT / "build/gba-tests"), str(ROOT / "assets"), str(work / "gba"), str(core), str(core.with_name("gpsp_libretro."+extension)), str(ROOT / "runtime/test-fixtures/gba")])
run("gb-gbc", [str(ROOT / "build/gb-tests"), str(ROOT / "assets"), str(work / "gb-gbc"), str(core), str(core.with_name("gambatte_libretro."+extension)), str(ROOT / "runtime/test-fixtures/gb")])
run("gba-runner", [sys.executable, str(ROOT / "tests/gba_runner_checks.py"), str(work / "gba-runner"), str(core), str(core.with_name("gpsp_libretro."+extension))])
run("gb-gbc-runners", [sys.executable, str(ROOT / "tests/gb_runner_checks.py"), str(work / "gb-runners"), str(core), str(core.with_name("gambatte_libretro."+extension))])
run("bluetooth", [str(ROOT / "build/bluetooth-tests"), str(ROOT / "assets"), str(work / "bluetooth")])
run("clock", [str(ROOT / "build/clock-tests"), str(ROOT / "assets"), str(work / "clock")])
run("battery-log", [str(ROOT / "build/battery-log-tests"), str(work / "battery-log")])
run("battery", [str(ROOT / "build/battery-tests"), str(ROOT / "assets"), str(work / "battery")])
run("wifi", [str(ROOT / "build/wifi-tests"), str(ROOT / "assets"), str(work / "wifi")])
run("timezone", [str(ROOT / "build/timezone-tests"), str(ROOT / "assets"), str(work / "timezone")])
run("model", [str(ROOT / "build/model-tests"), str(ROOT / "assets")])
run("volume", [str(ROOT / "build/volume-ui-tests"), str(ROOT / "assets"),
               str(work / "volume-renders")])
run("integration", [sys.executable, str(ROOT / "tests/run_checks.py"),
                    "--core", str(core), "--work", str(work / "integration")])
fixture = work / "integration/fixtures/card-a/NES/Rondo Test Cartridge/test.nes"
run("device-policy", [str(ROOT / "build/device-tests"), str(ROOT / "assets"),
                      str(work / "device-policy"), str(core), str(fixture)])
(work / "summary.json").write_text(json.dumps({
    "native_checks_passed": True,
    "core": str(core),
    "dummy_video_audio": True,
    "sd_accessed": False,
    "physical_hardware_verified": False
}, indent=2) + "\n")
print(f"Results: {work}")
