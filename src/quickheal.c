/* QuickHeal: inspecting a recovery terminal heals the party at once, with the heal sound and nothing on screen,
 * for the price the recovery menu would charge for everyone who needs it.
 *
 * A recovery terminal's field script ends in
 *     fade(0, 20)  wait(20)  facility(900)
 * (script commands 16, 14 and 527): fade to black, wait for it, then open the Life Terminal menu (event 900,
 * 0x13e5a8). The pnach points script command 16 here. When a fade to black is followed by exactly that wait and
 * facility(900), the party is healed and paid for and the script carries on after the facility call, so the
 * screen never fades and no menu opens. Every other fade runs the game's own command (0x10d908).
 *
 * Script code is an array of words: low half opcode (0x1d push, 0x08 call command), high half operand. The
 * interpreter (0x10c7f8) keeps its context at 0x3bd78c: +0x18 instruction index, +0xbc code.
 *
 * Members needing care and their cost are worked out as the recovery menu does (0x248d40 with the cost of
 * 0x248658); each is healed with the game's own routine (0x24a2b8: HP, MP, ailments). If the money doesn't
 * cover everyone, members are healed in party order while it lasts. */
#include "game.h"

#define FEATURES      ((volatile u32 *)0x000FD200)   /* [13] QuickHeal */
#define GBWK          RD32(0x003baa00u)              /* +0x3c money; party records at +0xa60, 0x1a4 apart */
#define SE_HEAL       0x10
#define SE_NONE       10                             /* menu cancel: nothing to heal or no money */

#define f_cost        ((int (*)(u32))0x00248658)
#define f_heal        ((void (*)(u32))0x0024a2b8)
#define f_money_add   ((void (*)(int))0x001198b8)
#define f_se          ((void (*)(u32, int, int))0x002e8f78)
#define f_script_arg  ((int (*)(int))0x0010d428)
#define f_cmd_frames  ((int (*)(void))0x0010d680)     /* frames since the current command started */
#define f_fade        ((int (*)(void))0x0010d908)     /* script command 16: fade(to, frames) */
#define f_enc_reset   ((void (*)(void))0x0011ce18)     /* the terminal restarts the encounter gauge */
#define f_field_on    ((void (*)(u32))0x00125dd0)     /* sets field-enable bits (gp-0x60f0) */
#define FIELD_PLAYER  0x40                            /* cleared by the event (script command 96) */
#define SCRIPT        RD32(0x003bd78c)
#define OP_PUSH       0x1d
#define OP_CALL       0x08
#define CMD_WAIT      14
#define CMD_FACILITY  527
#define EV_RECOVERY   900

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

static u32 op(u32 code, u32 i, u32 opc, int arg)
{
    u32 w = RD32(code + i * 4);
    return (w & 0xffff) == opc && (arg < 0 || (int)(short)(w >> 16) == arg);
}

/* Replaces script command 16 (fade) in the command table. */
int qh_fade(void)
{
    u32 ctx = SCRIPT, pc = RD32(ctx + 0x18), code = RD32(ctx + 0xbc);
    if (FEATURES[13] && f_cmd_frames() == 0 && f_script_arg(0) == 0
        && op(code, pc + 1, OP_PUSH, -1) && op(code, pc + 2, OP_CALL, CMD_WAIT)
        && op(code, pc + 3, OP_PUSH, EV_RECOVERY) && op(code, pc + 4, OP_CALL, CMD_FACILITY)) {
        heal_party();
        f_enc_reset();
        f_field_on(FIELD_PLAYER);                 /* the terminal gives control back by restarting the field */
        *(volatile u32 *)(ctx + 0x18) = pc + 5;   /* the interpreter pops the fade's arguments and goes on from here */
        return 1;
    }
    return f_fade();
}
