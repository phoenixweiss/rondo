"""Original MIT technical GB/GBC fixtures for CI, never a user-library demo."""
from pathlib import Path

def create(root):
    root=Path(root)
    for color,extension in [(False,'gb'),(True,'gbc')]:
        rom=bytearray(32768)
        rom[0x100:0x104]=bytes([0xc3,0x50,0x01,0])
        title=b'RONDO-CGB-TEST' if color else b'RONDO-DMG-TEST'
        rom[0x134:0x134+len(title)]=title
        rom[0x143]=0x80 if color else 0
        rom[0x147:0x14b]=bytes([0x10,0,3,1]) # MBC3 + RAM + battery + RTC, 32 KiB ROM/RAM
        program=bytes.fromhex('f3 31 fe ff af e0 40 3e 0a ea 00 00 21 00 80 06 10 3e aa 22 05 20 fa '
                              '21 00 98 01 00 04 af 22 0b 78 b1 20 f9 '
                              '3e e4 e0 47 3e 80 e0 68 21 00 02 06 08 2a e0 69 05 20 fa '
                              '3e 80 e0 26 3e 77 e0 24 3e 11 e0 25 3e 80 e0 11 3e f3 e0 12 3e 00 e0 13 3e 87 e0 14 '
                              '3e 91 e0 40 21 00 c0 34 18 fd')
        rom[0x150:0x150+len(program)]=program
        rom[0x200:0x208]=bytes.fromhex('ff7f e003 1f00 0000')
        checksum=0
        for value in rom[0x134:0x14d]:checksum=(checksum-value-1)&255
        rom[0x14d]=checksum
        rom[0x14e:0x150]=(sum(rom)&65535).to_bytes(2,'big')
        folder=root/('GBC' if color else 'GB');folder.mkdir(parents=True,exist_ok=True)
        (folder/('technical-test.'+extension)).write_bytes(rom)

if __name__=='__main__':
    create(Path(__file__).resolve().parent.parent/'runtime/test-fixtures/gb')
