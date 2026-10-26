"""Installer file/rollback checks use temporary directories, never mounted media."""
from pathlib import Path
import json
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import install_h700 as installer


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.card, self.boot = self.root / 'card', self.root / 'boot'
        self.card.mkdir(); self.boot.mkdir()
        (self.boot / 'bootlogo.bmp').write_bytes(b'original firmware logo')
        (self.boot / 'other.bin').write_bytes(b'untouched boot resource')
        state = self.card / 'RondoState'
        state.mkdir()
        (state / 'sram.bin').write_bytes(b'player progress')
        self.logo = (ROOT / 'assets/Boot/bootlogo.bmp').read_bytes()
        self.package = self.root / 'candidate.zip'
        with zipfile.ZipFile(self.package, 'w') as package:
            package.writestr('System/Rondo/build-info.json', json.dumps({
                'target': 'rg34xxsp', 'baseos_version': '1.3.0', 'source_id': 'fixture'}))
            package.writestr('System/Rondo/assets/Boot/bootlogo.bmp', self.logo)
            package.writestr('System/launch_frontend.sh', '#!/bin/sh\n')
        self.audit = patch.object(installer, 'audit', return_value={'package_sha256': 'fixture'})
        self.audit.start()

    def tearDown(self):
        self.audit.stop()
        self.temporary.cleanup()

    def install(self, name='backup', guard=lambda: None):
        return installer.install(self.package, self.card, self.boot, self.root / name, guard)

    def test_first_install_and_repeat_keep_the_first_original_and_saves(self):
        self.install()
        self.assertEqual((self.boot / 'bootlogo.bmp').read_bytes(), self.logo)
        self.assertEqual(installer.original_logo(self.card), b'original firmware logo')
        self.install('second-backup')
        self.assertEqual(installer.original_logo(self.card), b'original firmware logo')
        self.assertEqual((self.card / 'RondoState/sram.bin').read_bytes(), b'player progress')
        self.assertEqual((self.boot / 'other.bin').read_bytes(), b'untouched boot resource')

    def test_theme_choice_and_alias_change_only_the_rondo_logo(self):
        selected = (ROOT / 'assets/Boot/Themes/nord.bmp').read_bytes()
        with zipfile.ZipFile(self.package, 'a') as package:
            package.writestr('System/Rondo/assets/Boot/Themes/nord.bmp', selected)
        (self.card / 'RondoState/config.json').write_text('{"theme_id":"slate"}')
        self.install()
        self.assertEqual((self.boot / 'bootlogo.bmp').read_bytes(), selected)
        self.assertEqual(installer.original_logo(self.card), b'original firmware logo')
        installer.restore_logo(self.card, self.boot, lambda: None)
        self.assertEqual((self.boot / 'bootlogo.bmp').read_bytes(), b'original firmware logo')

    def test_disabled_preference_is_respected(self):
        (self.card / 'RondoState/config.json').write_text('{"replace_boot_logo":false}')
        self.install()
        self.assertEqual((self.boot / 'bootlogo.bmp').read_bytes(), b'original firmware logo')
        self.assertIsNone(installer.original_logo(self.card))

    def test_restore_disables_replacement_and_preserves_progress(self):
        self.install()
        installer.restore_logo(self.card, self.boot, lambda: None)
        self.assertEqual((self.boot / 'bootlogo.bmp').read_bytes(), b'original firmware logo')
        self.assertFalse(installer.preferences(self.card)['replace_boot_logo'])
        self.assertEqual((self.card / 'RondoState/sram.bin').read_bytes(), b'player progress')

    def test_corrupt_backup_and_missing_original_refuse_without_writes(self):
        self.install()
        original = self.card / 'RondoState/BootLogo/original.bmp'
        original.write_bytes(b'corrupt')
        before = installer.inventory(self.card), installer.inventory(self.boot)
        with self.assertRaises(ValueError): self.install('another-backup')
        self.assertEqual(before, (installer.inventory(self.card), installer.inventory(self.boot)))
        original.unlink()
        with self.assertRaises(ValueError): installer.restore_logo(self.card, self.boot, lambda: None)

    def test_identity_change_refuses_before_destination_write(self):
        before = installer.inventory(self.card), installer.inventory(self.boot)
        def fail(): raise ValueError('identity changed')
        with self.assertRaises(ValueError): self.install(guard=fail)
        self.assertEqual(before, (installer.inventory(self.card), installer.inventory(self.boot)))

    def test_rondo_without_original_and_symlinks_are_rejected(self):
        (self.boot / 'bootlogo.bmp').write_bytes(self.logo)
        with self.assertRaises(ValueError): self.install()
        (self.boot / 'bootlogo.bmp').write_bytes(b'original firmware logo')
        (self.card / 'System').symlink_to(self.root / 'elsewhere', target_is_directory=True)
        with self.assertRaises(ValueError): self.install()


if __name__ == '__main__':
    unittest.main()
