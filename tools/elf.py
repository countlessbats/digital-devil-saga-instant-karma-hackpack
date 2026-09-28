"""Minimal loader + disassembler helpers for SLUS_209.74 (DDS1 USA, CRC D7273511)."""
import struct, os, capstone
from capstone import mips as M

ELF = os.path.join(os.path.dirname(__file__), '..', 'extracted', 'SLUS_209.74')
DATA = open(ELF, 'rb').read()
# (vaddr, file offset, filesz)
SEGS = [(0x100000, 0x1000, 0x224378), (0x324380, 0x225380, 0x992bd)]
TEXT = (0x100000, 0x315b94)

def off(va):
    for a, o, n in SEGS:
        if a <= va < a + n:
            return o + va - a
    return None

def u32(va):
    o = off(va); return struct.unpack_from('<I', DATA, o)[0] if o is not None else None

def cstr(va, n=80):
    o = off(va)
    if o is None: return None
    e = DATA.find(b'\0', o, o + n)
    return DATA[o:e if e >= 0 else o + n]

md = capstone.Cs(capstone.CS_ARCH_MIPS, capstone.CS_MODE_MIPS64 | capstone.CS_MODE_LITTLE_ENDIAN)
md.detail = False

def dis(va, count):
    out = []
    for i in range(count):
        a = va + 4 * i; w = u32(a)
        ins = list(md.disasm(struct.pack('<I', w), a))
        if ins:
            out.append((a, w, ins[0].mnemonic, ins[0].op_str))
        else:
            out.append((a, w, '.word', '0x%08x' % w))
    return out

def pdis(va, count):
    for a, w, m, o in dis(va, count):
        print('%08x: %08x  %-8s %s' % (a, w, m, o))

def all_words():
    a, e = TEXT
    o = off(a)
    return a, struct.unpack_from('<%dI' % ((e - a) // 4), DATA, o)

def jal_callers(target):
    a, ws = all_words()
    enc = 0x0c000000 | ((target >> 2) & 0x3ffffff)
    return [a + 4 * i for i, w in enumerate(ws) if w == enc]

def func_start(va):
    """Walk back to the nearest 'addiu sp,sp,-X' (27bdXXXX negative)."""
    a = va
    while a > TEXT[0]:
        w = u32(a)
        if (w & 0xffff0000) == 0x27bd0000 and (w & 0x8000):
            return a
        a -= 4
    return None
