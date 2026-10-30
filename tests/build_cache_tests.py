"""Exercise invalidation and fail-closed restoration before any destination writes."""
from pathlib import Path
import json
import os
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'runtime'))
sys.path.insert(0, str(ROOT / 'tools'))
import build_core
from build_cache import input_key, restore, save
from dependency_cache import cache_key


class CacheTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.base = Path(self.temporary.name)
        self.source, self.cache, self.dest = [self.base / name for name in ['source', 'cache', 'destination']]
        (self.source / 'sdk/lib').mkdir(parents=True)
        (self.source / 'sdk/lib/core.so.1').write_bytes(b'compiled dependency')
        (self.source / 'sdk/lib/core.so.1').chmod(0o755)
        (self.source / 'sdk/lib/core.so').symlink_to('core.so.1')
        self.roots, self.key = ['sdk'], 'a' * 64

    def tearDown(self):
        self.temporary.cleanup()

    def seed(self):
        save(self.cache, self.key, self.source, self.roots)

    def test_round_trip_and_miss(self):
        self.assertFalse(restore(self.cache, self.key, self.dest, self.roots))
        self.seed()
        self.assertTrue(restore(self.cache, self.key, self.dest, self.roots))
        self.assertEqual((self.dest / 'sdk/lib/core.so').read_bytes(), b'compiled dependency')
        self.assertEqual(os.readlink(self.dest / 'sdk/lib/core.so'), 'core.so.1')
        self.assertEqual((self.dest / 'sdk/lib/core.so.1').stat().st_mode & 0o777, 0o755)

    def test_corrupt_missing_and_extra_files_rejected_before_restore(self):
        for action in ['corrupt', 'missing', 'extra']:
            with self.subTest(action=action):
                if self.cache.exists(): shutil.rmtree(self.cache)
                self.seed()
                payload = self.cache / self.key / 'payload'
                path = payload / 'sdk/lib/core.so.1'
                if action == 'corrupt': path.write_bytes(b'wrong')
                if action == 'missing': path.unlink()
                if action == 'extra': (payload / 'extra').write_bytes(b'wrong')
                with self.assertRaises(ValueError): restore(self.cache, self.key, self.dest, self.roots)
                self.assertFalse(self.dest.exists())

    def test_external_symlink_rejected(self):
        (self.source / 'sdk/lib/core.so').unlink()
        (self.source / 'sdk/lib/core.so').symlink_to(self.source / 'sdk/lib/core.so.1')
        with self.assertRaisesRegex(ValueError, 'Unsafe'): self.seed()

    def test_destination_symlink_rejected(self):
        self.seed()
        self.dest.mkdir()
        (self.dest / 'sdk').symlink_to(self.source / 'sdk', target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'Linked'): restore(self.cache, self.key, self.dest, self.roots)

    def test_identity_mismatch_rejected(self):
        self.seed()
        manifest = self.cache / self.key / 'manifest.json'
        data = json.loads(manifest.read_text()); data['key'] = 'b' * 64
        manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'identity'): restore(self.cache, self.key, self.dest, self.roots)

    def test_retains_two_entries_and_different_outputs_rejected(self):
        for letter in 'abc': save(self.cache, letter * 64, self.source, self.roots)
        self.assertEqual(len(list(self.cache.iterdir())), 2)
        (self.source / 'sdk/lib/core.so.1').write_bytes(b'different')
        with self.assertRaisesRegex(ValueError, 'Different'): save(self.cache, 'c' * 64, self.source, self.roots)

    def test_h700_key_tracks_dependency_flags_toolchain_and_pins_but_not_ui(self):
        # Copy only the real key inputs, not large assets or a build tree.
        root = self.base / 'project'
        for name in ['runtime', 'docs/dependencies', 'tools']:
            shutil.copytree(ROOT / name, root / name, ignore=shutil.ignore_patterns('__pycache__'))
        original = cache_key(root)
        (root / 'src').mkdir(); (root / 'src/ui.cpp').write_text('UI change')
        self.assertEqual(cache_key(root), original)
        for name in ['runtime/build-lock.json', 'runtime/build-cores.sh', 'docs/dependencies/psx-core-source.json', 'runtime/build-libraries.sh', 'runtime/lid-wake/profile.json']:
            path = root / name; before = path.read_bytes(); path.write_bytes(before + b'\n')
            self.assertNotEqual(cache_key(root), original, name)
            path.write_bytes(before)

    def test_context_invalidates_key(self):
        path = self.source / 'input'; path.write_text('pin')
        self.assertNotEqual(input_key(self.source, ['input'], {'compiler': 'a'}), input_key(self.source, ['input'], {'compiler': 'b'}))

    def test_native_key_tracks_flags_compiler_and_linux_packages(self):
        with patch.dict(os.environ, {}, clear=True), patch('build_core.platform.system', return_value='Linux'), patch('build_core.platform.platform', return_value='Linux-test'), patch('build_core.platform.machine', return_value='aarch64'), patch('build_core.shutil.which', return_value='/tool/compiler'), patch('build_core.digest', return_value='compiler-sha'), patch('build_core.subprocess.run', return_value=SimpleNamespace(stdout='compiler 1')), patch('build_core.subprocess.check_output', return_value='package=1') as packages:
            original = build_core.cache_key()
            with patch.dict(os.environ, {'CFLAGS': '-O3'}):
                self.assertNotEqual(build_core.cache_key(), original)
            packages.return_value = 'package=2'
            self.assertNotEqual(build_core.cache_key(), original)
            packages.return_value = 'package=1'
            with patch('build_core.digest', return_value='new-compiler-sha'):
                self.assertNotEqual(build_core.cache_key(), original)

    def test_release_packages_bypass_compiled_cache(self):
        workflow = (ROOT / '.github/workflows/h700.yml').read_text()
        commands = [line for line in workflow.splitlines() if 'tools/build_h700_runtime.py' in line]
        self.assertEqual(len(commands), 2)
        self.assertTrue(all('--fresh' in command for command in commands))


if __name__ == '__main__': unittest.main()
