"""WINDOW screen: for every stamp of a subject vtable, what happens while it is
the installed table?

For each subject STAMP at S writing to destination text D inside function F:
  window = [S, N) where N is the next STAMP to the SAME D in F, else end of F.
Report: (a) the vtable that closes the window (or "END OF FUNCTION"),
        (b) every CALL in the window, split direct / INDIRECT.

An INDIRECT call in the window is the only shape that could dispatch through the
installed table, so it is the thing this screen exists to find.

POSITIVE CONTROL (must read non-zero, printed first): the same screen run for
??_7BinaryReader@IO@@6B@, a CONCRETE leaf table.  A leaf table is installed for
the whole life of the object, so its windows MUST contain indirect calls.  If
the control reads zero indirect calls the screen is inert and no zero below is
trustworthy.
"""
import pefile, struct, re, collections, bisect
DLL=r"C:/GOG Games/Banished/Runtime-x64.dll"
pe=pefile.PE(DLL,fast_load=True); pe.parse_data_directories()
IB=pe.OPTIONAL_HEADER.ImageBase; data=pe.__data__
secs=[(s.Name.decode().rstrip('\0'),IB+s.VirtualAddress,s.Misc_VirtualSize,s.SizeOfRawData,s.PointerToRawData) for s in pe.sections]
exp=collections.defaultdict(list)
for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if e.name: exp[IB+e.address].append(e.name.decode())
VT={va:[n for n in ns if n.startswith("??_7")] for va,ns in exp.items() if any(n.startswith("??_7") for n in ns)}
funcs=[]
for n,b,vs,rs,o in secs:
    if n==".pdata":
        for i in range(0,rs,12):
            a,e2,u=struct.unpack_from('<III',data,o+i)
            if a or e2: funcs.append((IB+a,IB+e2))
funcs.sort()
def fo(va):
    i=bisect.bisect_right(funcs,(va,1<<62))-1
    return funcs[i] if i>=0 and funcs[i][0]<=va<funcs[i][1] else None
LINE=re.compile(r'^\s+([0-9a-f]+):\t([0-9a-f ]+)\t(.*)$')
insns={}
for line in open("rt.dis",encoding="utf-8",errors="replace"):
    m=LINE.match(line.rstrip("\n"))
    if m: insns[int(m.group(1),16)]=m.group(3).strip()
order=sorted(insns)
SUB={}
for r in ("ax","bx","cx","dx","si","di","bp","sp"):
    SUB["r"+r]="r"+r; SUB["e"+r]="r"+r; SUB[r]="r"+r
for i in range(8,16):
    for suf in ("","d","w","b"): SUB["r%d%s"%(i,suf)]="r%d"%i
for a,b in [("al","rax"),("bl","rbx"),("cl","rcx"),("dl","rdx"),("sil","rsi"),("dil","rdi"),("bpl","rbp"),("ah","rax"),("bh","rbx"),("ch","rcx"),("dh","rdx")]:
    SUB[a]=b
VOL={"rax","rcx","rdx","r8","r9","r10","r11"}
LEA=re.compile(r'^lea\s+\S+\(%rip\),%(\w+)\s+#\s+0x([0-9a-f]+)')
NOW=("cmp","test","push","jmp","je","jne","ret","call","nop","int3","ja","jb","jg","jl","jae","jbe","jge","jle","js","jns","ud2")
def nrm(r): return SUB.get(r,r)

def events_of(fs,fe):
    reg={}; ev=[]
    i=bisect.bisect_left(order,fs)
    while i<len(order) and order[i]<fe:
        va=order[i]; t=insns[va]; i+=1
        m=LEA.match(t)
        if m:
            tg=int(m.group(2),16); reg[nrm(m.group(1))]=tg if tg in VT else None; continue
        op=t.split()[0]
        if op.startswith("call"):
            for r in VOL: reg.pop(r,None)
            ev.append(("ICALL" if "*" in t else "DCALL",va,None,t)); continue
        ms=re.match(r'^mov\s+%(\w+),(.+)$',t)
        if ms and not ms.group(2).startswith('%'):
            r=nrm(ms.group(1))
            if reg.get(r): ev.append(("STAMP",va,reg[r],ms.group(2).strip()))
            continue
        if op not in NOW:
            last=t.split(',')[-1].strip(); mr=re.match(r'^%(\w+)$',last)
            if mr:
                d=nrm(mr.group(1)); mm=re.match(r'^mov\s+%(\w+),%(\w+)$',t)
                if mm and reg.get(nrm(mm.group(1))) is not None: reg[d]=reg[nrm(mm.group(1))]
                else: reg.pop(d,None)
    return ev

def run(subj,name):
    sites=[va for va in order if (m:=LEA.match(insns[va])) and int(m.group(2),16)==subj]
    byf=collections.OrderedDict()
    for s in sites: byf.setdefault(fo(s),[]).append(s)
    nwin=0; nind=0; rows=[]
    for f in byf:
        if f is None: continue
        ev=events_of(*f)
        for k,(kind,va,tg,txt) in enumerate(ev):
            if kind!="STAMP" or tg!=subj: continue
            nwin+=1
            closer=None; calls=[]
            for kind2,va2,tg2,txt2 in ev[k+1:]:
                if kind2=="STAMP" and txt2==txt:
                    closer=(va2,",".join(VT[tg2])); break
                if kind2 in ("ICALL","DCALL"): calls.append((kind2,va2,txt2))
            ind=[c for c in calls if c[0]=="ICALL"]
            nind+=len(ind)
            fn=",".join(exp.get(f[0],[])) or "(unexported %x)"%f[0]
            rows.append((fn,va,txt,closer,len(calls),ind))
    print("\n### %s : %d stamp-windows, %d indirect calls inside a window"%(name,nwin,nind))
    for fn,va,dst,closer,nc,ind in rows:
        c = ("closed by %s at %x"%(closer[1],closer[0])) if closer else "END OF FUNCTION"
        flag = "  <<< INDIRECT: %s"%[hex(a) for _,a,_ in ind] if ind else ""
        print("  %-70s stamp@%x -> %-14s %-46s calls=%d%s"%(fn[:70],va,dst,c,nc,flag))

print("=== POSITIVE CONTROL: a CONCRETE leaf table (must show indirect calls) ===")
run(0x1801552e0,"??_7BinaryReader@IO@@6B@ [CONTROL]")
print("\n=== SUBJECTS ===")
run(0x1801553b0,"??_7ReaderWriter@IO@@6B@")
run(0x1801549a0,"??_7Stream@IO@@6B@")
