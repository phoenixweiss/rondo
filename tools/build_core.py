"""Build the pinned NES core separately; never read or modify a user's ROMs."""
from pathlib import Path
import argparse, hashlib, io, json, platform, shutil, subprocess, tarfile, urllib.request
ROOT=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser();p.add_argument('--directory',type=Path,default=ROOT/'build/cores');args=p.parse_args()
meta=json.loads((ROOT/'docs/dependencies/core-source.json').read_text());rev=meta['revision'];directory=args.directory.resolve();directory.mkdir(parents=True,exist_ok=True)
source=directory/('libretro-fceumm-'+rev)
if not source.exists():
    data=urllib.request.urlopen('https://codeload.github.com/libretro/libretro-fceumm/tar.gz/'+rev,timeout=60).read()
    if hashlib.sha256(data).hexdigest()!=meta['archive_sha256']:raise RuntimeError('Source checksum mismatch')
    (directory/('fceumm-'+rev+'.tar.gz')).write_bytes(data)
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        for item in archive.getmembers():
            relative=Path(item.name)
            if relative.is_absolute() or '..' in relative.parts or item.issym() or item.islnk():raise RuntimeError('Unsafe source archive')
        archive.extractall(directory)
flags=['platform=osx' if platform.system()=='Darwin' else 'platform=unix','HAVE_HDPACK=0','HAVE_NTSC=0']
subprocess.run(['make','-j4',*flags],cwd=source,check=True)
name='fceumm_libretro.dylib' if platform.system()=='Darwin' else 'fceumm_libretro.so'
output=directory/name
shutil.copy2(source/name,output)
print(output)
