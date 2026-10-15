"""Build a standalone H700 development package; never access an SD device."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "runtime"))
from inputs import records, sha

SOURCE_TREES = {"src", "assets", "vendor", "tests", "tools", "runtime", "docs", "licenses", ".github", ".bumpster"}
ROOT_NAMES = {"README.md", "README_RU.md", "LICENSE", "THIRD_PARTY_NOTICES.md",
              "CONTRIBUTING.md", "CHANGELOG.md", "Makefile", ".clang-format",
              ".gitignore", ".bumpsterrc", ".editorconfig", ".gitattributes", "VERSION"}


def snapshot(destination):
    for name in sorted(SOURCE_TREES):
        path = ROOT / name
        if path.is_dir():
            if any(p.is_symlink() for p in path.rglob("*")):
                raise ValueError("Symlink in source inputs: " + name)
            shutil.copytree(path, destination / name,
                            ignore=shutil.ignore_patterns("__pycache__", "*.pyc", ".DS_Store", "._*"))
    for name in sorted(ROOT_NAMES):
        if (ROOT / name).is_symlink():
            raise ValueError("Symlink in source inputs: " + name)
        if (ROOT / name).is_file():
            shutil.copyfile(ROOT / name, destination / name)
    for path in destination.rglob("*"):
        if path.is_symlink():
            raise ValueError("Symlink in source snapshot: " + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True, help="New disposable build directory")
    parser.add_argument("--cache", type=Path, default=ROOT / "build/h700-cache")
    args = parser.parse_args()
    work, cache = args.work.resolve(), args.cache.resolve()
    if work == ROOT or ROOT in work.parents and work.relative_to(ROOT).parts[0] != "build":
        parser.error("Work must be outside the source tree or below build/")
    if cache == ROOT or ROOT in cache.parents and cache.relative_to(ROOT).parts[0] != "build":
        parser.error("Cache must be outside the source tree or below build/")
    work.mkdir(parents=True, exist_ok=False)
    cache.mkdir(parents=True, exist_ok=True)
    source = work / "source"
    source.mkdir()
    snapshot(source)
    source_inputs = {p.relative_to(source).as_posix(): sha(p) for p in sorted(source.rglob("*")) if p.is_file()}
    (work / "source-inputs.json").write_text(json.dumps(source_inputs, sort_keys=True) + "\n")
    lock = json.loads((source / "runtime/build-lock.json").read_text())
    if "@sha256:" not in lock["toolchain_image"]:
        raise ValueError("Toolchain must be pinned by digest")
    archives = work / "archives"
    archives.mkdir()
    for name, item in records(source).items():
        cached = cache / (item["sha256"] + ".tar.gz")
        if not cached.exists():
            temporary = cached.with_suffix(".download")
            print("Fetching " + name, flush=True)
            try:
                with urllib.request.urlopen(item["url"], timeout=60) as incoming, temporary.open("xb") as output:
                    shutil.copyfileobj(incoming, output)
                if sha(temporary) != item["sha256"]:
                    raise ValueError("Downloaded checksum mismatch: " + name)
                temporary.replace(cached)
            finally:
                if temporary.exists():
                    temporary.unlink()
        if sha(cached) != item["sha256"]:
            raise ValueError("Cached checksum mismatch: " + name)
        shutil.copyfile(cached, archives / (name + ".tar.gz"))
    command = ["docker", "run", "--rm", "--network", "none", "--platform", lock["platform"],
               "-v", str(source) + ":/src:ro", "-v", str(work) + ":/work",
               "--entrypoint", "/bin/bash", lock["toolchain_image"], "/src/runtime/build-all.sh"]
    print("Building from a frozen source snapshot in " + str(work), flush=True)
    with (work / "build.log").open("w") as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=1800)
    result = json.loads((work / "output/build-result.json").read_text())
    package = work / "output" / result["package"]
    if sha(package) != result["sha256"]:
        raise ValueError("Package readback mismatch")
    print(json.dumps({"package": str(package), "sha256": result["sha256"]}, indent=2))


if __name__ == "__main__":
    main()
