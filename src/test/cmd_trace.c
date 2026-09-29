/* Test only: logs field/event script native command ids (VM opcode 0x10c7f8) into a ring at 0xFE400:
 * [0] count, then (id, pc, ctx) triples. Hooked by pointing the opcode table entry 0x329950 here. */
#include "../game.h"
#define f_cmd ((u32 (*)(u32))0x0010c7f8)
u32 trace_cmd(u32 ctx)
{
    volatile u32 *r = (volatile u32 *)0xFE400;
    u32 pc = RD32(ctx + 0x18);
    short id = *(volatile short *)(RD32(ctx + 0xbc) + pc * 4 + 2);
    u32 res = f_cmd(ctx);
    u32 n = r[0];
    if (n) {                                   /* collapse repeats of the same waiting command */
        u32 j = (n - 1) % 80;
        if ((r[1 + j * 3] & 0xffff) == (u32)(u16)id && r[2 + j * 3] == pc) { r[1 + j * 3] = (u32)(u16)id | (res << 16); return res; }
    }
    u32 i = n % 80;
    r[1 + i * 3] = (u32)(u16)id | (res << 16);
    r[2 + i * 3] = pc;
    r[3 + i * 3] = ctx;
    r[0] = n + 1;
    return res;
}
