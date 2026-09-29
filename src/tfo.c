/* TwoForOne: N-for-1 mode (1..5, SELECT on the field cycles it, stored in the save).
 *   - random encounters: walked distance counts 1/N (0x11cb90 adds it to the encounter accumulator)
 *   - rewards from non-boss battles x N: EXP, atma (ep), macca, hunt bonus
 *   - item drops: chance x N; any part above 100% is the chance of an extra drop
 * Boss battles (the ones that forbid escape) are left alone. */
#include "game.h"

#define GP           0x003c0cf0u
#define FEATURES     ((volatile u32 *)0x000FD200)   /* [4] TwoForOne */
#define GBWK         RD32(0x003baa00u)
#define MODE_OFS     0x335F0                        /* spare byte at the end of the save block */
#define BATTLE       RD32(GP - 0x5a0c)
#define BTL_TABLE    RD32(GP - 0x62bc)              /* battle table, 0x28 per battle; +0 = no escape (boss) */

#define f_enc        ((u32 (*)(float, u32))0x0011cb90)
#define f_kill_total ((void (*)(u32))0x001a4328)
#define f_ep         ((u32 (*)(u32, u32))0x001a7ad8)
#define f_money      ((u32 (*)(u32, u32))0x001a7d38)
#define f_hunt_ep    ((u32 (*)(u32, u32))0x001a7df0)
#define f_add_item   ((void (*)(u32))0x001a4240)
#define f_rand100    ((u32 (*)(void))0x001ffcd8)    /* 0..99 */
#define f_item_ok    ((u32 (*)(u32))0x0021f600)

int tfo_mode(void)
{
    u32 g = GBWK;
    if (!FEATURES[4] || !g) return 1;
    int m = RD8(g + MODE_OFS);
    return m < 1 ? 1 : m > 5 ? 5 : m;
}

void tfo_set_mode(int m)
{
    u32 g = GBWK;
    if (g) RD8(g + MODE_OFS) = (u8)m;
}

static int boss_battle(void)
{
    u32 b = BATTLE;
    return b && RD8(RD32(b + 0x27c) * 0x28 + BTL_TABLE) != 0;
}

static int reward_mult(void)
{
    return boss_battle() ? 1 : tfo_mode();
}

/* encounter step (0x124d1c, 0x124d48, 0x216228) */
u32 tfo_enc(float dist, u32 area)
{
    int n = tfo_mode();
    return f_enc(n > 1 ? dist / (float)n : dist, area);
}

/* per-enemy battle totals (0x1d0338): EXP +0x2c8, atma +0x2cc, macca +0x2c0 */
void tfo_kill_total(u32 unit)
{
    u32 b = BATTLE;
    int n = reward_mult();
    if (!b || n == 1) { f_kill_total(unit); return; }
    u32 exp = RD32(b + 0x2c8), ep = RD32(b + 0x2cc), money = RD32(b + 0x2c0);
    f_kill_total(unit);
    RD32(b + 0x2c8) = exp + (RD32(b + 0x2c8) - exp) * n;
    RD32(b + 0x2cc) = ep + (RD32(b + 0x2cc) - ep) * n;
    RD32(b + 0x2c0) = money + (RD32(b + 0x2c0) - money) * n;
}

/* the killer's own atma and macca, and the hunt bonus (action resolver 0x1d12a0) */
u32 tfo_ep(u32 actor, u32 unit)      { return f_ep(actor, unit) * reward_mult(); }
u32 tfo_money(u32 actor, u32 unit)   { return f_money(actor, unit) * reward_mult(); }
u32 tfo_hunt_ep(u32 actor, u32 unit) { return f_hunt_ep(actor, unit) * reward_mult(); }

/* Drops for one chance: x n; each full 100% is a sure drop, the rest a roll for one more. */
static int drops(int chance, int n)
{
    int p = chance * n, k = 0;
    while (p >= 100) { k++; p -= 100; }
    if (p > 0 && (int)f_rand100() < p) k++;
    return k;
}

/* Replaces the drop roll 0x1a4130(unit, 0) at 0x1a44c4. Same order as the game: the conditional item
 * (species +0x42, chance +0x45, when 0x21f600 allows it), then the two regular items (+0x3e/+0x3f with
 * chances +0x40/+0x41); the first that drops wins. Returns one drop for the caller and adds the rest. */
#define f_drop_roll  ((u32 (*)(u32, u32))0x001a4130)
u32 tfo_drop(u32 unit, u32 mode)
{
    int n = reward_mult();
    if (n == 1 || mode == 1) return f_drop_roll(unit, mode);
    u32 sp = RD32(0x003baa1cu) + *(volatile u16 *)(unit + 0x124) * 0x4c;
    u32 item = 0; int k = 0;
    if (RD8(sp + 0x44) && *(volatile u16 *)(sp + 0x42) && f_item_ok(*(volatile u16 *)(sp + 0x42))) {
        k = drops(RD8(sp + 0x45), n);
        if (k) item = RD8(sp + 0x44);
    }
    for (int i = 0; i < 2 && !item; i++) {
        if (!RD8(sp + 0x3e + i)) continue;
        k = drops(RD8(sp + 0x40 + i), n);
        if (k) item = RD8(sp + 0x3e + i);
    }
    for (int i = 1; i < k; i++) f_add_item(item);
    return item;
}
