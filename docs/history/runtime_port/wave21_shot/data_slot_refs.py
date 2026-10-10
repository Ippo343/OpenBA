"""Who reads/writes a DATA slot inside Runtime-x64.dll, and is that function OURS?
Subject printed before any finding."""
import os, re, sys, bisect
sys.path.insert(0, os.path.join(os.getcwd(), "tools"))
import mint_extent_audit as X
import mint_stamper_census as C
import pefile

TARGET = int(sys.argv[1], 16)
DIS = sys.argv[2]
pe = pefile.PE(X.DEFAULT_DLL, fast_load=True)
pe.parse_data_directories(directories=[
    pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT'],
    pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']])
base = pe.OPTIONAL_HEADER.ImageBase
exports = {}
for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if s.name and s.address:
        exports[s.address] = s.name.decode()
pdata = sorted((f.struct.BeginAddress, f.struct.EndAddress)
               for f in pe.DIRECTORY_ENTRY_EXCEPTION)
pb = [p[0] for p in pdata]
expva = sorted(exports)


def attribute(va):
    rva = va - base
    j = bisect.bisect_right(pb, rva) - 1
    if j >= 0 and pdata[j][0] <= rva < pdata[j][1]:
        b = pdata[j][0]
        return exports.get(b, "FUN_%09x" % (b + base)), b + base
    k = bisect.bisect_right(expva, rva) - 1
    hint = exports[expva[k]] if k >= 0 else "?"
    return "(no .pdata) NEAREST-EXPORT-HINT " + hint, None


names, addrs, nf, nb = C.load_sources()
pat = re.compile(r"^\s+([0-9a-f]+):\s+(.*?)\s*# 0x%x$" % TARGET)
reads, writes = [], []
for line in open(DIS, encoding="utf-8", errors="replace"):
    m = pat.match(line)
    if not m:
        continue
    va = int(m.group(1), 16)
    ins = m.group(2)
    if re.match(r"mov\s+%\w+,0x[0-9a-f]+\(%rip\)", ins):
        writes.append((va, ins))
    else:
        reads.append((va, ins))

print("SUBJECT   0x%09x in %s" % (TARGET, X.DEFAULT_DLL))
print("CHANNEL   objdump text scan of %s" % DIS)
print("OWNERSHIP literal RegisterRuntimeExport/Addr scan, %d files / %d bytes, "
      "%d names / %d addrs" % (nf, nb, len(names), len(addrs)))
print("POS CONTROL ??0BinaryFileStream@IO@@QEAA@XZ registered: %s"
      % ("YES" if "??0BinaryFileStream@IO@@QEAA@XZ" in names else "NO -- SCAN DEAD"))
print("NEG CONTROL synthetic registered: %s"
      % ("NO (ok)" if C.NEG_CONTROL not in names else "YES -- SCAN DEAD"))
print("refs %d = %d reads + %d writes" % (len(reads) + len(writes), len(reads), len(writes)))

for kind, lst in (("WRITE", writes), ("READ", reads)):
    byfn = {}
    for va, ins in lst:
        fn, start = attribute(va)
        byfn.setdefault((fn, start), []).append(va)
    print("\n---- %s sites in %d distinct function(s) ----" % (kind, len(byfn)))
    ours = 0
    for (fn, start), vas in sorted(byfn.items(), key=lambda kv: kv[1][0]):
        o = (fn in names) or (start is not None and start in addrs)
        if o:
            ours += 1
        print("  %-6s %-76s x%d  @0x%09x" % ("OURS" if o else "retail", fn[:76],
                                             len(vas), vas[0]))
    print("  => %d of %d %s functions are ours-capable" % (ours, len(byfn), kind))
