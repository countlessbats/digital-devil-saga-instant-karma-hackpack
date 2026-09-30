"""Build the Prey Eyes icon atlas as a PS2 texture in the game's image format.

Sources: every icon is drawn in code (tools/icons.py and the Gun/Earth orbs and buff boxes here); no game art.
Outputs:
  build/prey_atlas.bin   0x40-byte header + CLUT (256 x RGBA32, CSM1 order) + 8-bit pixels
  build/prey_atlas.png   preview
  src/prey_atlas.h       sprite ids + rects
The header layout matches what the game's texture loader 0x2d3288 reads:
  +0x10 has-CLUT flag (1), +0x11 CLUT psm (0 = CT32), +0x12 width, +0x14 height, +0x16 psm (0x13 = PSMT8), +0x17 mip count,
  data at +0x40 (byte[1] & 0xf0 == 0): CLUT first, then pixels.
"""
import os, struct
import icons
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
W, H = 256, 256


def fit(im, size):
    im = im.copy()
    bb = im.getchannel('A').point(lambda a: 255 if a > 8 else 0).getbbox()
    if bb: im = im.crop(bb)
    w, h = im.size
    s = size / max(w, h)
    im = im.resize((max(1, round(w * s)), max(1, round(h * s))), Image.LANCZOS)
    out = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    out.alpha_composite(im, ((size - im.size[0]) // 2, (size - im.size[1]) // 2))
    return out


def ring(size):
    S = 256
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse((12, 12, S - 12, S - 12), outline=(255, 255, 255, 255), width=34)
    return im.resize((size, size), Image.LANCZOS)


def orb(glyph_fn, rim=(200, 200, 210), fill=(10, 10, 14)):
    S = 256
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse((8, 8, S - 8, S - 8), fill=rim + (255,))
    d.ellipse((26, 26, S - 26, S - 26), fill=fill + (255,))
    glyph_fn(d, S)
    im = im.filter(ImageFilter.SMOOTH)
    return im


def gun_glyph(d, S):
    c = S // 2
    # bullet: brass casing + copper tip, diagonal
    body = [(c - 60, c + 40), (c + 10, c - 30), (c + 34, c - 6), (c - 36, c + 64)]
    d.polygon(body, fill=(214, 170, 60, 255))
    tip = [(c + 10, c - 30), (c + 44, c - 64), (c + 34, c - 6)]
    d.polygon([(c + 8, c - 28), (c + 50, c - 58), (c + 36, c - 4)], fill=(222, 120, 60, 255))
    d.line([(c - 48, c + 52), (c + 22, c - 18)], fill=(255, 230, 140, 255), width=6)


def earth_glyph(d, S):
    c = S // 2
    d.polygon([(c - 80, c + 50), (c - 20, c - 50), (c + 10, c + 0), (c + 30, c - 30), (c + 80, c + 50)],
              fill=(176, 118, 60, 255))
    d.polygon([(c - 20, c - 50), (c - 5, c - 25), (c - 35, c - 25)], fill=(240, 220, 190, 255))
    d.polygon([(c + 30, c - 30), (c + 42, c - 12), (c + 18, c - 12)], fill=(240, 220, 190, 255))


def buff_icon(stat, up, level, size=20):
    """Box + stat glyph (icons.stat_glyph) + 1..4 arrows."""
    col = (190, 255, 60, 255) if up else (252, 70, 104, 255)
    glyph_col = (210, 255, 110) if up else (255, 144, 162)
    S = 36
    im = Image.new('RGBA', (S, S), (0, 0, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, S - 1, S - 1), outline=col, width=2)
    im.alpha_composite(icons.stat_glyph(stat, glyph_col + (255,)), (3, 3))
    # arrows: stacked in the right half
    ax, aw = 21, 11
    n = level
    step = 26 // max(n, 1)
    total = step * (n - 1) + 7
    y0 = (S - total) // 2
    for i in range(n):
        y = y0 + i * step
        if up:
            d.polygon([(ax, y + 7), (ax + aw // 2, y), (ax + aw, y + 7)], fill=col)
        else:
            d.polygon([(ax, y), (ax + aw // 2, y + 7), (ax + aw, y)], fill=col)
    return im.resize((size, size), Image.LANCZOS)


def sprites():
    out = []   # (name, image)
    for n in ('phys', 'gun', 'fire', 'ice', 'elec', 'force', 'earth', 'expel', 'death'):
        im = orb(gun_glyph) if n == 'gun' else orb(earth_glyph) if n == 'earth' else icons.element(n)
        out.append(('ELEM_' + n.upper(), fit(im, 24)))
    for n in ('weak', 'resist', 'null', 'reflect', 'drain', 'normal', 'unknown'):
        out.append(('RES_' + n.upper(), fit(icons.badge(n), 20)))
    for n in ('weak', 'resist', 'null', 'reflect', 'drain', 'normal', 'unknown'):
        out.append(('RET_' + n.upper(), ring(32) if n == 'normal' else fit(icons.reticle(n), 32)))
    for n in ('charm', 'poison', 'mute', 'panic', 'sleep'):
        out.append(('AIL_' + n.upper(), fit(icons.ailment(n), 16)))
    for stat in ('att', 'mag', 'def', 'acc'):
        for up in (1, 0):
            for lv in range(1, 5):
                out.append(('BUFF_%s_%s%d' % (stat.upper(), 'UP' if up else 'DN', lv), buff_icon(stat, up, lv)))
    # solid white square for tinted panels/lines
    out.append(('WHITE', Image.new('RGBA', (8, 8), (255, 255, 255, 255))))
    return out


def pack(items):
    atlas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    rects = []
    x = y = rowh = 0
    for name, im in sorted(items, key=lambda t: -t[1].size[1]):
        w, h = im.size
        if x + w + 1 > W:
            x, y, rowh = 0, y + rowh + 1, 0
        if y + h > H: raise SystemExit('atlas full')
        atlas.alpha_composite(im, (x, y))
        rects.append((name, x, y, w, h))
        x += w + 1; rowh = max(rowh, h)
    order = [n for n, _ in items]
    rects.sort(key=lambda r: order.index(r[0]))
    return atlas, rects


def to_ps2(atlas):
    q = atlas.quantize(colors=255, method=Image.FASTOCTREE, dither=Image.NONE)
    pal = q.getpalette()[:255 * 3]
    # alpha per palette entry: average alpha of pixels mapped to it
    idx = list(q.getdata()); al = [0] * 256; cnt = [0] * 256
    for i, (r, g, b, a) in zip(idx, atlas.getdata()):
        al[i] += a; cnt[i] += 1
    colors = []
    for i in range(255):
        a = al[i] // cnt[i] if cnt[i] else 0
        colors.append((pal[3 * i], pal[3 * i + 1], pal[3 * i + 2], (a + 1) // 2))
    colors.append((0, 0, 0, 0))
    # transparent pixels -> entry 255
    px = bytes(255 if a < 8 else i for i, (r, g, b, a) in zip(idx, atlas.getdata()))
    csm1 = [colors[(j & ~0x18) | ((j & 0x08) << 1) | ((j & 0x10) >> 1)] for j in range(256)]
    clut = b''.join(struct.pack('<4B', *c) for c in csm1)
    hdr = bytearray(0x40)
    struct.pack_into('<BBHHBB', hdr, 0x10, 1, 0, W, H, 0x13, 0)   # +0x10 = has CLUT
    return bytes(hdr) + clut + px, colors


def main():
    items = sprites()
    atlas, rects = pack(items)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    blob, colors = to_ps2(atlas)
    open(os.path.join(ROOT, 'build', 'prey_atlas.bin'), 'wb').write(blob)
    prev = Image.new('RGBA', (W * 2, H * 2), (40, 40, 60, 255))
    prev.alpha_composite(atlas.resize((W * 2, H * 2), Image.NEAREST))
    prev.save(os.path.join(ROOT, 'build', 'prey_atlas.png'))
    with open(os.path.join(ROOT, 'src', 'prey_atlas.h'), 'w') as f:
        f.write('/* generated by tools/atlas.py */\n#ifndef PREY_ATLAS_H\n#define PREY_ATLAS_H\n')
        f.write('#define ATLAS_W %d\n#define ATLAS_H %d\n#define ATLAS_BYTES %d\n' % (W, H, len(blob)))
        for i, (n, x, y, w, h) in enumerate(rects):
            f.write('#define SPR_%s %d\n' % (n, i))
        f.write('#define SPR_COUNT %d\n' % len(rects))
        f.write('static const unsigned char atlas_rects[SPR_COUNT][4] = {\n')
        for n, x, y, w, h in rects:
            f.write('    {%d, %d, %d, %d}, /* %s */\n' % (x, y, w, h, n))
        f.write('};\n#endif\n')
    print('%d sprites, blob %d bytes' % (len(rects), len(blob)))


if __name__ == '__main__':
    main()
