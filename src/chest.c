/* OpenChests: inspecting a chest opens it at once. The chest script's question ("A strange object lies on
 * the floor. Touch it?") and its Yes/No are answered inside the script, never shown; the "Obtained ..."
 * message stays for you.
 *
 * Field/event scripts call native commands through a table at 0x39e288 ({handler, arg count} pairs, run by
 * the VM opcode at 0x10c7f8; a handler returns 0 to wait, 1 when done). The chest script does
 *   command 0 (MSG, 0x19ad50): show the question and wait for a press
 *   command 3 (SELECT, 0x19b108): show Yes/No, store the chosen row (0 = Yes) in a script variable
 * The section points those two table entries here. MSG runs as normal; if the message it just started is a
 * chest question (message name TAKARA, or HOSEKI for floating jewels; window+0x2c points at the name) the
 * window is closed again in the same step, before anything is drawn, and the command reports done. The
 * next SELECT in that window stores Yes and reports done without showing the choice, and that script's
 * WAIT commands (command 0xe, the pause for the lid animation) finish at once until it shows its next
 * message ("Obtained ..."), so the item comes straight away. */
#include "game.h"

#define FEATURES      ((volatile u32 *)0x000FD200)   /* [7] OpenChests */
#define WINDOW(i)     RD32(0x003d6eccu + (i) * 0x14)

#define f_cmd_msg     ((u32 (*)(void))0x0019ad50)
#define f_cmd_select  ((u32 (*)(void))0x0019b108)
#define f_script_win  ((int (*)(void))0x0010d690)    /* message window of the running script */
#define f_script_arg  ((u32 (*)(int))0x0010d428)     /* argument n of the current command */
#define f_script_set  ((void (*)(u32, u32))0x0010d5f0)  /* store value into script variable */
#define f_msg_close   ((void (*)(int))0x0019b4a0)    /* close a message window (as the close command) */

static int pending_win;  /* window + 1 whose chest question was skipped; its SELECT answers Yes */
static u32 chest_ctx;    /* script running that chest: its waits are skipped until it shows "Obtained" */
#define SCRIPT_CTX    RD32(0x003bd78cu)             /* script context of the command being run */
#define f_cmd_wait    ((u32 (*)(void))0x0010d7c0)    /* command 0xe: wait n frames */

static int is_question(u32 name)
{
    if (name < 0x100000u || name >= 0x2000000u) return 0;
    const char *n = (const char *)name;
    int takara = n[0] == 'T' && n[1] == 'A' && n[2] == 'K' && n[3] == 'A' && n[4] == 'R' && n[5] == 'A' && n[6] == 0;
    int hoseki = n[0] == 'H' && n[1] == 'O' && n[2] == 'S' && n[3] == 'E' && n[4] == 'K' && n[5] == 'I' && n[6] == 0;
    return takara || hoseki;
}

u32 chest_cmd_msg(void)
{
    u32 r = f_cmd_msg();
    if (!FEATURES[7] || r != 0) return r;
    int win = f_script_win();
    if (win < 0 || win >= 32) return r;
    u32 w = WINDOW(win);
    if (!w || !is_question(RD32(w + 0x2c))) {
        if (SCRIPT_CTX == chest_ctx) chest_ctx = 0;    /* "Obtained ...": the chest's own pacing is over */
        return r;
    }
    f_msg_close(win);                                  /* never drawn: closed in the step that opened it */
    pending_win = win + 1;
    chest_ctx = SCRIPT_CTX;
    return 1;
}

/* command 0xe (wait n frames): the chest script waits ~1.5 s for the lid before giving the item; skipped */
u32 chest_cmd_wait(void)
{
    if (FEATURES[7] && chest_ctx && SCRIPT_CTX == chest_ctx) return 1;
    return f_cmd_wait();
}

u32 chest_cmd_select(void)
{
    if (FEATURES[7] && pending_win) {
        int win = f_script_win();
        if (win + 1 == pending_win) {
            pending_win = 0;
            f_script_set(0, f_script_arg(0));          /* row 0 = Yes */
            return 1;
        }
        pending_win = 0;
    }
    return f_cmd_select();
}

/* no per-frame work any more (kept so the shared pad hook's call stays valid) */
void chest_frame(void) {}
