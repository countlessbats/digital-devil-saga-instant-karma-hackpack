"""Drive the test instance: press buttons via injection, save states, extract them."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine import Pine
import sstate

VBL = 0x3bd2d8
BTN = dict(SQ=0x80, X=0x40, TRI=0x10, O=0x20, LEFT=0x8000, RIGHT=0x2000, UP=0x1000, DOWN=0x4000,
           L1=0x4, L2=0x1, R1=0x8, R2=0x2, START=0x800, SELECT=0x100, L3=0x200, R3=0x400)
SS = r'<local path>).%02d.p2s'
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
p = Pine()

def wait_vbl(n, timeout=20):
    v = p.r32(VBL); t = time.time()
    while p.r32(VBL) - v < n:
        if time.time() - t > timeout:
            raise SystemExit('emulator stalled (status %d)' % p.status())
        time.sleep(0.05)

def press(name, hold=6, after=30):
    m = 0
    for b in name.split('+'): m |= BTN[b]
    p.w16(0xF000C, m); wait_vbl(hold); p.w16(0xF000C, 0); wait_vbl(after)

def snap(slot, tag):
    path = SS % slot
    if os.path.exists(path): os.remove(path)
    p.save_state(slot)
    for _ in range(100):
        time.sleep(0.2)
        if os.path.exists(path):
            time.sleep(1.0); break
    d = os.path.join(ROOT, 'work', 'snap_' + tag)
    sstate.extract(path, d); return d

if __name__ == '__main__':
    # usage: drive.py ACTION... where ACTION is BUTTON[:hold[:after]] or snap:slot:tag or wait:vblanks
    for a in sys.argv[1:]:
        f = a.split(':')
        if f[0] == 'snap': print('snap ->', snap(int(f[1]), f[2]))
        elif f[0] == 'wait': wait_vbl(int(f[1]))
        else: press(f[0], *(int(x) for x in f[1:])); print('pressed', f[0])
