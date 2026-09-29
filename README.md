# Good Karma

Quality-of-life mods for **Shin Megami Tensei: Digital Devil Saga** (USA, SLUS-20974) on PCSX2 2.x.
Both mods live in one patch file and can be switched on or off independently in PCSX2.

## Mods

### Native Turbo
Runs the game's own logic faster, instead of fast-forwarding the emulator, so music keeps its normal speed.

| Button | Effect |
|---|---|
| Hold L2 | 3x speed |
| Hold R2 | 6x speed |
| L3 | Toggle 3x on/off |
| R3 | Toggle 6x on/off |

Turbo is off while the main menu is open; a toggle stays remembered and resumes when you close it.
6x needs **EE Cycle Rate 300%** for this game (the installer sets it; see below to set it by hand). Without it, 6x tops out around 4x.

### SET Screen
A redesigned skill SET screen:
- LEARNED shows **3 columns × 8 rows** (24 entries instead of 8).
- **D-pad** moves in all four directions and wraps at every edge. **L2/R2** change the skill category.
- **START** sorts LEARNED: Game order → Cost → A–Z. The choice sticks across visits and characters until the game restarts.
- ASSIGNED, HELP and the character status are moved into the corners to make room; the "new skill" marker and cursor arrow sit at the end of each skill name.

## Install

1. Copy `SLUS-20974_D7273511.pnach` into your PCSX2 `patches` folder (PCSX2: *Tools → Open Data Directory*, then `patches`).
2. In PCSX2, right-click the game → **Properties → Patches**, and tick **Good Karma - Native Turbo** and/or **Good Karma - SET Screen**.
3. For full 6x turbo: in the same Properties window, **Emulation → EE Cycle Rate → 300%**.
4. Start (or restart) the game. Patches apply at boot.

Savestates made with a mod on keep that mod's code in memory; after changing which mods are enabled, load from a memory-card save or boot fresh.

### Scripted install (optional)
With Python 3 from the repository:
```
python tools/install.py "<PCSX2 data folder>"               # both mods
python tools/install.py "<PCSX2 data folder>" --turbo-only
python tools/install.py "<PCSX2 data folder>" --set-only
```
This copies the patch file, enables the chosen mods and sets EE Cycle Rate 300% in the game's settings file (other settings are kept).

## Uninstall
Delete `patches\SLUS-20974_D7273511.pnach`, or untick the mods in **Properties → Patches**.

## Building from source
- `tools/build.py` assembles the turbo hook (keystone) and compiles `src/*.c` with Zig (`zig cc`, MIPS III n32) into `build/SLUS-20974_D7273511.pnach`.
- Requirements: Python 3 with `keystone-engine`, `pyelftools`, `capstone`; Zig 0.16 (path set in `tools/cbuild.py`).
- `PLAN.md` documents the reverse-engineered game structures and addresses.

## License
MIT. See `LICENSE`. This repository contains no game data; you need your own copy of the game.
