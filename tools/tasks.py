"""List scheduler tasks (fn pointers) in an extracted savestate dir."""
import struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fns
HEADS = {'active9b1c': 0x3ba80c, 'hi9b28': 0x3ba818, 'wait9b10': 0x3ba800}

def tasks(d):
    r = open(os.path.join(d, 'eeMemory.bin'), 'rb').read()
    u = lambda a: struct.unpack_from('<I', r, a & 0x1ffffff)[0]
    out = []
    for name, h in HEADS.items():
        t = u(h); n = 0
        while t and n < 200:
            fn = u(t + 0x30); f = fns.containing(fn) if 0x100000 <= fn < 0x316000 else None
            out.append((name, t, fn, u(t + 0x1c), u(t + 0x20), f[2] if f else '?'))
            t = u(t + 0x3c); n += 1
    return out

if __name__ == '__main__':
    for d in sys.argv[1:]:
        print('==', d)
        for x in tasks(d): print('  %-10s node %08x fn %08x flags %08x pri %08x %s' % x)
