"""Run the actual launcher with isolated process/filesystem adapters, never host power tools."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='rondo-power-') as directory:
    work = Path(directory)
    commands = work / 'commands'
    commands.mkdir()
    def executable(path, text):
        path.write_text('#!/bin/sh\n' + text)
        path.chmod(0o700)
    journal = work / 'calls'
    env = dict(os.environ, PATH=str(commands)+':'+os.environ['PATH'], CALLS=str(journal))
    executable(commands/'sync', 'echo sync >> "$CALLS"\nexit "${SYNC_STATUS:-0}"\n')
    executable(commands/'systemctl', 'echo forbidden-systemctl >> "$CALLS"\nexit 91\n')
    for operation in ['poweroff', 'reboot']:
        executable(commands/('baseos-'+operation), 'echo baseos-'+operation+' >> "$CALLS"\nexit "${POWER_STATUS:-0}"\n')
    executable(commands/'sleep', 'exit 1\n')  # Bound the launcher's failure fallback.
    helper = work / 'system-power.sh'
    actual = (ROOT / 'tools/system-power.sh').read_text()
    # Bind only the two explicit device paths into our isolated test directory.
    # No production environment override or host power command is used.
    for operation in ['poweroff', 'reboot']:
        target = '/usr/sbin/baseos-'+operation
        assert target in actual
        actual = actual.replace(target, str(commands/('baseos-'+operation)))
    helper.write_text(actual)
    for code, operation in [(64, 'poweroff'), (65, 'reboot')]:
        journal.write_text('')
        subprocess.run(['sh', str(helper), str(code)], env=env, check=True, timeout=5)
        assert journal.read_text().splitlines() == ['sync', 'baseos-'+operation]
    for args in [[], ['0'], ['1'], ['reboot']]:
        journal.write_text('')
        assert subprocess.run(['sh', str(helper), *args], env=env, timeout=5).returncode == 2
        assert journal.read_text() == ''
    journal.write_text('')
    assert subprocess.run(['sh', str(helper), '64'], env=dict(env, SYNC_STATUS='1'), timeout=5).returncode != 0
    assert journal.read_text().splitlines() == ['sync']
    journal.write_text('')
    assert subprocess.run(['sh', str(helper), '65'], env=dict(env, POWER_STATUS='1'), timeout=5).returncode != 0
    assert journal.read_text().splitlines() == ['sync', 'baseos-reboot']
    missing = commands/'baseos-poweroff'
    saved = missing.read_bytes()
    missing.unlink()
    journal.write_text('')
    assert subprocess.run(['sh', str(helper), '64'], env=env, timeout=5, stderr=subprocess.DEVNULL).returncode != 0
    assert journal.read_text() == ''
    missing.write_bytes(saved)
    missing.chmod(0o700)
    app = work / 'System/Rondo'
    (app/'tools').mkdir(parents=True)
    (app/'tools/system-power.sh').write_bytes(helper.read_bytes())
    loader = work / 'loader'
    executable(loader, 'echo frontend-exit >> "$CALLS"\nexit "$FRONTEND_STATUS"\n')
    source = (ROOT/'runtime/package/launch_frontend.sh').read_text()
    source = source.replace('/lib/ld-linux-aarch64.so.1', str(loader)).replace('/tmp/rondo-output', str(work/'output'))
    launcher = work / 'launcher.sh'
    launcher.write_text(source)
    for code, operation in [(64, 'poweroff'), (65, 'reboot'), (0, None), (1, None)]:
        journal.write_text('')
        subprocess.run(['sh', str(launcher)], cwd=work, env=dict(env, FRONTEND_STATUS=str(code)), timeout=5)
        expected = ['frontend-exit'] + (['sync', 'baseos-'+operation] if operation else [])
        assert journal.read_text().splitlines() == expected
print('PASS: shutdown/restart only after frontend exit and successful sync; errors/normal exit do not power off')
