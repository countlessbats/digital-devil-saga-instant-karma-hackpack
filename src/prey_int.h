/* Shared between prey.c and prey_preview.c. */
#ifndef PREY_INT_H
#define PREY_INT_H
#include "game.h"

typedef struct {
    u32 magic;
    int ret_size, ret_unknown;          /* reticle result icon size / ? size, px */
    int elem_pitch, elem_size, res_size;
    int board_y, res_dy;                /* element row y and result offset (lines*8) */
    int ail_y, ail_size, ail_gap;       /* ailment row: y, icon size, gap between groups (px) */
    int ebuf_pitch, ebuf_size;          /* enemy buffs above heads */
    int head_lift, head_dy;             /* overhead row: bone-0 height scale (x100), extra world lift */
    int pbuf_dx, pbuf_dy, pbuf_pitch, pbuf_size;   /* party buffs under the portraits */
    int help_y;                         /* battle help window y, lines (game default 406) */
    int debug_all_known;
    int head_axis;
    int debug_attr;                     /* test: 1 + attr forces the attribute used for the reticle */
    int head_bone;                      /* model anchor for the overhead row (2 = the game's HP/MP popup point) */
    int preview;                        /* 1: skill-hover preview of each enemy's result (prey_preview.c) */
    int preview_attr;                   /* diagnostic: attribute of the hovered command, -1 none */
    int preview_row, preview_skill;     /* diagnostic: hovered row and skill id */
} Prey;
#define PREY_MAGIC 0x5052450d
#define PR ((volatile Prey *)0x000FD000)

enum { R_NONE, R_UNKNOWN, R_WEAK, R_NORMAL, R_RESIST, R_NULL, R_REFLECT, R_DRAIN };

int is_enemy(u32 u);
int prey_result(u32 u, int attr);                   /* R_* for unit u hit by attribute attr (knowledge applied) */
void prey_icon(int spr, int x, int y, int size, u32 col);   /* atlas sprite, x/y in 1/16 px and 1/8 lines */
u32 prey_result_tint(int r);
u32 rgba(int r, int g, int b, int a);
extern const int prey_result_spr_ret[8];
void vf10_save(void *p);
void vf10_load(void *p);
#define BATTLE_WORK  RD32(0x003c0cf0u - 0x5a0c)

#endif
