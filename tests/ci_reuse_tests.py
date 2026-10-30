"""Only an exact, trusted dev push can replace the main native matrix."""
from pathlib import Path
import os
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'tools'))
from ci_reuse import main, select_run, native_jobs_eligible


class ReuseTests(unittest.TestCase):
    def setUp(self):
        self.sha = 'a' * 40
        self.run = {'id': 10, 'head_sha': self.sha, 'head_branch': 'dev', 'event': 'push',
                    'path': '.github/workflows/native.yml', 'head_repository': {'full_name': 'example/rondo'},
                    'status': 'completed', 'conclusion': 'success'}

    def select(self, run): return select_run([run], self.sha, 'example/rondo', 11)

    def test_success_and_running_are_eligible(self):
        self.assertEqual(self.select(self.run), '10')
        self.assertEqual(self.select({**self.run, 'status': 'in_progress', 'conclusion': None}), '10')

    def test_wrong_sha_branch_event_workflow_repo_self_and_failure_rejected(self):
        for change in [{'head_sha': 'b'*40}, {'head_branch': 'main'}, {'event': 'pull_request'},
                       {'path': 'other.yml'}, {'head_repository': {'full_name': 'fork/rondo'}},
                       {'id': 11}, {'conclusion': 'failure'}, {'conclusion': 'cancelled'}, {'conclusion': 'skipped'}]:
            with self.subTest(change=change): self.assertEqual(self.select({**self.run, **change}), '')

    def test_docs_only_or_partial_matrix_cannot_be_reused(self):
        jobs = [{'name': 'native (' + os + ')', 'status': 'completed', 'conclusion': 'success'} for os in ['ubuntu-24.04', 'macos-15']]
        self.assertTrue(native_jobs_eligible(jobs))
        self.assertFalse(native_jobs_eligible([]))
        self.assertFalse(native_jobs_eligible(jobs[:1]))
        self.assertFalse(native_jobs_eligible([jobs[0], {**jobs[1], 'conclusion': 'skipped'}]))
        self.assertFalse(native_jobs_eligible([jobs[0], {**jobs[1], 'conclusion': 'failure'}]))

    def test_non_main_never_queries_api(self):
        with tempfile.TemporaryDirectory() as work:
            output = Path(work) / 'output'
            with patch.dict(os.environ, {'GITHUB_OUTPUT': str(output), 'GITHUB_EVENT_NAME': 'pull_request', 'GITHUB_REF': 'refs/heads/main'}, clear=True), patch('ci_reuse.urllib.request.urlopen') as api:
                main(); api.assert_not_called()
            self.assertEqual(output.read_text(), 'run=\n')

    def test_api_failure_falls_back_to_full_checks(self):
        with tempfile.TemporaryDirectory() as work:
            output = Path(work) / 'output'
            env = {'GITHUB_OUTPUT': str(output), 'GITHUB_EVENT_NAME': 'push', 'GITHUB_REF': 'refs/heads/main', 'GITHUB_SHA': self.sha, 'GITHUB_REPOSITORY': 'example/rondo', 'GITHUB_TOKEN': 'test'}
            with patch.dict(os.environ, env, clear=True), patch('ci_reuse.subprocess.check_output', return_value=self.sha), patch('ci_reuse.urllib.request.urlopen', side_effect=OSError('unavailable')):
                main()
            self.assertEqual(output.read_text(), 'run=\n')


if __name__ == '__main__': unittest.main()
