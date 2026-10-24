"""Build pinned NES, GB/GBC and GBA cores; never access user ROMs."""
from pathlib import Path
import argparse, hashlib, io, json, os, platform, shutil, subprocess, tarfile, urllib.request
ROOT=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser();p.add_argument('--directory',type=Path,default=ROOT/'build/cores');args=p.parse_args()
directory=args.directory.resolve();directory.mkdir(parents=True,exist_ok=True)
for metadata,upstream,name,extra in [
    ('core-source.json','libretro-fceumm','fceumm',['HAVE_HDPACK=0','HAVE_NTSC=0']),
    ('gb-core-source.json','gambatte-libretro','gambatte',['-f','Makefile.libretro','HAVE_NETWORK=0','GIT_VERSION=']),
    ('gba-core-source.json','gpsp','gpsp',['GIT_VERSION=','HAVE_DYNAREC=0']),
    ('snes-core-source.json','supafaust','mednafen_supafaust',['GIT_VERSION='])]:
    meta=json.loads((ROOT/'docs/dependencies'/metadata).read_text());rev=meta['revision']
    source=directory/(upstream+'-'+rev)
    if not source.exists():
        with urllib.request.urlopen('https://codeload.github.com/libretro/'+upstream+'/tar.gz/'+rev,timeout=60) as response:data=response.read()
        if hashlib.sha256(data).hexdigest()!=meta['archive_sha256']:raise RuntimeError('Source checksum mismatch')
        (directory/(name+'-'+rev+'.tar.gz')).write_bytes(data)
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            for item in archive.getmembers():
                relative=Path(item.name)
                if relative.is_absolute() or '..' in relative.parts or item.issym() or item.islnk():raise RuntimeError('Unsafe source archive')
            archive.extractall(directory,filter='data')
    flags=['platform=osx' if platform.system()=='Darwin' else 'platform=unix']+extra
    environment=dict(os.environ)
    if name=='mednafen_supafaust' and platform.system()=='Darwin':environment['CXXFLAGS']='-DMDFN_USE_CONDVAR_SEMAPHORES'
    subprocess.run(['make','-j4',*flags],cwd=source,env=environment,check=True)
    filename=name+'_libretro.'+('dylib' if platform.system()=='Darwin' else 'so')
    shutil.copy2(source/filename,directory/filename);print(directory/filename)

# PicoDrive includes pinned submodules; a GitHub source tarball alone is incomplete.
import sys
sys.path.insert(0, str(ROOT / 'runtime'))
from inputs import records, extract, assemble_md
md=json.loads((ROOT/'docs/dependencies/md-core-source.json').read_text())
for name in ['picodrive']+[d['name'] for d in md['dependencies']]:
    item=records(ROOT)[name];archive=directory/(name+'.tar.gz')
    if not archive.exists():
        with urllib.request.urlopen(item['url'],timeout=60) as response:archive.write_bytes(response.read())
    if not (directory/item['directory']).exists():extract(archive,directory,item['sha256'],item['directory'])
    elif hashlib.sha256(archive.read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('Source checksum mismatch')
assemble_md(ROOT,directory)
source=directory/('picodrive-'+md['revision'])
subprocess.run(['make','-f','Makefile.libretro','-j4',
                'platform=osx' if platform.system()=='Darwin' else 'platform=unix',
                'use_libchdr=0','use_sh2drc=0','use_svpdrc=0','GIT_REVISION=','PLATFORM_TREMOR=1'],cwd=source,check=True)
filename='picodrive_libretro.'+('dylib' if platform.system()=='Darwin' else 'so')
shutil.copy2(source/filename,directory/filename);print(directory/filename)

# PSX: deterministic interpreter on desktop tests; H700 uses its ARM64 dynarec.
item=records(ROOT)['pcsx_rearmed'];archive=directory/'pcsx_rearmed.tar.gz'
if not archive.exists():
    with urllib.request.urlopen(item['url'],timeout=60) as response:archive.write_bytes(response.read())
if not (directory/item['directory']).exists():extract(archive,directory,item['sha256'],item['directory'])
elif hashlib.sha256(archive.read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('PSX source checksum mismatch')
subprocess.run(['make','-f','Makefile.libretro','-j4','platform=osx' if platform.system()=='Darwin' else 'platform=unix','DYNAREC=0','HAVE_CHD=1','HAVE_PHYSICAL_CDROM=0','USE_ASYNC_CDROM=0','NDRC_THREAD=0','GIT_VERSION='],cwd=directory/item['directory'],check=True)
filename='pcsx_rearmed_libretro.'+('dylib' if platform.system()=='Darwin' else 'so')
shutil.copy2(directory/item['directory']/filename,directory/filename);print(directory/filename)
