"""Validate release inputs and assemble verified development-preview assets."""
from pathlib import Path
import argparse
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "runtime"))
from inputs import sha
from audit_h700_package import audit
from build_h700_runtime import SOURCE_TREES, ROOT_NAMES


def version(text):
    value = text.strip()
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", value):
        raise ValueError("Expected a canonical X.Y.Z version")
    return value


def notes(changelog, number):
    sections = list(re.finditer(r"^## (.+)$", changelog, re.M))
    matches = [i for i, match in enumerate(sections)
               if re.fullmatch(r"\[" + re.escape(number) + r"\] - \d{4}-\d{2}-\d{2}", match[1])]
    if len(matches) != 1:
        raise ValueError("Expected one dated changelog section for " + number)
    i = matches[0]
    end = sections[i + 1].start() if i + 1 < len(sections) else len(changelog)
    body = changelog[sections[i].end():end].strip()
    if not body:
        raise ValueError("Empty release notes")
    return body + "\n"


def prepare_changelog(root, previous, upcoming):
    previous, upcoming = version(previous), version(upcoming)
    if version((root / "VERSION").read_text()) != previous:
        raise ValueError("VERSION does not match BUMPSTER_PREV_VERSION")
    if tuple(map(int, upcoming.split('.'))) <= tuple(map(int, previous.split('.'))):
        raise ValueError("Release version must increase")
    path = root / "CHANGELOG.md"
    text = path.read_text()
    if re.search(r"^## \[" + re.escape(upcoming) + r"\]", text, re.M):
        raise ValueError("Release already exists in changelog")
    if text.count("\n## Unreleased\n") != 1:
        raise ValueError("Expected exactly one Unreleased section")
    following = text.split("\n## Unreleased\n", 1)[1].split("\n## ", 1)[0]
    if not following.strip():
        raise ValueError("No unreleased changes")
    today = datetime.datetime.now(datetime.timezone.utc).date().isoformat()
    result = text.replace("\n## Unreleased\n", f"\n## Unreleased\n\n## [{upcoming}] - {today}\n", 1)
    notes(result, upcoming)
    temporary = path.with_suffix(".md.new")
    with temporary.open('x') as handle:
        handle.write(result)
    temporary.replace(path)


def validate_tag(root, tag):
    number = version((root / "VERSION").read_text())
    if number == "0.0.0" or tag != "v" + number:
        raise ValueError("Tag does not match a releasable VERSION")
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=root, text=True).strip()
    if git("cat-file", "-t", "refs/tags/" + tag) != "tag":
        raise ValueError("Release tag must be annotated")
    if git("rev-parse", tag + "^{commit}") != git("rev-parse", "HEAD"):
        raise ValueError("Checkout does not match release tag")
    subprocess.run(["git", "merge-base", "--is-ancestor", "HEAD", "origin/main"], cwd=root, check=True)
    return number, notes((root / "CHANGELOG.md").read_text(), number)


def bundle(root, first, second, output, tag=None):
    a = json.loads((first / "output/build-result.json").read_text())
    b = json.loads((second / "output/build-result.json").read_text())
    if a != b:
        raise ValueError("Independent builds differ")
    source_files = [root / name for name in ROOT_NAMES if (root / name).is_file()]
    for tree in SOURCE_TREES:
        source_files.extend(p for p in (root / tree).rglob('*') if p.is_file()
                            and '__pycache__' not in p.parts and not p.name.endswith('.pyc')
                            and p.name != '.DS_Store' and not p.name.startswith('._'))
    expected = {p.relative_to(root).as_posix(): sha(p) for p in source_files}
    for build in [first, second]:
        if json.loads((build / 'source-inputs.json').read_text()) != expected:
            raise ValueError("Build inputs differ from checkout")
        if Path(a["package"]).name != a["package"]:
            raise ValueError("Unsafe package name")
        if sha(build / "output" / a["package"]) != a["sha256"]:
            raise ValueError("Build ZIP readback mismatch")
    report = audit(first / "output" / a["package"])
    number, body = validate_tag(root, tag) if tag else (None, "Manual development candidate; not a release.\n")
    if number:
        with zipfile.ZipFile(first / "output" / a["package"]) as package:
            info = json.loads(package.read("System/Rondo/build-info.json"))
        if info.get("version") != number:
            raise ValueError("Packaged version differs from tag")
    output.mkdir(parents=True, exist_ok=False)
    name = f"Rondo-{number or 'development-' + a['source_id'][:12]}-BaseOS-RG34XXSP.zip"
    shutil.copy2(first / "output" / a["package"], output / name)
    report.update(independent_builds_identical=True, tag=tag, version=number)
    (output / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    (output / "release-notes.md").write_text(
        "Early development preview for RG34XXSP / BaseOS 1.3.0. Back up games, saves and preferences.\n\n"
        + body + "\nNative and packaging checks do not certify physical graphics, audio, input or suspend. "
        "See the repository installation and compatibility guides. Corresponding source and licenses are inside the ZIP.\n")
    (output / "SHA256SUMS.txt").write_text("".join(sha(p) + "  " + p.name + "\n"
        for p in sorted(output.iterdir()) if p.is_file()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("prepare-changelog")
    prepared = commands.add_parser("check-changelog")
    prepared.add_argument("version")
    check = commands.add_parser("validate-tag")
    check.add_argument("tag")
    build = commands.add_parser("bundle")
    build.add_argument("--first", type=Path, required=True)
    build.add_argument("--second", type=Path, required=True)
    build.add_argument("--output", type=Path, required=True)
    build.add_argument("--tag")
    args = parser.parse_args()
    if args.command == "prepare-changelog":
        prepare_changelog(ROOT, os.environ["BUMPSTER_PREV_VERSION"], os.environ["BUMPSTER_NEW_VERSION"])
    elif args.command == "check-changelog":
        notes((ROOT / "CHANGELOG.md").read_text(), version(args.version))
    elif args.command == "validate-tag":
        validate_tag(ROOT, args.tag)
    else:
        bundle(ROOT, args.first.resolve(), args.second.resolve(), args.output.resolve(), args.tag)


if __name__ == "__main__":
    main()
