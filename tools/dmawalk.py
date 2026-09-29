"""Walk a PS2 DMA source chain in an eeMemory dump and summarise it (tag, qwc, VIF codes)."""
import struct, sys
IDS = {0: 'refe', 1: 'cnt', 2: 'next', 3: 'ref', 4: 'refs', 5: 'call', 6: 'ret', 7: 'end'}
VIFCMD = {0: 'NOP', 1: 'STCYCL', 2: 'OFFSET', 3: 'BASE', 4: 'ITOP', 5: 'STMOD', 6: 'MSKPATH3', 7: 'MARK',
          0x10: 'FLUSHE', 0x11: 'FLUSH', 0x13: 'FLUSHA', 0x14: 'MSCAL', 0x15: 'MSCALF', 0x17: 'MSCNT',
          0x20: 'STMASK', 0x30: 'STROW', 0x31: 'STCOL', 0x4a: 'MPG', 0x50: 'DIRECT', 0x51: 'DIRECTHL'}

def vif(w):
    c = (w >> 24) & 0x7f
    if c >= 0x60: return 'UNPACK(%x,n%d,@%x)' % (c & 0xf, (w >> 16) & 0xff, w & 0x3ff)
    return '%s(%x)' % (VIFCMD.get(c, '?%x' % c), w & 0xffff)

def walk(m, start, limit=4000):
    r = lambda a: struct.unpack_from('<Q', m, a)[0]
    a, stack, out = start, [], []
    for _ in range(limit):
        t = r(a); hi = r(a + 8)
        qwc, tid, addr = t & 0xffff, (t >> 28) & 7, (t >> 32) & 0x7ffffff0
        v0, v1 = hi & 0xffffffff, hi >> 32
        out.append((a, IDS[tid], qwc, addr, vif(v0), vif(v1)))
        if tid == 1: a = a + 16 + qwc * 16
        elif tid == 2: a = addr
        elif tid in (3, 4): a = a + 16
        elif tid == 5: stack.append(a + 16 + qwc * 16); a = addr
        elif tid == 6:
            if not stack: break
            a = stack.pop()
        else: break
    return out

if __name__ == '__main__':
    m = open(sys.argv[1], 'rb').read()
    for row in walk(m, int(sys.argv[2], 16)):
        print('%08x %-5s qwc=%-4d addr=%08x %s %s' % row)
