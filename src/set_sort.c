/* SET menu LEARNED sorting: START cycles Default -> Cost -> A-Z, remembered across visits. */
#include "game.h"

#define MODES 3
#define MAX_ITEMS 400

static u8 sort_mode;        /* 0 default (game order), 1 cost, 2 alphabetical */
static u32 items[MAX_ITEMS];

/* Prompt shown in the SET header ("Set which skill?", 24-byte buffer). */
#define PROMPT_ADDR 0x003b1bc8u
static const char *const prompts[MODES] = {
    "Set skill (sort: Game)", "Set skill (sort: Cost)", "Set skill (sort: A-Z)",
};

static void set_prompt(void)
{
    const char *s = prompts[sort_mode];
    char *d = (char *)PROMPT_ADDR;
    int i = 0;
    for (; s[i] && i < 23; i++) d[i] = s[i];
    d[i] = 0;
}

static int is_blank(u32 id) { return id == 0 || id == 0xffff; }

static u32 char_data(u32 list)
{
    u32 wa = RD32(list + 0x30);
    u32 idx = RD32(RD32(RD32(wa + 0x7d8) + 0x1c));
    return RD32(PARTY_BASE) + idx * 0x1a4 + 0xa60;
}

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static int name_cmp(u32 a, u32 b)
{
    const u8 *p = (const u8 *)(RD32(SKILL_NAMES) + a * 17);
    const u8 *q = (const u8 *)(RD32(SKILL_NAMES) + b * 17);
    for (int i = 0; i < 17; i++) {
        int x = lower(p[i]), y = lower(q[i]);
        if (x != y) return x - y;
        if (!x) break;
    }
    return 0;
}

/* <0 if item a sorts before item b */
static int item_cmp(u32 a, u32 b, u32 cd)
{
    u32 ia = RD32(a + ITEM_ID), ib = RD32(b + ITEM_ID);
    int d = 0;
    if (sort_mode != 0) {
        int ba = is_blank(ia), bb = is_blank(ib);
        if (ba != bb) return ba - bb;           /* blanks last */
        if (!ba) {
            if (sort_mode == 1) {
                u32 ca = fn_skill_kind(ia) == 3 ? 0 : fn_skill_cost(ia, cd);
                u32 cb = fn_skill_kind(ib) == 3 ? 0 : fn_skill_cost(ib, cd);
                d = (int)ca - (int)cb;
            } else {
                d = name_cmp(ia, ib);
            }
        }
    }
    if (d) return d;
    return (int)RD32(a + ITEM_TAG) - (int)RD32(b + ITEM_TAG);
}

/* Swap everything but the index and the list links. */
static void swap_payload(u32 a, u32 b)
{
    for (u32 o = 4; o < 0x80; o += 4) {
        if (o == ITEM_NEXT || o == ITEM_PREV) continue;
        u32 t = RD32(a + o);
        RD32(a + o) = RD32(b + o);
        RD32(b + o) = t;
    }
}

static int collect(u32 list)
{
    int n = 0;
    for (u32 it = RD32(list + LIST_FIRST); it && n < MAX_ITEMS; it = RD32(it + ITEM_NEXT))
        items[n++] = it;
    return n;
}

static void sort_list(u32 list)
{
    int n = collect(list);
    if (n < 3) return;
    u32 cd = char_data(list);
    /* items[0] is Undo; selection sort the rest (n is small) */
    for (int i = 1; i < n - 1; i++) {
        int m = i;
        for (int j = i + 1; j < n; j++)
            if (item_cmp(items[j], items[m], cd) < 0) m = j;
        if (m != i) swap_payload(items[i], items[m]);
    }
}

/* Replaces the builder's per-tab jal 0x27e100 at 0x27862c. */
void set_finalize(u32 list, int a1, int a2)
{
    int n = collect(list);
    for (int i = 0; i < n; i++) RD32(items[i] + ITEM_TAG) = i;
    fn_list_finalize(list, a1, a2);
    if (sort_mode) sort_list(list);
    set_prompt();
}

/* Replaces the SET learned-list logic task (table entry 0x37cc9c). */
int set_logic(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
    if (RD8(MENU_PAD + PAD_START) & 0x80) {
        sort_mode = (sort_mode + 1) % MODES;
        u32 lists = RD32(fn_task_work((u32)a0) + 0x90c);   /* a0 = task */
        for (int t = 0; t < 4; t++) {
            u32 w = RD32(lists + 0x10 + t * 4);
            if (w) sort_list(RD32(w + 0x14));
        }
        set_prompt();
    }
    return fn_set_logic(a0, a1, a2, a3, a4, a5, a6, a7);
}
