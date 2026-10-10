# A vtable HEAD candidate must be: (a) the target of a .text disp32, (b) 8-byte
# ALIGNED, and (c) itself hold a pointer into .text.  (b) and (c) are what the
# first version of this screen lacked -- it returned unaligned coincidences like
# 0x1801552e7 and would have "sized" ??_7BinaryReader@ at 0 slots.
import pefile, struct, sys, bisect
P=r"C:\GOG Games\Banished\Runtime-x64.dll"
pe=pefile.PE(P, fast_load=True); base=pe.OPTIONAL_HEADER.ImageBase
secs={s.Name.rstrip(b'\0').decode():s for s in pe.sections}
t=secs['.text']; r=secs['.rdata']
tva=base+t.VirtualAddress; tb=t.get_data(); tlo,thi=tva,tva+t.Misc_VirtualSize
rva=base+r.VirtualAddress; rb=r.get_data(); rlo,rhi=rva,rva+len(rb)
def qw(va):
    o=va-rva
    return struct.unpack_from('<Q',rb,o)[0] if 0<=o<=len(rb)-8 else None
raw=set(); windows=0
for i in range(len(tb)-3):
    windows+=1
    d=struct.unpack_from('<i',tb,i)[0]; tgt=tva+i+4+d
    if rlo<=tgt<rhi: raw.add(tgt)
aligned={a for a in raw if a%8==0}
heads={a for a in aligned if (qw(a) or 0) >= tlo and (qw(a) or 0) < thi}
print(f"# windows={windows}  raw .rdata targets={len(raw)}  8-aligned={len(aligned)}  "
      f"holding a .text ptr (= HEADS)={len(heads)}", file=sys.stderr)
H=sorted(heads)
for arg in sys.argv[1:]:
    va=int(arg,16)
    i=bisect.bisect_right(H,va); nxt=H[i] if i<len(H) else rhi
    n=0; p=va; stop=None
    while p<nxt:
        v=qw(p)
        if v is None or v==0: stop='null qword'; break
        if not (tlo<=v<thi): stop=f'non-.text qword {v:#x}'; break
        p+=8; n+=1
    print(f"{va:#x}  next head {nxt:#x}  -> {n} slots   ({stop or 'ran into the next head'})")
