"""H700 dependency cache contract; application sources never enter this key."""
from pathlib import Path
import argparse
import json
import os
from build_cache import input_key, restore, save

CORES = ['fceumm', 'gambatte', 'gpsp', 'mednafen_supafaust', 'picodrive', 'pcsx_rearmed']
ROOTS = ['sdk', *['programs/' + name + '_libretro.so' for name in CORES],
         'lid-module/rondo_lid_wake.ko', 'lid-module/lid-module-verification.json', 'openbios-build.log']


def cache_key(root):
    names = ['runtime/' + name for name in ['build-lock.json', 'build_cache.py', 'dependency_cache.py',
             'build-all.sh', 'inputs.py', 'build_openbios.py', 'openbios-toolchain.json', 'build-libraries.sh',
             'build-cores.sh', 'build-lid-module.sh', 'lid_module.py']]
    names += ['tools/build_h700_runtime.py']
    names += [p.relative_to(root).as_posix() for p in (root / 'runtime/lid-wake').rglob('*') if p.is_file()]
    names += [p.relative_to(root).as_posix() for p in (root / 'docs/dependencies').glob('*.json')]
    return input_key(root, names, {'format': 1})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['restore', 'save'])
    args = parser.parse_args()
    root, work = Path('/src'), Path('/work')
    key = cache_key(root)
    if args.action == 'restore':
        enabled = os.environ.get('RONDO_DEPENDENCY_CACHE') == '1'
        hit = enabled and restore('/compiled-cache', key, work, ROOTS)
        (work / 'cache-result.json').write_text(json.dumps({'cache': 'hit' if hit else ('miss' if enabled else 'bypassed'), 'key': key}) + '\n')
        if hit:
            (work / '.dependencies-restored').touch()
    elif os.environ.get('RONDO_DEPENDENCY_CACHE') == '1':
        save('/compiled-cache', key, work, ROOTS)


if __name__ == '__main__':
    main()
