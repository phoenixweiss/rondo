"""Original MIT PS-X EXE + ISO9660/CD fixture; no Sony firmware or game data."""
from pathlib import Path
import struct,subprocess,tempfile

def create(destination,colour=255):
 def both16(n):return struct.pack('<H',n)+struct.pack('>H',n)
 def both32(n):return struct.pack('<I',n)+struct.pack('>I',n)
 code=[]
 def ins(n):code.append(n)
 def li(r,v):ins(0x3c000000|(r<<16)|(v>>16));ins(0x34000000|(r<<21)|(r<<16)|(v&65535))
 def sw(r,off=0):ins(0xad000000|(r<<16)|off)
 li(8,0x1f801810)
 for v in [0,0x03000000,0x05000000,0x060c6026,0x07042018,0x08000001]:li(9,v);sw(9,4)
 li(9,0x02000000|colour);sw(9);sw(0);li(9,(240<<16)|320);sw(9)
 li(8,0x80020000);li(9,colour);sw(9,4);loop=len(code)
 ins(0x8d090000);ins(0);ins(0x25290001);sw(9);ins(0x08000000|((0x80010000+loop*4)&0x0fffffff)>>2);ins(0)
 exe=bytearray(4096);exe[:8]=b'PS-X EXE';struct.pack_into('<IIII',exe,16,0x80010000,0,0x80010000,2048);struct.pack_into('<I',exe,48,0x801fff00)
 exe[2048:2048+len(code)*4]=struct.pack('<'+'I'*len(code),*code)
 image=bytearray(64*2048)
 def record(name,extent,size,directory=False):
  name=name if isinstance(name,bytes) else name.encode();length=33+len(name)+(len(name)%2==0);b=bytearray(length);b[0]=length;b[2:10]=both32(extent);b[10:18]=both32(size);b[18:25]=bytes([126,1,1,0,0,0,0]);b[25]=2 if directory else 0;b[28:32]=both16(1);b[32]=len(name);b[33:33+len(name)]=name;return b
 pvd=bytearray(2048);pvd[:7]=b'\x01CD001\x01';pvd[8:40]=b'PLAYSTATION'.ljust(32);pvd[40:72]=b'RONDO_TECHNICAL'.ljust(32);pvd[80:88]=both32(64);pvd[120:124]=both16(1);pvd[124:128]=both16(1);pvd[128:132]=both16(2048);pvd[132:140]=both32(10);struct.pack_into('<I',pvd,140,18);struct.pack_into('>I',pvd,148,19);pvd[156:190]=record(b'\0',20,2048,True);pvd[881]=1
 image[16*2048:17*2048]=pvd;image[17*2048:17*2048+7]=b'\xffCD001\x01'
 image[18*2048:18*2048+10]=b'\x01\x00'+struct.pack('<I',20)+struct.pack('<H',1)+b'\x00\x00';image[19*2048:19*2048+10]=b'\x01\x00'+struct.pack('>I',20)+struct.pack('>H',1)+b'\x00\x00'
 cnf=b'BOOT = cdrom:\\TECH.EXE;1\r\nTCB = 4\r\nEVENT = 10\r\nSTACK = 801fff00\r\n'
 directory=b''.join([record(b'\0',20,2048,True),record(b'\1',20,2048,True),record('SYSTEM.CNF;1',21,len(cnf)),record('TECH.EXE;1',22,len(exe))]);image[20*2048:20*2048+len(directory)]=directory;image[21*2048:21*2048+len(cnf)]=cnf;image[22*2048:22*2048+len(exe)]=exe
 destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='rondo-psx-disc-',dir=destination.parent) as work:
  work=Path(work);raw=bytearray(64*2352)
  for n in range(64):
   offset=n*2352;raw[offset:offset+12]=b'\x00'+b'\xff'*10+b'\x00';lba=n+150;msf=[lba//4500,(lba//75)%60,lba%75];raw[offset+12:offset+16]=bytes([((v//10)<<4)|(v%10) for v in msf]+[2]);raw[offset+16:offset+24]=bytes([0,0,8,0]*2);raw[offset+24:offset+2072]=image[n*2048:(n+1)*2048]
  (work/'technical.bin').write_bytes(raw);(work/'technical.cue').write_text('FILE "technical.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n')
  subprocess.run(['chdman','createcd','-i',str(work/'technical.cue'),'-o',str(destination),'-c','cdzl'],check=True,stdout=subprocess.DEVNULL)
if __name__=='__main__':
 import sys
 create(sys.argv[1],int(sys.argv[2]) if len(sys.argv)>2 else 255)
