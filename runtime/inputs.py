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


def archive_name(name, record):
    return name + record.get("archive_suffix", ".tar.gz")


def records(root):
    dependencies = Path(root) / "docs/dependencies"
    data = json.loads((dependencies / "runtime-sources.json").read_text())
    core = json.loads((dependencies / "core-source.json").read_text())
    revision = core["revision"]
    gb = json.loads((dependencies / "gb-core-source.json").read_text())
    gba = json.loads((dependencies / "gba-core-source.json").read_text())
    return {
        "sdl": {**data["sdl"], "directory": "SDL-malifbdev-rot-" +
                data["sdl"]["url"].rsplit("/", 1)[1]},
        "ttf": {**data["ttf"], "directory": "SDL2_ttf-2.20.2"},
        "fceumm": {"url": "https://codeload.github.com/libretro/libretro-fceumm/tar.gz/" + revision,
                   "sha256": core["archive_sha256"],
                   "directory": "libretro-fceumm-" + revision},
        "gambatte": {"url": gb["repository"].replace("github.com/","codeload.github.com/")+"/tar.gz/"+gb["revision"],
                     "sha256":gb["archive_sha256"],"directory":"gambatte-libretro-"+gb["revision"]},
        "gpsp": {"url": gba["repository"].replace("github.com/","codeload.github.com/")+"/tar.gz/"+gba["revision"],
                 "sha256":gba["archive_sha256"],"directory":"gpsp-"+gba["revision"]},
        "linux": data["linux"], "bc": data["bc"]
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
        destination = Path("/tmp/rondo-linux") if name == "linux" else sources
        destination.mkdir(exist_ok=True)
        extract(work / "archives" / archive_name(name, item), destination,
                item["sha256"], item["directory"])
        if name == "linux":
            # Keep large header trees on the container filesystem. Only COPYING
            # is consumed by packaging; the complete pinned archive is shipped.
            retained = sources / item["directory"]
            retained.mkdir()
            (retained / "COPYING").write_bytes((destination / item["directory"] / "COPYING").read_bytes())
    print("Pinned source archives verified and extracted", flush=True)
