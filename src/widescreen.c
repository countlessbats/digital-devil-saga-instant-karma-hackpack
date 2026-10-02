/* Widescreen: 3D is widened to 16:9 through the camera aspect (0x3245e4); this keeps the 2D interface at
 * its 4:3 proportions by squeezing it horizontally about the screen centre. Where WideToggle is on
 * (FEATURES[12]), d-pad DOWN on the field switches between widescreen and the original 4:3 (ws_toggle).
 *
 * Every finished draw packet is attached to one of the 16 draw layers through the layer's insert function
 * (table 0x324b58 + layer * 0x20, rebuilt every frame by 0x2d4240 / 0x2efd30 with 0x2d41c0 or 0x2efb30).
 * The pnach makes those set-ups store ws_ins_a / ws_ins_b instead, which rewrite the X of every vertex the
 * CPU built for the packet before passing it on. 3D models go to VU1 as UNPACK data and are never touched;
 * only GIF vertices (XYZ2/XYZF2/XYZ3/XYZF3, packed, A+D or reglist) built this frame in the packet heap
 * (gp-0x39d8 / gp-0x39d4, size gp-0x39dc) are, with any clip box (scissor) set alongside them.
 * 2D placed at a projected 3D point gets its projection spread first (ws_project), so it lands back on the
 * point after the squeeze; ws_world leaves a whole drawing alone (the minimap). */
#include "game.h"

#define GP        0x003c0cf0u
#define FEATURES  ((volatile u32 *)0x000FD200)   /* [11] Widescreen, [12] WideToggle */
#define ASPECT    0x003245e4u     /* camera aspect */
#define WIDE      0x3fd3a06du     /* 1.653: 16:9 */
#define NARROW    0x3f951eb8u     /* 1.165: the game's own */
#define INS_A     0x002d41c0u
#define INS_B     0x002efb30u

#define X_LEFT    0x7000          /* GS X of screen column 0 (1/16 px) */
#define X_RIGHT   0x9000          /* column 512 */
#define X_MID     0x8000

int ws_world;                     /* nonzero: 2D placed at a projected 3D point, leave it alone */
static int ws_noedge;             /* nonzero: squeeze even vertices on a screen edge (see ws_label) */
static int ws_off;                /* switched to 4:3 by WideToggle */

int ws_active(void) { return FEATURES[11] && !ws_off; }
void ws_toggle(void) { ws_off = !ws_off; }

typedef void (*ins_fn)(u32, u32);
void ws_ins_a(u32 layer, u32 obj);
void ws_ins_b(u32 layer, u32 obj);

static int in_heap(u32 a)
{
    u32 sz = RD32(GP - 0x39dc), h0 = RD32(GP - 0x39d8), h1 = RD32(GP - 0x39d4);
    return (a >= h0 && a < h0 + sz) || (a >= h1 && a < h1 + sz);
}

static int is_xyz(u32 reg) { return reg == 4 || reg == 5 || reg == 0xc || reg == 0xd; }

/* A vertex on a screen edge (within 2 px) stays there, so masks, edge-to-edge bars, fades and tiled
 * overlays still reach the edges while everything else, including sprites hanging off screen, is squeezed. */
#define EDGE 32
static u32 squeeze(u32 x)
{
    int v = (int)(x & 0xffff);
    if (!ws_noedge && ((v >= X_LEFT - EDGE && v <= X_LEFT + EDGE) || (v >= X_RIGHT - EDGE && v <= X_RIGHT + EDGE))) return (u32)v;
    return (u32)(X_MID + (v - X_MID) * 3 / 4) & 0xffff;
}

/* X is bits 0-15 of the first word in both packed and reglist form */
static void vtx(u32 a) { u32 w = RD32(a); RD32(a) = (w & 0xffff0000u) | squeeze(w); }

/* SCISSOR_1/2 (A+D 0x40/0x41), in pixels: a clip box narrower than the screen is squeezed with what it clips. */
static void scissor(u32 a)
{
    u32 w = RD32(a);
    int x0 = w & 0x7ff, x1 = (w >> 16) & 0x7ff;
    if (x0 == 0 && x1 >= 511) return;
    x0 = 256 + (x0 - 256) * 3 / 4;
    x1 = 256 + (x1 - 256 + 1) * 3 / 4;
    RD32(a) = (w & 0xf800f800u) | (u32)x0 | (u32)x1 << 16;
}

/* One GIF tag and its data; returns the address after it. */
static u32 gif_tag(u32 p, u32 end)
{
    u32 a = RD32(p), c = RD32(p + 4), rl = RD32(p + 8), rh = RD32(p + 12);
    int nloop = a & 0x7fff, flg = (c >> 26) & 3, nreg = (c >> 28) & 0xf;
    if (!nreg) nreg = 16;
    p += 16;
    if (flg == 0) {                                   /* PACKED: one quadword per register */
        for (int l = 0; l < nloop; l++)
            for (int r = 0; r < nreg; r++, p += 16) {
                if (p >= end) return end;
                u32 reg = (r < 8 ? rl >> (r * 4) : rh >> ((r - 8) * 4)) & 0xf;
                if (reg == 0xe) {                                /* A+D */
                    u32 addr = RD32(p + 8) & 0xff;
                    if (addr == 0x40 || addr == 0x41) { scissor(p); continue; }
                    reg = addr;
                }
                if (is_xyz(reg)) vtx(p);
            }
        return p;
    }
    if (flg == 1) {                                   /* REGLIST: one doubleword per register */
        u32 q = p;
        for (int l = 0; l < nloop; l++)
            for (int r = 0; r < nreg; r++, q += 8) {
                if (q >= end) return end;
                u32 reg = (r < 8 ? rl >> (r * 4) : rh >> ((r - 8) * 4)) & 0xf;
                if (is_xyz(reg)) vtx(q);
            }
        return p + ((q - p + 15) & ~15u);
    }
    return p + nloop * 16;                            /* IMAGE: texture data */
}

static void gif_stream(u32 p, u32 end)
{
    while (p + 16 <= end) {
        u32 next = gif_tag(p, end);
        if (next <= p) break;
        p = next;
    }
}

/* Walk the packet's DMA chain (built by 0x2e1428 and linked by 0x2d4038: VIF DIRECT + GIF data after each
 * tag). Only data built this frame in the packet heap is touched; data a packet refers to elsewhere may be
 * shared or in use and is left alone. The walk stops at the first tag that is not DIRECT, so 3D objects
 * (long chains of VU1 data) cost one look. */
static void ws_packet(u32 obj)
{
    u32 t = RD32(obj + 4) & 0x0ffffff0u, tail = RD32(obj + 8) & 0x0ffffff0u;
    for (int n = 0; t && n < 256; n++) {
        u32 lo = RD32(t), qwc = lo & 0xffff, id = (lo >> 28) & 7, vif = RD32(t + 12);
        if (((vif >> 24) & 0x7f) != 0x50 || !in_heap(t)) break;
        gif_stream(t + 16, t + 16 + (vif & 0xffff) * 16);
        if (t == tail) break;
        if (id == 2) t = RD32(t + 4) & 0x0ffffff0u;    /* next */
        else if (id == 1) t = t + 16 + qwc * 16;        /* cnt */
        else break;
    }
}

static void ws_ins(u32 layer, u32 obj, u32 orig)
{
    int on = ws_active();
    RD32(ASPECT) = on ? WIDE : NARROW;
    if (on && !ws_world && obj) ws_packet(obj);
    ((ins_fn)orig)(layer, obj);
}
void ws_ins_a(u32 layer, u32 obj) { ws_ins(layer, obj, INS_A); }
void ws_ins_b(u32 layer, u32 obj) { ws_ins(layer, obj, INS_B); }

/* The game's 3D-to-screen projection (0x1f6158: vf10 -> screen px/lines, 0 if off screen), used by the
 * battle reticle (call at 0x1bff7c) and by Prey Eyes. X is spread by 4/3 about the centre so that, after
 * the interface squeeze, things drawn there land on the 3D point again. */
#define f_project ((int (*)(int *))0x001f6158)
int ws_project(int *out)
{
    int r = f_project(out);
    if (r && ws_active()) out[0] = 256 + (out[0] - 256) * 4 / 3;
    return r;
}

/* Field minimap (0x149810, called at 0x149f84): its corridors come from data the packets refer to, which
 * can't be rewritten safely, so the whole minimap is left as the game draws it (at the right edge). */
#define f_minimap ((void (*)(u32, u32, u32, u32, u32, u32, u32, u32))0x00149810)
void ws_minimap(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7)
{
    ws_world = 1;
    f_minimap(a0, a1, a2, a3, a4, a5, a6, a7);
    ws_world = 0;
}

/* The menu's big HP/MP labels beside each party member (sprite call at 0x284574) end exactly at the right edge;
 * under the edge rule their right side would stay there and stretch them, so they are squeezed whole. */
#define f_sprite_a ((void (*)(u32, u32, u32, u32, u32, u32, u32, u32))0x002bf4e0)
void ws_label(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7)
{
    ws_noedge = 1;
    f_sprite_a(a0, a1, a2, a3, a4, a5, a6, a7);
    ws_noedge = 0;
}
