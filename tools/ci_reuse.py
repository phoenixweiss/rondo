"""Reuse an existing dev check only for a push of that exact commit to main."""
import json
import os
import re
import subprocess
import urllib.request


def select_run(runs, sha, repository, current):
    candidates = [run for run in runs if str(run.get('id')) != str(current)
                  and run.get('head_sha') == sha and run.get('head_branch') == 'dev'
                  and run.get('event') == 'push' and run.get('path') == '.github/workflows/native.yml'
                  and (run.get('head_repository') or {}).get('full_name') == repository
                  and (run.get('status') in ('queued', 'in_progress', 'pending', 'waiting')
                       or run.get('status') == 'completed' and run.get('conclusion') == 'success')]
    return str(max(candidates, key=lambda run: run['id'])['id']) if candidates else ''


NATIVE_JOBS = {'native (ubuntu-24.04)', 'native (macos-15)'}


def native_jobs_eligible(jobs):
    native = [job for job in jobs if job.get('name') in NATIVE_JOBS]
    return len(native) == 2 and {job['name'] for job in native} == NATIVE_JOBS and all(
        job.get('status') in ('queued', 'in_progress', 'waiting', 'pending') or
        job.get('status') == 'completed' and job.get('conclusion') == 'success' for job in native)


def main():
    reuse = ''
    sha, repo = os.environ.get('GITHUB_SHA', ''), os.environ.get('GITHUB_REPOSITORY', '')
    if os.environ.get('GITHUB_EVENT_NAME') == 'push' and os.environ.get('GITHUB_REF') == 'refs/heads/main' and re.fullmatch('[0-9a-f]{40}', sha) and re.fullmatch(r'[\w.-]+/[\w.-]+', repo):
        try:
            dev = subprocess.check_output(['git', 'rev-parse', 'refs/remotes/origin/dev'], text=True).strip()
            if dev == sha:
                url = 'https://api.github.com/repos/' + repo + '/actions/workflows/native.yml/runs?branch=dev&event=push&head_sha=' + sha + '&per_page=20'
                request = urllib.request.Request(url, headers={'Authorization': 'Bearer ' + os.environ['GITHUB_TOKEN'], 'Accept': 'application/vnd.github+json'})
                with urllib.request.urlopen(request, timeout=20) as response:
                    reuse = select_run(json.load(response)['workflow_runs'], sha, repo, os.environ.get('GITHUB_RUN_ID'))
                if reuse:
                    request = urllib.request.Request('https://api.github.com/repos/' + repo + '/actions/runs/' + reuse + '/jobs?per_page=100', headers=dict(request.headers))
                    with urllib.request.urlopen(request, timeout=20) as response:
                        if not native_jobs_eligible(json.load(response)['jobs']):
                            reuse = ''
        except (OSError, ValueError, KeyError, subprocess.CalledProcessError):
            reuse = ''
            print('No verified dev run available; run the full native matrix.')
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        output.write('run=' + reuse + '\n')
    print('Reusing dev run ' + reuse if reuse else 'Using the native matrix.')


if __name__ == '__main__':
    main()
