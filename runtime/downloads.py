"""Fetch pinned source inputs with bounded retries and atomic, verified caching."""
from pathlib import Path
import hashlib
import os
import re
import ssl
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request


def checksum(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def fetch(item, destination, attempts=2, timeout=20):
    destination = Path(destination)
    expected = item['sha256']
    if not re.fullmatch('[0-9a-f]{64}', expected):
        raise ValueError('Invalid pinned SHA-256')
    urls = list(dict.fromkeys([item['url'], *item.get('mirrors', [])]))
    if not 1 <= len(urls) <= 4 or not 1 <= attempts <= 3:
        raise ValueError('Invalid download retry policy')
    for url in urls:
        parsed = urllib.parse.urlsplit(url)
        if parsed.scheme != 'https' or not parsed.hostname or parsed.username or parsed.password:
            raise ValueError('Source URLs must use HTTPS without credentials')
    if destination.exists():
        if not destination.is_file() or checksum(destination) != expected:
            raise ValueError('Cached source checksum mismatch: ' + destination.name)
        return destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    errors = []
    for url in urls:
        host = urllib.parse.urlsplit(url).hostname
        for attempt in range(attempts):
            fd, name = tempfile.mkstemp(prefix='.source-', dir=destination.parent)
            temporary = Path(name)
            try:
                with os.fdopen(fd, 'wb') as output:
                    request = urllib.request.Request(url, headers={'User-Agent': 'Rondo source builder'})
                    with urllib.request.urlopen(request, timeout=timeout) as incoming:
                        digest = hashlib.sha256()
                        for chunk in iter(lambda: incoming.read(1024 * 1024), b''):
                            output.write(chunk)
                            digest.update(chunk)
                    output.flush()
                    os.fsync(output.fileno())
                # Integrity errors are fatal, including on a mirror. Never hide
                # changed upstream bytes by silently accepting another response.
                if digest.hexdigest() != expected:
                    raise ValueError('Downloaded source checksum mismatch: ' + destination.name)
                temporary.replace(destination)
                return destination
            except (urllib.error.URLError, TimeoutError, ConnectionError, ssl.SSLError) as error:
                code = getattr(error, 'code', None)
                errors.append(host + ': ' + (str(code) if code else type(error).__name__))
                if isinstance(error, urllib.error.HTTPError):
                    error.close()
                    if code not in (408, 429, 500, 502, 503, 504):
                        break
                if attempt + 1 < attempts:
                    time.sleep(min(attempt + 1, 2))
            finally:
                temporary.unlink(missing_ok=True)
    raise RuntimeError('Unable to fetch verified source: ' + '; '.join(errors))
