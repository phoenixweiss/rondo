"""Original MIT 68000/VDP/controller ABI fixture; no commercial ROM/BIOS data."""
from pathlib import Path
import struct


def create(destination):
    rom=bytearray(65536);code=bytearray();labels={};fixes=[]
    def word(value):code.extend(struct.pack('>H',value))
    def long(value):code.extend(struct.pack('>I',value))
    def write(size,address,value):
        word({'b':0x13fc,'w':0x33fc,'l':0x23fc}[size]);long(value) if size=='l' else word(value);long(address)
    def label(name):labels[name]=0x200+len(code)
    def branch(op,name):word(op<<8);fixes.append((len(code),name));word(0)
    def read(address,reg=0):word(0x1039+(reg<<9));long(address)
    def mask(value):word(0x0200);word(value)
    def compare(value):word(0x0c00);word(value)
    def color(value):write('l',0xc00004,0xc0000000);write('w',0xc00000,value)
    # Disable interrupts, unlock TMSS, reset VDP registers and clear VRAM.
    word(0x46fc);word(0x2700)
    write('l',0xa14000,0x53454741)
    regs=[0x04,0x04,0x30,0x3c,0x07,0x6c,0,0,0,0,0xff,0,0x81,0x37,0,2,0,0,0,0,0,0,0,0]
    for i,value in enumerate(regs):write('w',0xc00004,0x8000+(i<<8)+value)
    write('l',0xc00004,0x40000000)
    word(0x303c);word(32767) # move.w #32767,d0
    label('clear');write('w',0xc00000,0);word(0x51c8);fixes.append((len(code),'clear'));word(0) # dbra d0
    color(0x0e00);write('w',0xc00004,0x8144)
    write('b',0xa10009,0x40);write('b',0xa10003,0x40)
    # Poll once per VBlank, allowing the six-button handshake to time out.
    label('wait_end');read(0xc00005);mask(8);branch(0x66,'wait_end')
    label('wait_begin');read(0xc00005);mask(8);branch(0x67,'wait_begin')
    word(0x5279);long(0xff0000)  # Frame counter in work RAM for state checks.
    write('b',0xa10003,0x40);read(0xa10003);mask(0x3f);compare(0x3f);branch(0x66,'pressed')
    write('b',0xa10003,0);read(0xa10003);mask(0x30);compare(0x30);branch(0x66,'pressed')
    # Two more rising TH edges expose X/Y/Z/Mode on a six-button pad.
    for value in [0x40,0,0x40,0,0x40]:write('b',0xa10003,value)
    read(0xa10003);mask(0x0f);compare(0x0f);branch(0x66,'pressed')
    color(0x0e00);branch(0x60,'wait_end')
    label('pressed');color(0x000e);branch(0x60,'wait_end')
    for at,name in fixes:struct.pack_into('>h',code,at,labels[name]-(0x200+at))
    struct.pack_into('>II',rom,0,0x00ffff00,0x200)
    for at in range(8,0x100,4):struct.pack_into('>I',rom,at,0x200)
    rom[0x100:0x110]=b'SEGA MEGA DRIVE '
    rom[0x120:0x150]=b'RONDO MD ABI TEST'.ljust(48)
    rom[0x150:0x180]=b'RONDO MD ABI TEST'.ljust(48)
    rom[0x180:0x18e]=b'GM 00000000-00'
    rom[0x190:0x1a0]=b'J6'.ljust(16)
    struct.pack_into('>IIII',rom,0x1a0,0,len(rom)-1,0xff0000,0xffffff)
    rom[0x1b0:0x1b4]=b'RA\xf8\x20';struct.pack_into('>II',rom,0x1b4,0x200001,0x203fff)
    rom[0x1f0:0x200]=b'USA'.ljust(16)
    rom[0x200:0x200+len(code)]=code
    struct.pack_into('>H',rom,0x18e,sum(struct.unpack('>'+str((len(rom)-0x200)//2)+'H',rom[0x200:]))&0xffff)
    destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(rom)


if __name__=='__main__':
    import sys
    create(sys.argv[1] if len(sys.argv)>1 else Path(__file__).resolve().parent.parent/'runtime/test-fixtures/md/MD/technical-test.gen')
