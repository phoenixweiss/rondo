"""Run the TF2 bridge with mocked physical probes/mounts, never host media."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parent.parent
SCRIPT=(ROOT/'tools/tf2-launch.sh').read_text().rsplit('\nrondo_tf2_start\n',1)[0]

class TF2LauncherTests(unittest.TestCase):
    def run_bridge(self,case):
        with tempfile.TemporaryDirectory(prefix='rondo-tf2-') as temp:
            root=Path(temp).resolve();one=root/'card one';two=root/'card two'
            (one/'System/Rondo/bin').mkdir(parents=True);two.mkdir()
            (one/'System/Rondo/bin/rondo').write_text('fixture')
            (one/'System/launch_frontend.sh').write_text('#!/bin/sh\nprintf "%s\\n%s\\n" "$PWD" "$RONDO_TF2_ROOT" > "$RESULT"\n')
            def esc(p):return str(p).replace('\\','\\134').replace(' ','\\040')
            mounts=root/'mounts';rows=[f'/dev/mmcblk1p1 {esc(two)} exfat {"ro" if case=="readonly-tf2" else "rw"},noatime 0 0']
            if case not in ('mount','missing'):rows += [f'/dev/mmcblk0p7 {esc(one)} vfat {"ro" if case=="readonly" else "rw"},utf8 0 0']
            if case=='duplicate':rows += [f'/dev/mmcblk1p1 /elsewhere vfat rw 0 0']
            mounts.write_text('\n'.join(rows)+'\n')
            if case=='no-runtime':(one/'System/Rondo/bin/rondo').unlink()
            mock='''
rondo_mounts() { cat "$MOUNTS"; }
rondo_block() { [ "$CASE" != missing ]; }
rondo_pause() { :; }
rondo_mount_tf1() { printf '%s\\n' "$NEW_MOUNT" >> "$MOUNTS"; echo mounted > "$MOUNTED"; }
rondo_tf2_start
'''
            env={**os.environ,'MOUNTS':str(mounts),'RESULT':str(root/'result'),'MOUNTED':str(root/'mounted'),'CASE':case,
                 'NEW_MOUNT':f'/dev/mmcblk0p7 {esc(one)} vfat rw,utf8 0 0'}
            result=subprocess.run(['sh','-c',SCRIPT+mock],cwd=one if case=='wrong-cwd' else two,env=env,text=True,capture_output=True,timeout=5)
            if case in ('existing','mount','readonly-tf2'):
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual((root/'result').read_text().splitlines(),[str(one),str(two)])
                self.assertEqual((root/'mounted').exists(),case=='mount')
            else:
                self.assertNotEqual(result.returncode,0)
                expected={'missing':'TF1 is missing','readonly':'writable TF1 is unavailable','wrong-cwd':'wrong working directory','duplicate':'TF2 mount not identified','no-runtime':'install the main Rondo runtime'}[case]
                self.assertIn(expected,result.stderr)
                self.assertFalse((root/'result').exists())
                self.assertFalse((root/'mounted').exists())
    def test_existing_tf1_mount(self):self.run_bridge('existing')
    def test_mount_tf1_only_when_absent(self):self.run_bridge('mount')
    def test_readonly_tf2(self):self.run_bridge('readonly-tf2')
    def test_missing_tf1(self):self.run_bridge('missing')
    def test_readonly_tf1_is_not_remounted(self):self.run_bridge('readonly')
    def test_wrong_working_directory(self):self.run_bridge('wrong-cwd')
    def test_duplicate_mount_is_rejected(self):self.run_bridge('duplicate')
    def test_no_runtime_on_tf1(self):self.run_bridge('no-runtime')
    def test_source_contract(self):
        source=(ROOT/'runtime/package/launch_frontend.sh').read_text()
        self.assertIn('--state "$SD/RondoState"',source)
        self.assertIn('--volume "TF2=$RONDO_TF2_ROOT/RondoGames"',source)
        self.assertNotIn('fsck',SCRIPT)
        subprocess.run(['sh','-n',str(ROOT/'tools/tf2-launch.sh')],check=True)
        subprocess.run(['sh','-n',str(ROOT/'runtime/package/launch_frontend.sh')],check=True)
if __name__=='__main__':unittest.main()
