"""Audit the configured NES distribution from the ZIP's own frozen inputs."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import struct
import sys
import tarfile
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'runtime'))
from inputs import sha
from package import verify_zip


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def audit(path):
    verify_zip(path)
    with zipfile.ZipFile(path) as package:
        names = set(package.namelist())
        binaries = {}
        for name in sorted(names):
            data = package.read(name)
            if data[:4] == b'\x7fELF':
                require(data[4:6] == b'\x02\x01' and struct.unpack_from('<H', data, 18)[0] == 183,
                        'Not AArch64 ELF: ' + name)
                binaries[name] = digest(data)
        expected = {'System/Rondo/' + p for p in ['bin/rondo', 'cores/fceumm_libretro.so',
                    'lib/libSDL2-2.0.so.0', 'lib/libSDL2_ttf-2.0.so.0']}
        require(set(binaries) == expected, 'Unexpected installed ELF set')
        prefix = 'System/Rondo/'
        app = {n[len(prefix):] for n in names if n.startswith(prefix)}
        covered = set()
        for line in package.read(prefix + 'manifest.sha256').decode().splitlines():
            checksum, name = line.split('  ', 1)
            require(name not in covered and digest(package.read(prefix + name)) == checksum,
                    'App manifest mismatch: ' + name)
            covered.add(name)
        require(covered == app - {'manifest.sha256'}, 'Incomplete app manifest')
        source = {}
        with tarfile.open(fileobj=io.BytesIO(package.read('Sources/rondo-source.tar.gz'))) as archive:
            for member in archive.getmembers():
                require(member.isfile() and member.name.startswith('Rondo/'), 'Unexpected source member')
                name = member.name[len('Rondo/'):]
                require(name not in source, 'Duplicate source member')
                source[name] = archive.extractfile(member).read()
        inventory = json.loads(package.read('Sources/source-inputs.json'))
        require({n: digest(data) for n, data in source.items()} == inventory, 'Source inventory mismatch')
        info = json.loads(package.read(prefix + 'build-info.json'))
        require(info['source_id'] == digest(json.dumps(inventory, sort_keys=True).encode()), 'Source ID mismatch')
        required = ['FreeType-GPLv2.txt', 'FreeType-License-Choice.txt', 'GCC-Runtime-Exception-3.1.txt',
                    'GCC-GPLv3.txt', 'nlohmann-json-MIT.txt', 'Nested-Source-Notices.txt',
                    'SDL-yuv2rgb-BSD-3-Clause.txt', 'Lucide-Feather-Licenses.txt',
                    'Ubuntu-Font-Licence.txt', 'Rondo-Asset-Policy.md', 'FCEUmm-GPLv2.txt']
        for name in required:
            require('Licenses/' + name in names, 'Missing distribution notice: ' + name)
        if 'VERSION' in source:
            require(info.get('version') == source['VERSION'].decode().strip(), 'Packaged version mismatch')
            require('.bumpster/hooks/pre-bump' in source and 'tools/release.py' in source,
                    'Missing release build controls')
        runtime = json.loads(source['docs/dependencies/runtime-sources.json'])
        core = json.loads(source['docs/dependencies/core-source.json'])
        require(runtime['ttf']['freetype_license_for_gplv2_package'] == 'GPL-2.0-or-later',
                'FreeType GPLv2 alternative not selected')
        upstream = {}
        for name, checksum in [('sdl', runtime['sdl']['sha256']), ('ttf', runtime['ttf']['sha256']),
                               ('fceumm', core['archive_sha256'])]:
            data = package.read('Sources/' + name + '.tar.gz')
            require(digest(data) == checksum, 'Upstream archive hash mismatch: ' + name)
            upstream[name] = checksum
        with tarfile.open(fileobj=io.BytesIO(package.read('Sources/ttf.tar.gz'))) as archive:
            gpl = archive.extractfile('SDL2_ttf-2.20.2/external/freetype/docs/GPLv2.TXT').read()
            require(gpl == package.read('Licenses/FreeType-GPLv2.txt'), 'Wrong FreeType license text')
        license_inputs = json.loads(source['docs/dependencies/license-inputs.json'])
        for name, checksum in license_inputs['files'].items():
            require(digest(package.read('Licenses/' + name)) == checksum, 'SDK license hash mismatch')
        require(digest(package.read('Licenses/nlohmann-json-MIT.txt')) ==
                license_inputs['nlohmann_json_license']['sha256'], 'JSON license hash mismatch')
        asset_inputs = json.loads(source['docs/dependencies/asset-inputs.json'])
        for record in asset_inputs['fonts']:
            require(digest(package.read(prefix + record['path'])) == record['sha256'], 'Font hash mismatch')
        for record in json.loads(source['assets/Assets/Icons/sources.json']):
            require(digest(package.read(prefix + 'assets/' + record['path'])) == record['sha256'],
                    'Icon source hash mismatch')
        require(digest(package.read(prefix + 'assets/Assets/Icons/LICENSE')) ==
                asset_inputs['icons_license_sha256'], 'Icon license hash mismatch')
        require(package.read('Licenses/Rondo-Asset-Policy.md') == source['assets/LICENSE.md'],
                'Asset policy mismatch')
        for name in ['runtime/build-all.sh', 'runtime/build-libraries.sh', 'runtime/build-programs.sh',
                     'runtime/build-lock.json', 'tools/build_h700_runtime.py', 'vendor/libretro.h']:
            require(name in source, 'Missing corresponding build input: ' + name)
        require(not any(n.startswith(('RondoState/', 'RondoGames/')) for n in names), 'Private payload')
        return {'package_sha256': sha(path), 'source_id': info['source_id'], 'payload_files': len(names),
                'source_files_verified': len(source), 'installed_elf_sha256': binaries,
                'upstream_archive_sha256': upstream, 'freetype_selected_license': 'GPL-2.0-or-later',
                'combined_nes_distribution': 'GPLv2-compliant; original Rondo code/artwork MIT',
                'configured_package_audit_passed': True, 'sd_accessed': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = audit(args.package)
    output = json.dumps(result, indent=2) + '\n'
    if args.report:
        with args.report.open('x') as report:
            report.write(output)
    print(output, end='')


if __name__ == '__main__':
    main()
