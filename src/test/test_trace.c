/* TEST ONLY: logs sprite draws made through redirected call sites (see testpatch.py). */
#include "../game.h"
#define TRACE ((volatile u32 *)0x000FE000)   /* [0]=count, then 5 words per entry */
#define f_draw438 ((void (*)(int, int, int, u32, int, u32, int, int))0x002bf438)

void trace_draw438(int x, int y, int z, u32 cols, int flags, u32 sheet, int spr, int prio)
{
    u32 n = TRACE[0];
    if (TRACE[1023] == 0x7ACE && n < 150) {
        volatile u32 *e = TRACE + 2 + n * 6;
        e[0] = (u32)x; e[1] = (u32)y; e[2] = sheet; e[3] = (u32)spr; e[4] = (u32)flags;
        e[5] = cols ? RD32(cols) : 0;
        TRACE[0] = n + 1;
    }
    if (spr < 64 && ((TRACE[1020] >> (spr - 0x18)) & 1) && spr >= 0x18) return;
    f_draw438(x, y, z, cols, flags, sheet, spr, prio);
}
