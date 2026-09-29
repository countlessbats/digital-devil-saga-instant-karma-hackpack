# Good Karma

Quality-of-life mods for **Shin Megami Tensei: Digital Devil Saga** (USA, SLUS-20974) on PCSX2 2.x.
All the mods live in one patch file and can be switched on or off independently in PCSX2.

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
- **Reticle result:** while targeting, the centre of the reticle shows what the selected skill will do to that enemy and pulses with it: green **!** weak, red icons for null, reflect and drain, a split shield for resist, the game's own ring in white for a normal hit, and a white **?** when you don't know yet. Support skills leave the reticle unchanged.
- **Affinity board:** above the targeted enemy's name: all nine elements (Phys, Gun, Fire, Ice, Elec, Force, Earth, Expel, Death) with their results, and above them the ailments (Charm, Poison, Mute, Panic, Sleep) you know the enemy resists, blocks or is weak to. Unknown and normal ones are left out. This row is the enemy's resistance, not its current status.
- **Buffs and debuffs:** Attack, Magic, Defense and Hit/Evasion levels (-kaja/-nda, up to 4 steps) above each enemy's head (hidden while an attack plays, unless that attack changes buffs) and under each party member's portrait. The help bar moves down slightly to make room.
- **Knowledge:** affinities start unknown. Using a skill on an enemy teaches that attribute for its species; killing one or using Analyze reveals everything. Knowledge is stored inside your save file, so each save keeps its own.

### BattleButtons
In the battle command menu, **R1** passes the turn and **L1** retreats, both instantly with no menu. The **right stick** jumps the current list up or down by 4 entries.

### SubtleKarma
On the field, **d-pad UP** toggles random encounters off and on, with a system sound and an on-screen message. Only works while you are walking around (not in menus, battles or scenes).

### SunKing
On the field, **d-pad DOWN** switches solar noise between MAX and MIN.

### TwoForOne
On the field, **SELECT** cycles 1-for-1 → 2-for-1 → … → 5-for-1 (with a sound and an on-screen message). At N-for-1:
- random encounters come N times less often;
- EXP, atma and macca from normal battles are multiplied by N, including the hunt bonus;
- item drop chances are multiplied by N. Any chance above 100% becomes the chance of an extra drop (for example, a 30% item at 4-for-1 always drops once, with a 20% chance of a second).

Boss battles are unchanged. The mode is stored in your save.

### QuickStart
While the logos or the intro movie are playing:
- **any button** skips all of it and lands on the main menu;
- **START** skips all of it and loads your most recent save (picked by the memory card's save time, or the longest play time if the card has no dates).

### SceneSkip
Press **START** during a cutscene to skip it, including scenes the game normally refuses to skip ("This event cannot be skipped."). One press carries on through scenes that follow straight after, until you have control again. The parts between scene segments are fast-forwarded when Native Turbo is on.

### WordTripper
All text fades in at once instead of letter by letter.

## Install

1. Copy `SLUS-20974_D7273511.pnach` into your PCSX2 `patches` folder (PCSX2: *Tools → Open Data Directory*, then `patches`).
2. In PCSX2, right-click the game → **Properties → Patches**, and tick the Good Karma mods you want.
3. For full 6x turbo: in the same Properties window, **Emulation → EE Cycle Rate → 300%**.
4. Start (or restart) the game. Patches apply at boot.

Savestates made with a mod on keep that mod's code in memory; after changing which mods are enabled, load from a memory-card save or boot fresh.

### Scripted install (optional)
With Python 3 from the repository:
```
python tools/install.py "<PCSX2 data folder>"              # all mods
python tools/install.py "<PCSX2 data folder>" --no-turbo   # skip a mod: --no-turbo / --no-set / --no-prey / --no-buttons / --no-subtle / --no-sun / --no-wordtripper / --no-twoforone / --no-quickstart / --no-sceneskip
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
