r"""NumLock "move mode" for tuning the SET layout live in your PCSX2.

While PCSX2 is the foreground window and NumLock is ON, numpad 4/6/8/2 move the LEARNED
HP/MP unit label one framebuffer pixel (16 units) left/right/up/down. Values are written live
over PINE and saved to the file named by nudge_out (see localpaths.py).
Requires PCSX2 Settings > Advanced > PINE enabled (slot 28011).
Usage: nudge.py [slot]
"""
import ctypes, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine import Pine
import tune

SLOT = int(sys.argv[1]) if len(sys.argv) > 1 else 28011
import localpaths
OUT = localpaths.get('nudge_out')
BASE = 0xFF000
import re
MAGIC = int(re.search(r'#define LAY_MAGIC (0x[0-9a-fA-F]+)', open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'set_layout.c')).read()).group(1), 16)
u = ctypes.windll.user32; k = ctypes.windll.kernel32
VK = {0x64: (-16, 0), 0x66: (16, 0), 0x68: (0, -16), 0x62: (0, 16)}   # numpad 4 6 8 2
F = lambda n: BASE + 4 * tune.FIELDS.index(n)
def s32(v): return v - (1 << 32) if v >= 1 << 31 else v

def pcsx2_focused():
    h = u.GetForegroundWindow(); pid = ctypes.c_ulong(); u.GetWindowThreadProcessId(h, ctypes.byref(pid))
    hp = k.OpenProcess(0x1000, False, pid.value)
    if not hp: return False
    buf = ctypes.create_unicode_buffer(260); n = ctypes.c_ulong(260)
    ok = k.QueryFullProcessImageNameW(hp, 0, buf, ctypes.byref(n)); k.CloseHandle(hp)
    return ok and buf.value.lower().endswith('pcsx2-qt.exe')

def connect():
    while True:
        try: return Pine(SLOT)
        except OSError:
            print('waiting for PCSX2 (PINE slot %d)...' % SLOT); time.sleep(2)

p = connect()
print('connected. In the SET screen: NumLock ON = move mode; numpad 4/6/8/2 move the HP/MP label.')
held = {}; active = False
while True:
    try:
        on = bool(u.GetKeyState(0x90) & 1) and pcsx2_focused() and p.r32(BASE) == MAGIC
        if on != active:
            active = on; p.w32(F('nudge_on'), 1 if on else 0)
            print('move mode', 'ON' if on else 'off')
        now = time.time()
        for vk, (dx, dy) in VK.items():
            down = bool(u.GetAsyncKeyState(vk) & 0x8000)
            if not (down and active): held.pop(vk, None); continue
            t0 = held.get(vk)
            if t0 is None or now - t0 > 0.35:
                if t0 is None: held[vk] = now
                else: held[vk] = now - 0.29        # repeat every ~60 ms after the first 350 ms
                x = s32(p.r32(F('unit_dx'))) + dx; y = s32(p.r32(F('unit_dy'))) + dy
                p.w32(F('unit_dx'), x & 0xffffffff); p.w32(F('unit_dy'), y & 0xffffffff)
                line = 'unit_dx=%d unit_dy=%d' % (x, y)
                print(line); open(OUT, 'w').write(line + '\n')
        time.sleep(0.015)
    except OSError:
        print('PCSX2 connection lost; reconnecting'); active = False; p = connect()
