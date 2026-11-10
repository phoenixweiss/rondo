"""Extract pinned BaseOS userspace into a directory without mounting an image."""
from pathlib import Path
import argparse
import gzip
import json
import shutil
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "runtime"))
from downloads import checksum, fetch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    pins = json.loads((ROOT / "runtime/baseos-matrix.json").read_text())
    parser.add_argument("--version", choices=pins, required=True)
    parser.add_argument("--work", type=Path, required=True, help="New extraction directory")
    parser.add_argument("--archive", type=Path, help="Use a local copy of the pinned BaseOS update")
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    target = json.loads((ROOT / "runtime/build-lock.json").read_text())["target"]
    filename = f"baseos-{target}-{args.version}.bosupd"
    expected = pins[args.version]
    url = f"https://github.com/pvaibhav/BaseOS/releases/download/v{args.version}/{filename}"
    if args.archive is None:
        archive = fetch({"url": url, "sha256": expected}, work / filename)
    else:
        archive = args.archive.resolve(strict=True)
        if checksum(archive) != expected:
            raise ValueError("BaseOS update checksum mismatch")
    with tarfile.open(archive, "r:*") as update:
        member = update.getmember("rootfs.img.gz")
        if not member.isfile():
            raise ValueError("BaseOS rootfs member is not a regular file")
        image = work / "rootfs.img"
        with update.extractfile(member) as compressed, gzip.GzipFile(fileobj=compressed) as stream:
            with image.open("xb") as output:
                shutil.copyfileobj(stream, output)

    rootfs = work / "rootfs"
    rootfs.mkdir()
    subprocess.run(["debugfs", "-R", "rdump / rootfs", image.name],
                   cwd=work, check=True, timeout=90)
    image.unlink()
    library_dir = rootfs / "usr/lib"
    squashfs = rootfs / "usr/share/baseos/usr-lib.sqfs"
    if library_dir.resolve() != library_dir or squashfs.resolve() != squashfs:
        raise ValueError("Unexpected BaseOS system library paths")
    if squashfs.is_file():
        subprocess.run(["unsquashfs", "-no-xattrs", "-f", "-d", str(library_dir), str(squashfs)],
                       check=True, timeout=90)
    if not (library_dir / "aarch64-linux-gnu/ld-linux-aarch64.so.1").is_file():
        raise ValueError("BaseOS system libraries are missing")
    if args.archive is None:
        archive.unlink()
    print(rootfs)


if __name__ == "__main__":
    main()
