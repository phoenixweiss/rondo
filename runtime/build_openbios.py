"""Build OpenBIOS offline using separately pinned, build-only compiler packages."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import urllib.request


def toolchain_inputs(root):
    return json.loads((Path(root) / 'runtime/openbios-toolchain.json').read_text())['packages']


def fetch_toolchain(root, cache, destination):
    destination.mkdir(parents=True, exist_ok=True)
    for item in toolchain_inputs(root):
        cached = cache / (item['sha256'] + '.deb')
        if not cached.exists():
            partial = cached.with_suffix('.download')
            try:
                with urllib.request.urlopen(item['url'], timeout=90) as incoming, partial.open('wb') as out:
                    shutil.copyfileobj(incoming, out)
                if hashlib.sha256(partial.read_bytes()).hexdigest() != item['sha256']:
                    raise ValueError('Compiler package checksum mismatch: ' + item['file'])
                partial.replace(cached)
            finally:
                partial.unlink(missing_ok=True)
        if hashlib.sha256(cached.read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('Cached compiler checksum mismatch')
        shutil.copyfile(cached, destination / item['file'])


def build(root, work, check=True):
    meta = json.loads((root / 'docs/dependencies/openbios-source.json').read_text())
    sdk = work / 'openbios-sdk'
    sdk.mkdir(exist_ok=False)
    for item in toolchain_inputs(root):
        archive = work / 'openbios-toolchain' / item['file']
        if hashlib.sha256(archive.read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('Compiler input changed')
        subprocess.run(['dpkg-deb', '-x', str(archive), str(sdk)], check=True)
    source = work / 'sources' / ('nugget-' + meta['revision'])
    env = dict(os.environ, PATH=str(sdk / 'usr/bin') + ':' + os.environ['PATH'],
               GCC_EXEC_PREFIX=str(sdk / 'usr/lib/gcc-cross') + '/',
               LD_LIBRARY_PATH=str(sdk / 'usr/lib/aarch64-linux-gnu'), LC_ALL='C', TZ='UTC')
    env['SOURCE_DATE_EPOCH'] = str(json.loads((root / 'runtime/build-lock.json').read_text())['source_date_epoch'])
    # Strip host paths and disable the linker build ID, which otherwise includes
    # debug-path differences. This changes build flags, not upstream sources.
    cc = 'mipsel-linux-gnu-gcc -ffile-prefix-map=' + str(source) + '=/usr/src/openbios -Wl,--build-id=none'
    subprocess.run(['make', '-C', str(source / 'openbios'), '-j4', *meta['build_args'], 'CC=' + cc], env=env, check=True)
    output = source / 'openbios/openbios.bin'
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    if output.stat().st_size != 512 * 1024:
        raise ValueError('Unexpected OpenBIOS size')
    if check and digest != meta['output_sha256']:
        raise ValueError('Rebuilt OpenBIOS does not match the pinned asset: ' + digest)
    print(json.dumps({'openbios_sha256': digest, 'source_revision': meta['revision'], 'verified_against_asset': check}))
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path('/src'))
    parser.add_argument('--work', type=Path, default=Path('/work'))
    args = parser.parse_args()
    build(args.root, args.work)
