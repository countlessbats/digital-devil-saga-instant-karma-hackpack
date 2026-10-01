"""Assemble mods and emit the PCSX2 pnach.

Usage: python tools/build.py [--install DIR ...]
Writes build/D7273511.pnach; --install copies it into each given PCSX2 patches dir.
"""
import os, sys, struct, shutil, keystone
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cbuild, atlas

ROOT = os.path.join(os.path.dirname(__file__), '..')
VERSION = open(os.path.join(ROOT, 'VERSION')).read().strip()

# ---- Mod A: native turbo ---------------------------------------------------
# Main loop (0x1005c8) calls the task runner 0x101540 once per displayed frame.
# The hook runs it N-1 extra times with the render task's "skip submit" flag
# (bit 0x2000000 of 0x3ba904) set, re-reading the pad between passes so button
# edges fire once, then runs the normal pass that submits the frame.
# Each skipped pass restores the packet-buffer index (0x3bd2ea) so all passes
# build into the buffer the render thread is not holding.
# Turbo is off (and L3/R3 toggles ignored) while the camp menu is open.
TURBO_VARS = 0x000F0000   # +0 toggle mult, +1 debug override, +2 prev held, +4 last N, +8 extra passes
TURBO_CODE = 0x000F0020
HOOK_SITE = 0x001006AC    # jal 0x101540 inside main loop

TURBO_ASM = """
    .set noreorder
    addiu $sp, $sp, -0x20
    sd    $ra, 0($sp)
    sd    $s0, 8($sp)
    sd    $s1, 0x10($sp)
    lui   $s1, 0x000F
    lui   $8, 0x003C
    lhu   $8, -0x2c60($8)       # 0x3bd3a0 pad0 held (active high)
    lhu   $9, 2($s1)
    sh    $8, 2($s1)
    lui   $12, 0x003C
    lbu   $12, -0x394c($12)       # 0x3bc6b4 camp menu state (0 closed, 1 open, 2 closing)
    bne   $12, $zero, in_menu     # menu: 1x, toggles ignored
    nop
    xor   $10, $8, $9
    and   $10, $10, $8           # newly pressed
    lbu   $11, 0($s1)             # toggle multiplier (0 = off)
    andi  $12, $10, 0x400         # R3 -> toggle 3x
    beq   $12, $zero, no_l3
    addiu $13, $zero, 3
    bne   $11, $13, set_l3
    nop
    addiu $13, $zero, 0
set_l3:
    or    $11, $13, $zero
no_l3:
    andi  $12, $10, 0x200         # L3 -> toggle 6x
    beq   $12, $zero, no_r3
    addiu $13, $zero, 6
    bne   $11, $13, set_r3
    nop
    addiu $13, $zero, 0
set_r3:
    or    $11, $13, $zero
no_r3:
    sb    $11, 0($s1)
    or    $s0, $11, $zero         # N = toggle
    andi  $12, $8, 0x2           # hold R2 -> 3x
    beq   $12, $zero, no_l2
    nop
    addiu $s0, $zero, 3
no_l2:
    andi  $12, $8, 0x1           # hold L2 -> 6x
    beq   $12, $zero, no_r2
    nop
    addiu $s0, $zero, 6
no_r2:
    lbu   $12, 1($s1)             # debug override
    beq   $12, $zero, no_ovr
    nop
    or    $s0, $12, $zero
no_ovr:
    bgtz  $s0, n_ok
    nop
in_menu:
    addiu $s0, $zero, 1
n_ok:
    sb    $s0, 4($s1)
    addiu $s0, $s0, -1
extra:
    blez  $s0, final
    nop
    lui   $8, 0x003C
    lw    $9, -0x56fc($8)       # 0x3ba904 render flags
    lui   $10, 0x0200
    or    $9, $9, $10
    sw    $9, -0x56fc($8)
    jal   0x101540
    nop
    lui   $8, 0x003C
    lbu   $9, -0x2d16($8)         # 0x3bd2ea packet buffer index: undo this pass's flip
    xori  $9, $9, 1
    sb    $9, -0x2d16($8)
    jal   0x2e39c8                # refresh pad so edges don't repeat
    nop
    lui   $8, 0x003C
    lw    $9, -0x5900($8)       # 0x3ba700 frame counter
    addiu $9, $9, 1
    sw    $9, -0x5900($8)
    lw    $9, -0x58fc($8)       # 0x3ba704 loop counter
    addiu $9, $9, 1
    sw    $9, -0x58fc($8)
    lw    $9, 8($s1)
    addiu $9, $9, 1
    sw    $9, 8($s1)
    b     extra
    addiu $s0, $s0, -1
final:
    lui   $8, 0x003C
    lw    $9, -0x56fc($8)
    lui   $10, 0xFDFF
    ori   $10, $10, 0xFFFF
    and   $9, $9, $10
    sw    $9, -0x56fc($8)
    jal   0x101540
    nop
    ld    $ra, 0($sp)
    ld    $s0, 8($sp)
    ld    $s1, 0x10($sp)
    jr    $ra
    addiu $sp, $sp, 0x20
"""

ks = keystone.Ks(keystone.KS_ARCH_MIPS, keystone.KS_MODE_MIPS64 | keystone.KS_MODE_LITTLE_ENDIAN)

def asm(src, addr):
    enc, _ = ks.asm(src, addr)
    b = bytes(enc)
    return list(struct.unpack('<%dI' % (len(b) // 4), b))

def jal(target):
    return 0x0C000000 | ((target >> 2) & 0x03FFFFFF)

PNACH = 'SLUS-20974_D7273511.pnach'
PATCH_TURBO = 'Instant Karma - Native Turbo'
PATCH_SET = 'Instant Karma - SET Screen'
PATCH_PREY = 'Instant Karma - Prey Eyes'
PATCH_BB = 'Instant Karma - BattleButtons'
PATCH_SK = 'Instant Karma - SubtleKarma'
PATCH_SUN = 'Instant Karma - SunKing'
PATCH_WT = 'Instant Karma - WordTripper'
PATCH_TFO = 'Instant Karma - TwoForOne'
PATCH_QS = 'Instant Karma - QuickStart'
PATCH_SKIP = 'Instant Karma - SceneSkip'
PATCH_CHEST = 'Instant Karma - OpenChests'
PATCH_BADK = 'Instant Karma - BadKarma'
PATCH_GOODK = 'Instant Karma - GoodKarma'
PATCH_WS = 'Instant Karma - Widescreen'
PATCH_QH = 'Instant Karma - QuickHeal'
ATLAS_ADDR = 0x000A0000

def build(include_test=False, include_local=False):
    lines = ['gametitle=Shin Megami Tensei: Digital Devil Saga (USA) [SLUS-20974] (D7273511)', '',
             '[%s]' % PATCH_TURBO,
             'author=Instant Karma v%s' % VERSION,
             'description=Hold R2 = 3x, hold L2 = 6x, R3/L3 toggle 3x/6x. Off in the main menu. '
             'Music stays normal speed. 6x needs EE Cycle Rate 300%.']
    words = asm(TURBO_ASM, TURBO_CODE)
    for i, w in enumerate(words):
        lines.append('patch=1,EE,%08X,word,%08X' % (TURBO_CODE + 4 * i, w))
    lines.append('patch=1,EE,%08X,word,%08X' % (HOOK_SITE, jal(TURBO_CODE)))
    # ---- C mods (src/*.c): the code blob is emitted in every section that uses it ----
    segs, syms = cbuild.build(include_test, include_local)
    def blob(dst):
        for addr, data in segs:
            data = data + b'\0' * (-len(data) % 4)
            for i in range(0, len(data), 4):
                dst.append('patch=1,EE,%08X,word,%08X' % (addr + i, struct.unpack_from('<I', data, i)[0]))
    def hook(dst, site, target, comment, is_jal=True):
        dst.append('patch=1,EE,%08X,word,%08X' % (site, jal(target) if is_jal else target))
    lines += ['', '[%s]' % PATCH_SET,
              'author=Instant Karma v%s' % VERSION,
              'description=Skill SET screen: 3-column LEARNED grid (d-pad wraps, L2/R2 change tab), '
              'START sorts Game/Cost/A-Z, relaid-out panels.']
    blob(lines)
    hook(lines, 0x0027862C, syms['set_finalize'], 'builder finalize call')
    hook(lines, 0x0037CC9C, syms['set_logic'], 'SET logic task table', False)
    hook(lines, 0x0037CCA0, syms['set_draw'], 'SET draw task table', False)
    hook(lines, 0x0037CC68, syms['set_draw_slot'], 'SET slot-select draw', False)
    hook(lines, 0x00278020, syms['costfn_copy'], 'LEARNED cost/unit drawer')
    # ---- Prey Eyes ----
    lines += ['', '[%s]' % PATCH_PREY,
              'author=Instant Karma v%s' % VERSION,
              'description=Battle info: reticle shows the skill result on each target (green good, red bad, '
              '? unknown), affinity board for the targeted enemy, buff/debuff icons. Affinities are learned by using them.']
    ab = open(os.path.join(ROOT, 'build', 'prey_atlas.bin'), 'rb').read()
    ab += b'\0' * (-len(ab) % 4)
    for i in range(0, len(ab), 4):
        lines.append('patch=1,EE,%08X,word,%08X' % (ATLAS_ADDR + i, struct.unpack_from('<I', ab, i)[0]))
    blob(lines)
    hook(lines, 0x001C1100, syms['prey_reticle'], 'per-target reticle draw')
    hook(lines, 0x001C00EC, syms['prey_ring'], 'reticle ring sprite draw')
    hook(lines, 0x001B6E84, syms['prey_party_panel'], 'party panel per-member draw')
    hook(lines, 0x001A1158, syms['prey_battle_exit'], 'btlExit teardown call')
    hook(lines, 0x001CC320, syms['prey_buff_event'], 'buff change queued (skill result)')
    hook(lines, 0x001CD868, syms['prey_buff_event'], 'buff change queued (item/other result)')
    lines.append('patch=1,EE,000FD200,word,00000001')            # FEATURES[0]: Prey Eyes on
    hook(lines, 0x001C13A0, syms['bb_target_input'], 'target panel input (shared)')
    # ---- BattleButtons ----
    lines += ['', '[%s]' % PATCH_BB,
              'author=Instant Karma v%s' % VERSION,
              'description=Battle command menu: R1 passes and L1 retreats instantly; the right stick jumps a page.']
    blob(lines)
    lines.append('patch=1,EE,000FD204,word,00000001')            # FEATURES[1]: BattleButtons on
    hook(lines, 0x001BF134, syms['bb_command_hook'], 'command panel pre-input')
    hook(lines, 0x001C13A0, syms['bb_target_input'], 'target panel input (shared)')
    # ---- field toggles ----
    for name, flag, desc in ((PATCH_SK, 0x000FD208, 'Field: d-pad UP toggles random encounters on/off (with sound and message).'),
                             (PATCH_SUN, 0x000FD20C, 'Field: d-pad DOWN switches solar noise between MAX and MIN.')):
        lines += ['', '[%s]' % name, 'author=Instant Karma v%s' % VERSION, 'description=' + desc]
        blob(lines)
        lines.append('patch=1,EE,%08X,word,00000001' % flag)
        hook(lines, 0x00125980, syms['field_hook'], 'field player-control step')
    # ---- TwoForOne: SELECT on the field cycles 1:1..5:1 (fewer encounters, bigger rewards; bosses unchanged) ----
    lines += ['', '[%s]' % PATCH_TFO, 'author=Instant Karma v%s' % VERSION,
              'description=SELECT on the field cycles 1-for-1 to 5-for-1: N times fewer random encounters, N times the '
              'EXP, atma, macca and item drop chance (bosses unchanged).']
    blob(lines)
    lines.append('patch=1,EE,000FD210,word,00000001')            # FEATURES[4]: TwoForOne on
    hook(lines, 0x00125980, syms['field_hook'], 'field player-control step')
    for site in (0x00124D1C, 0x00124D48, 0x00216228):
        hook(lines, site, syms['tfo_enc'], 'encounter accumulator step')
    hook(lines, 0x001D0338, syms['tfo_kill_total'], 'per-enemy EXP/atma/macca totals')
    hook(lines, 0x001D2680, syms['tfo_ep'], "killer's atma")
    hook(lines, 0x001D26A8, syms['tfo_money'], "killer's macca")
    for site in (0x001D1CCC, 0x001D2564):
        hook(lines, site, syms['tfo_hunt_ep'], 'hunt atma bonus')
    hook(lines, 0x001A44C4, syms['tfo_drop'], 'item drop roll')
    # ---- QuickStart: one press skips all logos/intro to the main menu; START loads the most recent save ----
    lines += ['', '[%s]' % PATCH_QS, 'author=Instant Karma v%s' % VERSION,
              'description=Any button during the logos/intro skips straight to the main menu. START instead loads '
              'your most recent save.']
    blob(lines)
    lines.append('patch=1,EE,000FD214,word,00000001')            # FEATURES[5]: QuickStart on
    qa = syms['qs_title']; qhi = (qa + 0x8000) >> 16
    lines.append('patch=1,EE,0026B11C,word,%08X' % (0x3C080000 | qhi))            # lui $t0, hi(qs_title)
    lines.append('patch=1,EE,0026B12C,word,%08X' % (0x25080000 | (qa & 0xffff)))  # addiu $t0, $t0, lo(qs_title)
    hook(lines, 0x001006A4, syms['qs_pad'], 'main loop pad processing (autoload presses)')
    hook(lines, 0x00125980, syms['field_hook'], 'field player-control step (tells QuickStart the game is up)')
    # ---- SceneSkip: START skips the whole cutscene (every segment made skippable; the rest fast-forwarded) ----
    lines += ['', '[%s]' % PATCH_SKIP, 'author=Instant Karma v%s' % VERSION,
              'description=START during a cutscene skips the whole scene, including scenes the game normally '
              'refuses to skip (fast-forwards the parts in between with Native Turbo on).']
    blob(lines)
    lines.append('patch=1,EE,000FD218,word,00000001')            # FEATURES[6]: SceneSkip on
    lines.append('patch=1,EE,0022F2F4,word,24020000')            # 'cannot be skipped' check -> skippable
    hook(lines, 0x001006A4, syms['qs_pad'], 'main loop pad processing (shared with QuickStart)')
    hook(lines, 0x00125980, syms['field_hook'], 'field player-control step (tells SceneSkip the player has control)')
    # ---- OpenChests: inspecting a chest opens it at once (question and Yes answered automatically) ----
    lines += ['', '[%s]' % PATCH_CHEST, 'author=Instant Karma v%s' % VERSION,
              'description=Inspecting a chest opens it straight away: the "Touch it?" question and Yes/No are answered inside the script, never shown. '
              'The "Obtained" message stays so you see what you got.']
    blob(lines)
    lines.append('patch=1,EE,000FD21C,word,00000001')            # FEATURES[7]: OpenChests on
    lines.append('patch=1,EE,0039E288,word,%08X' % syms['chest_cmd_msg'])      # script command 0 (MSG)
    lines.append('patch=1,EE,0039E2A0,word,%08X' % syms['chest_cmd_select'])   # script command 3 (SELECT)
    lines.append('patch=1,EE,0039E2F8,word,%08X' % syms['chest_cmd_wait'])     # script command 0xe (WAIT)
    # ---- BadKarma / GoodKarma: d-pad RIGHT / LEFT forces an encounter (Omoikane for LEFT) ----
    for name, flag, desc in ((PATCH_BADK, 0x000FD224, 'd-pad RIGHT on the field starts a fight right away (TwoForOne rewards apply).'),
                             (PATCH_GOODK, 0x000FD228, 'd-pad LEFT on the field starts a rare Omoikane fight (TwoForOne rewards apply).')):
        lines += ['', '[%s]' % name, 'author=Instant Karma v%s' % VERSION, 'description=' + desc]
        blob(lines)
        lines.append('patch=1,EE,%08X,word,00000001' % flag)
        hook(lines, 0x00125980, syms['field_hook'], 'field player-control step')
        for site in (0x00124D1C, 0x00124D48, 0x00216228):
            hook(lines, site, syms['tfo_enc'], 'encounter roll (forced for one roll)')
    # ---- WordTripper: every glyph starts fading in at once (0x1955d8 reveal gate forced open) ----
    lines += ['', '[%s]' % PATCH_WT, 'author=Instant Karma v%s' % VERSION,
              'description=Text appears all at once, fading in together instead of letter by letter.']
    lines.append('patch=1,EE,00195664,word,00000000')   # no per-line wait (bne v1,v0)
    lines.append('patch=1,EE,0019566C,word,0000102D')   # previous-glyph alpha check -> always pass
    # ---- QuickHeal: a recovery terminal heals the party at once (menu price for everyone), no menu ----
    lines += ['', '[%s]' % PATCH_QH, 'author=Instant Karma v%s' % VERSION,
              'description=Inspecting a recovery terminal heals the party at once (same price as the menu), no menu.']
    blob(lines)
    lines.append('patch=1,EE,000FD234,word,00000001')            # FEATURES[13]: QuickHeal on
    lines.append('patch=1,EE,00249FA8,word,%08X' % (0x08000000 | (syms['qh_term_open'] >> 2)))   # j qh_term_open
    lines.append('patch=1,EE,00249FAC,word,00000000')            # (delay slot)
    # ---- Widescreen: 16:9 camera (the camera aspect constant) with the 2D interface kept at 4:3 proportions ----
    lines += ['', '[%s]' % PATCH_WS, 'author=Instant Karma v%s' % VERSION,
              'description=16:9 widescreen: wider 3D view; menus and text keep their shape.',
              'gsaspectratio=16:9']
    blob(lines)
    lines.append('patch=1,EE,000FD22C,word,00000001')            # FEATURES[11]: Widescreen on
    # draw-layer set-up (rebuilt every frame) stores our insert wrappers instead of 0x2d41c0 / 0x2efb30
    hi = lambda a: ((a + 0x8000) >> 16) & 0xffff
    lo = lambda a: a & 0xffff
    a, b = syms['ws_ins_a'], syms['ws_ins_b']
    lines.append('patch=1,EE,002D4240,word,%08X' % (0x3C080000 | hi(a)))   # lui   t0, hi(ws_ins_a)
    lines.append('patch=1,EE,002D4248,word,%08X' % (0x25020000 | lo(a)))   # addiu v0, t0, lo(ws_ins_a)
    lines.append('patch=1,EE,002D427C,word,%08X' % (0x25020000 | lo(a)))   # (second loop)
    lines.append('patch=1,EE,002EFD30,word,%08X' % (0x3C020000 | hi(b)))   # lui   v0, hi(ws_ins_b)
    lines.append('patch=1,EE,002EFD38,word,%08X' % (0x24420000 | lo(b)))   # addiu v0, v0, lo(ws_ins_b)
    hook(lines, 0x001BFF7C, syms['ws_project'], 'reticle projection (spread to match the squeezed interface)')
    hook(lines, 0x00149F84, syms['ws_minimap'], 'field minimap draw (squeezed without the edge rule)')
    # ---- extra sections kept outside the repository (local/sections.py), if present ----
    if include_local:
        import importlib.util
        spec = importlib.util.spec_from_file_location('local_sections', os.path.join(ROOT, 'local', 'sections.py'))
        mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
        mod.add(lines, blob, hook, syms, VERSION)
    build.syms = syms
    return '\n'.join(lines) + '\n', words

if __name__ == '__main__':
    text, words = build()
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    out = os.path.join(ROOT, 'build', PNACH)
    open(out, 'w').write(text)
    print('v%s: %d words of turbo code -> %s' % (VERSION, len(words), out))
    if os.path.exists(os.path.join(ROOT, 'local', 'sections.py')):
        text, _ = build(include_local=True)
        os.makedirs(os.path.join(ROOT, 'local', 'build'), exist_ok=True)
        out = os.path.join(ROOT, 'local', 'build', PNACH)
        open(out, 'w').write(text)
        print('with local sections ->', out)
    args = sys.argv[1:]
    while args:
        if args[0] == '--install':
            shutil.copy(out, os.path.join(args[1], PNACH)); print('installed ->', args[1]); args = args[2:]
        else:
            raise SystemExit('unknown arg ' + args[0])
