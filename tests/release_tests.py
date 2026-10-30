"""Exercise release refusal paths in disposable Git repositories only."""
from pathlib import Path
import subprocess
import json
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'tools'))
from release import notes, prepare_changelog, validate_tag, version, bundle


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.git('init', '-q', '-b', 'main')
        self.git('config', 'user.name', 'Release test')
        self.git('config', 'user.email', 'release-test@example.invalid')
        (self.root / 'VERSION').write_text('0.0.0\r\n')
        (self.root / 'CHANGELOG.md').write_text('# Changelog\n\n## Unreleased\n\n- First preview.\n')

    def tearDown(self):
        self.temp.cleanup()

    def git(self, *args):
        return subprocess.check_output(['git', *args], cwd=self.root, stderr=subprocess.STDOUT, text=True).strip()

    def commit(self):
        self.git('add', '.')
        self.git('commit', '-qm', 'Release fixture')

    def candidate(self):
        prepare_changelog(self.root, '0.0.0', '0.0.1')
        (self.root / 'VERSION').write_text('0.0.1\r\n')
        self.commit()
        self.git('update-ref', 'refs/remotes/origin/main', 'HEAD')

    def test_release_bundle_refuses_shared_or_cached_builds(self):
        first, second = self.root / 'first', self.root / 'second'
        first.mkdir(); second.mkdir()
        with self.assertRaisesRegex(ValueError, 'separate'):
            bundle(self.root, first, first, self.root / 'output')
        for value in ['hit', 'miss']:
            (first / 'cache-result.json').write_text(json.dumps({'cache': value}))
            with self.assertRaisesRegex(ValueError, '--fresh'):
                bundle(self.root, first, second, self.root / 'output')

    def test_first_release_closes_changelog_and_accepts_crlf_version(self):
        self.candidate()
        self.git('tag', '-a', 'v0.0.1', '-m', 'Preview')
        self.assertEqual(validate_tag(self.root, 'v0.0.1'), ('0.0.1', '- First preview.\n'))
        self.assertIn('## Unreleased\n\n## [0.0.1]', (self.root / 'CHANGELOG.md').read_text())

    def test_refuses_duplicate_without_changing_file(self):
        prepare_changelog(self.root, '0.0.0', '0.0.1')
        before = (self.root / 'CHANGELOG.md').read_bytes()
        with self.assertRaises(ValueError):
            prepare_changelog(self.root, '0.0.0', '0.0.1')
        self.assertEqual(before, (self.root / 'CHANGELOG.md').read_bytes())

    def test_refuses_previous_version_mismatch_and_empty_changes(self):
        before = (self.root / 'CHANGELOG.md').read_bytes()
        with self.assertRaises(ValueError):
            prepare_changelog(self.root, '0.0.2', '0.0.3')
        self.assertEqual(before, (self.root / 'CHANGELOG.md').read_bytes())
        (self.root / 'CHANGELOG.md').write_text('# Changelog\n\n## Unreleased\n')
        with self.assertRaises(ValueError):
            prepare_changelog(self.root, '0.0.0', '0.0.1')

    def test_refuses_lightweight_tag(self):
        self.candidate()
        self.git('tag', 'v0.0.1')
        with self.assertRaisesRegex(ValueError, 'annotated'):
            validate_tag(self.root, 'v0.0.1')

    def test_refuses_candidate_outside_main(self):
        self.candidate()
        self.git('checkout', '-qb', 'dev')
        (self.root / 'change').write_text('Unreleased change')
        self.commit()
        self.git('tag', '-a', 'v0.0.1', '-m', 'Unmerged preview')
        with self.assertRaises(subprocess.CalledProcessError):
            validate_tag(self.root, 'v0.0.1')

    def test_refuses_stale_checkout_and_wrong_version(self):
        self.candidate()
        self.git('tag', '-a', 'v0.0.1', '-m', 'Preview')
        with self.assertRaises(ValueError):
            validate_tag(self.root, 'v0.0.2')
        (self.root / 'change').write_text('Later work')
        self.commit()
        with self.assertRaisesRegex(ValueError, 'Checkout'):
            validate_tag(self.root, 'v0.0.1')

    def test_rejects_ambiguous_versions_and_notes(self):
        for value in ['01.0.0', '0.0.1-beta', '0.0.1\n0.0.2', '../1']:
            with self.assertRaises(ValueError):
                version(value)
        with self.assertRaises(ValueError):
            notes('## [0.0.1] - 2026-01-01\n\n## Unreleased\n', '0.0.1')


if __name__ == '__main__':
    unittest.main()
