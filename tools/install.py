r"""Install the built pnach + per-game EE overclock into a PCSX2 user folder.

Usage: python tools/install.py [PCSX2_USER_DIR]   (default <local path>)
The per-game ini only gets EmuCore/Speedhacks EECycleRate = 3 set; other keys are kept.
Uninstall: delete patches\D7273511.pnach and remove EECycleRate from
gamesettings\SLUS-20974_D7273511.ini.
"""
import os, sys, shutil, configparser

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
user = sys.argv[1] if len(sys.argv) > 1 else r'<local path>'
if not os.path.isdir(os.path.join(user, 'patches')) or not os.path.isdir(os.path.join(user, 'inis')):
    raise SystemExit('not a PCSX2 user folder: ' + user)
shutil.copy(os.path.join(ROOT, 'build', 'D7273511.pnach'), os.path.join(user, 'patches', 'D7273511.pnach'))
gs = os.path.join(user, 'gamesettings'); os.makedirs(gs, exist_ok=True)
ini = os.path.join(gs, 'SLUS-20974_D7273511.ini')
cp = configparser.ConfigParser(); cp.optionxform = str
if os.path.exists(ini): cp.read(ini)
if not cp.has_section('EmuCore/Speedhacks'): cp.add_section('EmuCore/Speedhacks')
cp.set('EmuCore/Speedhacks', 'EECycleRate', '3')
with open(ini, 'w') as f: cp.write(f, space_around_delimiters=True)
print('installed pnach + EE overclock into', user)
