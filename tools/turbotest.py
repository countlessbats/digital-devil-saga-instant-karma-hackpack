"""Measure logic frames per vblank on the test instance at each turbo override via PINE.
Speed-independent: 1x should read ~0.5 (game runs at 30 fps), 3x ~1.5, 6x ~3.0."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine import Pine

VBL = 0x3bd2d8   # pad-repeat timer, ticks per vblank
p = Pine()
def ratio(min_vbl=240, timeout=120):
    a, v = p.r32(0x3ba700), p.r32(VBL); t = time.time()
    while p.r32(VBL) - v < min_vbl and time.time() - t < timeout: time.sleep(0.5)
    dv = p.r32(VBL) - v
    return (p.r32(0x3ba700) - a) / max(dv, 1), dv
for n in [int(x) for x in (sys.argv[1:] or ['1', '3', '6', '1'])]:
    p.w8(0xF0001, n); time.sleep(0.5)
    r, dv = ratio()
    print('override %d: %.2f logic frames/vblank (x%.2f of normal) over %d vblanks' % (n, r, r / 0.5, dv))
p.w8(0xF0001, 0)
