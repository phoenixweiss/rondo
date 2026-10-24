from state_reader import state_metadata
"""Exercise GB/GBC routing through the real shell and separate runner process."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
work = Path(sys.argv[1]).resolve()
work.mkdir(parents=True, exist_ok=False)
core, gb_core = map(lambda value: Path(value).resolve(), sys.argv[2:4])
fixtures = ROOT / 'runtime/test-fixtures/gb'
state, output = work / 'state', work / 'renders'
base = [str(ROOT / 'build/rondo'), '--assets', str(ROOT / 'assets'), '--state', str(state),
        '--core', str(core), '--gb-core', str(gb_core), '--volume', 'GBTEST=' + str(fixtures),
        '--headless', '--output', str(output)]
exit_game = [{'action': 'MENU'}] + [{'action': 'DOWN'}] * 4 + [
    {'action': 'A', 'assert_screen': 'exit_confirm'}, {'action': 'DOWN'}, {'action': 'A'}]
run_game = [{'frames': 90, 'assert_screen': 'running'}] + exit_game

def execute(name, commands):
    script = work / (name + '.json')
    script.write_text(json.dumps(commands))
    with (work / (name + '.log')).open('w') as log:
        subprocess.run(base + ['--script', str(script)], check=True, stdout=log,
                       stderr=subprocess.STDOUT, timeout=45)
    return json.loads((output / 'shell-report.json').read_text())

report = execute('both-systems', [
    {'action': 'A', 'runner_script': run_game}, {'assert_screen': 'library'},
    {'action': 'DOWN'}, {'action': 'A', 'runner_script': run_game},
    {'assert_screen': 'library'}])
assert len(report['runner_sessions']) == 2
assert all(s['type'] == 'stopped' and s['report']['audio_frames'] > 0
           for s in report['runner_sessions'])
assert {g['system'] for g in report['config']['recent_games']} == {'GB', 'GBC'}
slots = list(state.glob('games/*/autosave.state'))
assert len(slots) == 2
for slot in slots:
    assert state_metadata(slot)['core'] == hashlib.sha256(gb_core.read_bytes()).hexdigest()
report = execute('startup-resume', [
    {'assert_screen': 'library'}, {'action': 'A'}, {'action': 'A', 'runner_script': run_game},
    {'assert_screen': 'library'}])
assert any(t['event'] == 'resumed_auto' for t in report['runner_sessions'][0]['report']['trace'])
print('PASS: separate GB/GBC runners, core routing, audio, exit autosave, MRU and startup continuation.')
