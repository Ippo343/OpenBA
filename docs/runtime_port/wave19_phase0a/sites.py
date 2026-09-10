"""Per-site classification of every stamp of the two subject vtables.  v2.

v1 had a real defect and it is recorded here rather than quietly fixed: it only
invalidated a tracked register on a reg->reg MOV, so `mov 0x10(%rdx),%rax`
left %rax still believed to hold the vtable and the screen INVENTED two extra
stamps inside ??0BinaryReader.  v2 invalidates the destination register of
EVERY instruction that writes one, and clobbers the volatile registers across
a CALL.  It therefore UNDER-reports rather than over-reports.

Both directions are checked: the total number of SUBJECT stamps emitted must
equal the number of LEA sites found by Route A/B, or the screen says so.
"""
import pefile, struct, re, collections, bisect

DLL = r"C:/GOG Games/Banished/Runtime-x64.dll"
pe = pefile.PE(DLL, fast_load=True); pe.parse_data_directories()
IB = pe.OPTIONAL_HEADER.ImageBase
data = pe.__data__
secs = [(s.Name.decode().rstrip('\0'), IB+s.VirtualAddress, s.Misc_VirtualSize,
         s.SizeOfRawData, s.PointerToRawData) for s in pe.sections]
exp_by_va = collections.defaultdict(list)
for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if e.name: exp_by_va[IB+e.address].append(e.name.decode())
VT = {va: [n for n in ns if n.startswith("??_7")]
      for va, ns in exp_by_va.items() if any(n.startswith("??_7") for n in ns)}

funcs = []
for n,b,vs,rs,o in secs:
    if n == ".pdata":
        for i in range(0, rs, 12):
            s,e,u = struct.unpack_from('<III', data, o+i)
            if s or e: funcs.append((IB+s, IB+e))
funcs.sort()
def func_of(va):
    i = bisect.bisect_right(funcs, (va, 1<<62)) - 1
    if i >= 0 and funcs[i][0] <= va < funcs[i][1]: return funcs[i]
    return None

LINE = re.compile(r'^\s+([0-9a-f]+):\t([0-9a-f ]+)\t(.*)$')
insns = {}
for line in open("rt.dis", encoding="utf-8", errors="replace"):
    m = LINE.match(line.rstrip("\n"))
    if m: insns[int(m.group(1),16)] = m.group(3).strip()
order = sorted(insns)

SUB = {"rax":"rax","eax":"rax","rbx":"rbx","ebx":"rbx","rcx":"rcx","ecx":"rcx",
       "rdx":"rdx","edx":"rdx","rsi":"rsi","esi":"rsi","rdi":"rdi","edi":"rdi",
       "rbp":"rbp","rsp":"rsp"}
for i in range(8,16):
    SUB["r%d"%i]="r%d"%i; SUB["r%dd"%i]="r%d"%i; SUB["r%dw"%i]="r%d"%i; SUB["r%db"%i]="r%d"%i
for a,b in [("al","rax"),("bl","rbx"),("cl","rcx"),("dl","rdx"),("ah","rax"),
            ("bh","rbx"),("ch","rcx"),("dh","rdx"),("sil","rsi"),("dil","rdi"),
            ("ax","rax"),("bx","rbx"),("cx","rcx"),("dx","rdx"),("si","rsi"),("di","rdi"),
            ("bpl","rbp"),("bp","rbp")]:
    SUB[a]=b
VOLATILE = {"rax","rcx","rdx","r8","r9","r10","r11"}
LEA = re.compile(r'^lea\s+(?:0x)?-?[0-9a-fx]*\(%rip\),%(\w+)\s+#\s+0x([0-9a-f]+)')
NOWRITE = ("cmp","test","push","jmp","je","jne","ret","call","nop","int3","ja","jb",
           "jg","jl","jae","jbe","jge","jle","js","jns","jo","jno","jp","jnp","ud2")

def norm(r): return SUB.get(r, r)

SUBJECTS = {0x1801553b0: "??_7ReaderWriter@IO@@6B@", 0x1801549a0: "??_7Stream@IO@@6B@"}

for subj, sname in SUBJECTS.items():
    sites = [va for va in order
             if (m := LEA.match(insns[va])) and int(m.group(2),16) == subj]
    print("="*78)
    print("%s VA=%x   LEA sites = %d" % (sname, subj, len(sites)))
    byf = collections.OrderedDict()
    for s in sites: byf.setdefault(func_of(s), []).append(s)
    emitted = 0
    for f, ss in byf.items():
        if f is None:
            print("\n  SITES OUTSIDE ANY .pdata FUNCTION: %s" % [hex(x) for x in ss]); continue
        fs, fe = f
        fn = ",".join(exp_by_va.get(fs, [])) or "(unexported %x)" % fs
        print("\n  FUNC %x..%x %s  [subject LEAs: %d]" % (fs, fe, fn, len(ss)))
        reg = {}
        i = bisect.bisect_left(order, fs)
        while i < len(order) and order[i] < fe:
            va = order[i]; t = insns[va]; i += 1
            m = LEA.match(t)
            if m:
                tgt = int(m.group(2),16)
                reg[norm(m.group(1))] = tgt if tgt in VT else None
                continue
            op = t.split()[0]
            if op.startswith("call"):
                for r in VOLATILE: reg.pop(r, None)
                print("      %x  CALL  %s" % (va, t)); continue
            ms = re.match(r'^mov\s+%(\w+),(.+)$', t)
            if ms and not ms.group(2).startswith('%'):
                r = norm(ms.group(1))
                if reg.get(r):
                    nm = ",".join(VT[reg[r]])
                    mark = "   <== SUBJECT" if reg[r]==subj else ""
                    if reg[r]==subj: emitted += 1
                    print("      %x  STAMP %-32s -> %s%s" % (va, nm, ms.group(2).strip(), mark))
                continue
            # invalidate destination register
            if op not in NOWRITE:
                last = t.split(',')[-1].strip()
                mr = re.match(r'^%(\w+)$', last)
                if mr:
                    d = norm(mr.group(1))
                    mm = re.match(r'^mov\s+%(\w+),%(\w+)$', t)
                    if mm and reg.get(norm(mm.group(1))) is not None:
                        reg[d] = reg[norm(mm.group(1))]
                    else:
                        reg.pop(d, None)
    print("\n  SELF-CHECK: subject stamps emitted = %d ; LEA sites = %d ; %s"
          % (emitted, len(sites), "MATCH" if emitted==len(sites) else "MISMATCH -- see note"))
