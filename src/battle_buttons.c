/* BattleButtons: in the battle command menu, R1 = Pass and L1 = Retreat, with no confirmation.
 * Works by driving the game's own menu: switch to the right tab, put the cursor on the entry,
 * press X, then press X again on the target step. Also hosts the shared target-panel input hook
 * (Prey Eyes learning uses it too); each patch section switches its part on via FEATURE flags. */
#include "game.h"

#define GP           0x003c0cf0u
#define FEATURES     ((volatile u32 *)0x000FD200)   /* [0] Prey Eyes, [1] BattleButtons */
#define PAD          0x00324530u                    /* this task's menu pad copy, 16 bytes */
enum { B_X = 1, B_LEFT = 4, B_RIGHT = 5, B_UP = 6, B_L1 = 8, B_R1 = 10 };
#define TABP         RD32(GP - 0x5910)              /* byte +1 = current command tab */
#define TAB_FIGHT    0
#define TAB_ESCAPE   5

#define f_task_by_id ((u32 (*)(u32))0x00101858)
#define f_cmd_pre    ((void (*)(void))0x001b5cd8)

static int st, goal, timer, steps;
static u32 confirm_at;             /* logic frame of our X press; the next target step within 90 frames is auto-confirmed */
#define FRAME RD32(0x003ba700u)
enum { IDLE, NAV, UP, PRESS };

static void press(int b, u8 bits) { RD8(PAD + b) = bits; }

static u32 command_work(void)
{
    u32 t = f_task_by_id(RD32(GP - 0x594c));        /* gp-0x594c: command panel task id */
    return t ? fn_task_work(t) : 0;
}

/* Replaces the command panel's first call (0x1b5cd8) at 0x1bf13c; runs before the panel reads
 * input each frame. */
void bb_command_hook(void)
{
    f_cmd_pre();
    if (!FEATURES[1]) return;
    u32 w = command_work();
    if (!w || RD32(w) != 2) { st = IDLE; return; }          /* 2 = waiting for a command */
    u8 l1 = RD8(PAD + B_L1), r1 = RD8(PAD + B_R1);
    if (st == IDLE) {
        if (r1 & 0x80) { st = NAV; goal = TAB_FIGHT; }
        else if (l1 & 0x80) { st = NAV; goal = TAB_ESCAPE; }
        else return;
        timer = 0; steps = 0;
        press(B_L1, 0); press(B_R1, 0);
    }
    if (timer > 0) { timer--; return; }
    u32 tp = TABP;
    int tab = tp ? RD8(tp + 1) : -1;
    switch (st) {
    case NAV:
        if (tab != goal) {
            if (++steps > 8) { st = IDLE; return; }       /* tab not available (e.g. no escape) */
            press(goal == TAB_ESCAPE ? B_LEFT : B_RIGHT, 0x82);
            timer = 8;
            return;
        }
        if (goal == TAB_FIGHT) {
            *(volatile u16 *)(w + 4) = 0;                 /* FIGHT list: top and cursor to Attack */
            *(volatile u16 *)(w + 6) = 0;
            st = UP; timer = 10;
        } else {
            st = PRESS; timer = 12;               /* let the tab switch settle */
        }
        return;
    case UP:                                              /* from the first entry, up wraps to Pass */
        press(B_UP, 0x82);
        st = PRESS; timer = 4;
        return;
    case PRESS:
        if (tab != goal) { st = NAV; return; }
        press(B_X, 0x81);
        confirm_at = FRAME | 1;
        st = IDLE;
        return;
    }
}

/* Replaces the target panel's input call at 0x1c13a0 (shared by Prey Eyes and BattleButtons). */
#define f_target_input ((void (*)(u32, u32, int))0x001c08b8)
void prey_after_target_input(u32 work, int confirm);
void bb_target_input(u32 battle, u32 work, int mode)
{
    if (confirm_at) {
        if (FEATURES[1] && FRAME - confirm_at < 90) RD8(PAD + B_X) = 0x81;
        confirm_at = 0;
    }
    int confirm = (RD8(PAD + B_X) & 0x80) != 0;
    f_target_input(battle, work, mode);
    if (FEATURES[0]) prey_after_target_input(work, confirm);
}
