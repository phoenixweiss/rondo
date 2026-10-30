"""Build pinned desktop cores, reusing only verified compiled dependencies."""
from pathlib import Path
import argparse
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys
import time
import tempfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'runtime'))
from build_cache import digest, input_key, restore, save

NAMES = ['fceumm', 'gambatte', 'gpsp', 'mednafen_supafaust', 'picodrive', 'pcsx_rearmed']


def cache_key():
    context = {'system': platform.platform(), 'machine': platform.machine(),
               'environment': {k: os.environ.get(k, '') for k in ['CC', 'CXX', 'AR', 'RANLIB', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'SDKROOT', 'MACOSX_DEPLOYMENT_TARGET', 'CPATH', 'LIBRARY_PATH', 'LD', 'MAKEFLAGS', 'SOURCE_DATE_EPOCH']}}
    for name, default in [('CC', 'cc'), ('CXX', 'c++'), ('AR', 'ar'), ('MAKE', 'make')]:
        command = shlex.split(os.environ.get(name, default))
        executable = Path(shutil.which(command[0]) or command[0]).resolve()
        version = subprocess.run([*command, '--version'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, check=False)
        context[name] = {'command': command, 'path': str(executable), 'sha256': digest(executable), 'version': version.stdout}

    if platform.system() == 'Linux' and shutil.which('dpkg-query'):
        # Distribution header/library updates can change a core even when GCC is unchanged.
        context['packages'] = subprocess.check_output(['dpkg-query', '-W', '-f=${Package}=${Version}\n'], text=True)
    if platform.system() == 'Darwin':
        context['sdk'] = subprocess.check_output(['xcrun', '--show-sdk-version'], text=True)
    names = ['tools/build_core.py', 'tools/build_core_fresh.py', 'runtime/build_cache.py', 'runtime/inputs.py']
    names += [p.relative_to(ROOT).as_posix() for p in (ROOT / 'docs/dependencies').glob('*core-source.json')]
    return input_key(ROOT, names, context)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=ROOT / 'build/cores')
    parser.add_argument('--compiled-cache', type=Path, default=ROOT / 'build/native-core-cache')
    parser.add_argument('--fresh', action='store_true', help='Ignore compiled caches and use a new source directory')
    args = parser.parse_args()
    directory = args.directory.resolve()
    if args.fresh and directory.exists() and any(directory.iterdir()):
        parser.error('--fresh requires an empty directory')
    directory.mkdir(parents=True, exist_ok=True)
    start = time.monotonic()
    suffix = '.dylib' if platform.system() == 'Darwin' else '.so'
    roots = [name + '_libretro' + suffix for name in NAMES]
    key = cache_key()
    hit = not args.fresh and restore(args.compiled_cache, key, directory, roots)
    if not hit:
        # A changed compiler/flags key must never reuse objects from an older build.
        with tempfile.TemporaryDirectory(prefix='.core-build-', dir=directory) as temporary:
            subprocess.run([sys.executable, str(ROOT / 'tools/build_core_fresh.py'), '--directory', temporary], check=True)
            for name in roots:
                shutil.copy2(Path(temporary) / name, directory / name)
        if not args.fresh:
            save(args.compiled_cache, key, directory, roots)
    result = {'cache': 'bypassed' if args.fresh else ('hit' if hit else 'miss'), 'key': key, 'seconds': round(time.monotonic() - start, 3)}
    (directory / 'cache-result.json').write_text(json.dumps(result, sort_keys=True) + '\n')
    print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
