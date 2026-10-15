"""Pinned source inputs and safe extraction for the H700 build."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import tarfile


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def records(root):
    dependencies = Path(root) / "docs/dependencies"
    data = json.loads((dependencies / "runtime-sources.json").read_text())
    core = json.loads((dependencies / "core-source.json").read_text())
    revision = core["revision"]
    return {
        "sdl": {**data["sdl"], "directory": "SDL-malifbdev-rot-" +
                data["sdl"]["url"].rsplit("/", 1)[1]},
        "ttf": {**data["ttf"], "directory": "SDL2_ttf-2.20.2"},
        "fceumm": {"url": "https://codeload.github.com/libretro/libretro-fceumm/tar.gz/" + revision,
                   "sha256": core["archive_sha256"],
                   "directory": "libretro-fceumm-" + revision}
    }


def extract(archive, destination, expected_hash, prefix):
    archive, destination = Path(archive), Path(destination)
    if sha(archive) != expected_hash:
        raise ValueError("Source checksum mismatch: " + archive.name)
    with tarfile.open(archive) as source:
        for member in source.getmembers():
            path = PurePosixPath(member.name)
            if path.is_absolute() or ".." in path.parts or not path.parts or path.parts[0] != prefix:
                raise ValueError("Unsafe source path: " + member.name)
        # Python 3.12 in the pinned image rejects external links and special files.
        source.extractall(destination, filter="data")


if __name__ == "__main__":
    root, work = Path("/src"), Path("/work")
    sources = work / "sources"
    sources.mkdir(exist_ok=False)
    for name, item in records(root).items():
        extract(work / "archives" / (name + ".tar.gz"), sources,
                item["sha256"], item["directory"])
    print("Pinned source archives verified and extracted", flush=True)
