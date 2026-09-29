/* BattleButtons: in the battle command menu, R1 = Pass and L1 = Retreat, committed directly
 * with no menu shown; the right stick jumps a page. Also hosts the shared target-panel input hook
 * (Prey Eyes learning uses it too); each patch section switches its part on via FEATURE flags. */
#include "game.h"

#define GP           0x003c0cf0u
#define FEATURES     ((volatile u32 *)0x000FD200)   /* [0] Prey Eyes, [1] BattleButtons */
#define PAD          0x00324530u                    /* this task's menu pad copy, 16 bytes */
enum { B_X = 1, B_L1 = 8, B_R1 = 10 };
#define TABP         RD32(GP - 0x5910)              /* byte +1 = current command tab */

#define f_task_by_id ((u32 (*)(u32))0x00101858)
#define f_cmd_pre    ((void (*)(void))0x001b5cd8)
#define f_list_kind  ((int (*)(u32, int))0x001bd0d0)         /* tab -> list kind */
#define f_list_count ((int (*)(u32, int, int))0x001bd190)    /* entries in the tab's list */
#define f_cmd_commit ((void (*)(u32, int, int, int))0x001b0d70)
#define f_menu_se    ((void (*)(int))0x001f3238)             /* 0 move, 8 confirm, 10 buzzer */
#define f_kind4_exit ((void (*)(u32, int, int))0x001b83d8)

static u32 confirm_at;             /* logic frame of an injected X; the next target step within 90 frames is auto-confirmed */
#define FRAME RD32(0x003ba700u)
static int stick_prev;
#define RAW_RSTICK_V RD8(0x003f9b11u)   /* raw pad: +0x10 right h, +0x11 right v (0x80 = centre) */

static void press(int b, u8 bits) { RD8(PAD + b) = bits; }

static u32 command_work(void)
{
    u32 t = f_task_by_id(RD32(GP - 0x594c));        /* gp-0x594c: command panel task id */
    return t ? fn_task_work(t) : 0;
}

/* Commit an action the way the command menu's own confirm (0x1bd750) does: action type into
 * the action record (work+0x28), panel state 3 (done), confirm sound. No tab switch, no list. */
static void commit(u32 w, int kind, int type, int arg)
{
    if (kind == 4) f_kind4_exit(RD32(w + 0x2c), 1, 0);
    RD32(RD32(w + 0x28)) = type;
    f_cmd_commit(w, type == 10 ? 10 : 0, arg, 0);
    RD32(w) = 3;
    f_menu_se(8);
}

/* Replaces the command panel's first call (0x1b5cd8) at 0x1bf134; runs before the panel reads
 * input each frame. */
void bb_command_hook(void)
{
    f_cmd_pre();
    if (!FEATURES[1]) return;
    u32 w = command_work();
    if (!w || RD32(w) != 2) return;                         /* 2 = waiting for a command */
    u8 l1 = RD8(PAD + B_L1), r1 = RD8(PAD + B_R1);
    press(B_L1, 0); press(B_R1, 0);                         /* L1/R1 are ours in this menu */
    u32 tp = TABP;
    if (!tp) return;
    int tab = RD8(tp + 1), kind = f_list_kind(w, tab);
    if (r1 & 0x80) {                                        /* Pass: action type 10 */
        commit(w, kind, 10, f_list_count(w, 0, 1) - 1);
        return;
    }
    if (l1 & 0x80) {                                        /* Escape: action type 6 (5 is Revert) */
        u32 bt = RD32(GP - 0x5a0c);                         /* battles that forbid escape: flag in the battle table */
        if (!bt || RD8(RD32(bt + 0x27c) * 0x28 + RD32(GP - 0x62bc))) { f_menu_se(10); return; }
        commit(w, kind, 6, 0);
        return;
    }
    /* right stick: jump a page (4 entries) up/down, no wrap; edge-triggered */
    int rv = RAW_RSTICK_V, sdir = rv > 0xc0 ? 1 : rv < 0x40 ? -1 : 0;
    if (sdir && !stick_prev) {
        volatile short *top = (volatile short *)(w + 4 + kind * 4), *cur = top + 1;
        int n = f_list_count(w, tab, 1), c = *cur, t = *top;
        int nc = c + sdir * 4;
        if (nc > n - 1) nc = n - 1;
        if (nc < 0) nc = 0;
        if (nc != c) {
            int nt = t + (nc - c);
            if (nt > n - 4) nt = n - 4;
            if (nt < 0) nt = 0;
            if (nc < nt) nt = nc;
            if (nc > nt + 3) nt = nc - 3;
            *top = nt; *cur = nc;
            f_menu_se(0);
        }
    }
    stick_prev = sdir;
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
