/* Prey Eyes skill-hover preview (after SMTV and Prey Eyes 2 for Nocturne): while the battle command menu waits for
 * a command and its cursor is on Attack or a skill, every enemy shows the result that attack would have on it, as
 * the reticle's result icon alone (no ring), at the point the game centres its reticle on. Only known, notable
 * results are shown (weak, resist, null, reflect, drain); normal and unknown show nothing. Once a command is chosen
 * the preview gives way to the game's targeting, where the full Prey Eyes reticle shows on the targeted enemy.
 *
 * Command menu (see battle_buttons.c): work = command panel task (id gp-0x594c) work, +0 state (2 = waiting for a
 * command), tab at *(gp-0x5910)+1, list kind 0x1bd0d0(work, tab), cursor short at work+6+kind*4. The FIGHT list
 * (kind 0) is Attack, then the skills 0x1bd2c0(work, &count, 0, 2, 3) returns (row r = skill r-1), then Pass
 * (row = count). Attribute: 0x1a2f00(actor, skill), actor = *(work+0x2c)+0x18; skill 0 is the actor's attack. */
#include "game.h"
#include "prey_int.h"
#include "prey_atlas.h"

#define GP            0x003c0cf0u
#define f_task_by_id  ((u32 (*)(u32))0x00101858)
#define f_list_kind   ((int (*)(u32, int))0x001bd0d0)
#define f_skill_list  ((u32 (*)(u32, short *, int, int, int))0x001bd2c0)
#define f_skill_attr  ((int (*)(u32, u32))0x001a2f00)
#define f_unit_anchor ((int (*)(u32, int))0x001d6360)   /* bone 0 position into vf10, 0 if none */
#define f_unit_pos    ((void (*)(u32))0x001f6498)       /* unit position into vf10 */
int ws_project(int *out);                          /* 0x1f6158, widescreen-aware (widescreen.c) */
#define f_project     ws_project
#define TARGET_PANEL  0x003a1eb0u                        /* task id "btl_panel_target" */

static float pv[4] __attribute__((aligned(16)));

/* Attribute of the command under the cursor, or -1 (not waiting for a command, not the FIGHT list, Pass). */
static int hover_attr(void)
{
    u32 t = f_task_by_id(RD32(GP - 0x594c));
    u32 w = t ? fn_task_work(t) : 0;
    u32 tp = RD32(GP - 0x5910);
    if (!w || RD32(w) != 2 || !tp || f_task_by_id(TARGET_PANEL)) return -1;
    int kind = f_list_kind(w, RD8(tp + 1));
    if (kind != 0) return -1;
    int row = *(volatile short *)(w + 6 + kind * 4);
    u32 owner = RD32(w + 0x2c);
    u32 actor = owner ? RD32(owner + 0x18) : 0;
    if (!actor) return -1;
    u32 skill = 0;                                        /* row 0: Attack */
    if (row > 0) {
        short count = 0;
        u32 list = f_skill_list(w, &count, 0, 2, 3);
        if (!list || row >= count) return -1;             /* row == count: Pass */
        skill = *(volatile u16 *)(list + (row - 1) * 2);
    }
    PR->preview_row = row; PR->preview_skill = (int)skill;
    return f_skill_attr(actor, skill) & 0xff;
}

/* Where the game centres its reticle on unit u (as 0x1bfde0 does), in px / lines. */
static int reticle_point(u32 u, int *sx, int *sy)
{
    if (!f_unit_anchor(u, 0)) {
        f_unit_pos(u);
    } else if (RD32(u + 0x110) & 0x8000000) {
        f_unit_pos(u);
        vf10_save(pv); pv[1] = -24.0f; vf10_load(pv);
    }
    int out[4];
    if (!f_project(out)) return 0;
    *sx = out[0]; *sy = out[1];
    return 1;
}

/* Test only: while 0xFD0FC = 'AFF!', the first two live enemies' affinity per attribute 0..9 goes to 0xFD100:
 * per enemy 10 x {aisyo, base, modifier}. */
#define f_aisyo_t ((u32 (*)(u32, u32, int))0x001a7410)
#define f_aff_base ((u32 (*)(u32, int))0x001a2f50)
#define f_aff_mod  ((u32 (*)(u32, int))0x001a51c8)
static void affinity_dump(u32 b)
{
    if (RD32(0xFD0FCu) != 0x21464641u) return;
    int e = 0, n = 0;
    for (u32 u = RD32(b + 0x228); u && n < 16 && e < 2; u = RD32(u + 0x344), n++) {
        if (!is_enemy(u) || *(volatile u16 *)(u + 0x126) == 0) continue;
        for (int a = 0; a < 10; a++) {
            u32 at = 0xFD100u + (e * 10 + a) * 12;
            RD32(at) = f_aisyo_t(u, 0, a); RD32(at + 4) = f_aff_base(u, a); RD32(at + 8) = f_aff_mod(u, a);
        }
        e++;
    }
}

/* Called every battle frame from the party panel draw (prey.c). */
void prey_preview(void)
{
    volatile Prey *p = PR;
    u32 b = BATTLE_WORK;
    if (b) affinity_dump(b);
    int attr = (b && p->preview) ? hover_attr() : -1;
    if (attr >= 0 && p->debug_attr) attr = p->debug_attr - 1;   /* test: force an attribute */
    p->preview_attr = attr;
    if (attr < 0) return;
    int n = 0;
    for (u32 u = RD32(b + 0x228); u && n < 16; u = RD32(u + 0x344), n++) {
        if (!is_enemy(u) || *(volatile u16 *)(u + 0x126) == 0 || !(RD32(u + 0x110) & 1)) continue;
        int r = prey_result(u, attr);
        if (r < R_WEAK || r == R_NORMAL) continue;        /* none, unknown, normal: nothing to say */
        int sx, sy;
        if (!reticle_point(u, &sx, &sy)) continue;
        int s = p->ret_size;
        int spr = r == R_RESIST ? SPR_RES_RESIST : prey_result_spr_ret[r];
        u32 tint = r == R_RESIST ? rgba(0x80, 0x80, 0x80, 0x80) : prey_result_tint(r);
        prey_icon(spr, sx * 16 - s * 8, sy * 8 - s * 4, s, tint);
    }
}
