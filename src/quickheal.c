/* QuickHeal: inspecting a recovery terminal heals the party at once, with the heal sound and no menu, for the
 * price the recovery menu would charge for everyone who needs it.
 *
 * Karma Terminals open through 0x249fa8 (mode, arg): mode 0 is a Large Terminal, 1 a Small one and 2 a
 * recovery terminal (its menu has only Recover and Exit, option table 0x36ac80). The pnach turns the first
 * instruction of 0x249fa8 into a jump here. For a recovery terminal the party is healed and paid for, and the
 * terminal reports itself closed right away (gp-0x4910 = 2, which the field polls); every other terminal
 * opens exactly as the original does.
 *
 * Members needing care and their cost are worked out as the recovery menu does (0x248d40 with the cost of
 * 0x248658); each is healed with the game's own routine (0x24a2b8: HP, MP, ailments). If the money doesn't
 * cover everyone, members are healed in party order while it lasts. */
#include "game.h"

#define GP            0x003c0cf0u
#define FEATURES      ((volatile u32 *)0x000FD200)   /* [13] QuickHeal */
#define GBWK          RD32(0x003baa00u)              /* +0x3c money; party records at +0xa60, 0x1a4 apart */
#define TERM_STATE    (*(volatile u8 *)(GP - 0x4910))   /* 1 open, 2 closed (polled by the field) */
#define TERM_TASK     RD32(GP - 0x490c)
#define MODE_RECOVERY 2
#define SE_HEAL       0x10
#define SE_NONE       10                             /* menu cancel: nothing to heal or no money */

#define f_cost        ((int (*)(u32))0x00248658)
#define f_heal        ((void (*)(u32))0x0024a2b8)
#define f_money_add   ((void (*)(int))0x001198b8)
#define f_se          ((void (*)(u32, int, int))0x002e8f78)
#define f_term_work   ((u32 (*)(u32, u32))0x00249e20)
#define f_task_new    ((u32 (*)(u32, u32, u32, u32, u32, u32, u32))0x00101570)

static u32 member(int i) { return GBWK + i * 0x1a4 + 0xa60; }

static void heal_party(void)
{
    int cost[5], total = 0;
    for (int i = 0; i < 5; i++) {
        u32 m = member(i);
        cost[i] = (*(volatile u16 *)m & 1) ? f_cost(m) : 0;
        total += cost[i];
    }
    int money = (int)RD32(GBWK + 0x3c), paid = 0, healed = 0;
    for (int i = 0; i < 5; i++) {
        if (!cost[i]) continue;
        if (total > money && paid + cost[i] > money) continue;   /* can't pay for everyone: as many as we can */
        f_heal(member(i));
        paid += cost[i];
        healed++;
    }
    if (!healed) { f_se(SE_NONE, 0x7f, 0x3f); return; }
    f_money_add(-paid);
    f_se(SE_HEAL, 0x7f, 0x3f);
}

void qh_term_open(u32 mode, u32 arg)
{
    if (mode == MODE_RECOVERY && FEATURES[13]) {
        heal_party();
        TERM_STATE = 2;
        return;
    }
    /* the original 0x249fa8 */
    u32 w = f_term_work(mode, arg);
    TERM_TASK = f_task_new(0x003af658u, 0x404, 1, 1, 0x0024a0d8u, 0, w);
    f_task_new(0x003af668u, 0x2b14, 1, 1, 0x0024a138u, 0, w);
    f_task_new(0x003af678u, 0x5210, 1, 1, 0x0024a170u, 0x00249f08u, w);
    TERM_STATE = 1;
}
