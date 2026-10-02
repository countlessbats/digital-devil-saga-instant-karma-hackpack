"""Build the distributable Instant Karma package: dist/InstantKarma-v<VERSION>.zip.

Contents (no game data):
  Install.bat                               launches the installer
  InstantKarma/InstantKarma-Setup.ps1             installer / uninstaller (Windows PowerShell 5.1, WinForms)
  InstantKarma/SLUS-20974_D7273511_InstantKarma.pnach   the mods (release build, player-facing descriptions)
  README.txt, LICENSE.txt
"""
import os, re, sys, shutil, zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build

PNACH_OUT = 'SLUS-20974_D7273511_InstantKarma.pnach'

# player-facing order and descriptions (section names stay as they are: PCSX2 settings refer to them)
MODULES = [
    (build.PATCH_CORE, 'The engine that makes the other Instant Karma features go. Always on while any of them is on; keep it ticked in the PCSX2 Patches list.'),
    (build.PATCH_PREY, 'A complete combat UI overhaul. Discover enemy weaknesses by experiment or by killing them, '
                       'then see them represented in a board and in targeting reticles, and previewed on enemies '
                       'as you browse skills. See buff and debuff stacks. '
                       "Inspired by SMTV's system."),
    (build.PATCH_TURBO, 'Speed up time without distorting audio. Hold R2/L2 for 3x/6x, click R3/L3 to toggle 3x/6x. '
                        'Paused while the main menu is open.'),
    (build.PATCH_SET, 'Increases the SET screen width to 3 columns and adds COST and ALPHABETICAL sort modes on START.'),
    (build.PATCH_BB, 'In combat, R1 quick-passes your turn, L1 quick-attempts to escape, and the right stick '
                     'skips a page up or down in the command lists.'),
    (build.PATCH_SK, 'Press up on the d-pad to toggle encounters.'),
    (build.PATCH_SUN, 'Press down on the d-pad to cycle sun state between min and max.'),
    (build.PATCH_BADK, 'Press right on the d-pad to instantly start a fight. TwoForOne reward bonuses apply.'),
    (build.PATCH_GOODK, 'Press left on the d-pad to instantly start a rare fight against a lone Omoikane. TwoForOne reward bonuses apply.'),
    (build.PATCH_TFO, 'Press SELECT to adjust combat frequency and rewards, from 1/2 to 1/5 as many; when you have '
                      'fewer encounters, you get equal-proportionately more rewards. Affects experience, atma, macca '
                      'and item drop rates--drop rates over 100% give a chance for extra items. No effect on bosses.'),
    (build.PATCH_QS, 'During bootup, pressing START will quickly speed through to load your latest save.'),
    (build.PATCH_SKIP, 'Press START to hyper-turbo through cutscenes.'),
    (build.PATCH_CHEST, 'No more confirmation on opening chests.'),
    (build.PATCH_WT, 'Dialogue appears instantly.'),
    (build.PATCH_QH, 'Inspecting a recovery terminal heals the whole party instantly, no menu. Karma Terminals '
                     'heal the party as their menu opens. Costs the same macca as healing everyone in the menu.'),
    (build.PATCH_WS, '16:9 widescreen. The 3D view gets wider while menus, text and portraits keep their '
                     'original shape. Sets PCSX2 to 16:9 while it is on.'),
]


def sections(text):
    head, secs, cur = [], {}, None
    for line in text.splitlines():
        m = re.match(r'\[(.+)\]$', line)
        if m:
            cur = m.group(1); secs[cur] = []
        elif cur is None:
            head.append(line)
        else:
            secs[cur].append(line)
    return head, secs


def main():
    text, _ = build.build(False)
    head, secs = sections(text)
    out = [l for l in head if l.strip()] + ['']
    for name, desc in MODULES:
        body = [l for l in secs[name] if l.strip()]
        body = ['description=' + desc if l.startswith('description=') else l for l in body]
        out += ['[%s]' % name] + body + ['']
    pnach = '\n'.join(out)

    name = 'InstantKarma-v%s' % build.VERSION
    dist = os.path.join(ROOT, 'dist')
    stage = os.path.join(dist, name)
    if os.path.isdir(stage): shutil.rmtree(stage)
    os.makedirs(os.path.join(stage, 'InstantKarma'))
    crlf = lambda s: s.replace('\r\n', '\n').replace('\n', '\r\n')
    def put(rel, data, text=True):
        with open(os.path.join(stage, rel), 'wb') as f:
            f.write(crlf(data).encode('utf-8') if text else data)
    put(os.path.join('InstantKarma', PNACH_OUT), pnach)
    setup = open(os.path.join(ROOT, 'installer', 'InstantKarma-Setup.ps1'), encoding='utf-8').read()
    put(os.path.join('InstantKarma', 'InstantKarma-Setup.ps1'), setup.replace('@@VERSION@@', build.VERSION))
    put('Install.bat', open(os.path.join(ROOT, 'installer', 'Install.bat'), encoding='utf-8').read())
    readme = open(os.path.join(ROOT, 'installer', 'README.txt'), encoding='utf-8').read()
    mods = '\n'.join('  %s\n    %s\n' % (n.replace('Instant Karma - ', ''), d) for n, d in MODULES if n != build.PATCH_CORE)
    put('README.txt', readme.replace('@@VERSION@@', build.VERSION).replace('@@MODULES@@', mods))
    put('LICENSE.txt', open(os.path.join(ROOT, 'LICENSE'), encoding='utf-8').read())

    zpath = os.path.join(dist, name + '.zip')
    if os.path.exists(zpath): os.remove(zpath)
    with zipfile.ZipFile(zpath, 'w', zipfile.ZIP_DEFLATED) as z:
        for base, _, files in os.walk(stage):
            for fn in sorted(files):
                full = os.path.join(base, fn)
                z.write(full, os.path.join(name, os.path.relpath(full, stage)))
    print(zpath)


if __name__ == '__main__':
    main()
