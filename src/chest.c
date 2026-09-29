/* OpenChests: inspecting a chest opens it at once. The "A strange object lies on the floor. Touch it?"
 * question and its Yes/No choice are answered automatically; the "Obtained ..." message stays for you.
 *
 * Field messages run in message window 16 (pointer at 0x3d6ecc + 16*0x14). Its current message is
 * window+0x2c, which points at the message's name in the loaded message data: chests use TAKARA (with
 * the TAKARA_SEL choice), floating jewels HOSEKI (HOSEKI_SEL). While a choice is up, window+0x50 holds
 * the option count and +0x52/+0x54 the cursor (0 = Yes). X is written into the pad data right after the
 * pad processor (called from qs_pad), so the game's own script opens the chest and gives the items. */
#include "game.h"

#define FEATURES     ((volatile u32 *)0x000FD200)   /* [7] OpenChests */
#define FIELD_WINDOW RD32(0x003d700cu)

static int timer;

static int is_chest_question(u32 name)
{
    const char *n = (const char *)name;
    int takara = n[0] == 'T' && n[1] == 'A' && n[2] == 'K' && n[3] == 'A' && n[4] == 'R' && n[5] == 'A' && n[6] == 0;
    int hoseki = n[0] == 'H' && n[1] == 'O' && n[2] == 'S' && n[3] == 'E' && n[4] == 'K' && n[5] == 'I' && n[6] == 0;
    return takara || hoseki;
}

static void press_x(void)
{
    RD8(0x003244d1u) = 0x83;
    RD8(0x003244f1u) = 0x83;
}

/* Called every frame from the pad hook, after the pad processor and before any task runs. */
void chest_frame(void)
{
    if (!FEATURES[7]) return;
    u32 w = FIELD_WINDOW;
    u32 name = w ? RD32(w + 0x2c) : 0;
    if (!name || name < 0x100000u || name >= 0x2000000u || !is_chest_question(name)) { timer = 0; return; }
    volatile u16 *h = (volatile u16 *)(w + 0x50);
    if (h[0] && h[1] != 0xffff) { h[1] = 0; h[2] = 0; }     /* choice up: cursor on Yes */
    if ((++timer & 3) == 0) press_x();                       /* page on / confirm, as the game's own input */
}
