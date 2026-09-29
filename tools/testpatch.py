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
    lhu   $9, 0x0c($8)
    lui   $10, 0x0040
    lhu   $11, -0x64fa($10)       # 0x3f9b06 pad0 raw held
    sh    $11, 8($sp)
    or    $11, $11, $9
    jal   0x2e39c8
    sh    $11, -0x64fa($10)
    lhu   $11, 8($sp)             # restore raw value
    lui   $10, 0x0040
    sh    $11, -0x64fa($10)
    ld    $ra, 0($sp)
    jr    $ra
    addiu $sp, $sp, 0x10
"""

if __name__ == '__main__':
    dest = sys.argv[1] if len(sys.argv) > 1 else r'<local path>'
    text, _ = build.build(include_test=True)
    syms = build.build.syms
    lines = [text, '[Test Only - Pad Injection]']
    for i, w in enumerate(build.asm(INJ_ASM, INJ_CODE)):
        lines.append('patch=1,EE,%08X,word,%08X' % (INJ_CODE + 4 * i, w))
    lines.append('patch=1,EE,%08X,word,%08X' % (INJ_SITE, build.jal(INJ_CODE)))
    for site in (0x1c0244, 0x1c038c):   # arrows only; 0x1c00ec belongs to Prey Eyes
        lines.append('patch=1,EE,%08X,word,%08X' % (site, build.jal(syms['trace_draw438'])))
    open(os.path.join(dest, build.PNACH), 'w').write('\n'.join(lines) + '\n')
    print('test pnach ->', dest)
