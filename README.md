# Instant Karma

Quality-of-life mods for **Shin Megami Tensei: Digital Devil Saga** (USA, SLUS-20974) on PCSX2 2.x.
All the mods live in one patch file and can be switched on or off independently in PCSX2.

## Mods

### Native Turbo
Runs the game's own logic faster, instead of fast-forwarding the emulator, so music keeps its normal speed.

| Button | Effect |
|---|---|
| Hold R2 | 3x speed |
| Hold L2 | 6x speed |
| R3 | Toggle 3x on/off |
| L3 | Toggle 6x on/off |

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
- **Skill preview:** in the command menu, hovering Attack or a skill marks every enemy with the result it would have, as the reticle's result icon alone: only known weak, resist, null, reflect and drain results show. Choosing the skill hands over to the full reticle on the enemy you target.
- **Affinity board:** above the targeted enemy's name: all nine elements (Phys, Gun, Fire, Ice, Elec, Force, Earth, Expel, Death) with their results, and above them the ailments (Charm, Poison, Mute, Panic, Sleep) you know the enemy resists, blocks or is weak to. Unknown and normal ones are left out. This row is the enemy's resistance, not its current status.
- **Buffs and debuffs:** Attack, Magic, Defense and Hit/Evasion levels (-kaja/-nda, up to 4 steps) above each enemy's head (hidden while an attack plays, unless that attack changes buffs) and under each party member's portrait. The help bar moves down slightly to make room.
- **Knowledge:** affinities start unknown. Using a skill on an enemy teaches that attribute for its species; killing one or using Analyze reveals everything. Knowledge is stored inside your save file, so each save keeps its own.

### BattleButtons
In the battle command menu, **R1** passes the turn and **L1** retreats, both instantly with no menu. The **right stick** jumps the current list up or down by 4 entries.

### SubtleKarma
On the field, **d-pad UP** toggles random encounters off and on, with a system sound and an on-screen message. Only works while you are walking around (not in menus, battles or scenes).

### SunKing
On the field, **d-pad DOWN** switches solar noise between MAX and MIN.

### BadKarma
On the field, **d-pad RIGHT** starts a random battle right away, as if the encounter had just rolled. Areas with no random encounters (safe rooms, many chest rooms) say "No enemies here". TwoForOne's reward bonus applies.

### GoodKarma
On the field, **d-pad LEFT** starts a rare Omoikane fight (one or two Omoikane). Same rules as BadKarma, and TwoForOne's reward bonus applies.

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
Press **START** during a cutscene to skip it, including scenes the game normally refuses to skip ("This event cannot be skipped."). One press carries on through scenes that follow straight after, until you have control again. The scene's script still runs to the end (fast-forwarded when Native Turbo is on), so story state is exactly as if you had watched it.

### OpenChests
Inspecting a chest (or a floating jewel) opens it straight away: the "A strange object lies on the floor. Touch it?" question and its Yes/No are answered inside the game's own chest script, so they never appear, and the script's pause for the lid animation is skipped: "Obtained ..." comes up straight away and clears like any other message.

### WordTripper
All text fades in at once instead of letter by letter.

## Install

Download `InstantKarma-v<version>.zip` (it contains no game data), extract it, close PCSX2, and run `Install.bat`.
Pick your Digital Devil Saga (USA) disc image (it is checked, never changed), confirm the PCSX2 data folder, tick the
modules you want and click **Install**. Start the game in PCSX2; mods load at boot.

- With Native Turbo on, the installer sets EE Cycle Rate 300% for this game (full 6x needs it); the earlier value is
  restored when Native Turbo is turned off or Instant Karma is removed. Manual installs: *Properties > Emulation > EE Cycle Rate*.
- Change modules later by running `Install.bat` again, or in PCSX2: right-click the game > **Properties > Patches**.
- Manual install: copy `InstantKarma\SLUS-20974_D7273511_InstantKarma.pnach` into PCSX2's `patches` folder
  (*Tools > Open Data Directory*) and tick the mods in **Properties > Patches**.
- Savestates keep the mods that were on when they were made; after changing modules, load from a memory-card save.
- PCSX2's achievements hardcore mode disables patches.

## Uninstall
Run `instantkarma\Uninstall Instant Karma.bat` in your PCSX2 data folder (or click **Remove Instant Karma** in the installer).
It removes the patch file and the Instant Karma entries in the game's settings, nothing else.

## Building from source
- `tools/build.py` assembles the turbo hook (keystone) and compiles `src/*.c` with Zig (`zig cc`, MIPS III n32) into `build/SLUS-20974_D7273511.pnach`.
- Requirements: Python 3 with `keystone-engine`, `pyelftools`, `capstone`; Zig 0.16 (path set in `tools/cbuild.py`).
- `tools/package.py` builds the distributable zip in `dist/` (installer sources in `installer/`);
  `tools/test_installer.ps1` checks install/uninstall against scratch PCSX2 folders.
- `PLAN.md` documents the reverse-engineered game structures and addresses.

## Credits
Prey Eyes is inspired by Shin Megami Tensei V's affinity display and the Prey Eyes 2 mod for SMT III Nocturne HD.
All icons are original, drawn in code by `tools/icons.py` and `tools/atlas.py`.

## License
MIT. See `LICENSE`. This repository contains no game data; you need your own copy of the game.
