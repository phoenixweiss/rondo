"""Checks for source integrity, extraction boundaries and package reproducibility."""
from pathlib import Path
import hashlib
import io
import os
import sys
import tarfile
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "runtime"))
from inputs import extract, sha, archive_name
from package import source_archive, verify_zip


class RuntimeBuildTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def archive(self, name):
        path = self.root / "input.tar.gz"
        with tarfile.open(path, "w:gz") as archive:
            member = tarfile.TarInfo(name)
            member.size = 4
            archive.addfile(member, io.BytesIO(b"test"))
        return path

    def test_host_can_replace_read_only_container_timing(self):
        import importlib.util
        import json
        path = Path(__file__).resolve().parent.parent / "tools/build_h700_runtime.py"
        spec = importlib.util.spec_from_file_location("h700_build_timing", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        target = self.root / "cache-result.json"
        target.write_text(json.dumps({"cache": "bypassed", "components": {"cores": "bypassed"}}))
        target.chmod(0o444)
        try:
            module.record_build_timing(self.root, 1.23456)
            self.assertEqual(json.loads(target.read_text()), {
                "cache": "bypassed", "components": {"cores": "bypassed"}, "seconds": 1.235})
            self.assertEqual(list(self.root.glob(".cache-result-*")), [])
        finally:
            target.chmod(0o600)

    def test_native_and_packaged_scaling_sequences_agree(self):
        import json
        root = Path(__file__).resolve().parent.parent
        def sequence(value):
            if isinstance(value, list):
                for i, item in enumerate(value):
                    if isinstance(item, dict) and item.get("capture") == "scaling-on":
                        return [(step["action"], step["assert_integer_scaling"])
                                for step in value[i:i+6]]
                    result = sequence(item)
                    if result is not None: return result
            elif isinstance(value, dict):
                for item in value.values():
                    result = sequence(item)
                    if result is not None: return result
        native = sequence(json.loads((root/"tests/vertical-slice.json").read_text()))
        packaged = sequence(json.loads((root/"runtime/package/smoke.json").read_text()))
        self.assertIsNotNone(native)
        self.assertEqual(native, packaged)

    def test_md_fixture_is_reproducible(self):
        import importlib.util
        root = Path(__file__).resolve().parent.parent
        spec = importlib.util.spec_from_file_location('md_fixture', root / 'tools/md_fixture.py')
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        output = self.root / 'technical-test.gen'; module.create(output)
        self.assertEqual(output.read_bytes(), (root / 'runtime/test-fixtures/md/MD/technical-test.gen').read_bytes())

    def test_pinned_archive_extension_is_preserved(self):
        self.assertEqual(archive_name('linux', {'archive_suffix': '.tar.xz'}), 'linux.tar.xz')
        self.assertEqual(archive_name('bc', {}), 'bc.tar.gz')

    def test_rejects_corrupt_download_before_extraction(self):
        archive = self.archive("pkg/file")
        with self.assertRaisesRegex(ValueError, "checksum"):
            extract(archive, self.root / "output", "0" * 64, "pkg")
        self.assertFalse((self.root / "output").exists())

    def test_rejects_archive_traversal(self):
        archive = self.archive("../escaped")
        with self.assertRaisesRegex(ValueError, "Unsafe"):
            extract(archive, self.root / "output", sha(archive), "pkg")
        self.assertFalse((self.root / "escaped").exists())

    def test_rejects_external_symlink(self):
        path = self.root / "link.tar.gz"
        with tarfile.open(path, "w:gz") as archive:
            member = tarfile.TarInfo("pkg/link")
            member.type = tarfile.SYMTYPE
            member.linkname = str(self.root / "outside")
            archive.addfile(member)
        with self.assertRaises(tarfile.FilterError):
            extract(path, self.root / "output", sha(path), "pkg")
        self.assertFalse((self.root / "outside").exists())

    def test_source_archive_ignores_host_timestamps_and_modes(self):
        source = self.root / "source"
        source.mkdir()
        script = source / "build.sh"
        script.write_text("#!/bin/sh\nexit 0\n")
        a, b = self.root / "a.tar.gz", self.root / "b.tar.gz"
        source_archive(source, a, 1792022400)
        os.utime(script, (2000000000, 2000000000))
        script.chmod(0o600)
        source_archive(source, b, 1792022400)
        self.assertEqual(a.read_bytes(), b.read_bytes())

    def test_source_archive_preserves_executable_release_hook(self):
        source = self.root / 'source'
        hook = source / '.bumpster/hooks/pre-bump'
        hook.parent.mkdir(parents=True)
        hook.write_text('#!/bin/sh\nexit 0\n')
        output = self.root / 'source.tar.gz'
        source_archive(source, output, 1792022400)
        with tarfile.open(output) as archive:
            self.assertEqual(archive.getmember('Rondo/.bumpster/hooks/pre-bump').mode, 0o755)

    def test_zip_corruption_is_not_acknowledged(self):
        path = self.root / "corrupt.zip"
        with zipfile.ZipFile(path, "w") as archive:
            archive.writestr("data", b"changed")
            archive.writestr("PACKAGE.sha256", hashlib.sha256(b"original").hexdigest() + "  data\n")
        with self.assertRaisesRegex(ValueError, "checksum"):
            verify_zip(path)

    def test_zip_path_traversal_is_rejected(self):
        path = self.root / "escape.zip"
        with zipfile.ZipFile(path, "w") as archive:
            archive.writestr("../outside", "not allowed")
        with self.assertRaisesRegex(ValueError, "Unsafe"):
            verify_zip(path)


if __name__ == "__main__":
    unittest.main()
