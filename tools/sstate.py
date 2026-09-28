"""Extract a PCSX2 .p2s savestate (zip with zstd members, method 93) into work/ssNN."""
import zipfile, struct, sys, os, zstandard

def extract(path, dest):
    os.makedirs(dest, exist_ok=True)
    z = zipfile.ZipFile(path)
    raw = open(path, 'rb').read()
    for i in z.infolist():
        o = i.header_offset
        n, e = struct.unpack_from('<HH', raw, o + 26)
        blob = raw[o + 30 + n + e: o + 30 + n + e + i.compress_size]
        if i.compress_type == 0: data = blob
        elif i.compress_type == 93: data = zstandard.ZstdDecompressor().decompress(blob, max_output_size=i.file_size)
        elif i.compress_type == 8:
            import zlib; data = zlib.decompress(blob, -15)
        else: raise ValueError(i.compress_type)
        open(os.path.join(dest, i.filename), 'wb').write(data)

if __name__ == '__main__':
    extract(sys.argv[1], sys.argv[2])
