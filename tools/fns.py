"""Function map from Ghidra export (work/ghidra/functions.txt) + decomp lookup."""
import bisect, os, re, sys
W = os.path.join(os.path.dirname(__file__), '..', 'work', 'ghidra')
_f = []
for line in open(os.path.join(W, 'functions.txt')):
    a, n, name = line.split(None, 2)
    _f.append((int(a, 16), int(n), name.strip()))
_f.sort()
_starts = [x[0] for x in _f]

def containing(va):
    i = bisect.bisect_right(_starts, va) - 1
    return _f[i] if i >= 0 else None

_dec = None
def decomp(va):
    global _dec
    if _dec is None:
        _dec = {}
        txt = open(os.path.join(W, 'decomp.c'), encoding='utf-8', errors='replace').read()
        for m in re.finditer(r'^// ==== \S+ @ ([0-9a-f]+)\n(.*?)(?=^// ==== |\Z)', txt, re.S | re.M):
            _dec[int(m.group(1), 16)] = m.group(2)
    return _dec.get(va)

def callers_text(name):
    """Functions whose decompiled body mentions name."""
    decomp(0)
    return [hex(a) for a, body in _dec.items() if name in body]

if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'in':
        for a in sys.argv[2:]: f = containing(int(a, 16)); print(a, hex(f[0]), f[2])
    elif cmd == 'dec':
        for a in sys.argv[2:]: print(decomp(int(a, 16)))
    elif cmd == 'refs':
        print(callers_text(sys.argv[2]))
