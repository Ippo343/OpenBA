# HEADS, decoder route: every target of an actual `lea reg,[rip+d]` in .text
# that lands in .rdata. This is what "STAMPED address" means -- an operand of a
# real instruction, not a byte window that happens to decode as one.
# Cross-checked against heads2.py's disp32 superset in the summary line.
import re,sys,struct,bisect,pefile
SP=r"C:\Users\USPC\AppData\Local\Temp\claude\C--code-GIT-untitled-rev-eng-project\d375e719-cc64-44c2-bf76-75d78df40cfd\scratchpad\rt.dis"
P=r"C:\GOG Games\Banished\Runtime-x64.dll"
pe=pefile.PE(P,fast_load=True); base=pe.OPTIONAL_HEADER.ImageBase
secs={s.Name.rstrip(b'\0').decode():s for s in pe.sections}
t=secs['.text']; r=secs['.rdata']
tlo=base+t.VirtualAddress; thi=tlo+t.Misc_VirtualSize
rva=base+r.VirtualAddress; rb=r.get_data(); rlo,rhi=rva,rva+len(rb)
def qw(va):
    o=va-rva
    return struct.unpack_from('<Q',rb,o)[0] if 0<=o<=len(rb)-8 else None
lea=re.compile(r'^\s+([0-9a-f]+):\t[0-9a-f ]+\tlea\s+0x[0-9a-f]+\(%rip\),%\w+\s+#\s+0x([0-9a-f]+)')
heads=set(); nlea=0
for line in open(SP,encoding='utf-8',errors='replace'):
    m=lea.match(line)
    if not m: continue
    nlea+=1
    tgt=int(m.group(2),16)
    if rlo<=tgt<rhi and tgt%8==0:
        v=qw(tgt)
        if v is not None and tlo<=v<thi: heads.add(tgt)
print(f"# LEA instructions decoded: {nlea}   .rdata+aligned+holds-a-.text-ptr HEADS: {len(heads)}",file=sys.stderr)
H=sorted(heads)
for arg in sys.argv[1:]:
    va=int(arg,16)
    if va not in heads: print(f"{va:#x}  *** NOT ITSELF A LEA TARGET ***",file=sys.stderr)
    i=bisect.bisect_right(H,va); nxt=H[i] if i<len(H) else rhi
    n=0;p=va;stop=None
    while p<nxt:
        v=qw(p)
        if v is None or v==0: stop='null qword'; break
        if not (tlo<=v<thi): stop=f'non-.text qword {v:#x}'; break
        p+=8;n+=1
    print(f"{va:#x}  next STAMPED head {nxt:#x}  -> {n} slots   ({stop or 'ran into the next head'})")
