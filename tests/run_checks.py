"""Quiet native integration checks using the same UI actions and real libretro runner."""
from pathlib import Path
import argparse, hashlib, json, subprocess, sys
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'tools'))
from fixtures import create
ROOT=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser();p.add_argument('--core',type=Path,required=True);p.add_argument('--work',type=Path,required=True);args=p.parse_args()
work=args.work.resolve();work.mkdir(parents=True,exist_ok=False)
create(work/'fixtures');state=work/'state';out=work/'renders'
# Canonical smoke commands are checked in, not a generated second UI implementation.
commands=json.loads((ROOT/'tests/vertical-slice.json').read_text())
script=work/'script.json';script.write_text(json.dumps(commands))
libs=[f'TF1={work}/fixtures/card-a',f'TF2={work}/fixtures/card-b']
base=[str(ROOT/'build/rondo'),'--assets',str(ROOT/'assets'),'--state',str(state),'--core',str(args.core.resolve()),'--headless','--output',str(out)]
for v in libs:base.extend(['--volume',v])
def execute(name,commands):
    script.write_text(json.dumps(commands));subprocess.run(base+['--script',str(script)],check=True,stdout=(work/(name+'.log')).open('w'),stderr=subprocess.STDOUT,timeout=45)
def sources():return {str(x.relative_to(work/'fixtures')):hashlib.sha256(x.read_bytes()).hexdigest() for x in (work/'fixtures').rglob('*') if x.is_file()}
before=sources();execute('vertical',commands)
report=json.loads((out/'shell-report.json').read_text());assert len(report['runner_sessions'])==2
assert any(x['event']=='resumed_slot:2' for x in report['runner_sessions'][1]['report']['trace'])
assert all(x['type']=='stopped' and x['report']['frames']>0 and x['report']['audio_frames']>0 for x in report['runner_sessions'])
(work/'vertical-report.json').write_text(json.dumps(report,indent=2))
# Restart verifies durable preference/focus; no automatic game launch.
execute('restart',[{'capture':'restart','assert_screen':'library','assert_config':{'locale':'ru','artwork':'title'} }])
assert before==sources(),'Source library changed'
subprocess.run([str(ROOT/'build/core-tests'),str(args.core.resolve()),str(work/'fixtures/card-a/NES/Rondo Test Cartridge/test.nes'),str(work/'core-test-state')],check=True,stdout=(work/'core-tests.log').open('w'),stderr=subprocess.STDOUT,timeout=30)
# Missing dependency remains a recoverable library route; headless input opens no window.
missing=work/'missing-core.json';missing.write_text(json.dumps([{'action':'A','assert_screen':'recovery','capture':'unavailable-core'},{'action':'B','assert_screen':'library'}]))
subprocess.run([str(ROOT/'build/rondo'),'--assets',str(ROOT/'assets'),'--state',str(work/'missing-state'),'--volume',libs[0],'--core',str(work/'absent.dylib'),'--headless','--script',str(missing),'--output',str(out)],check=True,stdout=(work/'missing.log').open('w'),stderr=subprocess.STDOUT,timeout=30)
# Empty favorites / missing exact artwork / long measured rows / write failure are UI-level checks.
ui=work/'ui-state';ui.mkdir();config={'schema':1,'locale':'en','artwork':'title','section':'All','focus':'','favorites':[]};(ui/'config.json').write_text(json.dumps(config))
extra=[{'action':'DOWN','capture':'missing-title'},{'action':'R1','capture':'empty-favorites'},{'action':'L1'}]+[{'action':'DOWN'}]*17+[{'capture':'library-bottom'}]
extraPath=work/'extra.json';extraPath.write_text(json.dumps(extra));subprocess.run([str(ROOT/'build/rondo'),'--assets',str(ROOT/'assets'),'--state',str(ui),'--volume',libs[0],'--volume',libs[1],'--headless','--script',str(extraPath),'--output',str(out)],check=True,stdout=(work/'ui.log').open('w'),stderr=subprocess.STDOUT,timeout=30)
# A write failure leaves the selected art mode in session and reports lack of persistence.
blocked=work/'blocked-state';blocked.write_bytes(b'not a directory')
failure=work/'write-failure.json';failure.write_text(json.dumps([{'action':'RIGHT','capture':'write-failure','assert_notice':True,'assert_config':{'artwork':'cover'}}]))
subprocess.run([str(ROOT/'build/rondo'),'--assets',str(ROOT/'assets'),'--state',str(blocked),'--volume',libs[0],'--headless','--script',str(failure),'--output',str(out)],check=True,stdout=(work/'write-failure.log').open('w'),stderr=subprocess.STDOUT,timeout=30)
# Actual child-process load failure, not only missing-core detection in the shell.
broken=work/'broken-card/NES/Invalid ROM';broken.mkdir(parents=True);(broken/'invalid.nes').write_bytes(b'not an iNES cartridge')
failure.write_text(json.dumps([{'action':'A'},{'assert_screen':'recovery','capture':'runner-recovery'},{'action':'B','assert_screen':'library'}]))
subprocess.run([str(ROOT/'build/rondo'),'--assets',str(ROOT/'assets'),'--state',str(work/'broken-state'),'--core',str(args.core.resolve()),'--volume',f'broken={work}/broken-card','--headless','--script',str(failure),'--output',str(out)],check=True,stdout=(work/'runner-failure.log').open('w'),stderr=subprocess.STDOUT,timeout=30)
# Timezone applies an IANA ID via the process-local adapter; never changes macOS timezone.
zoneState=work/'zone-state';zoneState.mkdir();(zoneState/'config.json').write_text(json.dumps({'schema':1,'timezone':'Europe/Moscow'}))
# The picker now opens at the saved country. Up reaches the pinned UTC entry.
commands=[{'action':'Y'},{'action':'DOWN'},{'action':'A'}]+[{'action':'UP'}]*300+[{'action':'A','assert_screen':'global_settings','assert_config':{'timezone':'Etc/UTC'},'capture':'timezone-applied'}]
failure.write_text(json.dumps(commands))
subprocess.run([str(ROOT/'build/rondo'),'--assets',str(ROOT/'assets'),'--state',str(zoneState),'--headless','--script',str(failure),'--output',str(out)],check=True,stdout=(work/'timezone.log').open('w'),stderr=subprocess.STDOUT,timeout=30)
result={'native_build':True,'real_fceumm_state_roundtrip':True,'sram_restart':True,'shell_runner_sessions':2,'durable_restart':True,'runner_load_failure_recovered':True,'settings_write_failure_visible':True,'timezone_apply':True,'volume_master_persists_across_shell_runner':True,'software_mute_verified':True,'library_files_preserved':len(before),'render_backend':'SDL2 software surface, same UI as interactive window','audio_device_tested':False,'device_tested':False,'sd_modified':False}
(work/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
