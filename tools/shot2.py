"""Capture the test PCSX2 window even when other windows cover it (PrintWindow, full content)."""
import ctypes, ctypes.wintypes as W, sys, os
from PIL import Image
u = ctypes.windll.user32; k = ctypes.windll.kernel32; g = ctypes.windll.gdi32
ctypes.windll.shcore.SetProcessDpiAwareness(2)
target = r'<local path>'
found = []
@ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
def cb(h, _):
    if not u.IsWindowVisible(h): return True
    pid = W.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(pid))
    hp = k.OpenProcess(0x1000, False, pid.value)
    buf = ctypes.create_unicode_buffer(260); n = W.DWORD(260)
    if hp and k.QueryFullProcessImageNameW(hp, 0, buf, ctypes.byref(n)) and buf.value.lower().replace(chr(92), '/') == target:
        r = W.RECT(); u.GetWindowRect(h, ctypes.byref(r)); found.append((h, (r.left, r.top, r.right, r.bottom)))
    if hp: k.CloseHandle(hp)
    return True
u.EnumWindows(cb, 0)
if not found: sys.exit('no test window')
h, r = max(found, key=lambda f: (f[1][2]-f[1][0])*(f[1][3]-f[1][1]))
w, hgt = r[2]-r[0], r[3]-r[1]
hdc = u.GetWindowDC(h); mdc = g.CreateCompatibleDC(hdc); bmp = g.CreateCompatibleBitmap(hdc, w, hgt); g.SelectObject(mdc, bmp)
ok = u.PrintWindow(h, mdc, 2)
class BIH(ctypes.Structure):
    _fields_ = [('a', W.DWORD), ('w', W.LONG), ('h', W.LONG), ('p', W.WORD), ('b', W.WORD), ('c', W.DWORD), ('s', W.DWORD), ('x', W.LONG), ('y', W.LONG), ('u', W.DWORD), ('i', W.DWORD)]
bi = BIH(ctypes.sizeof(BIH), w, -hgt, 1, 32, 0, 0, 0, 0, 0, 0)
data = ctypes.create_string_buffer(w * hgt * 4)
g.GetDIBits(mdc, bmp, 0, hgt, data, ctypes.byref(bi), 0)
img = Image.frombuffer('RGBA', (w, hgt), data, 'raw', 'BGRA', 0, 1).convert('RGB')
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'work', 'shot.png')
img.save(out); print('saved', out, 'printwindow', ok)
g.DeleteObject(bmp); g.DeleteDC(mdc); u.ReleaseDC(h, hdc)
