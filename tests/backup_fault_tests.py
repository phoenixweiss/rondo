"""Interrupt the real multi-file restore on Linux without an SD or full disk."""
from pathlib import Path
import os
import platform
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parent.parent
if platform.system()!='Linux':
    print('SKIP: Linux transaction syscall adapters; portable restore tests still run')
else:
    with tempfile.TemporaryDirectory(prefix='rondo-backup-faults-') as directory:
        work=Path(directory)
        subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O1','-ffunction-sections','-fdata-sections','-Isrc','-Ivendor',
            'tests/backup_fault_tests.cpp','src/model.cpp','src/preferences.cpp','src/psx.cpp','src/state.cpp','src/restore.cpp','src/backup.cpp','src/progress.cpp','src/game_statistics.cpp',
            '-Wl,--gc-sections,--wrap=rename,--wrap=write,--wrap=fsync','-pthread','-o',str(work/'test')],cwd=ROOT,check=True,timeout=180)
        subprocess.run([str(work/'test'),str(work/'data')],check=True,timeout=90)
