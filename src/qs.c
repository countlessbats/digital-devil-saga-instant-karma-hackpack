/* QuickStart: one press during the logos/intro movie skips all of it and lands on the main menu.
 * START (instead of any other button) also picks Load and loads the most recent save.
 *
 * titleProc (0x26b1f0, work at gp-0x4720) runs the boot sequence as states: 5..0xc logos, 0xd..0x15 the
 * intro movie, 0x16..0x18 title set-up, 0x19 "press start", 0x1e the main menu, 0x1f leaving the menu
 * (menu value 0 = Load: starts file process 3). Its task function is pointed here by patching the
 * constant loaded at 0x26b11c/0x26b12c when the task is created. */
#include "game.h"

#define GP           0x003c0cf0u
#define FEATURES     ((volatile u32 *)0x000FD200)   /* [5] QuickStart */
#define TW           RD32(GP - 0x4720)              /* titleProc work */
#define ST(w)        RD32((w) + 0x10)
#define CNT(w)       RD32((w) + 0x14)
#define QS_PAD_START    RD8(0x0032453cu)               /* this task's pad copy: START, 0x80 = pressed */

#define f_title      ((u32 (*)(u32))0x0026b1f0)
#define f_any_press  ((u32 (*)(void))0x0026bf38)
#define f_movie_stop ((void (*)(void))0x00270030)
#define f_fade_in    ((void (*)(int, int, int, int))0x00105ae0)
#define f_cursor     ((void (*)(int))0x0026bee8)    /* main menu cursor to item n */
#define f_blink_end  ((void (*)(void))0x0026d270)   /* "press start" clean-up */
#define f_menu_open  ((void (*)(void))0x0026d648)
#define f_se         ((void (*)(u32, int, int))0x002e8f78)

static int skipping;     /* 1: jumped past the intro, heading for the menu */
int qs_autoload;         /* 1: START was the skip press: choose Load at the menu (and the latest save) */

/* ---- autoload: find the newest save and load it ------------------------------------------------ */
/* Saves are memory card folders BASLUS-20974-new-N (N = list row 0..9). One sceMcGetDir on the wildcard
 * gives every folder's modify time; the newest is picked (falling back to the longest play time from
 * the load list's summaries when the card has no dates). On the load list (fileMan, cursor gp-0x44ac,
 * top row gp-0x44b0) the cursor is set and X pressed; on the Yes/No prompt (cursor gp-0x44c0, 0 = Yes)
 * Yes is chosen and X pressed. Presses are written into the pad data right after the pad processor. */
#define f_mc_getdir  ((int (*)(int, int, const char *, int, int, void *))0x002f6b80)
#define f_mc_sync    ((int (*)(int, int *, int *))0x002f6858)
#define f_padproc    ((void (*)(void))0x00103b10)
#define LIST_CURSOR  RD32(GP - 0x44ac)
#define LIST_TOP     RD32(GP - 0x44b0)
#define LIST_CONFIRM RD32(GP - 0x44c4)
#define YN_CURSOR    RD32(GP - 0x44c0)
#define YN_RESULT    RD32(GP - 0x44b8)
#define YN_YES_FN    RD32(GP - 0x33e8)              /* callback for Yes, set when the prompt opens */
#define MSG_WAITING  RD32(GP - 0x44e0)              /* a message box is waiting for a key */
#define LIST_REPEAT  RD32(0x0037d4a8u)              /* list's key-repeat counter, +1 per list frame */
#define SUMMARY(i)   (0x003dc800u + (i) * 0x30)     /* load list summaries: +0 'VER', +8 play time */

enum { AL_OFF, AL_WANT_DIR, AL_DIR, AL_READY, AL_LIST, AL_PROMPT, AL_MSG, AL_DONE };
static int al_phase, al_target, al_timer, al_wait;   /* al_target is set by autoload_start before use */
static u8 dirtbl[16 * 64] __attribute__((aligned(64)));
static const char dir_pattern[] = "/BASLUS-20974-new-*";

extern int hold_black;
extern u32 field_control_frame;
static void autoload_start(void) { al_phase = AL_WANT_DIR; al_timer = 0; al_target = -1; hold_black |= 2; }
static void autoload_end(void)
{
    hold_black &= ~2;
}


static void pick_newest(int n)
{
    u32 best_d = 0, best_t = 0;
    al_target = -1;
    for (int i = 0; i < n && i < 16; i++) {
        const u8 *e = dirtbl + i * 64;
        const char *name = (const char *)e + 0x20;
        int k = 0;
        while (name[k] && k < 32) k++;
        if (k < 18 || name[k - 1] < '0' || name[k - 1] > '9') continue;
        int num = name[k - 1] - '0';
        if (k >= 2 && name[k - 2] >= '0' && name[k - 2] <= '9') num += (name[k - 2] - '0') * 10;
        const u8 *m = e + 8;                           /* _Modify: resv, sec, min, hour, day, month, year */
        u32 d = (u32)(m[6] | (m[7] << 8)) << 16 | (u32)m[5] << 8 | m[4];
        u32 t = (u32)m[3] << 16 | (u32)m[2] << 8 | m[1];
        if (al_target < 0 || d > best_d || (d == best_d && t > best_t)) { al_target = num; best_d = d; best_t = t; }
    }
    if (best_d == 0) al_target = -2;                   /* no dates: decide from play time on the list */
    if ((RD32(0xFE0F4) & 0xffff0000u) == 0x51530000u) al_target = RD32(0xFE0F4) & 0xff;   /* test only */
}

static void press_x(void)
{
    RD8(0x003244d1u) = 0x83;                          /* pad 0 X: held + newly pressed */
    RD8(0x003244f1u) = 0x83;
}

/* Replaces the main loop's pad processing call (0x103b10) at 0x1006a4. */
void skip_frame(void);
void chest_frame(void);
void qs_pad(void)
{
    f_padproc();
    skip_frame();
    chest_frame();
    if (!FEATURES[5]) return;
    if (al_phase == AL_OFF || al_phase == AL_DONE) {
        if (hold_black & 2) {                          /* keep black until the game is up (or we gave up) */
            if (al_phase == AL_OFF || field_control_frame + 2 >= RD32(0x003ba700u) || ++al_wait > 60 * 20) autoload_end();
        }
        return;
    }
    if (++al_timer > 60 * 30) { al_phase = AL_OFF; return; }  /* give up after ~30 s: normal load list */
    switch (al_phase) {
    case AL_WANT_DIR:
        if (f_mc_getdir(0, 0, dir_pattern, 0, 16, dirtbl) == 0) al_phase = AL_DIR;
        break;
    case AL_DIR: {
        int cmd = 0, res = 0;
        if (f_mc_sync(1, &cmd, &res) == 1) {
            if (res > 0) pick_newest(res); else al_target = -3;
            al_phase = al_target == -3 ? AL_OFF : AL_READY;
        }
        break;
    }
    case AL_READY:                                    /* wait for the load list to run */
        if (LIST_REPEAT != 0) {                       /* it ran since we zeroed it */
            int t = al_target;
            if (t == -2) {                             /* longest play time */
                u32 best = 0;
                for (int i = 0; i < 10; i++)
                    if (RD32(SUMMARY(i)) == 0x03524556u && RD32(SUMMARY(i) + 8) > best) { best = RD32(SUMMARY(i) + 8); t = i; }
            }
            if (t < 0 || t > 9) { al_phase = AL_OFF; break; }
            int top = t - 1 < 0 ? 0 : t - 1 > 7 ? 7 : t - 1;
            LIST_TOP = top; LIST_CURSOR = t;
            al_phase = AL_LIST;
        } else {
            LIST_REPEAT = 0;
        }
        break;
    case AL_LIST:                                     /* press X until the list takes it */
        if (LIST_CONFIRM == 1 || YN_YES_FN) { al_phase = AL_PROMPT; break; }
        press_x();
        break;
    case AL_PROMPT:                                   /* Yes on "load this file?" */
        if (YN_RESULT != 0) { al_phase = AL_MSG; al_wait = 0; break; }
        if (YN_YES_FN) { YN_CURSOR = 0; press_x(); }
        break;
    case AL_MSG:                                      /* dismiss "Load successful." */
        if (MSG_WAITING) { if (++al_wait > 2) press_x(); }
        else if (al_wait) { al_phase = AL_DONE; al_wait = 0; }
        break;
    }
}

int qs_autoload_ready(void) { return al_phase >= AL_READY || al_phase == AL_OFF; }

/* Test only: the test pnach sets 0xFE0F0 = 0x5153000b (b = pad bits: 1 X, 2 START) to press that button
 * once at the first logo through the test pad injection (0xF000C). Never set by release sections. */
static int test_frames;
static void test_script(u32 w)
{
    u32 t = RD32(0xFE0F0);
    if ((t & 0xffff0000u) != 0x51530000u) return;
    volatile u16 *inj = (volatile u16 *)0x000F000Cu;
    if (ST(w) == 6 && test_frames < 6) { *inj = (t & 1) ? 0x40 : 0x800; test_frames++; }
    else if (test_frames) *inj = 0;
}

u32 qs_title(u32 task)
{
    u32 w = TW;
    if (w) test_script(w);
    if (FEATURES[5] && w) {
        u32 st = ST(w);
        if (st >= 5 && st <= 0x15 && !skipping && f_any_press()) {
            qs_autoload = (QS_PAD_START & 0x80) != 0;
            if (qs_autoload) autoload_start();
            f_movie_stop();
            f_fade_in(0, 0, 0, 0x14);
            ST(w) = 0x16; CNT(w) = 0;
            skipping = 1;
        } else if (st == 0x19 && skipping) {
            /* do what a press does on "press start": open the main menu */
            f_se(8, 0x7f, 0x3f);
            f_cursor(0);
            f_blink_end();
            ST(w) = 0x1e; CNT(w) = 0;
            f_menu_open();
            skipping = 0;
            return 0;
        } else if (st == 0x1e && qs_autoload && RD32(w + 0x34) == 2 && qs_autoload_ready()) {
            /* menu ready for input: confirm Load (item 0) the way the menu's own confirm does */
            f_cursor(0);
            RD32(w + 0x34) = 3; CNT(w) = 0;
            f_se(8, 0x7f, 0x3f);
            qs_autoload = 0;
        }
    }
    return f_title(task);
}
