"""Build a standalone H700 development package; never access an SD device."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "runtime"))
from inputs import records, sha, archive_name
from build_openbios import fetch_toolchain

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



def record_build_timing(work, seconds):
    target = work / "cache-result.json"
    timing = json.loads(target.read_text())
    timing["seconds"] = round(seconds, 3)
    # Docker owns the existing file on Linux; the host owns its parent directory.
    # Replace the entry atomically instead of opening the container's inode for write.
    with tempfile.NamedTemporaryFile(mode="w", dir=work, prefix=".cache-result-", delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(json.dumps(timing, sort_keys=True) + "\n")
    try:
        temporary.replace(target)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True, help="New disposable build directory")
    parser.add_argument("--cache", type=Path, default=ROOT / "build/h700-cache")
    parser.add_argument("--compiled-cache", type=Path, default=ROOT / "build/h700-dependencies")
    parser.add_argument("--fresh", action="store_true", help="Build all dependencies independently, without compiled caches")
    args = parser.parse_args()
    started = time.monotonic()
    work, cache = args.work.resolve(), args.cache.resolve()
    if work == ROOT or ROOT in work.parents and work.relative_to(ROOT).parts[0] != "build":
        parser.error("Work must be outside the source tree or below build/")
    if cache == ROOT or ROOT in cache.parents and cache.relative_to(ROOT).parts[0] != "build":
        parser.error("Cache must be outside the source tree or below build/")
    compiled_cache = args.compiled_cache.resolve()
    if compiled_cache == ROOT or ROOT in compiled_cache.parents and compiled_cache.relative_to(ROOT).parts[0] != "build":
        parser.error("Compiled cache must be outside the source tree or below build/")
    if compiled_cache == work or work in compiled_cache.parents or compiled_cache in work.parents:
        parser.error("Compiled cache and work must be separate directories")
    if not args.fresh:
        compiled_cache.mkdir(parents=True, exist_ok=True)
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
        cached = cache / (item["sha256"] + item.get("archive_suffix", ".tar.gz"))
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
        shutil.copyfile(cached, archives / archive_name(name, item))
    fetch_toolchain(source, cache, work / "openbios-toolchain")
    command = ["docker", "run", "--rm", "--network", "none", "--platform", lock["platform"],
               "-v", str(source) + ":/src:ro", "-v", str(work) + ":/work",
               "--entrypoint", "/bin/bash", lock["toolchain_image"], "/src/runtime/build-all.sh"]
    if not args.fresh:
        command[2:2] = ["-v", str(compiled_cache) + ":/compiled-cache", "-e", "RONDO_DEPENDENCY_CACHE=1"]
    print("Building from a frozen source snapshot in " + str(work), flush=True)
    with (work / "build.log").open("w") as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=1800)
    result = json.loads((work / "output/build-result.json").read_text())
    package = work / "output" / result["package"]
    if sha(package) != result["sha256"]:
        raise ValueError("Package readback mismatch")
    record_build_timing(work, time.monotonic() - started)
    print(json.dumps({"package": str(package), "sha256": result["sha256"]}, indent=2))


if __name__ == "__main__":
    main()
