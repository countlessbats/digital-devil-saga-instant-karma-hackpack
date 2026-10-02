"""Screenshot the test PCSX2 window (by exe path) to work/shot.png."""
import ctypes, ctypes.wintypes as W, sys, os
from PIL import ImageGrab
u = ctypes.windll.user32; k = ctypes.windll.kernel32
ctypes.windll.shcore.SetProcessDpiAwareness(2)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import localpaths
target = os.path.join(localpaths.get('test_pcsx2'), 'pcsx2-qt.exe').lower().replace(chr(92), '/')
found = []
@ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
def cb(h, _):
    if not u.IsWindowVisible(h): return True
    pid = W.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(pid))
    hp = k.OpenProcess(0x1000, False, pid.value)
    buf = ctypes.create_unicode_buffer(260); n = W.DWORD(260)
    if hp and k.QueryFullProcessImageNameW(hp, 0, buf, ctypes.byref(n)) and buf.value.lower().replace(chr(92), '/') == target:
        t = ctypes.create_unicode_buffer(256); u.GetWindowTextW(h, t, 256)
        r = W.RECT(); u.GetWindowRect(h, ctypes.byref(r))
        found.append((h, t.value, (r.left, r.top, r.right, r.bottom)))
    if hp: k.CloseHandle(hp)
    return True
u.EnumWindows(cb, 0)
for h, t, r in found: print(hex(h), repr(t), r)
if found:
    h, t, r = max(found, key=lambda f: (f[2][2]-f[2][0])*(f[2][3]-f[2][1]))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'work', 'shot.png')
    ImageGrab.grab(r, all_screens=True).save(out); print('saved', out)
