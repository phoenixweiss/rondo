"""Generate an original MIT SNES ABI fixture; no borrowed ROM or BIOS data."""
from pathlib import Path
import struct


def create(destination):
    rom = bytearray([0xEA] * 32768)
    code = bytearray()
    labels = {}
    fixes = []

    def emit(*values): code.extend(values)
    def label(name): labels[name] = len(code)
    def branch(op, target):
        emit(op, 0)
        fixes.append((len(code)-1, target))
    def write(address, value):
        emit(0xA9, value, 0x8D, address & 255, address >> 8)
    def color(lo, hi):
        write(0x2121, 0)
        write(0x2122, lo)
        write(0x2122, hi)

    # Native 65C816, 8-bit accumulator / 16-bit index, interrupts disabled.
    emit(0x78, 0x18, 0xFB, 0xC2, 0x10, 0xA2, 0xFF, 0x01, 0x9A, 0xE2, 0x20)
    write(0x4200, 0)
    write(0x2100, 0x80)
    for address in [0x2105, 0x212C, 0x212D, 0x2130, 0x2131]: write(address, 0)
    color(0x1F, 0)
    write(0x2100, 0x0F)
    write(0x4200, 1)  # Automatic joypad reading, no NMI handler required.
    label('end')
    emit(0xAD, 0x12, 0x42, 0x29, 0x80)
    branch(0xD0, 'end')
    label('begin')
    emit(0xAD, 0x12, 0x42, 0x29, 0x80)
    branch(0xF0, 'begin')
    label('joy')
    emit(0xAD, 0x12, 0x42, 0x29, 1)
    branch(0xD0, 'joy')
    emit(0xAD, 0x18, 0x42, 0x0D, 0x19, 0x42)
    branch(0xF0, 'neutral')
    color(0xE0, 3)
    branch(0x80, 'again')
    label('neutral')
    color(0x1F, 0)
    label('again')
    emit(0x4C, labels['end'] & 255, 0x80 + (labels['end'] >> 8))
    for offset, target in fixes:
        displacement = labels[target] - offset - 1
        assert -128 <= displacement <= 127
        code[offset] = displacement & 255
    assert len(code) < 0xF0
    rom[:len(code)] = code
    rom[0xF0] = 0x40  # RTI for otherwise unused interrupt vectors.
    rom[0x7FC0:0x7FD5] = b'RONDO SNES ABI TEST   '
    rom[0x7FD5:0x7FDC] = bytes([0x20, 2, 5, 3, 1, 0, 0])
    rom[0x7FDC:0x7FE0] = bytes(4)
    for offset in range(0x7FE0, 0x8000, 2): struct.pack_into('<H', rom, offset, 0x80F0)
    struct.pack_into('<H', rom, 0x7FFC, 0x8000)
    checksum = (sum(rom) + 510) & 0xFFFF
    struct.pack_into('<HH', rom, 0x7FDC, checksum ^ 0xFFFF, checksum)
    assert sum(rom) & 0xFFFF == checksum
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(rom)


if __name__ == '__main__':
    import sys
    create(Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parent.parent/'runtime/test-fixtures/snes/SNES/technical-test.sfc')
