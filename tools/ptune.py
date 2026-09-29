"""Live-tune the Prey Eyes table at 0xFD000 (same usage as tune.py)."""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
from drive import p, wait_vbl
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'prey.c')).read()
body = src[src.index('typedef struct {'):src.index('} Prey;')]
FIELDS = []
for line in body.splitlines()[1:]:
    line = line.split('/*')[0].strip().rstrip(';')
    if not line: continue
    typ, names = line.split(None, 1)
    FIELDS += [n.strip() for n in names.split(',')]
BASE = 0xFD000
def get(): return {f: (lambda v: v - (1 << 32) if v >= 1 << 31 else v)(p.r32(BASE + 4 * i)) for i, f in enumerate(FIELDS)}
if __name__ == '__main__':
    snap = None
    for a in sys.argv[1:]:
        k, v = a.split('=')
        if k == 'snap': snap = v; continue
        p.w32(BASE + 4 * FIELDS.index(k), int(v, 0) & 0xffffffff)
    print(get())
    if snap: wait_vbl(8); print(drive.snap(4, snap))
