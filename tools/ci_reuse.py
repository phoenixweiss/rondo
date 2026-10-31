"""Reuse an existing dev check only for a push of that exact commit to main."""
import json
import os
import re
import subprocess
import time
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


def await_run(get_runs, get_jobs, sha, repository, current, wait_seconds=45,
              clock=time.monotonic, pause=time.sleep):
    """Allow the exact dev matrix to register after an atomic dev/main push."""
    deadline = clock() + wait_seconds
    while True:
        runs = get_runs()
        candidate = select_run(runs, sha, repository, current)
        if candidate:
            jobs = get_jobs(candidate)
            if native_jobs_eligible(jobs):
                return candidate
            native = [job for job in jobs if job.get('name') in NATIVE_JOBS]
            # A skipped/failed matrix cannot become eligible by waiting.
            if any(job.get('status') == 'completed' and job.get('conclusion') != 'success'
                   for job in native):
                return ''
        remaining = deadline - clock()
        if remaining <= 0:
            return ''
        pause(min(5, remaining))


def main():
    reuse = ''
    sha, repo = os.environ.get('GITHUB_SHA', ''), os.environ.get('GITHUB_REPOSITORY', '')
    if os.environ.get('GITHUB_EVENT_NAME') == 'push' and os.environ.get('GITHUB_REF') == 'refs/heads/main' and re.fullmatch('[0-9a-f]{40}', sha) and re.fullmatch(r'[\w.-]+/[\w.-]+', repo):
        try:
            dev = subprocess.check_output(['git', 'rev-parse', 'refs/remotes/origin/dev'], text=True).strip()
            if dev == sha:
                url = 'https://api.github.com/repos/' + repo + '/actions/workflows/native.yml/runs?branch=dev&event=push&head_sha=' + sha + '&per_page=20'
                headers = {'Authorization': 'Bearer ' + os.environ['GITHUB_TOKEN'],
                           'Accept': 'application/vnd.github+json'}
                def query(target):
                    request = urllib.request.Request(target, headers=headers)
                    with urllib.request.urlopen(request, timeout=20) as response:
                        return json.load(response)
                reuse = await_run(lambda: query(url)['workflow_runs'],
                    lambda run: query('https://api.github.com/repos/' + repo +
                        '/actions/runs/' + run + '/jobs?per_page=100')['jobs'],
                    sha, repo, os.environ.get('GITHUB_RUN_ID'))
        except (OSError, ValueError, KeyError, subprocess.CalledProcessError):
            reuse = ''
            print('No verified dev run available; run the full native matrix.')
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        output.write('run=' + reuse + '\n')
    print('Reusing dev run ' + reuse if reuse else 'Using the native matrix.')


if __name__ == '__main__':
    main()
