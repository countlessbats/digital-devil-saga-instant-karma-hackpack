"""Test-only pnach: release mods + pad injection for driving the game over PINE.

Never install this into your PCSX2. Usage: python tools/testpatch.py [PATCHES_DIR]
(default <local path>). Write a button mask (game layout, see PLAN.md)
to INJECT via PINE; it is OR'd into pad0's raw held word for the main-loop pad read.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build

INJECT = 0x000F000C        # u16 buttons to inject
INJ_CODE = 0x000F0300
INJ_SITE = 0x00100674      # jal 0x2e39c8 (pad read) in main loop
INJ_ASM = """
    .set noreorder
    addiu $sp, $sp, -0x10
    sd    $ra, 0($sp)
    lui   $8, 0x000F
    lw    $12, 0x10($8)           # 0xF0010: raw analog override (0 = none)
    beq   $12, $zero, noan
    lui   $10, 0x0040
    sw    $12, -0x64f0($10)       # 0x3f9b10 raw analog bytes
noan:
    lhu   $9, 0x0c($8)
    lui   $10, 0x0040
    lhu   $11, -0x64fa($10)       # 0x3f9b06 pad0 raw held
    sh    $11, 8($sp)
    or    $11, $11, $9
    jal   0x2e39c8
    sh    $11, -0x64fa($10)
    # raw value is left injected for the rest of the frame (field code reads it later)
    ld    $ra, 0($sp)
    jr    $ra
    addiu $sp, $sp, 0x10
"""

TR_CODE = 0x000F0400
TR_ASM = '''
    .set noreorder
    lui   $8, 0x000F
    lw    $9, -0x1004($8)        # 0xFEFFC... use 0xFE000 buffer: $8=0xF0000 -> base 0xFE000
    nop
'''
# entry trace of 0x2bf438: logs (ra, x, y, spr) while 0xFEFFC == 0x7ACE. Uses only $1/$24/$25.
TR_ASM = '''
    .set noreorder
    .set noat
    lui   $24, 0x0010
    lw    $25, -0x1004($24)
    addiu $1, $zero, 0x7ACE
    bne   $25, $1, go
    nop
    lw    $25, -0x2000($24)
    sltiu $1, $25, 200
    beq   $1, $zero, go
    nop
    addiu $1, $25, 1
    sw    $1, -0x2000($24)
    sll   $25, $25, 4
    addu  $25, $25, $24
    sw    $31, -0x1ff0($25)
    sw    $4, -0x1fec($25)
    sw    $5, -0x1fe8($25)
    sw    $10, -0x1fe4($25)
go:
    addiu $29, $29, -0x60
    j     0x2bf440
    sd    $21, 0x38($29)
'''

if __name__ == '__main__':
    dest = sys.argv[1] if len(sys.argv) > 1 else r'<local path>'
    text, _ = build.build(include_test=True)
    syms = build.build.syms
    lines = [text, '[Test Only - Pad Injection]']
    if os.environ.get('QS_SLOT'): lines.append('patch=1,EE,000FE0F4,word,%08X' % (0x51530000 | int(os.environ['QS_SLOT'])))
    if os.environ.get('QS_TEST'): lines.append('patch=1,EE,000FE0F0,word,%08X' % (0x51530000 | int(os.environ['QS_TEST'])))
    for i, w in enumerate(build.asm(INJ_ASM, INJ_CODE)):
        lines.append('patch=1,EE,%08X,word,%08X' % (INJ_CODE + 4 * i, w))
    lines.append('patch=1,EE,%08X,word,%08X' % (INJ_SITE, build.jal(INJ_CODE)))
    for site in (0x1c0244, 0x1c038c):   # arrows only; 0x1c00ec belongs to Prey Eyes
        lines.append('patch=1,EE,%08X,word,%08X' % (site, build.jal(syms['trace_draw438'])))
    if os.environ.get('TRACE438'):
        tw = build.asm(TR_ASM, TR_CODE)
        for i, w in enumerate(tw): lines.append('patch=1,EE,%08X,word,%08X' % (TR_CODE + 4 * i, w))
        lines.append('patch=1,EE,002BF438,word,%08X' % (0x08000000 | (TR_CODE >> 2)))
        lines.append('patch=1,EE,002BF43C,word,00000000')
    open(os.path.join(dest, build.PNACH), 'w').write('\n'.join(lines) + '\n')
    print('test pnach ->', dest)
