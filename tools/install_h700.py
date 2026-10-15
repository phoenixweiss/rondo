"""Install on explicitly selected, mounted macOS BaseOS volumes; never flash a disk."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import plistlib
import shutil
import struct
import subprocess
import tempfile
import zipfile

from audit_h700_package import audit


IGNORED = {'.fseventsd', '.Spotlight-V100', '.Trashes', '.TemporaryItems', '.DS_Store'}


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def inventory(root):
    result = {}
    for path in sorted(root.rglob('*')):
        relative = path.relative_to(root)
        if any(part in IGNORED or part.startswith('._') for part in relative.parts):
            continue
        if path.is_symlink():
            raise ValueError('Symlink on destination: ' + str(relative))
        if path.is_file():
            result[relative.as_posix()] = sha(path)
    return result


def atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix='.rondo-install-', dir=path.parent)
    temporary = Path(name)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        if sha(temporary) != hashlib.sha256(data).hexdigest():
            raise ValueError('Write readback mismatch: ' + str(path))
        os.replace(temporary, path)
        if sha(path) != hashlib.sha256(data).hexdigest():
            raise ValueError('Replacement readback mismatch: ' + str(path))
    finally:
        if temporary.exists():
            temporary.unlink()


def preferences(card):
    path = card / 'RondoState/config.json'
    data = json.loads(path.read_text()) if path.exists() else {}
    if not isinstance(data, dict) or not isinstance(data.get('replace_boot_logo', True), bool):
        raise ValueError('Invalid boot-logo preference; destination unchanged')
    return data


def original_logo(card):
    backup = card / 'RondoState/BootLogo'
    image, manifest = backup / 'original.bmp', backup / 'original.sha256'
    if image.exists() != manifest.exists():
        raise ValueError('Incomplete original boot-logo backup')
    if image.exists():
        if manifest.read_text().split() != [sha(image), 'original.bmp']:
            raise ValueError('Original boot-logo backup checksum mismatch')
        return image.read_bytes()
    return None


def identify(card, boot):
    def info(path):
        return plistlib.loads(subprocess.check_output(['diskutil', 'info', '-plist', str(path)], timeout=20))
    data, resource = info(card), info(boot)
    disk = info(data['ParentWholeDisk'])
    if (disk.get('Internal') is not False or not disk.get('WholeDisk')
            or disk.get('VirtualOrPhysical') != 'Physical' or disk.get('Content') != 'GUID_partition_scheme'
            or data['ParentWholeDisk'] != resource['ParentWholeDisk']
            or data['DeviceIdentifier'] != disk['DeviceIdentifier'] + 's7'
            or resource['DeviceIdentifier'] != disk['DeviceIdentifier'] + 's2'
            or data.get('FilesystemType') != 'msdos' or resource.get('FilesystemType') != 'msdos'
            or data.get('PartitionMapPartitionOffset') != 1430257664
            or resource.get('PartitionMapPartitionOffset') != 104857600
            or resource.get('IOKitSize') != 33554432
            or not 0 <= disk['TotalSize'] - data['PartitionMapPartitionOffset'] - data['IOKitSize'] <= 2 * 1024 * 1024
            or Path(data['MountPoint']).resolve() != card or Path(resource['MountPoint']).resolve() != boot):
        raise ValueError('Expected expanded BaseOS TF1 data/boot volumes on one external physical disk')
    if not (card / 'baseos.conf').is_file() or not (boot / 'bootlogo.bmp').is_file():
        raise ValueError('Missing BaseOS configuration or boot logo')
    return {'disk': disk['DeviceIdentifier'], 'bytes': disk['TotalSize'], 'bus': disk.get('BusProtocol'),
            'data_uuid': data['VolumeUUID'], 'boot_uuid': resource.get('DiskUUID'),
            'data_offset': data['PartitionMapPartitionOffset'], 'data_bytes': data['IOKitSize']}


def install(package, card, boot, backup, guard):
    report = audit(package)
    if card in backup.parents or boot in backup.parents or backup in card.parents or backup in boot.parents:
        raise ValueError('Backup must be separate from both card volumes')
    with zipfile.ZipFile(package) as archive:
        info = json.loads(archive.read('System/Rondo/build-info.json'))
        if info['target'] != 'rg34xxsp' or info['baseos_version'] != '1.3.0':
            raise ValueError('Unsupported package target')
        payload = {name: archive.read(name) for name in archive.namelist() if name.startswith('System/')}
    logo = payload['System/Rondo/assets/Boot/bootlogo.bmp']
    if logo[:2] != b'BM' or struct.unpack_from('<iiHHI', logo, 18) != (720, 480, 1, 24, 0):
        raise ValueError('Expected uncompressed 720x480 RGB24 boot logo')
    enabled = preferences(card).get('replace_boot_logo', True)
    original = original_logo(card)
    current_logo = (boot / 'bootlogo.bmp').read_bytes()
    if enabled and original is None and current_logo == logo:
        raise ValueError('Rondo logo already installed without an original backup; refusing to call it original')
    before, boot_before = inventory(card), inventory(boot)
    # Reject symlink ancestors or obstructions before any destination writes.
    for name in payload:
        path = card / name
        if path.is_dir() or any(p.exists() and not p.is_dir() for p in path.parents if p != card.parent):
            raise ValueError('Obstructed destination: ' + name)
    guard()
    backup.mkdir(parents=True, exist_ok=False)
    for prefix, source, files in [('data', card, before), ('boot', boot, boot_before)]:
        for name, digest in files.items():
            target = backup / prefix / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / name, target)
            if sha(target) != digest:
                raise ValueError('Backup verification failed: ' + name)
    receipt = {'package_sha256': report['package_sha256'], 'source_id': info['source_id'],
               'before': before, 'boot_before': boot_before, 'replace_boot_logo': enabled, 'complete': False}
    receipt_path = backup / 'receipt.json'
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    guard()
    if inventory(card) != before or inventory(boot) != boot_before:
        raise ValueError('Destination changed during backup')
    if enabled and original is None:
        saved = card / 'RondoState/BootLogo'
        atomic(saved / 'original.bmp', current_logo)
        atomic(saved / 'original.sha256', (hashlib.sha256(current_logo).hexdigest() + '  original.bmp\n').encode())
        original_logo(card)
    for name, data in payload.items():
        atomic(card / name, data)
    if enabled:
        atomic(boot / 'bootlogo.bmp', logo)
    for name, data in payload.items():
        if sha(card / name) != hashlib.sha256(data).hexdigest():
            raise ValueError('Installed file mismatch: ' + name)
    for name, digest in before.items():
        if name not in payload and sha(card / name) != digest:
            raise ValueError('Preserved file changed: ' + name)
    for name, digest in boot_before.items():
        if (name != 'bootlogo.bmp' or not enabled) and sha(boot / name) != digest:
            raise ValueError('Boot resource changed: ' + name)
    receipt.update(complete=True, installed={n: hashlib.sha256(b).hexdigest() for n, b in payload.items()},
                   boot_after=inventory(boot), original_logo_sha256=sha(card / 'RondoState/BootLogo/original.bmp')
                   if (card / 'RondoState/BootLogo/original.bmp').exists() else None)
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt


def restore_logo(card, boot, guard):
    config = preferences(card)
    original = original_logo(card)
    if original is None:
        raise ValueError('No verified original boot logo')
    guard()
    atomic(boot / 'bootlogo.bmp', original)
    config['replace_boot_logo'] = False
    atomic(card / 'RondoState/config.json', (json.dumps(config, indent=2) + '\n').encode())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--card', type=Path, required=True)
    parser.add_argument('--boot', type=Path, required=True)
    parser.add_argument('--backup', type=Path)
    parser.add_argument('--expected-uuid')
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--restore-logo', action='store_true')
    args = parser.parse_args()
    card, boot = args.card.resolve(), args.boot.resolve()
    identity = identify(card, boot)
    print(json.dumps(identity, indent=2))
    if args.package:
        audit(args.package)
    if not args.apply:
        print('Read-only plan. No destination files changed.')
        return
    if args.expected_uuid != identity['data_uuid']:
        parser.error('--expected-uuid must match the inspected data volume')
    def guard():
        if identify(card, boot) != identity:
            raise ValueError('Destination identity changed')
    if args.restore_logo:
        restore_logo(card, boot, guard)
    else:
        if not args.package or not args.backup:
            parser.error('--package and a new external --backup directory are required')
        install(args.package.resolve(), card, boot, args.backup.resolve(), guard)
    subprocess.run(['sync'], check=True, timeout=60)
    print('Files and boot logo verified. Safely eject the physical disk before disconnecting.')


if __name__ == '__main__':
    main()
