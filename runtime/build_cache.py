"""Bounded, content-checked cache of compiled dependencies, never application objects."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import stat
import tempfile


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def input_key(root, names, context):
    inputs = {name: digest(Path(root) / name) for name in sorted(names)}
    return hashlib.sha256(json.dumps({'inputs': inputs, 'context': context}, sort_keys=True).encode()).hexdigest()


def inventory(base, roots):
    base = Path(base).resolve()
    result = {}
    for name in roots:
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts or not relative.parts:
            raise ValueError('Unsafe cache root')
        top = base / relative
        if not top.exists() or top.is_symlink():
            raise ValueError('Missing or linked cache root: ' + name)
        paths = [top, *sorted(top.rglob('*'))] if top.is_dir() else [top]
        for path in paths:
            key = path.relative_to(base).as_posix()
            if path.is_symlink():
                target = os.readlink(path)
                resolved = path.resolve()
                if Path(target).is_absolute() or not resolved.is_relative_to(top) or not resolved.exists():
                    raise ValueError('Unsafe cache symlink: ' + key)
                result[key] = {'link': target}
            elif path.is_dir():
                result[key] = {'directory': True}
            elif stat.S_ISREG(path.stat().st_mode):
                result[key] = {'sha256': digest(path), 'mode': stat.S_IMODE(path.stat().st_mode)}
            else:
                raise ValueError('Unsupported cache entry: ' + key)
    return result


def restore(cache, key, destination, roots):
    entry = Path(cache) / key
    if not entry.exists():
        return False
    if entry.is_symlink() or (entry / 'payload').is_symlink():
        raise ValueError('Linked cache entry')
    manifest = json.loads((entry / 'manifest.json').read_text())
    if manifest.get('format') != 1 or manifest.get('key') != key or manifest.get('roots') != roots:
        raise ValueError('Cache identity mismatch')
    payload = entry / 'payload'
    actual = inventory(payload, roots)
    all_paths = {p.relative_to(payload).as_posix() for p in payload.rglob('*')}
    allowed = set(actual)
    for name in actual:
        allowed.update(str(p) for p in Path(name).parents if str(p) != '.')
    if actual != manifest['files'] or all_paths != allowed:
        raise ValueError('Cache content mismatch')
    destination = Path(destination).resolve()
    for name in actual:
        path = destination / name
        if path.is_symlink() or any(parent.is_symlink() for parent in path.parents if parent != destination and parent.is_relative_to(destination)):
            raise ValueError('Linked cache destination')
    for name in roots:
        source, target = payload / name, destination / name
        if source.is_dir() and target.exists():
            raise ValueError('Cache destination directory already exists: ' + name)
    for name in roots:
        source, target = payload / name, destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            shutil.copytree(source, target, symlinks=True)
        else:
            shutil.copy2(source, target)
    os.utime(entry, None)
    return True


def save(cache, key, source, roots):
    cache = Path(cache)
    cache.mkdir(parents=True, exist_ok=True)
    files = inventory(source, roots)
    entry = cache / key
    if entry.exists():
        manifest = json.loads((entry / 'manifest.json').read_text())
        if manifest['files'] != files:
            raise ValueError('Different compiled outputs for the same cache key')
        return
    temporary = Path(tempfile.mkdtemp(prefix='.pending-', dir=cache))
    try:
        payload = temporary / 'payload'
        for name in roots:
            origin, target = Path(source) / name, payload / name
            target.parent.mkdir(parents=True, exist_ok=True)
            if origin.is_dir():
                shutil.copytree(origin, target, symlinks=True)
            else:
                shutil.copy2(origin, target)
        (temporary / 'manifest.json').write_text(json.dumps({'format': 1, 'key': key, 'roots': roots, 'files': files}, sort_keys=True) + '\n')
        temporary.rename(entry)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)
    entries = sorted((p for p in cache.iterdir() if p.is_dir() and not p.is_symlink() and len(p.name) == 64 and all(c in '0123456789abcdef' for c in p.name) and (p / 'manifest.json').is_file()), key=lambda p: p.stat().st_mtime, reverse=True)
    for old in entries[2:]:
        shutil.rmtree(old)
