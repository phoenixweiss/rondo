"""Create original NROM/SRAM test content. No commercial ROMs or user data."""
from pathlib import Path
import argparse, struct
from PIL import Image, ImageDraw

def rom():
    code=bytearray(); labels={}; patches=[]
    def emit(*values):code.extend(values)
    def label(name):labels[name]=len(code)
    def branch(op,name):emit(op,0);patches.append((len(code)-1,name,'relative'))
    label('reset');emit(0x78,0xd8,0xa2,0xff,0x9a,0xe8,0x8e,0x00,0x20,0x8e,0x01,0x20,0x8e,0x10,0x40)
    label('wait1');emit(0x2c,0x02,0x20);branch(0x10,'wait1')
    label('wait2');emit(0x2c,0x02,0x20);branch(0x10,'wait2')
    emit(0xa9,0x3f,0x8d,0x06,0x20,0xa9,0x00,0x8d,0x06,0x20,0xa2,0x00)
    label('palette');emit(0xbd,0,0);patches.append((len(code)-2,'colors','absolute'));emit(0x8d,0x07,0x20,0xe8,0xe0,0x20);branch(0xd0,'palette')
    emit(0xa9,0x20,0x8d,0x06,0x20,0xa9,0x00,0x8d,0x06,0x20,0xa0,4,0xa2,0,0xa9,1)
    label('tiles');emit(0x8d,0x07,0x20,0xe8);branch(0xd0,'tiles');emit(0x88);branch(0xd0,'tiles')
    emit(0xa9,0,0x8d,0x05,0x20,0x8d,0x05,0x20,0xa9,0x80,0x8d,0,0x20,0xa9,0x0a,0x8d,1,0x20)
    emit(0xa9,0x40,0x8d,0x17,0x40,0xa9,1,0x8d,0x15,0x40,0xa9,0xbf,0x8d,0x00,0x40,0xa9,8,0x8d,1,0x40,0xa9,0xfd,0x8d,2,0x40,0xa9,8,0x8d,3,0x40)
    label('loop');emit(0x4c,0,0);patches.append((len(code)-2,'loop','absolute'))
    label('nmi');emit(0x48,0xee,0x00,0x60,0x2c,2,0x20,0xa9,0x3f,0x8d,6,0x20,0xa9,0,0x8d,6,0x20,0xad,0,0x60,0x29,0x3f,0x8d,7,0x20,0xa9,0x20,0x8d,6,0x20,0xa9,0,0x8d,6,0x20,0x8d,5,0x20,0x8d,5,0x20,0x68,0x40)
    label('irq');emit(0x40)
    label('colors');emit(*([0x0f,0x16,0x27,0x30]*8))
    for pos,name,kind in patches:
        if kind=='relative':code[pos]=(labels[name]-(pos+1))&255
        else:code[pos:pos+2]=struct.pack('<H',0x8000+labels[name])
    prg=bytearray([0xea]*16384);prg[:len(code)]=code
    prg[-6:]=struct.pack('<HHH',0x8000+labels['nmi'],0x8000+labels['reset'],0x8000+labels['irq'])
    chr=bytearray(8192);chr[16:32]=bytes([0x55,0xaa]*4+[0,0,0xff,0xff]*2)
    return b'NES\x1a'+bytes([1,1,2,0])+bytes(8)+prg+chr

def create(root):
    data=rom()
    for n in range(18):
        folder=root/'card-a'/'NES'/('Rondo Test Cartridge' if n==0 else f'Test Cartridge {n:02d}')
        folder.mkdir(parents=True,exist_ok=True);(folder/'test.nes').write_bytes(data)
        for mode,size in [('cover',(180,240)),('title',(256,240)),('screenshot',(256,240))]:
            im=Image.new('RGB',size,'#1C1B18');d=ImageDraw.Draw(im);d.rectangle((12,12,size[0]-12,size[1]-12),outline='#CB5945',width=4)
            d.text((24,50),'RONDO',fill='#F4EBD8');d.text((24,90),f'{mode.upper()} {n+1}',fill='#BBAF9F');im.save(folder/(mode+'.png'))
    # Duplicate game name/ROM on another card with independent artwork.
    folder=root/'card-b'/'NES'/'Rondo Test Cartridge';folder.mkdir(parents=True,exist_ok=True);(folder/'test.nes').write_bytes(data)
    Image.new('RGB',(200,200),'#CB5945').save(folder/'cover.png')
    return data
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('directory',type=Path);args=p.parse_args();create(args.directory)
