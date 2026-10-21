"""Generate an original MIT GBA technical cartridge for isolated checks only."""
from pathlib import Path
import struct

def create(root):
    rom=bytearray(32768)
    rom[0:4]=struct.pack('<I',0xea00002e)  # Entry at 0xc0; no Nintendo logo.
    rom[0xa0:0xac]=b'RONDO-GBA   '
    rom[0xac:0xb0]=b'RND0';rom[0xb2]=0x96
    rom[0xbd]=(-sum(rom[0xa0:0xbd])-0x19)&255
    words=[];literals=[]
    def emit(value):words.append(value)
    def load(register,value):
        literals.append((len(words),register,value));emit(0)
    load(0,0x04000000);load(1,0x403);emit(0xe1c010b0)  # Mode 3, BG2.
    load(2,0x06000000);emit(0xe3a0301f);load(4,38400)
    start=len(words);emit(0xe0c230b2);emit(0xe2544001)
    emit(0x1a000000|((start-len(words)-2)&0xffffff))
    # Audible square wave, no copyrighted music.
    for address,value in [(0x04000084,0x80),(0x04000080,0x1177),(0x04000062,0xf080),(0x04000064,0x8700)]:
        load(0,address);load(1,value);emit(0xe1c010b0)
    load(6,0x04000130);load(7,0x06000000)
    start=len(words);emit(0xe1d650b0);emit(0xe1c750b0)
    emit(0xea000000|((start-len(words)-2)&0xffffff))
    for index,register,value in literals:
        offset=(len(words)-index-2)*4;assert 0<=offset<4096
        words[index]=0xe59f0000|(register<<12)|offset;emit(value)
    rom[0xc0:0xc0+len(words)*4]=struct.pack('<'+'I'*len(words),*words)
    rom[0x300:0x309]=b'SRAM_V113'
    folder=Path(root)/'GBA';folder.mkdir(parents=True,exist_ok=True)
    (folder/'technical-test.gba').write_bytes(rom)

if __name__=='__main__':create(Path(__file__).resolve().parent.parent/'runtime/test-fixtures/gba')
