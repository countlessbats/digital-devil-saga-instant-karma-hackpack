"""Contact sheet of snapshot screenshots: sheet.py OUT.png COLS name1 name2 ... (work/snap_<name>)"""
import sys, os
from PIL import Image, ImageDraw
out, cols, names = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
W, H = 400, 300
rows = (len(names) + cols - 1) // cols
sheet = Image.new('RGB', (W * cols, (H + 16) * rows), 'black')
d = ImageDraw.Draw(sheet)
for i, n in enumerate(names):
    im = Image.open(os.path.join('work', 'snap_' + n, 'Screenshot.png')).convert('RGB').resize((W, H))
    x, y = (i % cols) * W, (i // cols) * (H + 16)
    sheet.paste(im, (x, y + 16)); d.text((x + 4, y + 2), n, fill='yellow')
sheet.save(out)
