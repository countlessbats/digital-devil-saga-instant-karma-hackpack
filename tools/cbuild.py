"""Compile src/*.c with zig (MIPS III n32) and link at fixed addresses.
Returns (load_segments[(addr, bytes)], symbols{name: addr})."""
import os, subprocess, glob
from elftools.elf.elffile import ELFFile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
ZIG = r'<local path>'
CFLAGS = ['-target', 'mips64el-freestanding-gnuabin32', '-mcpu=mips3', '-O2', '-fno-pic',
          '-Xclang', '-target-feature', '-Xclang', '+noabicalls', '-G0', '-ffreestanding', '-fno-builtin', '-nostdlib',
          '-fno-stack-protector', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-Wall']

def build(include_test=False):
    import atlas; atlas.main()          # regenerates src/prey_atlas.h
    out = os.path.join(ROOT, 'build', 'obj'); os.makedirs(out, exist_ok=True)
    objs = []
    srcs = sorted(glob.glob(os.path.join(ROOT, 'src', '*.c')))
    if include_test: srcs += sorted(glob.glob(os.path.join(ROOT, 'src', 'test', '*.c')))
    for c in srcs:
        o = os.path.join(out, os.path.basename(c)[:-2] + '.o')
        subprocess.run([ZIG, 'cc', *CFLAGS, '-c', c, '-o', o], check=True)
        objs.append(o)
    elf = os.path.join(out, 'mods_test.elf' if include_test else 'mods.elf')
    subprocess.run([ZIG, 'ld.lld', '-m', 'elf32ltsmipn32', '-T', os.path.join(ROOT, 'src', 'mods.ld'),
                    '--no-relax', '-o', elf, *objs], check=True)
    f = ELFFile(open(elf, 'rb'))
    segs = []
    for s in f.iter_sections():
        if s['sh_type'] == 'SHT_PROGBITS' and s['sh_flags'] & 2 and s.data_size:
            segs.append((s['sh_addr'], s.data()))
    syms = {}
    for s in f.get_section_by_name('.symtab').iter_symbols():
        if s['st_info']['type'] in ('STT_FUNC', 'STT_OBJECT') and s.name:
            syms[s.name] = s['st_value']
    # sanity: mod code must never touch $gp (game code, including interrupt handlers, relies on it)
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_MIPS, capstone.CS_MODE_MIPS64 | capstone.CS_MODE_LITTLE_ENDIAN)
    text = f.get_section_by_name('.text')
    for i in md.disasm(text.data(), text['sh_addr']):
        if '$gp' in i.op_str:
            raise SystemExit('mod code uses $gp at %08x: %s %s' % (i.address, i.mnemonic, i.op_str))
    return segs, syms

if __name__ == '__main__':
    segs, syms = build()
    for a, b in segs: print('%08x %d bytes' % (a, len(b)))
    for k, v in sorted(syms.items(), key=lambda x: x[1]): print('%08x %s' % (v, k))
