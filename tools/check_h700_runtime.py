"""Verify a built ZIP in read-only BaseOS userspace with synthetic devices."""
from pathlib import Path
import argparse
import json
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "runtime"))
from inputs import sha
from package import verify_zip


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--rootfs", type=Path, required=True,
                        help="Owner-supplied extracted BaseOS rootfs; mounted read-only")
    parser.add_argument("--work", type=Path, required=True, help="New validation directory")
    parser.add_argument("--baseos-version", choices=["1.3.0", "1.5.2"],
                        help="Explicit compatibility probe; defaults to the package baseline")
    args = parser.parse_args()
    build, rootfs, work = args.build.resolve(), args.rootfs.resolve(), args.work.resolve()
    lock = json.loads((build / "source/runtime/build-lock.json").read_text())
    expected_version = args.baseos_version or lock["baseos_version"]
    release = (rootfs / "etc/baseos-release").read_text().splitlines()
    for marker in ["BASEOS_PLATFORM=h700", "BASEOS_TARGET=" + lock["target"],
                   "BASEOS_VERSION=" + expected_version]:
        if marker not in release:
            raise ValueError("Wrong rootfs: expected " + marker)
    if "ID=baseos" not in (rootfs / "etc/os-release").read_text().splitlines():
        raise ValueError("Not a BaseOS rootfs")
    result = json.loads((build / "output/build-result.json").read_text())
    name = result["package"]
    if Path(name).name != name:
        raise ValueError("Unsafe package filename")
    package = build / "output" / name
    if sha(package) != result["sha256"]:
        raise ValueError("Package readback mismatch")
    verify_zip(package)
    if work == ROOT or ROOT in work.parents and work.relative_to(ROOT).parts[0] != "build":
        parser.error("Work must be outside the source tree or below build/")
    if rootfs == work or rootfs in work.parents or work in rootfs.parents:
        parser.error("Validation output and rootfs must be separate")
    work.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(package) as archive:
        archive.extractall(work / "package")
        for item in archive.infolist():
            (work / "package" / item.filename).chmod((item.external_attr >> 16) & 0o777)
    # Only the tmpfs chroot sees this mixer stub. No host ALSA or devices are exposed.
    mixer = work / "alsactl"
    mixer.write_text('#!/bin/sh\nset -eu\ncase "$3" in\n store) : > "$2";;\n'
                     ' restore) : > /tmp/rondo-mixer-restored;;\n *) exit 1;;\nesac\n')
    mixer.chmod(0o755)
    command = ["docker", "run", "--rm", "--network", "none", "--platform", lock["platform"],
               "-v", str(rootfs) + ":/baseos:ro",
               "--tmpfs", "/baseos/tmp:rw,exec", "--tmpfs", "/baseos/sys:rw", "--tmpfs", "/baseos/dev:rw",
               "--tmpfs", "/baseos/proc:rw", "--tmpfs", "/baseos/run:rw,exec", "--tmpfs", "/baseos/data:rw",
               "-v", str(mixer) + ":/baseos/usr/sbin/alsactl:ro",
               "-v", str(work / "package") + ":/package:ro",
               "-v", str(build / "programs") + ":/programs:ro",
               "-v", str(work) + ":/results",
               "-v", str(build / "source/runtime/preflight-isolated.sh") + ":/check.sh:ro",
               "--entrypoint", "/bin/bash", lock["toolchain_image"], "/check.sh"]
    with (work / "baseos-preflight.log").open("w") as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
    if (work / "captures/rondo-power-calls").read_text().splitlines() != ["poweroff", "reboot"]:
        raise ValueError("BaseOS native power wrapper sequence mismatch")
    summary = {"native_power_wrappers_fake_busybox_passed": True, "package_sha256": result["sha256"], "baseos_version": expected_version,
               "package_baseos_baseline": lock["baseos_version"],
               "target": lock["target"], "packaged_preflight_passed": True,
               "synthetic_device_tests_passed": True, "rootfs_read_only": True,
               "physical_hardware_verified": False, "sd_accessed": False}
    (work / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
