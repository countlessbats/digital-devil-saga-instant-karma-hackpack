/* SceneSkip: START during a cutscene skips the whole scene.
 *
 * A scene is an event task named eNNN_NNN (priority 0x3eb) that plays one or more event-player segments
 * ("(ZikkiPlayMode)Event") between its own script steps. The game can already skip a segment on START
 * (0x22f2e0), but refuses scenes whose messages are flagged by 0x243390 ("This event cannot be
 * skipped."); the section patches that check at 0x22f2f4 so every segment is skippable, while the
 * script-level no-skip flag (GBWK+0x388, 0x10ee30) is still honoured.
 * After one START, until the player has control again (or ~1 s passes with no segment playing, e.g. a battle
 * starts), including scenes chained straight after it: each new segment gets START pressed as soon as the
 * game accepts it, and the script steps in between run fast-forwarded (turbo's override, 8 logic passes
 * per frame, when the Native Turbo section is on). A pre-rendered movie (player task gp-0x46c4) can't be
 * sped up and would fall behind the frame-timed subtitles, so it is stopped (0x270030, as the game's own
 * movie skip does); the event carries on without it, behind a black screen at 64 passes per frame (the
 * script itself can run for minutes; without the Native Turbo section the movie is left alone). A choice
 * ends the skip: the decision is made at normal
 * speed and START starts skipping again from there. Every choice is opened by 0x19beb0 (field scripts'
 * SELECT commands and the event player alike); the section points its four calls at skip_choice. */
#include "game.h"

#define GP           0x003c0cf0u
#define FEATURES     ((volatile u32 *)0x000FD200)   /* [6] SceneSkip */
#define TASKS        RD32(GP - 0x64d8)
#define TURBO_CODE   0x000F0020u
#define TURBO_OVR    RD8(0x000F0001u)               /* turbo debug override: logic passes per frame */
#define START_DOWN   (RD8(0x003244dcu) & 0x80)      /* pad 0 START, newly pressed */

int hold_black;          /* bit 0: SceneSkip, bit 1: QuickStart autoload (see black_draw) */
static u32 skip_task;    /* the event task being skipped */
static int skip_timer, idle, movie_cut;
extern u32 field_control_frame;          /* set by the field control step (field_toggles.c) */
#define FRAME        RD32(0x003ba700u)

static void stop(void)
{
    skip_task = 0; idle = 0; movie_cut = 0;
    hold_black &= ~1;
    if (RD32(TURBO_CODE)) TURBO_OVR = 0;
}


#define MOVIE_TASK   RD32(GP - 0x46c4)           /* movie player task (0 = none) */
#define f_movie_stop ((void (*)(void))0x00270030)
#define f_choice_start ((void (*)(int))0x0019beb0)
void skip_choice(int win)
{
    if (FEATURES[6] && skip_task) stop();
    f_choice_start(win);
}

static int is_event_task(u32 t)
{
    const char *n = (const char *)t;
    return n[0] == 'e' && n[1] >= '0' && n[1] <= '9' && n[2] >= '0' && n[2] <= '9' && n[3] >= '0' && n[3] <= '9' && n[4] == '_';
}

static u32 find_task(int want_event, u32 must_be)
{
    for (u32 t = TASKS; t; t = RD32(t + 0x3c)) {
        if (must_be) { if (t == must_be) return t; continue; }
        if (want_event ? is_event_task(t) : (RD8(t) == '(' && RD8(t + 1) == 'Z')) return t;
    }
    return 0;
}

static void press_start(void)
{
    RD8(0x003244dcu) = 0x83;
    RD8(0x003244fcu) = 0x83;
}

/* ---- black screen while something runs unseen ------------------------------------------------ */
/* The screen fade (colour gp-0x63d0..-0x63ce, amount gp-0x63cd, 0x80 = full) is drawn by 0x105dd8 from
 * RequestDraw (call at 0x102528). While hold_black is set the overlay is drawn fully black for that
 * frame only; the game's own fade value is restored straight after, so its fade logic is untouched. */
#define FADE_RGB(i)  RD8(GP - 0x63d0 + (i))
#define FADE_AMOUNT  RD8(GP - 0x63cd)
#define f_fade_draw  ((void (*)(void))0x00105dd8)
void black_draw(void)
{
    if (!hold_black) { f_fade_draw(); return; }
    u8 keep[4];
    for (int i = 0; i < 3; i++) { keep[i] = FADE_RGB(i); FADE_RGB(i) = 0; }
    keep[3] = FADE_AMOUNT; FADE_AMOUNT = 0x80;
    f_fade_draw();
    for (int i = 0; i < 3; i++) FADE_RGB(i) = keep[i];
    FADE_AMOUNT = keep[3];
}

/* Called every frame from the pad hook (qs_pad), after the pad processor and before any task runs. */
void skip_frame(void)
{
    if (!FEATURES[6]) return;
    if (!skip_task) {
        if (START_DOWN) {
            u32 ev = find_task(1, 0);
            if (ev) { skip_task = ev; skip_timer = 0; idle = 0; }
        }
        return;
    }
    /* Done once the player has control (the field control step ran), or when no segment has played for
     * ~1 s (event tasks can stay alive in the field, so their presence alone doesn't mean a scene). */
    if (field_control_frame + 2 >= FRAME && field_control_frame) { stop(); return; }
    if (find_task(0, 0)) idle = 0;
    else if (++idle > 30) { stop(); return; }
    if (MOVIE_TASK && RD32(TURBO_CODE)) { f_movie_stop(); movie_cut = 1; hold_black |= 1; }   /* movie: cut it */
    if (RD32(TURBO_CODE)) TURBO_OVR = movie_cut ? 64 : 8;
    if (find_task(0, 0)) { skip_timer++; press_start(); }            /* segment playing: ask it to skip */
}
