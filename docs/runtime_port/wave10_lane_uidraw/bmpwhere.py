import struct, sys
S = r"C:\Users\USPC\AppData\Local\Temp\claude\C--code-GIT-untitled-rev-eng-project\7c6de0ac-376e-4534-86cb-5df0fa9314d7\scratchpad"

def load(p):
    b = open(p, "rb").read()
    off = struct.unpack_from("<I", b, 10)[0]
    w   = struct.unpack_from("<i", b, 18)[0]
    h   = struct.unpack_from("<i", b, 22)[0]
    return b, off, w, abs(h)

a, oa, w, h = load(S + r"\shot_OFF.bmp")
b, ob, _, _ = load(S + r"\shot_OFF2.bmp")
c, oc, _, _ = load(S + r"\shot_ON.bmp")
stride = (w * 3 + 3) & ~3

def diffs(x, ox, y, oy):
    out = set()
    for row in range(h):
        rx, ry = ox + row * stride, oy + row * stride
        for col in range(w):
            i, j = rx + col * 3, ry + col * 3
            if x[i] != y[j] or x[i+1] != y[j+1] or x[i+2] != y[j+2]:
                out.add((col, row))
    return out

N = diffs(a, oa, b, ob)
T = diffs(a, oa, c, oc)
only = T - N
print("h=%d w=%d  (BMP rows are BOTTOM-UP)" % (h, w))
print("NOISE=%d TEST=%d TEST-only=%d" % (len(N), len(T), len(only)))
for name, s in (("NOISE", N), ("TEST", T), ("TEST_ONLY", only)):
    if not s: continue
    xs = [p[0] for p in s]; ys = [p[1] for p in s]
    print("%-10s bbox x[%d..%d] y[%d..%d]" % (name, min(xs), max(xs), min(ys), max(ys)))
    # coarse 8-band row histogram, in BMP row order (0 = bottom of the image)
    band = [0]*8
    for _, y in s: band[min(7, y*8//h)] += 1
    print("           row bands bottom->top: %s" % band)
