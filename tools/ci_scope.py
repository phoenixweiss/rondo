"""Select native compilation conservatively; documentation checks always run."""
from pathlib import Path, PurePosixPath
import json
import os
import re
import subprocess


def needs_runtime(paths):
    if not paths:
        return True
    for name in paths:
        path = PurePosixPath(name)
        documentation = (name in {"CHANGELOG.md", "CONTRIBUTING.md"} or
                         (path.parent == PurePosixPath(".") and path.name.startswith("README") and path.suffix == ".md") or
                         (name.startswith("docs/") and path.suffix == ".md" and not name.startswith("docs/dependencies/")))
        if not documentation:
            return True
    return False


def changed_paths(event, kind, head):
    if kind == "workflow_dispatch":
        return None
    base = event.get("pull_request", {}).get("base", {}).get("sha") if kind == "pull_request" else event.get("before")
    target = event.get("pull_request", {}).get("head", {}).get("sha") if kind == "pull_request" else head
    if not all(isinstance(ref, str) and re.fullmatch(r"[0-9a-f]{40}", ref) and ref != "0" * 40 for ref in (base, target)):
        return None
    # Include both sides of a move: src/foo.cpp -> docs/foo.md must remain full.
    revisions = [base + "..." + target] if kind == "pull_request" else [base, target]
    try:
        result = subprocess.check_output(["git", "diff", "--name-only", "--no-renames", "-z", *revisions], timeout=30)
        return result.decode("utf-8").rstrip("\0").split("\0") if result else []
    except (subprocess.SubprocessError, UnicodeError):
        return None


if __name__ == "__main__":
    try:
        paths = changed_paths(json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text()),
                              os.environ.get("GITHUB_EVENT_NAME", ""), os.environ.get("GITHUB_SHA", ""))
    except (OSError, ValueError, KeyError):
        paths = None
    runtime = paths is None or needs_runtime(paths)
    with Path(os.environ["GITHUB_OUTPUT"]).open("a") as output:
        output.write("runtime=" + str(runtime).lower() + "\n")
    print("Full native compilation" if runtime else "Documentation and release-tool checks")
