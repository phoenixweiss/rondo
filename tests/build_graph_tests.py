"""Exercise incremental make dependencies with tiny real C++ translation units."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parent.parent


class BuildGraphTests(unittest.TestCase):
    def test_shared_objects_and_incremental_invalidation(self):
        with tempfile.TemporaryDirectory(prefix='rondo-make-') as temporary:
            root = Path(temporary)
            shutil.copyfile(ROOT / 'Makefile', root / 'Makefile')
            for directory in ('src', 'tests'):
                (root / directory).mkdir()
                for source in (ROOT / directory).glob('*.cpp'):
                    text = 'int main() { return 0; }\n' if directory == 'tests' or source.stem == 'main' else 'int stub_' + source.stem + ';\n'
                    if source.stem in {"preferences_tests", "screenshot_tests", "library_tests"}:
                        text = '#include "test_dependency.hpp"\n' + text
                    if source.stem == 'ui':
                        text = '#include "ui.hpp"\n' + text
                    (root / directory / source.name).write_text(text)
            (root / "tests/test_dependency.hpp").write_text("// Late-declared target dependency\n")
            (root / 'src/ui.hpp').write_text('#include "nested.hpp"\n')
            (root / 'src/nested.hpp').write_text('// Transitive dependency\n')

            def make(*flags):
                result = subprocess.run(['make', '-j4', 'native-programs',
                                         'PKG_CONFIG=true', 'CPPFLAGS=', 'CXXFLAGS=-std=c++17',
                                         'LDLIBS=', *flags], cwd=root, text=True,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
                self.assertEqual(result.returncode, 0, result.stdout)
                return result.stdout

            def compiles(output):
                return [line for line in output.splitlines() if ' -c ' in line]

            output = make()
            self.assertEqual(len(compiles(output)), 52, output)
            self.assertEqual(sum(' -c src/model.cpp ' in line for line in compiles(output)), 1)
            self.assertEqual(make(), '')
            time.sleep(1.05)
            (root / 'src/ui.cpp').touch()
            self.assertEqual(len(compiles(make())), 1)
            time.sleep(1.05)
            (root / 'src/nested.hpp').touch()
            self.assertEqual(len(compiles(make())), 1)
            self.assertEqual(make(), '')
            time.sleep(1.05)
            (root / 'tests/test_dependency.hpp').touch()
            self.assertEqual(len(compiles(make())), 3)
            self.assertEqual(make(), '')
            time.sleep(1.05)
            self.assertEqual(len(compiles(make('CXXFLAGS=-std=c++17 -O1'))), 52)
            time.sleep(1.05)
            self.assertEqual(len(compiles(make())), 52)  # Switching back is a rebuild too.
            time.sleep(1.05)
            linked = make('LDFLAGS=-s')
            self.assertEqual(compiles(linked), [])
            self.assertEqual(sum(' -o build/' in line for line in linked.splitlines()), 26)
            self.assertEqual(make('LDFLAGS=-s'), '')
            # Cross-adapter wrappers stay on link commands, never object commands.
            dry = subprocess.run(['make', '-n', 'h700-programs', 'PKG_CONFIG=true'],
                                 cwd=root, text=True, capture_output=True, check=True).stdout
            self.assertIn('-Wl,--wrap=ioctl,--wrap=write', dry)
            self.assertIn('-Wl,--wrap=posix_spawn', dry)
            self.assertIn('-Wl,--wrap=adjtimex,--wrap=clock_settime,--wrap=clock_gettime,--wrap=ioctl', dry)
            self.assertFalse(any('--wrap' in line for line in compiles(dry)))


if __name__ == '__main__':
    unittest.main()
