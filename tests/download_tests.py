"""Pinned downloads survive transport errors without weakening integrity checks."""
from pathlib import Path
import hashlib
import io
import sys
import tempfile
import unittest
import urllib.error
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'runtime'))
from downloads import fetch


class DownloadTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.output = self.root / 'source.tar.gz'
        self.item = {'url': 'https://primary.example/source',
                     'mirrors': ['https://mirror.example/source'],
                     'sha256': hashlib.sha256(b'verified').hexdigest()}

    def tearDown(self): self.temporary.cleanup()

    def test_primary_and_verified_cache_need_no_second_download(self):
        with patch('downloads.urllib.request.urlopen', return_value=io.BytesIO(b'verified')) as api:
            fetch(self.item, self.output)
            fetch(self.item, self.output)
            self.assertEqual(api.call_count, 1)
        self.assertEqual(self.output.read_bytes(), b'verified')
        self.assertEqual(list(self.root.glob('.source-*')), [])

    def test_transport_retry_then_exact_mirror(self):
        with patch('downloads.time.sleep'), patch('downloads.urllib.request.urlopen', side_effect=[TimeoutError(), TimeoutError(), io.BytesIO(b'verified')]) as api:
            fetch(self.item, self.output)
        self.assertEqual([c.args[0].full_url for c in api.call_args_list], [self.item['url'], self.item['url'], self.item['mirrors'][0]])
        self.assertTrue(all(c.kwargs['timeout'] == 20 for c in api.call_args_list))
        self.assertEqual(list(self.root.glob('.source-*')), [])

    def test_http_404_uses_mirror_without_retry(self):
        missing = urllib.error.HTTPError(self.item['url'], 404, 'missing', {}, None)
        with patch('downloads.urllib.request.urlopen', side_effect=[missing, io.BytesIO(b'verified')]) as api:
            fetch(self.item, self.output)
        self.assertEqual(api.call_count, 2)

    def test_corrupt_primary_is_fatal_without_mirror(self):
        with patch('downloads.urllib.request.urlopen', return_value=io.BytesIO(b'changed')) as api:
            with self.assertRaisesRegex(ValueError, 'checksum'): fetch(self.item, self.output)
        self.assertEqual(api.call_count, 1)
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.root.glob('.source-*')), [])

    def test_corrupt_cache_is_not_overwritten(self):
        self.output.write_bytes(b'changed')
        with patch('downloads.urllib.request.urlopen') as api:
            with self.assertRaisesRegex(ValueError, 'Cached'): fetch(self.item, self.output)
            api.assert_not_called()
        self.assertEqual(self.output.read_bytes(), b'changed')

    def test_interrupted_body_leaves_no_partial_cache(self):
        class Interrupted(io.BytesIO):
            def read(self, size): raise TimeoutError()
        with patch('downloads.time.sleep'), patch('downloads.urllib.request.urlopen', side_effect=lambda *a, **k: Interrupted()) as api:
            with self.assertRaises(RuntimeError): fetch(self.item, self.output)
        self.assertEqual(api.call_count, 4)
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.root.glob('.source-*')), [])

    def test_http_503_retries_primary(self):
        busy = urllib.error.HTTPError(self.item['url'], 503, 'busy', {}, None)
        with patch('downloads.time.sleep') as pause, patch('downloads.urllib.request.urlopen', side_effect=[busy, io.BytesIO(b'verified')]) as api:
            fetch(self.item, self.output)
        self.assertEqual([c.args[0].full_url for c in api.call_args_list], [self.item['url']] * 2)
        pause.assert_called_once_with(1)

    def test_disk_failure_is_not_retried_as_network_failure(self):
        with patch('downloads.os.fsync', side_effect=OSError('disk full')), patch('downloads.urllib.request.urlopen', return_value=io.BytesIO(b'verified')) as api:
            with self.assertRaises(OSError): fetch(self.item, self.output)
        self.assertEqual(api.call_count, 1)
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.root.glob('.source-*')), [])

    def test_non_https_and_credentials_are_rejected_before_network(self):
        for url in ['http://primary.example/source', 'https://user:password@primary.example/source']:
            with patch('downloads.urllib.request.urlopen') as api:
                with self.assertRaises(ValueError): fetch({**self.item, 'url': url}, self.output)
                api.assert_not_called()


if __name__ == '__main__': unittest.main()
