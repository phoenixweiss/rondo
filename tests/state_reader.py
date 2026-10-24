"""Independent reader used to verify packaged state containers in runner checks."""
import hashlib
import json
import struct

def state_metadata(path):
    with path.open('rb') as stream:
        header = stream.read(84)
        assert header[:8] == b'RONDOST\x02'
        size, preview, payload = struct.unpack('<III', header[8:20])
        assert 0 < size <= 16384 and preview <= 192*144*4 and 0 < payload <= 32*1024*1024
        assert path.stat().st_size == 84 + size + preview + payload
        raw = stream.read(size)
        assert hashlib.sha256(raw).hexdigest().encode() == header[20:84]
        meta = json.loads(raw)
        image = stream.read(preview)
        assert hashlib.sha256(image).hexdigest() == meta['preview_sha256']
        assert hashlib.sha256(stream.read(payload)).hexdigest() == meta['sha256']
        return meta
