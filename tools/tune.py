"""Live-tune the SET layout table at 0xFF000 via PINE, then snapshot.
Usage: tune.py [field=value ...] [snap=NAME]   (values accept 0x.., negatives; px:NN converts px)"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
from drive import p, wait_vbl
FIELDS = ['magic', 'asg_x', 'asg_y', 'asg_rowh', 'help_dx', 'help_dy', 'port_dx', 'port_dy', 'cat_x', 'cat_y',
          'grid_x', 'grid_y', 'grid_pitch', 'grid_cols', 'grid_rows', 'grid_rowh', 'sort_x', 'sort_y',
          'strip_w', 'decor', 'hide_header', 'lbl_x', 'lbl_y', 'name_dx', 'cost_dx', 'bang_x', 'bang_y', 'skip', 'unit_dx', 'unit_dy', 'hint_probe', 'h_y', 'h_text_y', 'h_start_x', 'h_type_x', 'h_tri_x', 'h_l1_x', 'h_x_x', 'h_o_x', 'h_color', 'h_type_y', 'nudge_on', 'frames', 'div_color', 'div_dx', 'div_w', 'undo_keep', 'undo_lx', 'undo_x', 'undo_rx', 'box_x0', 'box_y0', 'box_x1', 'box_y1', 'line_w', 'line_h', 'mark_dx', 'mark_dy', 'arrow_dx', 'row_lines', 'row_line_dy', 'box_fill', 'cat_crop_top', 'cat_crop_right', 'cat_crop_left', 'cat_label_dy', 'cat_crop_bottom']
BASE = 0xFF000
def get(): return {f: (lambda v: v - (1 << 32) if v >= 1 << 31 else v)(p.r32(BASE + 4 * i)) for i, f in enumerate(FIELDS)}
if __name__ == '__main__':
    snap = None
    for a in sys.argv[1:]:
        k, v = a.split('=')
        if k == 'snap': snap = v; continue
        x = int(v, 0)
        p.w32(BASE + 4 * FIELDS.index(k), x & 0xffffffff)
    print(get())
    if snap: wait_vbl(8); print(drive.snap(4, snap))
