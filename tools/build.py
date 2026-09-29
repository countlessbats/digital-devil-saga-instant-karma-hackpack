"""Assemble mods and emit the PCSX2 pnach.

Usage: python tools/build.py [--install DIR ...]
Writes build/D7273511.pnach; --install copies it into each given PCSX2 patches dir.
"""
import os, sys, struct, shutil, keystone
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cbuild

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
    andi  $12, $10, 0x200         # L3 -> toggle 3x
    beq   $12, $zero, no_l3
    addiu $13, $zero, 3
    bne   $11, $13, set_l3
    nop
    addiu $13, $zero, 0
set_l3:
    or    $11, $13, $zero
no_l3:
    andi  $12, $10, 0x400         # R3 -> toggle 6x
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
    andi  $12, $8, 0x1           # hold L2 -> 3x
    beq   $12, $zero, no_l2
    nop
    addiu $s0, $zero, 3
no_l2:
    andi  $12, $8, 0x2           # hold R2 -> 6x
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
PATCH_TURBO = 'Good Karma - Native Turbo'
PATCH_SET = 'Good Karma - SET Screen'

def build():
    lines = ['gametitle=Shin Megami Tensei: Digital Devil Saga (USA) [SLUS-20974] (D7273511)', '',
             '[%s]' % PATCH_TURBO,
             'author=Good Karma v%s' % VERSION,
             'description=Hold L2 = 3x, hold R2 = 6x, L3/R3 toggle 3x/6x. Off in the main menu. '
             'Music stays normal speed. 6x needs EE Cycle Rate 300%.']
    words = asm(TURBO_ASM, TURBO_CODE)
    for i, w in enumerate(words):
        lines.append('patch=1,EE,%08X,word,%08X' % (TURBO_CODE + 4 * i, w))
    lines.append('patch=1,EE,%08X,word,%08X' % (HOOK_SITE, jal(TURBO_CODE)))
    # ---- C mods (src/*.c) ----
    segs, syms = cbuild.build()
    lines += ['', '[%s]' % PATCH_SET,
              'author=Good Karma v%s' % VERSION,
              'description=Skill SET screen: 3-column LEARNED grid (d-pad wraps, L2/R2 change tab), '
              'START sorts Game/Cost/A-Z, relaid-out panels.']
    for addr, data in segs:
        data = data + b'\0' * (-len(data) % 4)
        for i in range(0, len(data), 4):
            lines.append('patch=1,EE,%08X,word,%08X' % (addr + i, struct.unpack_from('<I', data, i)[0]))
    lines.append('patch=1,EE,%08X,word,%08X' % (0x0027862C, jal(syms['set_finalize'])))  # builder finalize call
    lines.append('patch=1,EE,%08X,word,%08X' % (0x0037CC9C, syms['set_logic']))           # SET logic task table
    lines.append('patch=1,EE,%08X,word,%08X' % (0x0037CCA0, syms['set_draw']))            # SET draw task table
    lines.append('patch=1,EE,%08X,word,%08X' % (0x0037CC68, syms['set_draw_slot']))       # SET slot-select draw
    lines.append('patch=1,EE,%08X,word,%08X' % (0x00278020, jal(syms['costfn_copy'])))    # LEARNED cost/unit drawer
    return '\n'.join(lines) + '\n', words

if __name__ == '__main__':
    text, words = build()
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    out = os.path.join(ROOT, 'build', PNACH)
    open(out, 'w').write(text)
    print('v%s: %d words of turbo code -> %s' % (VERSION, len(words), out))
    args = sys.argv[1:]
    while args:
        if args[0] == '--install':
            shutil.copy(out, os.path.join(args[1], PNACH)); print('installed ->', args[1]); args = args[2:]
        else:
            raise SystemExit('unknown arg ' + args[0])
