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
run("runtime-build-tools", [sys.executable, str(ROOT / "tests/runtime_build_tests.py")])
run("build", ["make", "-j4", "all", "build/model-tests", "build/core-tests",
              "build/device-tests", "build/volume-ui-tests"], timeout=300)
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
