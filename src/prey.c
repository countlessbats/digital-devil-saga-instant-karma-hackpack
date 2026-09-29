/* Prey Eyes for DDS1: affinity-coloured reticles, affinity board, buff icons, knowledge.
 * Icons come from an atlas texture (tools/atlas.py) the pnach places at ATLAS_ADDR; it is
 * loaded into VRAM with the game's own texture loader when a battle first needs it and
 * released at battle exit. A private sprite sheet built over it lets the game's sprite
 * routines draw our icons (tint, alpha, pulse all work the same). */
#include "game.h"
#include "prey_atlas.h"

#define ATLAS_ADDR   0x000A0000u
#define WBLOB        0x000BF000u      /* scratch texture blob for the white ring CLUT */
/* Knowledge lives in the unused tail of the game's save block (GBWK, 0x33600 bytes, pointer at
 * 0x3baa00), so it is written to and read from the memory card with the rest of the save.
 * u16 per species: bit n = attr n known, 0x8000 = everything known. */
#define GBWK         RD32(0x003baa00u)
#define KNOW_OFS     0x33000
#define KNOW_MAX     0x2a0
#define GP           0x003c0cf0u
#define BATTLE       RD32(GP - 0x5a0c)

/* game functions */
#define f_tex_load    ((u32 (*)(u32))0x002d3288)
#define f_tex_free    ((void (*)(u32))0x002d2cb8)
#define f_draw_c      ((void (*)(int, int, int, u32 *, int, u32, int, int))0x002bf438)
#define f_aisyo       ((u32 (*)(u32, u32, int))0x001a7410)
#define f_skill_attr  ((int (*)(u32, u32))0x001a2f00)

#define PRIO 0x53

typedef struct {
    u32 magic;
    int ret_size, ret_unknown;          /* reticle result icon size / ? size, px */
    int elem_pitch, elem_size, res_size;
    int board_y, res_dy;                /* element row y and result offset (lines*8) */
    int ail_y, ail_size, ail_gap;       /* ailment row: y, icon size, gap between groups (px) */
    int ebuf_pitch, ebuf_size;          /* enemy buffs above heads */
    int head_lift, head_dy;             /* world lift (x100) and screen offset (lines*8) */
    int pbuf_dx, pbuf_dy, pbuf_pitch, pbuf_size;   /* party buffs under the portraits */
    int help_y;                         /* battle help window y, lines (game default 406) */
    int debug_all_known;
    int debug_attr;                     /* test: 1 + attr forces the attribute used for the reticle */
} Prey;
#define PREY_MAGIC 0x50524508
#define PR ((volatile Prey *)0x000FD000)

static void prey_defaults(void)
{
    volatile Prey *p = PR;
    if (p->magic == PREY_MAGIC) return;
    p->ret_size = 28; p->ret_unknown = 22;
    /* board: centred on the name bar (x 4096), stacked above it: ailments, elements, results */
    p->elem_pitch = 0x120; p->elem_size = 16; p->res_size = 14;
    p->board_y = 254; p->res_dy = 144;
    p->ail_y = 110; p->ail_size = 15; p->ail_gap = 5;
    p->ebuf_pitch = 0x110; p->ebuf_size = 16;
    p->head_lift = 0; p->head_dy = -0x1e0;
    p->pbuf_dx = 0x1c0; p->pbuf_dy = 616; p->pbuf_pitch = 0xf0; p->pbuf_size = 14;
    p->help_y = 410;
    p->debug_all_known = 0; p->debug_attr = 0;
    p->magic = PREY_MAGIC;
}

/* ---- atlas / private sprite sheet ------------------------------------------ */

static u32 tex, tex_owner;
static u32 sheet[16];
static u8 imgs[SPR_COUNT][0x80] __attribute__((aligned(16)));
static u8 defs[SPR_COUNT][0xa0] __attribute__((aligned(16)));
static u32 texarr[1];

#define W32(p, o) (*(u32 *)((u8 *)(p) + (o)))

static void build_sheet(void)
{
    for (int i = 0; i < 16; i++) sheet[i] = 0;
    sheet[2] = SPR_COUNT;
    sheet[4] = (u32)imgs;
    sheet[6] = (u32)defs;
    sheet[7] = 1;
    sheet[9] = (u32)texarr;
    texarr[0] = tex;
    for (int i = 0; i < SPR_COUNT; i++) {
        u8 *im = imgs[i], *d = defs[i];
        for (int o = 0; o < 0x80; o++) im[o] = 0;
        for (int o = 0; o < 0xa0; o++) d[o] = 0;
        int x = atlas_rects[i][0], y = atlas_rects[i][1], w = atlas_rects[i][2], h = atlas_rects[i][3];
        W32(im, 0x14) = 0; W32(im, 0x18) = 4;
        W32(im, 0x54) = x; W32(im, 0x58) = y; W32(im, 0x5c) = x + w; W32(im, 0x60) = y + h;
        for (int c = 0; c < 4; c++) W32(im, 0x64 + 4 * c) = 0x80808080;
        W32(d, 0x0c) = w * 16; W32(d, 0x10) = h * 8;
        for (int c = 0; c < 4; c++) W32(d, 0x14 + 4 * c) = 0x80808080;
        W32(d, 0x2c) = 0x10000; W32(d, 0x30) = 1; W32(d, 0x40) = 0x10000; W32(d, 0x44) = 1;
        W32(d, 0x60) = (u32)sheet; W32(d, 0x64) = i; W32(d, 0x68) = 0x80808080;
        W32(d, 0x7c) = w; W32(d, 0x80) = h;
        for (int c = 0; c < 4; c++) W32(d, 0x84 + 4 * c) = 0x80808080;
    }
}

/* Sheet for the current battle, loading the atlas on first use. */
static u32 prey_sheet(void)
{
    u32 b = BATTLE;
    if (!b) return 0;
    if (tex && tex_owner == b) return (u32)sheet;
    if (tex) f_tex_free(tex);          /* stale from an earlier battle (should not happen) */
    tex = f_tex_load(ATLAS_ADDR);
    tex_owner = b;
    if (!tex) return 0;
    build_sheet();
    return (u32)sheet;
}

static u32 wtex, wsheet_src, wowner;

/* Replaces btlExit's call to 0x1f2618 (0x1a1158): release our textures with the battle. */
#define f_exit_orig ((void (*)(void))0x001f2618)
void prey_battle_exit(void)
{
    if (tex) { f_tex_free(tex); tex = 0; tex_owner = 0; }
    if (wtex) { f_tex_free(wtex); wtex = 0; wsheet_src = 0; wowner = 0; }
    f_exit_orig();
}

/* sprite vertex colours are 0xRRGGBBAA (0x80 = full) */
static u32 rgba(int r, int g, int b, int a) { return (u32)r << 24 | (u32)g << 16 | (u32)b << 8 | (u32)a; }

static u32 mulc(u32 c, u32 t)
{
    u32 o = 0;
    for (int s = 0; s < 32; s += 8) {
        u32 v = (((c >> s) & 0xff) * ((t >> s) & 0xff)) >> 7;
        if (v > 0xff) v = 0xff;
        o |= v << s;
    }
    return o;
}

/* Draw sprite `spr` of our atlas scaled to `size` px square with corner colours. */
static void icon(int spr, int x, int y, int size, u32 col)
{
    u32 sh = prey_sheet();
    if (!sh) return;
    u8 *d = defs[spr];
    W32(d, 0x0c) = size * 16; W32(d, 0x10) = size * 8;
    u32 cols[4] = { col, col, col, col };
    f_draw_c(x, y, -1, cols, 0, sh, spr, PRIO);
}

/* ---- affinity / knowledge --------------------------------------------------- */

enum { R_NONE, R_UNKNOWN, R_WEAK, R_NORMAL, R_RESIST, R_NULL, R_REFLECT, R_DRAIN };

#define KNOW ((volatile u16 *)(GBWK + KNOW_OFS))

static int unit_species(u32 u) { return *(volatile u16 *)(u + 0x124); }
static int is_enemy(u32 u) { return (RD32(u + 0x110) & 0x400) != 0; }

static int known(u32 u, int attr)
{
    if (!is_enemy(u) || PR->debug_all_known) return 1;
    int s = unit_species(u);
    if (s >= KNOW_MAX) return 1;
    u16 k = KNOW[s];
    return (k & 0x8000) || (k & (1u << attr));
}

void prey_learn(u32 u, int attr)
{
    if (!u || !is_enemy(u)) return;
    int s = unit_species(u);
    if (s >= KNOW_MAX) return;
    KNOW[s] = KNOW[s] | (attr < 0 ? 0x8000 : (1u << attr));
}

static int raw_result(u32 u, int attr)
{
    u32 a = f_aisyo(u, 0, attr);
    if (a & 0x40000) return R_DRAIN;
    if (a & 0x20000) return R_REFLECT;
    if (a & 0x10000) return R_NULL;
    int pct = a & 0xffff;
    if (pct == 0) return R_NULL;
    if (pct > 100) return R_WEAK;
    if (pct < 100) return R_RESIST;
    return R_NORMAL;
}

static int result(u32 u, int attr)
{
    if (attr < 0 || attr > 14 || attr == 7) return attr == 7 ? R_NORMAL : R_NONE;
    if (!known(u, attr)) return R_UNKNOWN;
    return raw_result(u, attr);
}

static u32 action_rec(void)
{
    u32 b = BATTLE;
    return b ? RD32(b + 0x164) : 0;
}

static int action_attr(void)
{
    if (PR->debug_attr) return PR->debug_attr - 1;
    u32 rec = action_rec();
    if (!rec) return -1;
    u32 actor = RD32(rec + 0x18), skill = RD32(rec + 0x24);
    if (!actor || skill == 0xffffffff) return -1;
    return f_skill_attr(actor, skill) & 0xff;
}

static const int result_spr_ret[8] = { -1, SPR_RET_UNKNOWN, SPR_RET_WEAK, SPR_RET_NORMAL, SPR_RET_RESIST,
                                       SPR_RET_NULL, SPR_RET_REFLECT, SPR_RET_DRAIN };
static const int result_spr_res[8] = { -1, SPR_RES_UNKNOWN, SPR_RES_WEAK, SPR_RES_NORMAL, SPR_RES_RESIST,
                                       SPR_RES_NULL, SPR_RES_REFLECT, SPR_RES_DRAIN };

static u32 result_tint(int r)
{
    if (r == R_WEAK) return rgba(0x30, 0x80, 0x30, 0x80);
    if (r >= R_RESIST) return rgba(0x80, 0x20, 0x20, 0x80);
    return rgba(0x80, 0x80, 0x80, 0x80);
}

/* ---- reticle ---------------------------------------------------------------- */

static u32 cur_target;

/* Replaces the per-target reticle call at 0x1c1100: remember which unit is being drawn. */
#define f_reticle ((void (*)(u32, u32, int))0x001bfde0)
static void draw_board(u32 u);
void prey_reticle(u32 unit, u32 work, int i)
{
    prey_defaults();
    cur_target = unit;
    f_reticle(unit, work, i);
    cur_target = 0;
    u32 list = RD32(work + 0x10);
    if (list && RD32(list + 4) == 1 && unit && is_enemy(unit)) draw_board(unit);
}

/* White copy of the game's reticle ring (normal hits). The ring sprites live in an 8-bit
 * paletted texture. We load a tiny texture whose CLUT is the game's CLUT turned white, then draw
 * the game's own ring sprite through a copy of its sheet whose texture object uses that CLUT:
 * same sprite, same pulse, white instead of red. */
static u32 wsheet[16], wtexarr[8];
static u32 wobj[16] __attribute__((aligned(16)));
static u32 wgs[16] __attribute__((aligned(16)));

static u32 white_sheet(u32 sh)
{
    u32 bt = BATTLE;
    if (wtex && wsheet_src == sh && wowner == bt) return (u32)wsheet;
    u32 gimg = RD32(sh + 0x10) + 0x1a * 0x80;
    u32 ti = RD32(gimg + 0x14);
    if (ti >= 8 || RD32(sh + 0x1c) > 8) return 0;
    u32 gobj = RD32(RD32(sh + 0x24) + ti * 4);
    u32 clut = gobj ? RD32(gobj + 0x30) : 0;
    if (!clut || !RD32(gobj + 0x28)) return 0;
    volatile u8 *b = (volatile u8 *)WBLOB;
    for (int i = 0; i < 0x40; i++) b[i] = 0;
    b[0x10] = 1; b[0x12] = 16; b[0x14] = 16; b[0x16] = 0x13;
    for (int i = 0; i < 256; i++) {
        u32 c = RD32(clut + i * 4);
        u32 r = c & 0xff, g = (c >> 8) & 0xff, bl = (c >> 16) & 0xff;
        u32 m = r > g ? r : g;
        if (bl > m) m = bl;
        m = m * 3 / 2;
        if (m > 255) m = 255;
        b[0x40 + i * 4] = m; b[0x41 + i * 4] = m; b[0x42 + i * 4] = m; b[0x43 + i * 4] = c >> 24;
    }
    for (int i = 0; i < 256; i++) b[0x440 + i] = 0;
    if (wtex) f_tex_free(wtex);
    wtex = f_tex_load(WBLOB);
    wowner = bt;
    if (!wtex || !RD32(wtex + 0x14)) return 0;
    u32 cbp = RD32(RD32(wtex + 0x14) + 0xc) >> 6;
    for (int i = 0; i < 16; i++) { wobj[i] = RD32(gobj + i * 4); wgs[i] = RD32(RD32(gobj + 0x28) + i * 4); }
    wobj[0x28 / 4] = (u32)wgs;
    wgs[0x24 / 4] = (wgs[0x24 / 4] & ~(0x3fffu << 5)) | (cbp << 5);   /* TEX0.CBP (bits 37..50) */
    for (int i = 0; i < 16; i++) wsheet[i] = RD32(sh + i * 4);
    for (u32 i = 0; i < RD32(sh + 0x1c); i++) wtexarr[i] = RD32(RD32(sh + 0x24) + i * 4);
    wtexarr[ti] = (u32)wobj;
    wsheet[9] = (u32)wtexarr;
    wsheet_src = sh;
    return (u32)wsheet;
}

/* Replaces the ring draw calls at 0x1c00ec: sprites 0x1a/0x1b (red inner ring + its pulse).
 * Normal hits keep the game's ring, drawn white; other results draw the result icon in its
 * place. The caller has already scaled the ring's definition for the pulse (0x1c2e90) and
 * offset (x, y) to keep it centred, so the icon uses the same centre and scale. */
#define f_draw438 ((void (*)(int, int, int, u32 *, int, u32, int, int))0x002bf438)
void prey_ring(int x, int y, int z, u32 *cols, int flags, u32 sh, int spr, int prio)
{
    if ((spr == 0x1a || spr == 0x1b) && cur_target && is_enemy(cur_target)) {
        int r = result(cur_target, action_attr());
        if (r == R_NORMAL) {
            u32 ws = white_sheet(sh);
            if (ws) { f_draw438(x, y, z, cols, flags, ws, spr, prio); return; }
        } else if (r != R_NONE && prey_sheet()) {
            u32 gd = RD32(sh + 0x18) + spr * 0xa0;
            if (RD32(gd + 0x9c)) gd = RD32(gd + 0x9c);
            int gw = (int)RD32(gd + 0x0c), gh = (int)RD32(gd + 0x10);
            int cx = x + gw / 2, cy = y + gh / 2;
            int s = r == R_UNKNOWN ? PR->ret_unknown : PR->ret_size;
            int sw = s * 16 * gw / 0x1f0, shh = s * 8 * gw / 0x1f0;   /* scale with the pulse */
            u32 t = result_tint(r);
            u32 c2[4];
            for (int k = 0; k < 4; k++) c2[k] = mulc(cols[k], t);
            u8 *d = defs[result_spr_ret[r]];
            W32(d, 0x0c) = sw; W32(d, 0x10) = shh;
            f_draw_c(cx - sw / 2, cy - shh / 2, z, c2, 0, prey_sheet(), result_spr_ret[r], prio);
            return;
        }
    }
    f_draw438(x, y, z, cols, flags, sh, spr, prio);
}

/* ---- affinity board --------------------------------------------------------- */

static const int board_attrs[9] = { 0, 1, 2, 3, 4, 5, 6, 8, 9 };
static const int board_elem[9] = { SPR_ELEM_PHYS, SPR_ELEM_GUN, SPR_ELEM_FIRE, SPR_ELEM_ICE, SPR_ELEM_ELEC,
                                   SPR_ELEM_FORCE, SPR_ELEM_EARTH, SPR_ELEM_EXPEL, SPR_ELEM_DEATH };
static const int ail_attrs[5] = { 10, 11, 12, 13, 14 };
static const int ail_spr[5] = { SPR_AIL_CHARM, SPR_AIL_POISON, SPR_AIL_MUTE, SPR_AIL_PANIC, SPR_AIL_SLEEP };

static int buff_icon(int stat, int lv)
{
    static const int base[4] = { SPR_BUFF_ATT_UP1, SPR_BUFF_MAG_UP1, SPR_BUFF_DEF_UP1, SPR_BUFF_ACC_UP1 };
    if (lv == 0) return -1;
    if (lv > 4) lv = 4;
    if (lv < -4) lv = -4;
    return lv > 0 ? base[stat] + lv - 1 : base[stat] + 4 + (-lv) - 1;
}

/* buff stats shown: attack (0), magic (1), defense (3), hit/evasion (2) */
static const int buff_stat_idx[4] = { 0, 1, 3, 2 };
static int buff_level(u32 u, int k) { return *(volatile short *)(u + 0x2c6 + buff_stat_idx[k] * 6); }

static void draw_buffs(u32 u, int x, int y, int pitch, int size)
{
    int n = 0;
    for (int k = 0; k < 4; k++) {
        int spr = buff_icon(k, buff_level(u, k));
        if (spr < 0) continue;
        icon(spr, x + n * pitch, y, size, rgba(0x80, 0x80, 0x80, 0x80));
        n++;
    }
}

static void draw_board(u32 u)
{
    volatile Prey *p = PR;
    if (!prey_sheet()) return;
    u32 white = rgba(0x80, 0x80, 0x80, 0x80);
    int attr_now = action_attr();
    int x0 = 4096 - (8 * p->elem_pitch + p->elem_size * 16) / 2;
    for (int i = 0; i < 9; i++) {
        int x = x0 + i * p->elem_pitch;
        int hl = board_attrs[i] == attr_now;
        icon(board_elem[i], x, p->board_y, p->elem_size, hl ? white : rgba(0x70, 0x70, 0x70, 0x80));
        int spr = result_spr_res[result(u, board_attrs[i])];
        if (spr >= 0) icon(spr, x + (p->elem_size - p->res_size) * 8, p->board_y + p->res_dy, p->res_size, white);
    }
    /* ailments: only those that are not a plain hit (unknown ones show ?), centred */
    int show[5], n = 0;
    for (int i = 0; i < 5; i++) {
        int r = result(u, ail_attrs[i]);
        if (r != R_NORMAL && r != R_NONE) show[n++] = i;
    }
    int gw = (p->ail_size * 2 + 2) * 16, pitch = gw + p->ail_gap * 16;
    int ax = 4096 - (n * pitch - p->ail_gap * 16) / 2;
    for (int k = 0; k < n; k++) {
        int i = show[k], x = ax + k * pitch;
        icon(ail_spr[i], x, p->ail_y, p->ail_size, white);
        int spr = result_spr_res[result(u, ail_attrs[i])];
        if (spr >= 0) icon(spr, x + (p->ail_size + 2) * 16, p->ail_y, p->ail_size, white);
    }
}

/* ---- enemy buffs above their heads ------------------------------------------------ */

/* VU0 helpers: save/restore vf10, the vector the game's projection routines work on. */
__asm__(".text\n.set push\n.set noreorder\n"
        ".globl vf10_save\nvf10_save:\n .word 0xF88A0000\n jr $31\n nop\n"
        ".globl vf10_load\nvf10_load:\n .word 0xD88A0000\n jr $31\n nop\n.set pop\n");
void vf10_save(void *p);
void vf10_load(void *p);
#define f_unit_anchor ((int (*)(u32, int))0x001d6360)   /* bone 0 position into vf10, 0 if none */
#define f_unit_pos    ((void (*)(u32))0x001f6498)       /* unit position into vf10 */
#define f_project     ((int (*)(int *))0x001f6158)      /* vf10 -> screen px/lines, 0 if off */

static float vbuf[4] __attribute__((aligned(16)));

/* Screen position for a unit's overhead row. The unit's root position is stable; its bone 0
 * (roughly the chest/head) bobs with the idle animation. So the height above the root is
 * measured once, the first time the unit is seen, and the row follows the root. */
static u32 hp_unit[16];
static int hp_dy[16];

static int project_root(u32 u, int *sx, int *sy)
{
    int out[4];
    f_unit_pos(u);
    if (PR->head_lift) {
        vf10_save(vbuf);
        vbuf[1] -= (float)PR->head_lift / 100.0f;
        vf10_load(vbuf);
    }
    if (!f_project(out)) return 0;
    *sx = out[0]; *sy = out[1];
    return 1;
}

static int head_pos(int slot, u32 u, int *sx, int *sy)
{
    if (!project_root(u, sx, sy)) return 0;
    if (hp_unit[slot] != u) {
        int out[4];
        hp_unit[slot] = u; hp_dy[slot] = 0;
        if (f_unit_anchor(u, 0) && f_project(out)) hp_dy[slot] = out[1] - *sy;
    }
    *sy += hp_dy[slot];
    return 1;
}

static void draw_enemy_buffs(void)
{
    volatile Prey *p = PR;
    u32 b = BATTLE;
    if (!b || !prey_sheet()) return;
    int n = 0;
    for (u32 u = RD32(b + 0x228); u && n < 16; u = RD32(u + 0x344), n++) {
        if (!is_enemy(u) || *(volatile u16 *)(u + 0x126) == 0 || !(RD32(u + 0x110) & 1)) continue;
        int cnt = 0;
        for (int k = 0; k < 4; k++) if (buff_level(u, k)) cnt++;
        if (!cnt) continue;
        int sx, sy;
        if (!head_pos(n, u, &sx, &sy)) continue;
        int w = (cnt - 1) * p->ebuf_pitch + p->ebuf_size * 16;
        draw_buffs(u, sx * 16 - w / 2, sy * 8 + p->head_dy, p->ebuf_pitch, p->ebuf_size);
    }
}

static void learn_kills(void);

/* ---- party buffs: replaces the per-member panel call at 0x1b6e84 -------------- */

#define f_panel_part ((void (*)(u32, u32, int))0x001bb6c8)
void prey_party_panel(u32 unit, u32 work, int slot)
{
    prey_defaults();
    f_panel_part(unit, work, slot);
    volatile Prey *p = PR;
    if (slot == 0) {
        learn_kills();
        draw_enemy_buffs();
        u32 hw = RD32(GP - 0x5914);                 /* battle help window: +0x3c = y (lines) */
        if (hw && p->help_y) RD32(hw + 0x3c) = p->help_y;
    }
    int off = (slot * 41) * 16;                       /* ((5*slot)*8 + slot) * 16 */
    int x = (int)RD32(work + 0xc + off), y = (int)RD32(work + 0x10 + off);
    draw_buffs(unit, x * 16 + p->pbuf_dx, y * 8 + p->pbuf_dy, p->pbuf_pitch, p->pbuf_size);
}

/* ---- learning ------------------------------------------------------------------ */

#define ANALYZE 0xbf
#define MENU_X  0x00324531u     /* menu pad copy: X, bit 0x80 = pressed this frame */

/* Called from the target panel input hook (battle_buttons.c) around the game's handler. When X
 * confirms a target, learn the skill's attribute for every selected enemy (Analyze teaches
 * everything). */
void prey_after_target_input(u32 work, int confirm)
{
    if (!confirm) return;
    int st = (int)RD32(work);
    if (st != 3 && st != 5) return;
    u32 rec = action_rec();
    if (!rec) return;
    u32 skill = RD32(rec + 0x24);
    int attr = action_attr();
    u32 list = RD32(work + 0x10);
    if (!list) return;
    int n = (int)RD32(list + 4);
    for (int i = 0; i < n; i++) {
        u32 u = RD32(RD32(list + 8) + i * 4);
        if (skill == ANALYZE) prey_learn(u, -1);
        else if (attr >= 0 && attr <= 14) prey_learn(u, attr);
    }
}

/* Called every battle frame from the party panel hook: an enemy whose HP drops from above 0
 * to 0 was killed, which reveals everything about its species. */
static u32 kill_unit[16];
static u16 kill_hp[16];
static void learn_kills(void)
{
    u32 b = BATTLE;
    if (!b) return;
    int n = 0;
    for (u32 u = RD32(b + 0x228); u && n < 16; u = RD32(u + 0x344), n++) {
        u16 hp = *(volatile u16 *)(u + 0x126);
        if (kill_unit[n] == u && kill_hp[n] > 0 && hp == 0 && is_enemy(u)) prey_learn(u, -1);
        kill_unit[n] = u; kill_hp[n] = hp;
    }
}
