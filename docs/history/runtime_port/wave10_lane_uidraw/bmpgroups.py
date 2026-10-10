import struct, itertools, os
S = r"C:\Users\USPC\AppData\Local\Temp\claude\C--code-GIT-untitled-rev-eng-project\7c6de0ac-376e-4534-86cb-5df0fa9314d7\scratchpad"

def load(p):
    b = open(p, "rb").read()
    off = struct.unpack_from("<I", b, 10)[0]
    return b, off

OFFS = ["OFF", "OFF2", "OFF3"]
ONS  = ["ON", "ON2", "ON3"]
imgs = {}
for n in OFFS + ONS:
    p = os.path.join(S, "shot_%s.bmp" % n)
    if os.path.exists(p):
        imgs[n] = load(p)
    else:
        print("MISSING", n)

def npix_diff(x, y):
    a, oa = x; b, ob = y
    n = min(len(a) - oa, len(b) - ob) // 3
    c = 0
    for i in range(n):
        ia, ib = oa + i*3, ob + i*3
        if a[ia] != b[ib] or a[ia+1] != b[ib+1] or a[ia+2] != b[ib+2]:
            c += 1
    return c

def report(title, pairs):
    vals = []
    for u, v in pairs:
        if u in imgs and v in imgs:
            d = npix_diff(imgs[u], imgs[v])
            vals.append(d)
            print("  %-12s %-12s  %7d differing pixels" % (u, v, d))
    if vals:
        print("  %s: n=%d  min=%d  max=%d  mean=%.0f" %
              (title, len(vals), min(vals), max(vals), sum(vals)/len(vals)))
    return vals

print("WITHIN retail (detour OFF, retail's own Draw) -- pure run-to-run noise")
w1 = report("within-OFF", list(itertools.combinations(OFFS, 2)))
print("WITHIN port (detour ON, our Draw) -- same noise, our body")
w2 = report("within-ON", list(itertools.combinations(ONS, 2)))
print("BETWEEN groups -- retail's Draw vs ours")
bt = report("between", [(a, b) for a in OFFS for b in ONS])

within = w1 + w2
if within and bt:
    print()
    print("VERDICT INPUTS")
    print("  within-group range : %d .. %d" % (min(within), max(within)))
    print("  between-group range: %d .. %d" % (min(bt), max(bt)))
    print("  between separable from within? %s" %
          ("YES -- min(between) > max(within)" if min(bt) > max(within)
           else "NO -- the ranges overlap, so the port is indistinguishable "
                "from run-to-run noise on this instrument"))
