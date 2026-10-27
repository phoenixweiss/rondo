"""Fault-inject Linux durability syscalls in the actual atomic writer."""
from pathlib import Path
import os
import platform
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
if platform.system() != 'Linux':
    print('SKIP: Linux syncfs/linker-wrap checks; portable interruption checks run in state-tests')
else:
    with tempfile.TemporaryDirectory(prefix='rondo-storage-sync-') as directory:
        work = Path(directory)
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O1',
                        '-ffunction-sections', '-fdata-sections', '-Isrc', '-Ivendor',
                        'tests/storage_sync_tests.cpp', 'src/model.cpp',
                        '-Wl,--gc-sections,--wrap=write,--wrap=fsync,--wrap=rename,--wrap=syncfs',
                        '-o', str(work / 'storage-sync')], cwd=ROOT, check=True, timeout=120)
        subprocess.run([str(work / 'storage-sync'), str(work / 'files')], check=True, timeout=30)
