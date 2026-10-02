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
    jal   0xF0280                 # pad read through the turbo pad filter (needs Native Turbo on)
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

MT_CODE = 0x000F0500
MT_ASM = """
    .set noreorder
    addiu $8, $zero, 0x15
    bne   $4, $8, mt_out
    lui   $8, 0x000F
    lw    $9, 0x4030($8)
    addiu $9, $9, 1
    sw    $9, 0x4030($8)
    sw    $31, 0x4034($8)
    lw    $9, 0($5)
    sw    $9, 0x4038($8)
    lw    $9, 4($5)
    sw    $9, 0x403c($8)
mt_out:
    addiu $sp, $sp, -0x30
    j     0x1028f0
    sd    $18, 0x10($sp)
"""

SP_CODE = 0x000F0600
SP_ASM = """
    .set noreorder
    lui   $24, 0x000F
    lw    $25, 0x4ffc($24)
    sltiu $1, $25, 0x1000
    beq   $1, $zero, sp_out
    addu  $1, $24, $25
    sw    $31, 0x5000($1)
    sw    $4, 0x5004($1)
    sw    $5, 0x5008($1)
    sw    $7, 0x500c($1)
    addiu $25, $25, 16
    sw    $25, 0x4ffc($24)
sp_out:
    addiu $sp, $sp, -0x40
    j     0x2bf798
    sd    $21, 0x28($sp)
"""

if __name__ == '__main__':
    dest = sys.argv[1] if len(sys.argv) > 1 else r'<local path>'
    release_layout = bool(os.environ.get('RELEASE_LAYOUT'))   # exact release blob + pad injection only
    text, _ = build.build(include_test=not release_layout)
    syms = build.build.syms
    lines = [text, '[Test Only - Pad Injection]']
    if os.environ.get('CMD_TRACE') and 'trace_cmd' in syms: lines.append('patch=1,EE,00329950,word,%08X' % syms['trace_cmd'])
    if os.environ.get('QS_SLOT'): lines.append('patch=1,EE,000FE0F4,word,%08X' % (0x51530000 | int(os.environ['QS_SLOT'])))
    if os.environ.get('QS_TEST'): lines.append('patch=1,EE,000FE0F0,word,%08X' % (0x51530000 | int(os.environ['QS_TEST'])))
    for i, w in enumerate(build.asm(INJ_ASM, INJ_CODE)):
        lines.append('patch=1,EE,%08X,word,%08X' % (INJ_CODE + 4 * i, w))
    lines.append('patch=1,EE,%08X,word,%08X' % (INJ_SITE, build.jal(INJ_CODE)))
    for site in (0x1c0244, 0x1c038c):   # arrows only; 0x1c00ec belongs to Prey Eyes
        if 'trace_draw438' in syms: lines.append('patch=1,EE,%08X,word,%08X' % (site, build.jal(syms['trace_draw438'])))
    if os.environ.get('TRACE438'):
        tw = build.asm(TR_ASM, TR_CODE)
        for i, w in enumerate(tw): lines.append('patch=1,EE,%08X,word,%08X' % (TR_CODE + 4 * i, w))
        lines.append('patch=1,EE,002BF438,word,%08X' % (0x08000000 | (TR_CODE >> 2)))
        lines.append('patch=1,EE,002BF43C,word,00000000')
    if os.environ.get('MODE_TRACE'):    # log requests for game mode 0x15 (Karma Terminal): count, caller, args
        lines = [chr(10).join(l for l in lines[0].split(chr(10)) if ',002266E' not in l)] + lines[1:]
        mw = build.asm(MT_ASM, MT_CODE)
        for i, w in enumerate(mw): lines.append('patch=1,EE,%08X,word,%08X' % (MT_CODE + 4 * i, w))
        lines.append('patch=1,EE,001028E8,word,%08X' % (0x08000000 | (MT_CODE >> 2)))
        lines.append('patch=1,EE,001028EC,word,00000000')
    if os.environ.get('SPRITE_TRACE'):    # log (ra, x, y, sheet) of 0x2bf790 sprite calls; reset 0xF4FFC to 0 to start
        src = SP_ASM
        if os.environ.get('SPRITE_TRACE') == 'alpha':
            src = src.replace('-0x40', '-0x80').replace('0x2bf798', '0x2bf4e8').replace('sd    $21, 0x28($sp)', 'sd    $22, 0x60($sp)')
        sw = build.asm(src, SP_CODE)
        for i, w in enumerate(sw): lines.append('patch=1,EE,%08X,word,%08X' % (SP_CODE + 4 * i, w))
        site = 0x2bf4e0 if os.environ.get('SPRITE_TRACE') == 'alpha' else 0x2bf790
        lines.append('patch=1,EE,%08X,word,%08X' % (site, 0x08000000 | (SP_CODE >> 2)))
        lines.append('patch=1,EE,%08X,word,00000000' % (site + 4))
    open(os.path.join(dest, build.PNACH), 'w').write('\n'.join(lines) + '\n')
    print('test pnach ->', dest)
