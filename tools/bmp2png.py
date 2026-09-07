import struct, zlib, sys
src, dst, step = sys.argv[1], sys.argv[2], int(sys.argv[3])
d = open(src,'rb').read()
off = struct.unpack_from('<I', d, 10)[0]
hdr = struct.unpack_from('<I', d, 14)[0]
w, h = struct.unpack_from('<ii', d, 18)
planes, bpp = struct.unpack_from('<HH', d, 26)
print('bmp', w, h, 'bpp', bpp, 'dataoff', off, file=sys.stderr)
bot_up = h > 0; H = abs(h)
Bpp = bpp//8
stride = ((w*Bpp + 3)//4)*4
rows = []
for y in range(0, H, step):
    sy = (H-1-y) if bot_up else y
    base = off + sy*stride
    row = bytearray()
    for x in range(0, w, step):
        p = base + x*Bpp
        b, g, r = d[p], d[p+1], d[p+2]
        row += bytes((r, g, b))
    rows.append(bytes(row))
W2, H2 = len(rows[0])//3, len(rows)
raw = b''.join(b'\x00'+r for r in rows)
def chunk(t, data):
    c = struct.pack('>I', len(data)) + t + data
    return c + struct.pack('>I', zlib.crc32(t+data) & 0xffffffff)
png = (b'\x89PNG\r\n\x1a\n'
       + chunk(b'IHDR', struct.pack('>IIBBBBB', W2, H2, 8, 2, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(raw, 6))
       + chunk(b'IEND', b''))
open(dst,'wb').write(png)
print('png', W2, H2, file=sys.stderr)
