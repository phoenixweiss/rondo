"""Assemble deterministic source-complete H700 development ZIPs in the SDK."""
from pathlib import Path, PurePosixPath
import datetime
import gzip
import hashlib
import io
import json
import re
import shutil
import stat
import subprocess
import tarfile
import zipfile
from inputs import records, sha
from notices import bundle


def files(root):
    return [p for p in sorted(Path(root).rglob("*"))
            if p.is_file() and "__pycache__" not in p.parts and not p.name.endswith(".pyc")]


def inventory(root):
    return {p.relative_to(root).as_posix(): sha(p) for p in files(root)}


def source_archive(source, output, epoch):
    with Path(output).open("wb") as handle:
        with gzip.GzipFile(fileobj=handle, mode="wb", filename="", mtime=epoch) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                for path in files(source):
                    name = "Rondo/" + path.relative_to(source).as_posix()
                    data = path.read_bytes()
                    member = tarfile.TarInfo(name)
                    member.size = len(data)
                    member.mtime = epoch
                    member.mode = 0o755 if path.suffix == ".sh" else 0o644
                    archive.addfile(member, io.BytesIO(data))


def write_manifest(root, name):
    (root / name).write_text("".join(sha(p) + "  " + p.relative_to(root).as_posix() + "\n"
                                   for p in files(root) if p != root / name))


def verify_zip(path):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(set(names)) != len(names):
            raise ValueError("Duplicate ZIP paths")
        for item in archive.infolist():
            name = PurePosixPath(item.filename)
            if name.is_absolute() or ".." in name.parts or stat.S_ISLNK(item.external_attr >> 16):
                raise ValueError("Unsafe ZIP member")
        if archive.testzip() is not None:
            raise ValueError("ZIP CRC failure")
        manifest = archive.read("PACKAGE.sha256").decode()
        covered = set()
        for line in manifest.splitlines():
            digest, name = line.split("  ", 1)
            if name in covered or hashlib.sha256(archive.read(name)).hexdigest() != digest:
                raise ValueError("Package checksum mismatch: " + name)
            covered.add(name)
        if covered != set(names) - {"PACKAGE.sha256"}:
            raise ValueError("Package manifest does not cover all files")
        required = {"System/launch_frontend.sh", "System/Rondo/bin/rondo",
                    "System/Rondo/cores/fceumm_libretro.so", "Sources/rondo-source.tar.gz"}
        if not required <= covered:
            raise ValueError("Incomplete package")


def main():
    root, work = Path("/src"), Path("/work")
    lock = json.loads((root / "runtime/build-lock.json").read_text())
    epoch = lock["source_date_epoch"]
    source_inputs = inventory(root)
    if source_inputs != json.loads((work / "source-inputs.json").read_text()):
        raise ValueError("Source snapshot changed during the build")
    source_id = hashlib.sha256(json.dumps(source_inputs, sort_keys=True).encode()).hexdigest()
    sources = records(root)
    payload = work / "package"
    payload.mkdir(exist_ok=False)
    app = payload / "System/Rondo"
    for directory in ["bin", "cores", "lib", "tools"]:
        (app / directory).mkdir(parents=True, exist_ok=True)
    for name in ["rondo", "fceumm_libretro.so"]:
        shutil.copyfile(work / "programs" / name, app / ("bin" if name == "rondo" else "cores") / name)
    for name in ["libSDL2-2.0.so.0", "libSDL2_ttf-2.0.so.0"]:
        shutil.copyfile(work / "sdk/lib" / name, app / "lib" / name)
    shutil.copytree(root / "assets", app / "assets")
    shutil.copytree(root / "runtime/fixtures", app / "Demo/card-a")
    for name in ["preflight.sh", "smoke.json"]:
        shutil.copyfile(root / "runtime/package" / name, app / "tools" / name)
    shutil.copyfile(root / "tools/boot-logo.sh", app / "tools/boot-logo.sh")
    shutil.copyfile(root / "runtime/package/launch_frontend.sh", payload / "System/launch_frontend.sh")
    compiler = subprocess.check_output([
        "/opt/aarch64-nextui-linux-gnu/bin/aarch64-nextui-linux-gnu-g++", "--version"
    ], text=True).splitlines()[0]
    info = {"format": 1, "target": lock["target"], "baseos_version": lock["baseos_version"],
            "package_kind": lock["package_kind"], "source_id": source_id,
            "source_date_epoch": epoch, "toolchain_image": lock["toolchain_image"],
            "compiler": compiler, "inputs": sources,
            "build_flags": {"core": ["platform=unix", "ARCH=arm64", "HAVE_HDPACK=0", "HAVE_NTSC=0"],
                            "ttf": ["embedded FreeType", "--disable-harfbuzz"]}}
    (app / "build-info.json").write_text(json.dumps(info, sort_keys=True, indent=2) + "\n")
    licenses = payload / "Licenses"
    licenses.mkdir()
    for src, name in [
        (root / "LICENSE", "Rondo-MIT.txt"),
        (work / "sources" / sources["sdl"]["directory"] / "LICENSE.txt", "SDL-LICENSE.txt"),
        (work / "sources" / sources["ttf"]["directory"] / "LICENSE.txt", "SDL_ttf-LICENSE.txt"),
        (work / "sources" / sources["ttf"]["directory"] / "external/freetype/docs/FTL.TXT", "FreeType-FTL.txt"),
        (work / "sources" / sources["fceumm"]["directory"] / "Copying", "FCEUmm-GPLv2.txt")
    ]:
        shutil.copyfile(src, licenses / name)
    bundle(root, {name: work / "sources" / item["directory"] for name, item in sources.items()}, licenses)
    header = (work / "sources" / sources["ttf"]["directory"] /
              "external/freetype/include/freetype/freetype.h").read_text()
    version = ".".join(re.search(r"#define FREETYPE_" + key + r"\s+(\d+)", header)[1]
                       for key in ["MAJOR", "MINOR", "PATCH"])
    if version != sources["ttf"]["embedded_freetype"]:
        raise ValueError("Unexpected embedded FreeType")
    shutil.copyfile(root / "THIRD_PARTY_NOTICES.md", payload / "THIRD_PARTY_NOTICES.md")
    material = payload / "Sources"
    material.mkdir()
    source_archive(root, material / "rondo-source.tar.gz", epoch)
    for name in sources:
        shutil.copyfile(work / "archives" / (name + ".tar.gz"), material / (name + ".tar.gz"))
    (material / "source-inputs.json").write_text(json.dumps(source_inputs, sort_keys=True, indent=2) + "\n")
    (payload / "README.md").write_text(
        "# Rondo — RG34XXSP development package\n\n"
        "Built offline from pinned source inputs for BaseOS 1.3.0. This is a frontend ZIP, not an SD image. "
        "No card is accessed by the builder. Preserve games, saves, preferences and original boot-logo backups. "
        "Install only after separately identifying and backing up the intended card.\n\n"
        "Run System/Rondo/tools/preflight.sh only in the supported BaseOS userspace. "
        "It checks loader dependencies, hashes and a synthetic save/resume sequence without hardware UI/audio. "
        "A successful preflight is not a physical panel/input/suspend test.\n\n"
        "Original Rondo source is MIT; third-party components retain their own licenses. "
        "Corresponding source archives, build scripts and notices accompany this package. "
        "The combined NES binary distribution follows GPLv2, with original Rondo code/artwork remaining MIT. "
        "FreeType uses its GPLv2 alternative here, not FTL. This local candidate is not a public release.\n")
    write_manifest(app, "manifest.sha256")
    write_manifest(payload, "PACKAGE.sha256")
    output = work / "output"
    output.mkdir()
    package = output / ("Rondo-BaseOS-RG34XXSP-development-" + source_id[:12] + ".zip")
    timestamp = datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc).timetuple()[:6]
    with zipfile.ZipFile(package, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in files(payload):
            relative = path.relative_to(payload).as_posix()
            item = zipfile.ZipInfo(relative, timestamp)
            item.create_system = 3
            mode = 0o755 if path.suffix in {".sh", ".so"} or relative == "System/Rondo/bin/rondo" else 0o644
            item.external_attr = (stat.S_IFREG | mode) << 16
            item.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(item, path.read_bytes(), compresslevel=9)
    verify_zip(package)
    result = {"package": package.name, "sha256": sha(package), "source_id": source_id,
              "payload_files": len(files(payload)), "embedded_freetype": version,
              "binaries": {n: sha(app / n) for n in ["bin/rondo", "cores/fceumm_libretro.so",
                           "lib/libSDL2-2.0.so.0", "lib/libSDL2_ttf-2.0.so.0"]}}
    (output / (package.name + ".sha256")).write_text(result["sha256"] + "  " + package.name + "\n")
    (output / "build-result.json").write_text(json.dumps(result, sort_keys=True, indent=2) + "\n")
    print(json.dumps(result, indent=2), flush=True)


if __name__ == "__main__":
    main()
