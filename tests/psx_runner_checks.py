from state_reader import state_metadata
"""Check real PSX shell/runner routing and continuation without an SD card."""
from pathlib import Path
import json,subprocess,sys,hashlib
ROOT=Path(__file__).resolve().parent.parent
binary=(Path(sys.argv[4]).resolve() if len(sys.argv)>4 else ROOT/'build')/'rondo'
work=Path(sys.argv[1]).resolve();work.mkdir(parents=True,exist_ok=False)
core,psx_core=[Path(x).resolve() for x in sys.argv[2:4]]
base=[str(binary),'--assets',str(ROOT/'assets'),'--state',str(work/'state'),'--core',str(core),
      '--volume','PSXFIXTURE='+str(ROOT/'runtime/test-fixtures/psx'),'--headless','--output',str(work/'renders')]
exit_game=[{'action':'MENU'},{'menu_item':'exit','action':'A'},{'action':'DOWN'},{'action':'A'}]
play=[{'frames':1200,'assert_screen':'running'}]+exit_game
for name,commands,extra in [
 ('sibling',[{'action':'MENU','assert_screen':'global_settings'},{'action':'B'},{'open_search':True,'assert_screen':'search'},{'action':'B'},{'action':'A','runner_script':play}],[]),
 ('explicit-resume',[{'assert_screen':'library'},{'action':'A'},{'action':'A','runner_script':play}],['--psx-core',str(psx_core)]),
 ('unfinished',[{'assert_screen':'library'},{'action':'A'},{'action':'A','runner_script':[{'frames':5}]}],[]),
 ('startup-resume',[{'assert_screen':'startup_resume'},{'action':'A','runner_script':play}],[])]:
 script=work/(name+'.json');script.write_text(json.dumps(commands))
 with (work/(name+'.log')).open('w') as log:subprocess.run(base+extra+['--script',str(script)],check=True,stdout=log,stderr=subprocess.STDOUT,timeout=45)
 report=json.loads((work/'renders/shell-report.json').read_text());assert len(report['runner_sessions'])==1
 assert bool(report['config']['resume_game'])==(name=='unfinished')
 if name=='unfinished':
  # An interrupted runner does not acknowledge an explicit close.
  assert report['runner_sessions'][0] is None
 else:
  assert report['runner_sessions'][0]['report']['audio_frames']>0
  assert report['runner_sessions'][0]['game_closed']
 if name in {'explicit-resume','startup-resume'}:assert any(e['event']=='resumed_auto' for e in report['runner_sessions'][0]['report']['trace'])
slots=list((work/'state/games').glob('*/autosave.state'));assert len(slots)==1
assert state_metadata(slots[0])['core']==hashlib.sha256(psx_core.read_bytes()).hexdigest()
print('PASS: PSX separate runner, sibling/explicit core selection, library keys, audio and startup continuation.')
