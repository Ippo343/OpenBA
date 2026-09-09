import struct, os, itertools
S = r"C:\Users\USPC\AppData\Local\Temp\claude\C--code-GIT-untitled-rev-eng-project\7c6de0ac-376e-4534-86cb-5df0fa9314d7\scratchpad"

def load(n):
    b = open(os.path.join(S, "shot_%s.bmp" % n), "rb").read()
    off = struct.unpack_from("<I", b, 10)[0]
    w = struct.unpack_from("<i", b, 18)[0]
    h = abs(struct.unpack_from("<i", b, 22)[0])
    return b, off, w, h

names = ["OFF","OFF2","OFF3","ON","ON2","ON3","POSOFF","POSON"]
imgs = {n: load(n) for n in names}
_,_,W,H = imgs["OFF"]
STRIDE = (W*3+3) & ~3
YMAX = H*7//8          # exclude the top eighth: the live status bar

def d(x, y):
    a, oa, _, _ = x; b, ob, _, _ = y
    c = 0
    for row in range(YMAX):
        ra, rb = oa+row*STRIDE, ob+row*STRIDE
        for col in range(W):
            i, j = ra+col*3, rb+col*3
            if a[i]!=b[j] or a[i+1]!=b[j+1] or a[i+2]!=b[j+2]:
                c += 1
    return c

print("region: rows 0..%d of %d (top eighth -- the live status bar -- EXCLUDED), %d px"
      % (YMAX-1, H, YMAX*W))
def show(t, pairs):
    v=[]
    for u,w in pairs:
        n=d(imgs[u],imgs[w]); v.append(n)
        print("  %-7s %-7s %7d" % (u,w,n))
    print("  %-15s n=%d min=%d max=%d" % (t,len(v),min(v),max(v)))
    return v
w1=show("within-OFF", list(itertools.combinations(["OFF","OFF2","OFF3"],2)))
w2=show("within-ON",  list(itertools.combinations(["ON","ON2","ON3"],2)))
bt=show("between",    [(a,b) for a in ["OFF","OFF2","OFF3"] for b in ["ON","ON2","ON3"]])
pc=show("POSITIVE CONTROL (dialog on screen vs not, retail both sides)",
        [(a,"POSOFF") for a in ["OFF","OFF2","OFF3"]])
print()
wi=w1+w2
print("within  : %d..%d" % (min(wi),max(wi)))
print("between : %d..%d" % (min(bt),max(bt)))
print("poscontrol: %d..%d" % (min(pc),max(pc)))
print("instrument CAN see a dialog appear? %s" %
      ("YES -- min(positive control) > max(within)" if min(pc)>max(wi) else "NO"))
print("port distinguishable from noise?    %s" %
      ("YES" if min(bt)>max(wi) else "NO -- ranges overlap"))
