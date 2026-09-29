# Good Karma

Quality-of-life mods for **Shin Megami Tensei: Digital Devil Saga** (USA, SLUS-20974) on PCSX2 2.x.
All three mods live in one patch file and can be switched on or off independently in PCSX2.

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

### Prey Eyes
Battle information, adapted from the Prey Eyes 2 mod for SMT III Nocturne HD:
- **Reticle result:** while targeting, the centre of the targeting reticle shows what the selected skill will do to that enemy, pulsing like the original ring: green **!** weak, red shield/reflect/drain for null, reflect and drain, a split shield for resist, a white ring for a normal hit, and a white **?** when you don't know yet. Support skills leave the reticle unchanged.
- **Affinity board:** for a single targeted enemy, all nine elements (Phys, Gun, Fire, Ice, Elec, Force, Earth, Expel, Death) and five ailments (Charm, Poison, Mute, Panic, Sleep) with their result icons, plus the enemy's active buffs and debuffs.
- **Buffs and debuffs:** Attack, Magic, Defense and Hit/Evasion levels (-kaja/-nda, up to 4 steps) as small icons under each party member's portrait.
- **Knowledge:** affinities start unknown. Using a skill on an enemy teaches that attribute for its species; killing one or using Analyze reveals everything. Knowledge is stored inside your save file, so each save keeps its own.

## Install

1. Copy `SLUS-20974_D7273511.pnach` into your PCSX2 `patches` folder (PCSX2: *Tools → Open Data Directory*, then `patches`).
2. In PCSX2, right-click the game → **Properties → Patches**, and tick **Good Karma - Native Turbo** and/or **Good Karma - SET Screen**.
3. For full 6x turbo: in the same Properties window, **Emulation → EE Cycle Rate → 300%**.
4. Start (or restart) the game. Patches apply at boot.

Savestates made with a mod on keep that mod's code in memory; after changing which mods are enabled, load from a memory-card save or boot fresh.

### Scripted install (optional)
With Python 3 from the repository:
```
python tools/install.py "<PCSX2 data folder>"              # all mods
python tools/install.py "<PCSX2 data folder>" --no-turbo   # skip a mod: --no-turbo / --no-set / --no-prey
```
This copies the patch file, enables the chosen mods and sets EE Cycle Rate 300% in the game's settings file (other settings are kept).

## Uninstall
Delete `patches\SLUS-20974_D7273511.pnach`, or untick the mods in **Properties → Patches**.

## Building from source
- `tools/build.py` assembles the turbo hook (keystone) and compiles `src/*.c` with Zig (`zig cc`, MIPS III n32) into `build/SLUS-20974_D7273511.pnach`.
- Requirements: Python 3 with `keystone-engine`, `pyelftools`, `capstone`; Zig 0.16 (path set in `tools/cbuild.py`).
- `PLAN.md` documents the reverse-engineered game structures and addresses.

## Credits
Prey Eyes icons and design from Prey Eyes 2 (SMT III Nocturne HD mod, MIT); see `assets/preyeyes/LICENSE-PreyEyes2`.

## License
MIT. See `LICENSE`. This repository contains no game data; you need your own copy of the game.
