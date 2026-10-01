/* Field toggles, active only while walking around (not in menus, battles or events):
 *   SubtleKarma: d-pad UP toggles random encounters, with a system sound and a message.
 *   SunKing:     d-pad DOWN switches solar noise between MAX and MIN.
 *   TwoForOne:   SELECT cycles 1-for-1 .. 5-for-1 (tfo.c).
 *   BadKarma:    d-pad RIGHT starts a random encounter now; GoodKarma: d-pad LEFT starts a rare Omoikane fight
 *                (both through the encounter roll in tfo.c, so TwoForOne's reward bonus applies).
 * Both run from the field's player-control step (call to 0x1249e0 at 0x125980), which the game
 * only reaches while the player has control. */
#include "game.h"

int tfo_mode(void);
void tfo_set_mode(int m);

#define GP           0x003c0cf0u
#define FEATURES     ((volatile u32 *)0x000FD200)   /* [2] SubtleKarma, [3] SunKing, [4] TwoForOne, [9] BadKarma, [10] GoodKarma */
#define GBWK         RD32(0x003baa00u)
#define BATTLE       RD32(GP - 0x5a0c)
#define CAMP_STATE   RD8(0x003bc6b4u)
#define RAW_HELD     (*(volatile u16 *)0x003f9b06u)  /* pad 0 raw buttons (d-pad only, no stick) */
#define BTN_UP       0x1000
#define BTN_DOWN     0x4000
#define BTN_SELECT   0x0100
#define BTN_RIGHT    0x2000
#define BTN_LEFT     0x8000
extern int karma_force;     /* tfo.c: 1 = force an encounter on the next roll, 2 = an Omoikane one */
extern int karma_none;      /* tfo.c: the forced roll found no encounters in this area */

#define f_field_ctl  ((u32 (*)(void))0x001249e0)
#define f_se         ((void (*)(u32, int, int))0x002e8f78)
#define f_set_phase  ((void (*)(u32))0x002286f8)
#define f_text       ((u32 (*)(int, int, int, u32, const char *, int))0x00197760)
#define f_textprep   ((void (*)(u32, int, int))0x001958a0)
#define f_textsubmit ((void (*)(u32))0x00194920)
#define f_fillquad   ((void (*)(int, int, int, int, int, int, int))0x002c0dd8)
#define f_text_w     ((int (*)(u32))0x00195c88)
#define f_txt_pos    ((void (*)(u32, int, int))0x00195450)

#define SE_ON  9      /* battle menu confirm */
#define SE_OFF 10     /* battle menu cancel */

static u8 enc_off, inited;
static u16 prev_held;
static int msg_timer;
static const char *msg;

static void show(const char *m) { msg = m; msg_timer = 120; }

static void draw_msg(void)
{
    if (msg_timer <= 0 || !msg) return;
    msg_timer--;
    int a = msg_timer > 100 ? (120 - msg_timer) * 6 : msg_timer >= 20 ? 0x80 : msg_timer * 6;
    if (a > 0x80) a = 0x80;
    u32 t = f_text(0, 0x180, 0, 0xa09dc300u | (u32)a, msg, 0);
    int w = f_text_w(t) * 16;
    f_txt_pos(t, 4096 - w / 2, 0x180);                /* centre now that the width is known */
    f_fillquad(4096 - w / 2 - 0x100, 0x160, 0, w + 0x200, 0x100, (int)(0x00000000u | (u32)(a * 3 / 4)), 0x53);
    f_textprep(t, 1, 0x54);
    f_textsubmit(t);
}

u32 field_control_frame;   /* logic frame of the last player-control step (read by SceneSkip) */

u32 field_hook(void)
{
    field_control_frame = RD32(0x003ba700u);
    u16 held = RAW_HELD;
    u16 edge = held & ~prev_held;
    prev_held = held;
    if (!inited) { inited = 1; edge = 0; }
    int ok = !BATTLE && CAMP_STATE == 0;

    if (ok && FEATURES[2] && (edge & BTN_UP)) {
        enc_off ^= 1;
        f_se(enc_off ? SE_ON : SE_OFF, 0x7f, 0x3f);
        show(enc_off ? "Encounters OFF" : "Encounters ON");
    }
    if (ok && FEATURES[3] && (edge & BTN_DOWN)) {
        int full = RD8(GBWK + 0xa41) == 8;
        f_set_phase(full ? 0 : 8);
        f_se(full ? SE_ON : SE_OFF, 0x7f, 0x3f);
    }
    if (ok && FEATURES[4] && (edge & BTN_SELECT)) {       /* TwoForOne: cycle 1:1 .. 5:1 */
        static const char *const names[5] = {
            "1-for-1: normal encounters and rewards",
            "2-for-1: 1/2 encounters, 2x rewards",
            "3-for-1: 1/3 encounters, 3x rewards",
            "4-for-1: 1/4 encounters, 4x rewards",
            "5-for-1: 1/5 encounters, 5x rewards" };
        int m = tfo_mode() % 5 + 1;
        tfo_set_mode(m);
        f_se(m == 1 ? SE_OFF : SE_ON, 0x7f, 0x3f);
        show(names[m - 1]);
    }
    if (ok && FEATURES[9] && (edge & BTN_RIGHT)) karma_force = 1;   /* BadKarma */
    if (ok && FEATURES[10] && (edge & BTN_LEFT)) karma_force = 2;   /* GoodKarma */
    if (karma_none) {                                                 /* nothing to fight in this area */
        karma_none = 0;
        f_se(SE_OFF, 0x7f, 0x3f);
        show("No enemies here");
    }
    if (FEATURES[2] && enc_off && !karma_force) {
        /* the encounter roll adds walked distance into +0x1360 and bumps the danger counter at
         * +0x1364 every 100 units; holding both at zero means no battle can trigger */
        *(volatile float *)(GBWK + 0x1360) = 0.0f;
        *(volatile u16 *)(GBWK + 0x1364) = 0;
    }
    u32 r = f_field_ctl();
    if (ok) draw_msg();
    return r;
}
