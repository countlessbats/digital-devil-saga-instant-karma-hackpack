"""Prey Eyes icons drawn from scratch (no game art): element orbs, result badges, reticle glyphs, ailments and
buff-stat glyphs. Everything is drawn at 256 px (S) and scaled down by the atlas builder."""
import math
from PIL import Image, ImageDraw, ImageFilter

S = 256
C = S // 2


def canvas():
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def star(cx, cy, r_out, r_in, n, rot=-90):
    pts = []
    for i in range(2 * n):
        r = r_out if i % 2 == 0 else r_in
        a = math.radians(rot + i * 180 / n)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def arc_band(d, box, start, end, width, fill):
    d.arc(box, start, end, fill=fill, width=width)


# ---- element orbs: silver rim, dark face, coloured glyph --------------------------------------------------------
def orb(glyph, rim=(196, 198, 210), face=(12, 12, 16)):
    im, d = canvas()
    d.ellipse((6, 6, S - 6, S - 6), fill=rim + (255,))
    d.ellipse((22, 22, S - 22, S - 22), fill=face + (255,))
    # soft top highlight on the rim
    hl = Image.new('RGBA', (S, S), (0, 0, 0, 0)); hd = ImageDraw.Draw(hl)
    hd.arc((10, 10, S - 10, S - 10), 200, 320, fill=(255, 255, 255, 170), width=10)
    im.alpha_composite(hl)
    g, gd = canvas()
    glyph(gd)
    mask = Image.new('L', (S, S), 0); ImageDraw.Draw(mask).ellipse((24, 24, S - 24, S - 24), fill=255)
    g.putalpha(Image.composite(g.getchannel('A'), Image.new('L', (S, S), 0), mask))
    im.alpha_composite(g)
    return im


def g_phys(d):                       # impact burst from the lower left
    ox, oy = 70, 190
    for i, (ang, ln, w) in enumerate(((-20, 150, 26), (-45, 170, 30), (-70, 150, 26), (-32, 120, 16), (-58, 120, 16))):
        a = math.radians(ang)
        x, y = ox + ln * math.cos(a), oy + ln * math.sin(a)
        d.polygon([(ox, oy - w / 2), (x, y), (ox + w / 2, oy)], fill=(255, 196, 40, 255))
    d.ellipse((ox - 26, oy - 26, ox + 26, oy + 26), fill=(255, 240, 170, 255))


def g_fire(d):
    d.polygon([(C, 40), (C + 62, 120), (C + 58, 176), (C, 214), (C - 58, 176), (C - 62, 120), (C - 20, 96)],
              fill=(232, 64, 24, 255))
    d.polygon([(C + 4, 100), (C + 36, 150), (C + 28, 186), (C, 204), (C - 30, 184), (C - 30, 150)],
              fill=(255, 160, 40, 255))
    d.ellipse((C - 18, 160, C + 18, 200), fill=(255, 238, 150, 255))


def g_ice(d):
    col = (170, 226, 255, 255)
    for k in range(6):
        a = math.radians(90 + k * 60)
        x, y = C + 84 * math.cos(a), C + 84 * math.sin(a)
        d.line([(C, C), (x, y)], fill=col, width=14)
        for t in (0.55,):
            bx, by = C + 84 * t * math.cos(a), C + 84 * t * math.sin(a)
            for s in (-1, 1):
                b = a + s * math.radians(40)
                d.line([(bx, by), (bx + 30 * math.cos(b), by + 30 * math.sin(b))], fill=col, width=10)
    d.ellipse((C - 14, C - 14, C + 14, C + 14), fill=(235, 250, 255, 255))


def g_elec(d):
    d.polygon([(C + 22, 34), (C - 50, 138), (C - 4, 138), (C - 28, 222), (C + 54, 110), (C + 6, 110), (C + 34, 34)],
              fill=(255, 226, 40, 255))


def g_force(d):
    col = (70, 220, 90, 255)
    for i, dx in enumerate((-40, 0, 40)):                  # three crescents sweeping right
        r = 70 - i * 6
        d.arc((C + dx - r, C - r, C + dx + r, C + r), 120, 240, fill=col, width=20 - i * 3)


def g_expel(d):
    col = (150, 236, 250, 255)
    d.ellipse((C - 78, C - 78, C + 78, C + 78), outline=col, width=14)
    d.ellipse((C - 34, C - 34, C + 34, C + 34), outline=col, width=12)
    for k in range(4):
        a = math.radians(45 + k * 90)
        d.line([(C + 38 * math.cos(a), C + 38 * math.sin(a)), (C + 74 * math.cos(a), C + 74 * math.sin(a))], fill=col, width=12)


def g_death(d):
    col = (180, 60, 220, 255)
    d.ellipse((C - 80, C - 80, C + 80, C + 80), outline=col, width=12)
    d.line(star(C, C + 4, 78, 78, 5)[::2] + [star(C, C + 4, 78, 78, 5)[0]], fill=col, width=0)
    pts = [star(C, C + 4, 76, 76, 5)[i] for i in (0, 4, 8, 2, 6, 0)]
    d.line(pts, fill=col, width=10)
    d.ellipse((C - 16, C - 12, C + 16, C + 20), fill=(90, 20, 120, 255))


ELEMENTS = {'phys': g_phys, 'fire': g_fire, 'ice': g_ice, 'elec': g_elec, 'force': g_force,
            'expel': g_expel, 'death': g_death}


def element(name):
    return orb(ELEMENTS[name])


# ---- result glyphs (shapes in white; badges colour them) ----------------------------------------------------------
def shield_pts(inset=0):
    i = inset
    return [(40 + i, 44 + i), (C, 28 + i * 1.2), (S - 40 - i, 44 + i), (S - 48 - i, 150), (C, 228 - i * 1.3), (48 + i, 150)]


def glyph(kind, col=(255, 255, 255, 255), bg=None):
    """kind: weak, resist, null, reflect, drain, normal, unknown. bg: badge fill colour or None (reticle glyph)."""
    im, d = canvas()
    if bg is not None:
        d.rounded_rectangle((8, 8, S - 8, S - 8), 36, fill=bg)
    if kind == 'weak':
        d.ellipse((34, 34, S - 34, S - 34), outline=col, width=22)
        d.rounded_rectangle((C - 13, 72, C + 13, 146), 10, fill=col)
        d.ellipse((C - 15, 160, C + 15, 190), fill=col)
    elif kind in ('resist', 'null'):
        d.polygon(shield_pts(), fill=col)
        d.polygon(shield_pts(20), fill=(0, 0, 0, 0) if bg is None else bg)
        if kind == 'resist':          # half filled
            inner = shield_pts(34)
            d.polygon([inner[0], inner[1], (C, inner[4][1]), inner[5]], fill=col)
        else:                         # filled core
            d.polygon(shield_pts(36), fill=col)
    elif kind == 'reflect':
        d.arc((56, 56, 200, 200), 200, 520, fill=col, width=24)
        d.polygon([(40, 96), (98, 70), (86, 136)], fill=col)
    elif kind == 'drain':
        for k in range(3):
            a0 = k * 120
            d.arc((50, 50, 206, 206), a0, a0 + 80, fill=col, width=20)
            d.arc((88, 88, 168, 168), a0 + 60, a0 + 130, fill=col, width=14)
        d.ellipse((C - 14, C - 14, C + 14, C + 14), fill=col)
    elif kind == 'normal':
        d.rounded_rectangle((56, C - 14, S - 56, C + 14), 8, fill=col)
    elif kind == 'unknown':
        d.arc((74, 30, 182, 138), 180, 450, fill=col, width=28)        # top of the hook, ends pointing down
        d.line([(C, 124), (C, 170)], fill=col, width=28)
        d.ellipse((C - 17, 188, C + 17, 222), fill=col)
    return im


BADGE = {  # board badge colours (glyph, background)
    'weak': ((70, 230, 70, 255), (14, 26, 14, 255)),
    'resist': ((250, 214, 40, 255), None),
    'null': ((230, 40, 40, 255), None),
    'reflect': ((255, 255, 255, 255), (200, 30, 40, 255)),
    'drain': ((255, 255, 255, 255), (200, 30, 40, 255)),
    'normal': ((235, 235, 235, 255), (30, 30, 34, 255)),
    'unknown': ((240, 240, 240, 255), (30, 30, 34, 255)),
}


def badge(kind):
    col, bg = BADGE[kind]
    return glyph(kind, col, bg)


def reticle(kind):
    return glyph(kind)


# ---- ailments (16 px on the board) -------------------------------------------------------------------------------
def ailment(name):
    im, d = canvas()
    if name == 'charm':
        d.ellipse((34, 50, 132, 148), fill=(255, 110, 180, 255)); d.ellipse((124, 50, 222, 148), fill=(255, 110, 180, 255))
        d.polygon([(38, 114), (218, 114), (C, 228)], fill=(255, 110, 180, 255))
    elif name == 'poison':
        d.polygon([(C, 24), (C + 70, 140), (C - 70, 140)], fill=(170, 80, 230, 255))
        d.ellipse((C - 74, 96, C + 74, 236), fill=(170, 80, 230, 255))
        d.ellipse((C - 40, 140, C - 6, 176), fill=(230, 190, 255, 255))
    elif name == 'mute':
        d.ellipse((30, 30, S - 30, S - 30), outline=(250, 200, 60, 255), width=26)
        d.line([(70, 70), (S - 70, S - 70)], fill=(250, 200, 60, 255), width=26)
        d.rounded_rectangle((78, 118, 178, 150), 14, fill=(250, 200, 60, 255))
    elif name == 'panic':
        col = (255, 150, 40, 255)
        for i, r in enumerate((96, 66, 36)):
            d.arc((C - r, C - r, C + r, C + r), 40 + i * 120, 330 + i * 120, fill=col, width=22)
    elif name == 'sleep':
        col = (120, 170, 255, 255)
        for (x, y, s) in ((34, 116, 104), (150, 36, 70)):
            w = max(10, s // 5)
            d.line([(x, y), (x + s, y)], fill=col, width=w)
            d.line([(x + s, y), (x, y + s)], fill=col, width=w)
            d.line([(x, y + s), (x + s, y + s)], fill=col, width=w)
    return im


# ---- buff stat glyphs (drawn into the 36 px buff box, left half) -------------------------------------------------
def stat_glyph(stat, col):
    """15 x 30 glyph image in col."""
    W, H = 60, 120
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    if stat == 'att':        # sword
        d.polygon([(30, 4), (40, 20), (38, 78), (22, 78), (20, 20)], fill=col)
        d.rectangle((8, 78, 52, 88), fill=col); d.rectangle((25, 88, 35, 110), fill=col); d.ellipse((22, 106, 38, 118), fill=col)
    elif stat == 'mag':      # wand with a star
        d.line([(12, 114), (38, 44)], fill=col, width=9)
        d.polygon(star(40, 30, 22, 9, 5), fill=col)
    elif stat == 'def':      # shield
        d.polygon([(6, 24), (30, 12), (54, 24), (50, 70), (30, 100), (10, 70)], fill=col)
        d.polygon([(16, 32), (30, 25), (44, 32), (41, 66), (30, 84), (19, 66)], fill=(0, 0, 0, 0))
    elif stat == 'acc':      # target
        d.ellipse((4, 34, 56, 86), outline=col, width=8); d.ellipse((20, 50, 40, 70), fill=col)
    return im.resize((15, 30), Image.LANCZOS)


def sheet(path):
    """Preview of every icon on a dark background."""
    items = [element(n) for n in ELEMENTS] + [badge(k) for k in BADGE] + [reticle(k) for k in BADGE] + \
            [ailment(n) for n in ('charm', 'poison', 'mute', 'panic', 'sleep')]
    cell = 96
    out = Image.new('RGBA', (cell * 7, cell * 4), (50, 50, 70, 255))
    for i, im in enumerate(items):
        out.alpha_composite(im.resize((cell - 8, cell - 8), Image.LANCZOS), ((i % 7) * cell + 4, (i // 7) * cell + 4))
    out.save(path)


if __name__ == '__main__':
    import sys
    sheet(sys.argv[1] if len(sys.argv) > 1 else 'icons_preview.png')
