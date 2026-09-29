r"""Install Good Karma into a PCSX2 user folder.

Usage: python tools/install.py [PCSX2_USER_DIR] [--turbo-only | --set-only]   (default <local path>)
Copies patches\SLUS-20974_D7273511.pnach, removes this project's older patches\D7273511.pnach,
enables the chosen patches in gamesettings\SLUS-20974_D7273511.ini ([Patches] Enable = ...)
and sets EmuCore/Speedhacks EECycleRate = 3 there (needed for full 6x turbo). Other keys are kept.
"""
import os, sys, shutil, re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build

args = [a for a in sys.argv[1:] if not a.startswith('--')]
flags = [a for a in sys.argv[1:] if a.startswith('--')]
user = args[0] if args else r'<local path>'
if not os.path.isdir(os.path.join(user, 'patches')) or not os.path.isdir(os.path.join(user, 'inis')):
    raise SystemExit('not a PCSX2 user folder: ' + user)
want = [build.PATCH_TURBO, build.PATCH_SET]
if '--turbo-only' in flags: want = [build.PATCH_TURBO]
if '--set-only' in flags: want = [build.PATCH_SET]

shutil.copy(os.path.join(ROOT, 'build', build.PNACH), os.path.join(user, 'patches', build.PNACH))
old = os.path.join(user, 'patches', 'D7273511.pnach')
if os.path.exists(old) and 'DDS1 mods v' in open(old, errors='replace').read():
    os.remove(old)                                   # only our own pre-rename file

ini = os.path.join(user, 'gamesettings', 'SLUS-20974_D7273511.ini')
os.makedirs(os.path.dirname(ini), exist_ok=True)
text = open(ini).read() if os.path.exists(ini) else ''
# parse into ordered sections of raw lines (duplicate keys allowed, as PCSX2 uses them)
secs, cur = {}, None
order = []
for line in text.splitlines():
    m = re.match(r'\s*\[(.+)\]\s*$', line)
    if m:
        cur = m.group(1); secs.setdefault(cur, []); order.append(cur) if cur not in order else None
    elif cur is not None and line.strip():
        secs[cur].append(line.strip())
def ensure(sec):
    if sec not in secs: secs[sec] = []; order.append(sec)
ensure('Patches')
secs['Patches'] = [l for l in secs['Patches'] if not re.match(r'Enable\s*=\s*Good Karma', l)] + ['Enable = ' + w for w in want]
ensure('EmuCore/Speedhacks')
secs['EmuCore/Speedhacks'] = [l for l in secs['EmuCore/Speedhacks'] if not l.startswith('EECycleRate')] + ['EECycleRate = 3']
with open(ini, 'w') as f:
    for s in order:
        f.write('[%s]\n' % s + ''.join(l + '\n' for l in secs[s]) + '\n')
print('installed Good Karma (%s) into %s' % (', '.join(want), user))
