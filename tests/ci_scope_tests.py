"""Keep docs-only checks light without skipping runtime changes or unknown diffs."""
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
from ci_scope import needs_runtime, changed_paths


class ScopeTests(unittest.TestCase):
    def test_docs_only(self):
        self.assertFalse(needs_runtime(["README.md", "README_RU.md", "CHANGELOG.md", "docs/display.md"]))

    def test_runtime_and_unknown_paths(self):
        for path in ["src/core.cpp", "assets/tokens.json", "vendor/libretro.h", "tests/run_checks.py",
                     "tools/release.py", "runtime/package.py", "docs/dependencies/core-source.json",
                     "docs/dependencies/notes.md", "licenses/core.txt", "Makefile", "VERSION",
                     ".bumpsterrc", ".github/workflows/native.yml", "unknown.conf"]:
            self.assertTrue(needs_runtime(["README.md", path]), path)
        self.assertTrue(needs_runtime([]))
        self.assertTrue(needs_runtime(["src/deleted.cpp", "docs/moved.md"]))

    def test_dispatch_first_push_and_unreadable_diff_are_full(self):
        self.assertIsNone(changed_paths({}, "workflow_dispatch", "a" * 40))
        self.assertIsNone(changed_paths({"before": "0" * 40}, "push", "a" * 40))
        with patch("ci_scope.subprocess.check_output", side_effect=subprocess.CalledProcessError(1, "git")):
            self.assertIsNone(changed_paths({"before": "b" * 40}, "push", "a" * 40))

    def test_null_delimited_paths_and_pr_merge_base(self):
        event = {"pull_request": {"base": {"sha": "b" * 40}, "head": {"sha": "a" * 40}}}
        with patch("ci_scope.subprocess.check_output", return_value=b"docs/a b.md\0docs/new\nline.md\0") as run:
            self.assertEqual(changed_paths(event, "pull_request", "c" * 40), ["docs/a b.md", "docs/new\nline.md"])
            self.assertIn("--no-renames", run.call_args.args[0])
            self.assertIn("b" * 40 + "..." + "a" * 40, run.call_args.args[0])


if __name__ == "__main__":
    unittest.main()
