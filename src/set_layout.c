/* SET menu relayout: replaces the SET "learned" draw task (table 0x37cca0) and draws
 * LEARNED as a multi-column grid. Coordinates are GS 1/16 px on a 512x224 frame
 * (~12.8 units per screen px across, ~7.47 down). All positions live in a RAM table so
 * they can be tuned live; defaults are applied when the magic is missing. */
#include "game.h"

typedef struct {
    u32 magic;
    int asg_x, asg_y, asg_rowh;         /* ASSIGNED widget anchor + row pitch (0 = game default) */
    int help_dx, help_dy;               /* HELP offset from its original spot */
    int port_dx, port_dy;               /* portrait/status offset */
    int cat_x, cat_y;                   /* category tab row anchor (orig 0xde0,0x350) */
    int grid_x, grid_y, grid_pitch;     /* LEARNED grid anchor (orig 0x1220,0x678) and column pitch */
    int grid_cols, grid_rows, grid_rowh;
    int sort_x, sort_y;                 /* sort indicator text position */
    int strip_w;                        /* row strip sprite width override (0 = native) */
    int decor;                          /* draw the original side decorations */
    int hide_header;                    /* hide "SET MENU" header and prompt */
    int lbl_x, lbl_y;                   /* anchor of the widget header call: positions the "!" new-skill marker */
    int name_dx;                        /* name offset within a cell (game: widget+8 = 0x110) */
    int cost_dx;                        /* cost block offset within a cell (game: 0x930) */
    int bang_x, bang_y;                 /* anchor of the "LEARNED" title sprite (widget+0x34) */
    u32 skip;                           /* debug: bit mask of draw calls to skip (see set_draw) */
    int unit_dx, unit_dy;               /* HP/MP unit label offset in LEARNED (game: +0x140, +0) */
    int hint_probe;                     /* debug: draw hint-sheet sprites 0..15 */
    int h_y, h_text_y;                  /* hint bar: sprite row y, text row y */
    int h_start_x, h_type_x, h_tri_x, h_l1_x, h_x_x, h_o_x;   /* hint bar item x positions */
    u32 h_color;                        /* hint text colour (pointer into the game's colour table) */
    int h_type_y;                       /* "L2/R2" label beside the tab row: h_type_x, h_type_y */
    int nudge_on;                       /* set by tools/nudge.py while NumLock move mode is active */
} Layout;

#define LAY_MAGIC 0x4c41590f
#define LAY ((volatile Layout *)0x000FF000)

/* game functions */
#define f_sprite     ((void (*)(int, int, int, int, u32, int, int))0x002bf790)
#define f_sprite_a   ((void (*)(int, int, int, int, int, u32, int, int))0x002bf4e0)   /* with alpha */
#define f_widget     ((void (*)(int, int, int, u32, int))0x0027cdd0)
#define f_frame      ((void (*)(int, int, int, u32, int))0x0027ca78)
#define f_header     ((void (*)(int, int, int, u32, int))0x0027ca90)
#define f_cursor     ((void (*)(int, int, int, u32, int))0x0027ccd0)
#define f_rows       ((void (*)(int, int, int, int, int, int, int, u32, int))0x0027c140)
#define f_tabsel     ((void (*)(u32, u32))0x00282d98)
#define f_tabrow     ((void (*)(int, int, int, u32, int))0x00282da0)
#define f_fade       ((void (*)(u32, int))0x0027b268)
#define f_camp_ok    ((int (*)(u32))0x002719f0)
#define f_backdrop   ((void (*)(int, int, int, u32, int))0x0027e8d8)
#define f_status     ((void (*)(int, int, int, u32, int))0x00282be8)
#define f_prompt     ((void (*)(int))0x00272350)
#define f_hints      ((void (*)(int, u32))0x002723b0)
#define f_component  ((u32 (*)(u32, u32, int, u32))0x00285670)
#define f_helptext   ((u32 (*)(int, int, int, int, u32, int))0x00197c40)
#define f_textcolor  ((void (*)(u32, u32))0x001954c8)
#define f_textprep   ((void (*)(u32, int, int))0x001958a0)
#define f_textsubmit ((void (*)(u32))0x00194920)
#define f_text       ((u32 (*)(int, int, int, u32, const char *, int))0x00197760)

#define f_txt_flag_on  ((void (*)(int))0x00195520)
#define f_txt_flag_off ((void (*)(int))0x00195530)
#define f_txt_make     ((u32 (*)(const char *, int, int, int, u32))0x00195160)
#define f_txt_pos      ((void (*)(u32, int, int))0x00195450)
#define f_txt_z        ((void (*)(u32, u32))0x00195460)
#define f_txt_link     ((u32 (*)(u32, u32, int))0x00195b78)

/* ASCII text object in a given font (0x197760 hardwires font 1); submit with f_textprep+f_textsubmit */
static u32 text_font(int x, int y, u32 color, const char *str, int font)
{
    f_txt_flag_on(1);
    u32 o = f_txt_make(str, font, 0, 0, 0);
    f_txt_flag_on(2);
    f_txt_flag_off(1);
    f_txt_pos(o, x, y);
    f_txt_z(o, 0);
    f_textcolor(o, color);
    return f_txt_link(0, o, 0);
}

#define HELP_SRC     0x003baa98u   /* gp-0x6258: help text table */
#define PRIO         0x53

extern const char *sort_label(void);   /* set_sort.c */

static void layout_defaults(void)
{
    volatile Layout *l = LAY;
    if (l->magic == LAY_MAGIC) return;
    /* ASSIGNED top-left, HELP top-right, status under HELP, tabs under ASSIGNED,
     * LEARNED as a 3x8 grid across the full width (px ~ units/12.8 across, /7.47 down) */
    l->asg_x = 256;   l->asg_y = 187;  l->asg_rowh = 0;
    l->help_dx = 4416; l->help_dy = -2442;
    l->port_dx = 0;    l->port_dy = 709;
    l->cat_x = -608;   l->cat_y = 1438;
    l->grid_x = 0;     l->grid_y = 1941; l->grid_pitch = 2662;
    l->grid_cols = 3;  l->grid_rows = 8;  l->grid_rowh = 0;
    l->sort_x = 5786;  l->sort_y = 1747;
    l->strip_w = 0;
    l->decor = 0;      l->hide_header = 1;
    l->lbl_x = -192;   l->lbl_y = 2524;
    l->name_dx = 0x60; l->cost_dx = 0x863;
    l->bang_x = 4186;  l->bang_y = 1889;
    l->skip = 0;
    l->unit_dx = -0x20; l->unit_dy = 0;
    l->hint_probe = 0;
    l->h_y = 0xcf0; l->h_text_y = 0xcf0;
    l->h_start_x = 422; l->h_tri_x = 2035; l->h_l1_x = 3840; l->h_x_x = 5709; l->h_o_x = 6848;
    l->h_type_x = 2624; l->h_type_y = 1590;
    l->h_color = 0xa09dc340;
    l->magic = LAY_MAGIC;
}

/* Private copy of the cost/unit drawer 0x277640 (shared by 4 menus) for LEARNED only: the row
 * callback's call at 0x278020 is pointed at this copy by the pnach. The copy is position
 * independent (relative branches, absolute jal, sp-relative stack args). We patch:
 *   +0x124 "addiu s3,s3,0x140"  -> unit label x offset
 *   +0x18c / +0x1c0 "move a1,s5" -> "addiu a1,s5,dy" unit label y offset */
#define COSTFN      0x00277640u
#define COSTFN_LEN  (0x204 / 4)
u32 costfn_copy[COSTFN_LEN] __attribute__((aligned(16)));
static int costfn_ready, costfn_dx, costfn_dy;
void costfn_prepare(void)
{
    volatile Layout *l = LAY;
    int fresh = !costfn_ready;
    if (fresh) {
        for (int i = 0; i < COSTFN_LEN; i++) costfn_copy[i] = RD32(COSTFN + i * 4);
        costfn_ready = 1;
    }
    if (fresh || l->unit_dx != costfn_dx || l->unit_dy != costfn_dy) {
        costfn_dx = l->unit_dx; costfn_dy = l->unit_dy;
        costfn_copy[0x124 / 4] = 0x26730000u | ((u32)(0x140 + costfn_dx) & 0xffff);
        costfn_copy[0x18c / 4] = 0x26a50000u | ((u32)costfn_dy & 0xffff);
        costfn_copy[0x1c0 / 4] = 0x26a50000u | ((u32)costfn_dy & 0xffff);
    }
}

/* The LEARNED row callback (0x277df8, used only by LEARNED lists) places the cost block at
 * cell x + 0x930 via "addiu a0, s5, 0x930" at 0x277fe4. Rewrite the immediate when it changes. */
#define COST_INSN 0x00277fe4u
static void set_cost_offset(int dx)
{
    u32 want = 0x26a40000u | ((u32)dx & 0xffff);
    if (RD32(COST_INSN) != want) RD32(COST_INSN) = want;
}

/* Camp backdrop (original 0x27b268): same layers, minus sprite 3 of the ring sheet — a
 * glowing orb the old category panel used to cover; exposed, it reads like a "!" icon. */
#define f_gsmode    ((void (*)(int, int))0x002c0950)   /* (mode, prio) */
#define f_fillquad  ((void (*)(int, int, int, int, int, int, int))0x002c0dd8)
#define f_bg_rings  ((void (*)(u32, int))0x0027b1b0)
#define f_blend_on  ((void (*)(int))0x002c14e0)
#define f_blend_x   ((void (*)(int, int))0x002c1548)
#define f_blend_off ((void (*)(int))0x002c1588)
#define f_anim_tick ((void (*)(u32, int))0x002bf970)
static void draw_camp_bg(u32 obj, int prio)
{
    u32 k = LAY->skip;
    f_gsmode(0x30000, prio);
    if (!(k & 0x80000)) f_fillquad(0, 0, 0, 0x2000, 0xe00, (int)0x80808080, prio);
    if (!(k & 0x2000)) f_sprite(0, 0, 0, 0, RD32(obj), 0, prio);
    if (!(k & 0x80)) f_bg_rings(obj, prio);
    u32 sh = RD32(obj + 0x10);
    f_blend_on(prio);
    if (!(k & 0x100)) f_sprite(-0x70, 0xa0, 0, 0x61, sh, 0, prio);
    if (!(k & 0x200)) f_sprite(-0x470, 0x808, 0, 0x61, sh, 1, prio);
    if (!(k & 0x400)) f_sprite(0x1050, -0x3e8, 0, 0x61, sh, 2, prio);
    if (!(k & 0x4000)) f_sprite(0x10b0, 0x3c0, 0, 0x61, sh, 3, prio);
    if (!(k & 0x800)) f_sprite(0x1300, 0xb70, 0, 0x61, sh, 4, prio);
    f_blend_x(0, prio);
    if (!(k & 0x8000)) f_anim_tick(sh, 0);
    if (!(k & 0x10000)) f_anim_tick(sh, 1);
    if (!(k & 0x1000)) f_sprite(0, 0, 0, 0x60, RD32(obj + 4), 0, prio);
    if (!(k & 0x20000)) f_anim_tick(RD32(obj + 4), 0);
    f_blend_off(prio);
}

/* ---- LEARNED grid ------------------------------------------------------ */

#define GRID_MAX 400
static u32 cells[GRID_MAX];
static u32 grid_list;     /* list the scroll state belongs to */
static int grid_top;      /* first visible grid row */

static int list_cells(u32 L)
{
    int n = 0;
    for (u32 it = RD32(L + LIST_FIRST); it && n < GRID_MAX; it = RD32(it + ITEM_NEXT)) cells[n++] = it;
    return n;
}

int grid_cursor_index(u32 L)
{
    u32 cur = RD32(L + 0x1c);
    return cur ? (int)RD32(cur) : 0;
}

static void draw_grid(u32 W)
{
    volatile Layout *l = LAY;
    u32 L = RD32(W + 0x14);
    int cols = l->grid_cols, rows = l->grid_rows;
    int n = list_cells(L);
    int cur = grid_cursor_index(L);
    int rowh = l->grid_rowh ? l->grid_rowh : (int)RD32(L + 0x28);
    if (cols < 1) cols = 1;

    if (L != grid_list) { grid_list = L; grid_top = 0; }
    int crow = cur / cols, ccol = cur % cols;
    if (crow < grid_top) grid_top = crow;
    if (crow >= grid_top + rows) grid_top = crow - rows + 1;

    /* save list state we temporarily repurpose */
    u32 s_top = RD32(L + 0x18), s_rows = RD32(L + 0x0c), s_crow = RD32(L + 0x24);
    u32 s_flags = RD32(L), s_rowh = RD32(L + 0x28), s_cnt = RD32(L + 0x20);
    RD32(L + 0x28) = rowh;

    u32 alpha = RD32(W + 0x88);
    RD32(L + 0x3c) = alpha;
    /* header: "!" marker at lbl_x/lbl_y; the "LEARNED" title (widget+0x34) separately at bang_x/bang_y */
    u32 bang = RD32(W + 0x34);
    RD32(W + 0x34) = 0;
    if (!(LAY->skip & 32)) f_header(l->lbl_x, l->lbl_y, 0, W, PRIO);
    RD32(W + 0x34) = bang;
    if (bang) {
        u32 sheet = bang, idx = RD32(W + 0x38);
        if (RD32(W + 4) & 4) { sheet = RD32(W + 0x3c); idx = RD32(W + 0x40); }
        f_sprite_a(l->bang_x, l->bang_y, 0, (int)alpha, 0, sheet, (int)idx, PRIO);
    }
    set_cost_offset(l->cost_dx);
    costfn_prepare();
    if (l->nudge_on) {
        static char buf[32];
        int k = 0;
        const char *pre = "MOVE ";
        while (*pre) buf[k++] = *pre++;
        for (int a = 0; a < 2; a++) {
            int v = a ? l->unit_dy : l->unit_dx;
            char d[8]; int m = 0;
            if (v < 0) { buf[k++] = '-'; v = -v; }
            do { d[m++] = '0' + v % 10; v /= 10; } while (v && m < 7);
            while (m) buf[k++] = d[--m];
            buf[k++] = a ? 0 : ',';
        }
        u32 t = text_font(l->sort_x, l->sort_y - 0xa0, 0xa09dc366, buf, 1);
        f_textprep(t, 1, 0x54);
        f_textsubmit(t);
    }

    for (int c = 0; c < cols; c++) {
        int x = l->grid_x + c * l->grid_pitch;
        int first = grid_top * cols + c;
        RD32(L + 0x0c) = rows;
        RD32(L + 0x18) = first < n ? cells[first] : cells[0];
        if (c == ccol) { RD32(L + 0x24) = crow - grid_top; RD32(L) = s_flags & ~8u; }
        else           { RD32(L + 0x24) = 0; RD32(L) = s_flags | 8u; }
        f_frame(x, l->grid_y, 0, W, PRIO);
    }
    RD32(L) = s_flags;

    /* cursor highlight in the cursor's column */
    if (s_cnt) {
        RD32(L + 0x24) = 0;
        f_cursor(l->grid_x + ccol * l->grid_pitch, l->grid_y + (crow - grid_top) * rowh, 0, W, PRIO);
    }

    /* one row per cell; each cell is drawn as the list's "top" item, and the callback draws the
     * tab's new-skill marker for the top item when L+0x38 is set, so allow it for one cell only */
    u32 s_mark = RD32(L + 0x38);
    RD32(L + 0x0c) = 1;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int i = (grid_top + r) * cols + c;
            if (i >= n) break;
            RD32(L + 0x38) = (r == 0 && c == 0) ? s_mark : 0;
            RD32(L + 0x18) = cells[i];
            f_rows(l->grid_x + c * l->grid_pitch, l->grid_y + r * rowh, 0,
                   l->name_dx, (int)RD32(W + 0xc), (int)alpha, (int)RD32(W + 4), L, PRIO);
        }
    }

    RD32(L + 0x38) = s_mark;
    RD32(L + 0x18) = s_top; RD32(L + 0x0c) = s_rows; RD32(L + 0x24) = s_crow; RD32(L + 0x28) = s_rowh;
    if ((int)RD32(W + 0x88) < 0x100) RD32(W + 0x88) = RD32(W + 0x88) + 0x20;
    RD32(W + 4) |= 4;

    /* sort indicator next to the LEARNED label */
    u32 t = f_text(l->sort_x, l->sort_y, 0, 0xa09dc35a, sort_label(), 0);
    f_textprep(t, 1, 0x54);
    f_textsubmit(t);
}

/* ---- screen ------------------------------------------------------------ */

/* Per-tab "new skills" marker positions (x, y pairs; drawn at x+0x30) used by the LEARNED row
 * callback. They are relative to the original tab row anchor (0xde0, 0x350), so shift them
 * with the tab row. */
#define TAB_MARK 0x003b22f0u
static const short tab_mark_orig[8] = { 0x17f0, 0x390, 0x17c0, 0x390, 0x1950, 0x390, 0x19d0, 0x390 };
static void place_tab_marks(int dx, int dy)
{
    for (int i = 0; i < 4; i++) {
        RD32(TAB_MARK + i * 8) = tab_mark_orig[i * 2] + dx;
        RD32(TAB_MARK + i * 8 + 4) = tab_mark_orig[i * 2 + 1] + dy;
    }
}

static void draw_learned_side(u32 wa, u32 s1, u32 tab)
{
    volatile Layout *l = LAY;
    place_tab_marks(l->cat_x - 0xde0, l->cat_y - 0x350);
    u32 W = RD32(s1 + 0x10 + tab * 4);
    if (l->decor) {
        f_sprite(0x10e0, 0x598, 0, 1, RD32(wa + 0xe8), 9, PRIO);
        f_sprite(0x10e0, 0xc88, 0, 1, RD32(wa + 0xe8), 10, PRIO);
        f_sprite(0x10e0, 0xa18, 0, 1, RD32(wa + 0xe0), 0x1d, PRIO);
    }
    f_tabsel(RD32(s1 + 0x20), tab);
    if (!(l->skip & 1)) f_tabrow(l->cat_x, l->cat_y, 0, RD32(s1 + 0x20), PRIO);
    if (l->grid_cols <= 1 && !l->grid_rowh) f_widget(l->grid_x, l->grid_y, 0, W, PRIO);
    else if (!(l->skip & 0x400000)) draw_grid(W);
    /* keep the other tabs' widgets in the same animation state (as the game does) */
    for (u32 i = 0; i < 4; i++) {
        if (i == tab) continue;
        u32 d = RD32(s1 + 0x10 + i * 4);
        for (u32 o = 0x4c; o < 0x84; o += 4) RD32(d + o) = RD32(W + o);
        RD32(d + 0x88) = RD32(W + 0x88);
        RD32(d + 4) = RD32(W + 4);
    }
}

static void draw_help(u32 wa, u32 id)
{
    volatile Layout *l = LAY;
    int dx = l->help_dx, dy = l->help_dy;
    f_sprite(0x1c0 + dx, 0xa20 + dy, 0, 1, RD32(wa + 0x74), 0x2e, PRIO);
    f_sprite(0x150 + dx, 0x9c0 + dy, 0, 1, RD32(wa + 0x74), 0, PRIO);
    f_sprite(0x280 + dx, 0x9a0 + dy, 0, 1, RD32(wa + 0x64), 2, PRIO);
    if (id != 0 && id != 0xffff) {
        u32 t = f_helptext(0x2c0 + dx, 0xa70 + dy, 0, (int)(id & 0xffff), RD32(HELP_SRC), 1);
        f_textcolor(t, 0xa09dc366);
        f_textprep(t, 0, PRIO);
        f_textsubmit(t);
    }
}

/* LEARNED hint bar: the game's sprites for Rearrange / L1R1 Character / Select / Cancel
 * (hint sheet 6, 3, 4, 5), plus text hints for the new controls. */
static void hint_text(int x, int y, const char *str)
{
    u32 t = text_font(x, y, LAY->h_color, str, 1);
    f_textprep(t, 1, 0x54);
    f_textsubmit(t);
}
static void draw_learned_hints(u32 sheet)
{
    volatile Layout *l = LAY;
    hint_text(l->h_start_x, l->h_text_y, "START Sort");
    hint_text(l->h_type_x, l->h_type_y, "L2/R2");
    f_sprite(l->h_tri_x, l->h_y, 0, 1, sheet, 6, PRIO);
    f_sprite(l->h_l1_x, l->h_y, 0, 1, sheet, 3, PRIO);
    f_sprite(l->h_x_x, l->h_y, 0, 1, sheet, 4, PRIO);
    f_sprite(l->h_o_x, l->h_y, 0, 1, sheet, 5, PRIO);
}

/* Shared SET screen draw. slot_mode = 0: LEARNED has focus (original 0x279f88);
 * slot_mode = 1: picking an ASSIGNED slot / rearranging (original 0x279568). */
static void draw_set_screen(u32 task, int slot_mode)
{
    layout_defaults();
    volatile Layout *l = LAY;
    if (l->skip & 0x40000) return;
    u32 wa = fn_task_work(task);
    u32 s1 = RD32(wa + 0x90c);
    u32 tab = RD32(RD32(RD32(s1 + 0xc) + 0x1c));
    u32 W = RD32(s1 + 0x10 + tab * 4);
    u32 asg = RD32(s1 + 0x24);
    /* the character list's cursor index; the slot draw skips LEARNED when it is 0 */
    int who = (int)RD32(RD32(RD32(RD32(wa + 0x124) + 0x14) + 0x1c));

    if ((!slot_mode || who != 0) && !(l->skip & 0x100000)) draw_learned_side(wa, s1, tab);

    /* backdrop + status block (original 0x272688 with header param 1) */
    if (!(l->skip & 64)) draw_camp_bg(wa + 0x13c, 0x20);
    if (f_camp_ok(task)) {
        if (!l->hide_header) f_backdrop(-0x10, -8, 0, RD32(wa + 0x138), PRIO);
        if (!(l->skip & 2)) f_status(l->port_dx, l->port_dy, 0, wa + 0x15c, PRIO);
    }
    if (!l->hide_header) f_prompt(slot_mode ? 0xc : 0xb);

    u32 focus = slot_mode ? asg : W;
    u32 id = RD32(RD32(RD32(focus + 0x14) + 0x1c) + 0x60);
    if (!(l->skip & 4)) draw_help(wa, id);

    if (!slot_mode) RD32(RD32(asg + 0x14)) |= 8;
    u32 AL = RD32(asg + 0x14);
    u32 s_rowh = RD32(AL + 0x28);
    if (l->asg_rowh) RD32(AL + 0x28) = l->asg_rowh;
    if (!(l->skip & 0x200000)) f_widget(l->asg_x, l->asg_y, 0, asg, PRIO);
    RD32(AL + 0x28) = s_rowh;

    if (!(l->skip & 16)) {
        if (slot_mode) f_hints(0, RD32(wa + 0x78));
        else draw_learned_hints(RD32(wa + 0x78));
    }
    if (l->hint_probe == 2)
        for (int f = 0; f < 4; f++) {
            u32 t = text_font(0x300, 0x600 + f * 0x180, 0xa09dc35a, "L2R2 Type  START Sort", f);
            f_textprep(t, 1, 0x54);
            f_textsubmit(t);
        }
    if (l->hint_probe == 1)
        for (int i = 0; i < 16; i++)
            f_sprite(0x200 + (i % 4) * 0x7c0, 0x500 + (i / 4) * 0x280, 0, 1, RD32(wa + 0x78), i, PRIO);
    if (!(l->skip & 8)) f_component(wa + 8, wa + 0x54, 1, task);
}

/* Replaces the SET learned-list draw task (table entry 0x37cca0; original 0x279f88). */
void set_draw(u32 task) { draw_set_screen(task, 0); }

/* Replaces the SET slot-select / rearrange draw task (table entry 0x37cc68; original 0x279568). */
void set_draw_slot(u32 task) { draw_set_screen(task, 1); }

/* ---- grid navigation (called from the SET logic wrapper before the game's handler) ---- */

#define PAD_A 0x00324510u   /* per-task pad copies (same layout, both read by menus) */
#define PAD_B 0x00324530u
enum { P_LEFT = 4, P_RIGHT = 5, P_UP = 6, P_DOWN = 7, P_L2 = 9, P_R2 = 11 };
#define f_cursor_anim ((void (*)(u32))0x0027d740)   /* restart cursor animation (widget+0x4c) */

/* Put the LEARNED cursor on absolute index t, keeping the game's own 8-row window
 * (top item L+0x18, visible row L+0x24) consistent so selection and help text follow. */
static void set_cursor(u32 W, u32 L, int t)
{
    int n = list_cells(L);
    if (t < 0 || t >= n) return;
    int vis = (int)RD32(L + 0x0c);
    u32 topit = RD32(L + 0x18);
    int top = topit ? (int)RD32(topit) : 0;
    if (t < top) top = t;
    if (t > top + vis - 1) top = t - vis + 1;
    if (top < 0) top = 0;
    u32 old = RD32(L + 0x1c);
    if (old) RD32(old + 0x50) = 0x100;        /* same flash the game gives the item left behind */
    RD32(L + 0x18) = cells[top];
    RD32(L + 0x1c) = cells[t];
    RD32(L + 0x24) = t - top;
    if ((int)RD32(L + 0x20) >= 2) RD32(L) &= ~3u;   /* clear the list's end-of-list hold flags */
    f_cursor_anim(W + 0x4c);
}

static int pulse(int b) { return RD8(PAD_B + b) & 2; }
static void swallow(int b) { RD8(PAD_A + b) = 0; RD8(PAD_B + b) = 0; }

void grid_input(u32 task)
{
    layout_defaults();
    volatile Layout *l = LAY;
    int cols = l->grid_cols;
    if (cols <= 1) return;
    u32 s1 = RD32(fn_task_work(task) + 0x90c);
    u32 tab = RD32(RD32(RD32(s1 + 0xc) + 0x1c));
    u32 W = RD32(s1 + 0x10 + tab * 4);
    u32 L = RD32(W + 0x14);
    int n = (int)RD32(L + 0x20);
    int up = pulse(P_UP), dn = pulse(P_DOWN), lf = pulse(P_LEFT), rt = pulse(P_RIGHT);
    int l2 = pulse(P_L2), r2 = pulse(P_R2);
    swallow(P_UP); swallow(P_DOWN); swallow(P_LEFT); swallow(P_RIGHT);
    /* L2/R2 switch category tabs through the game's own left/right handling */
    if (l2) RD8(PAD_B + P_LEFT) = 2;
    else if (r2) RD8(PAD_B + P_RIGHT) = 2;
    if (n <= 0 || !(up | dn | lf | rt)) return;

    int cur = grid_cursor_index(L);
    int nrows = (n + cols - 1) / cols;
    int r = cur / cols, c = cur % cols, t = cur;
    if (up) {
        r = r > 0 ? r - 1 : nrows - 1;
        t = r * cols + c;
        if (t >= n) t -= cols;                    /* short last row */
    } else if (dn) {
        t = (r + 1) * cols + c;
        if (t >= n) t = c;                        /* wrap to top */
    } else if (lf) {
        int last = r * cols + cols - 1;
        if (last >= n) last = n - 1;
        t = c > 0 ? cur - 1 : last;
    } else if (rt) {
        t = cur + 1;
        if (c == cols - 1 || t >= n) t = r * cols;
    }
    if (t < 0) t = 0;
    if (t != cur) set_cursor(W, L, t);
}
