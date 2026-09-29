/* DDS1 (SLUS-209.74) game addresses and helpers for mod code.
 * Compiled for the EE as MIPS III n32 (64-bit saves, 32-bit pointers); never use gp. */
#ifndef GAME_H
#define GAME_H

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#define RD32(a) (*(volatile u32 *)(u32)(a))
#define RD8(a)  (*(volatile u8 *)(u32)(a))

/* Per-task pad copy used by menus: 16 bytes in mask order
 * SQ X TRI O LEFT RIGHT UP DOWN L1 L2 R1 R2 START SELECT L3 R3; bit 0x80 = pressed this frame. */
#define MENU_PAD      0x00324530u
#define PAD_START     12

/* Menu list widget: list struct L = *(widget + 0x14).
 * L+0x10 first item, L+0x18 top visible, L+0x1c cursor item, L+0x20 count, L+0x24 cursor index,
 * L+0x30 owner work area. Items are 0x80 bytes: +0 index, +4 name, +0x58 next, +0x5c prev,
 * +0x60 skill id (0 / 0xffff = blank), +0x64 tab. */
#define LIST_FIRST    0x10
#define ITEM_NEXT     0x58
#define ITEM_PREV     0x5c
#define ITEM_ID       0x60
#define ITEM_TAG      0x7c   /* unused by the game: we store the original build position here */

/* Globals */
#define PARTY_BASE    0x003baa00u   /* u32 pointer; party record stride 0x1a4 */
#define SKILL_NAMES   0x003baa8cu   /* u32 pointer; names are 17-byte entries by skill id */

/* Game functions */
#define fn_list_finalize ((void (*)(u32, int, int))0x0027e100)
#define fn_task_work     ((u32 (*)(u32))0x00101a70)    /* task -> work area */
#define fn_skill_cost    ((u32 (*)(u32, u32))0x002862e0)
#define fn_skill_kind    ((int (*)(u32))0x002860b8)   /* 3 = passive */
#define fn_set_logic     ((int (*)(int, int, int, int, int, int, int, int))0x00279d68)

#endif
