"""Build contract for the experimental, exact-kernel RG34XXSP lid adapter."""
from pathlib import Path
import hashlib
import json
import struct
import sys


def sections(blob):
    if blob[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', blob, 18)[0] != 183:
        raise ValueError('Lid module must be little-endian AArch64 ELF64')
    offset = struct.unpack_from('<Q', blob, 40)[0]
    stride, count, names_index = struct.unpack_from('<HHH', blob, 58)
    headers = [struct.unpack_from('<IIQQQQIIQQ', blob, offset + i * stride) for i in range(count)]
    name_header = headers[names_index]
    names = blob[name_header[4]:name_header[4] + name_header[5]]
    result = {}
    for header in headers:
        end = names.index(b'\0', header[0])
        name = names[header[0]:end].decode()
        result[name] = b'' if header[1] == 8 else blob[header[4]:header[4] + header[5]]
    return result


def validate(blob, profile):
    data = sections(blob)
    info = dict(item.split('=', 1) for item in data['.modinfo'].decode().split('\0') if '=' in item)
    if info['vermagic'] != profile['vermagic'] or len(data['.gnu.linkonce.this_module']) != profile['struct_module_size']:
        raise ValueError('Lid module kernel identity/layout mismatch')
    imports = {}
    for offset in range(0, len(data['__versions']), 64):
        crc = struct.unpack_from('<Q', data['__versions'], offset)[0]
        name = data['__versions'][offset + 8:offset + 64].split(b'\0')[0].decode()
        imports[name] = crc
    strings = data['.strtab']
    undefined = set()
    for offset in range(0, len(data['.symtab']), 24):
        name, _, _, index, _, _ = struct.unpack_from('<IBBHQQ', data['.symtab'], offset)
        if name and index == 0:
            undefined.add(strings[name:strings.index(b'\0', name)].decode())
    required = {'module_layout', 'set_os_sleep_mode', 'get_os_sleep_mode', 'init_uts_ns'}
    if not required <= imports.keys() or not undefined <= imports.keys():
        raise ValueError('Missing or unversioned lid module imports')
    if any(profile['imports'].get(name) != crc for name, crc in imports.items()):
        raise ValueError('Lid module vendor symbol CRC mismatch')
    return {'vermagic': info['vermagic'], 'imports_verified': len(imports),
            'sha256': hashlib.sha256(blob).hexdigest(), 'physical_module_load_verified': False}


def prepare(source, output):
    profile = json.loads((source / 'profile.json').read_text())
    if hashlib.sha256((source / 'kernel.config').read_bytes()).hexdigest() != profile['config_sha256']:
        raise ValueError('Audited lid kernel configuration mismatch')
    header = '#define VENDOR_RELEASE ' + json.dumps(profile['release']) + '\n'
    header += '#define VENDOR_VERSION ' + json.dumps(profile['version']) + '\n'
    header += 'static const unsigned char vendor_set_code[] = {'
    header += ','.join(hex(b) for b in bytes.fromhex(profile['set_function_code'])) + '};\n'
    (output / 'vendor_profile.h').write_text(header)


if __name__ == '__main__':
    action, source, output = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
    if action == 'prepare':
        prepare(source, output)
    elif action == 'verify':
        profile = json.loads((source / 'profile.json').read_text())
        blob = (output / 'rondo_lid_wake.ko').read_bytes()
        result = validate(blob, profile)
        # Check that verification really rejects altered vendor CRC and identity metadata.
        bad = {**profile, 'vermagic': 'unsupported'}
        try:
            validate(blob, bad)
        except ValueError:
            pass
        else:
            raise ValueError('Module verification accepted unsupported kernel')
        bad = {**profile, 'imports': {**profile['imports'], 'set_os_sleep_mode': 0}}
        try:
            validate(blob, bad)
        except ValueError:
            pass
        else:
            raise ValueError('Module verification accepted incorrect vendor CRC')
        (output / 'lid-module-verification.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
    else:
        raise ValueError('Unknown module action')
