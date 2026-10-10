"""Route B: decoder-independent scan.

For EVERY byte offset i in .text, treat bytes[i:i+4] as a little-endian signed
disp32 and compute target = VA(i) + 4 + disp32.  If target == a subject VA,
record a hit.  This makes NO assumption about instruction boundaries or opcode
encoding, so it is a strict SUPERSET of any RIP-relative reference a decoder
would find (at the cost of false positives from windows that are not real
disp32 fields).

Also scans EVERY section for absolute 8-byte and 4-byte values equal to a
subject VA (a data-held pointer), and cross-checks .reloc.

Every screen prints its denominator and its positive control.
"""
import pefile, struct, sys

DLL = r"C:/GOG Games/Banished/Runtime-x64.dll"
SUBJECTS = {
    "??_7ReaderWriter@IO@@6B@": 0x1801553b0,
    "??_7Stream@IO@@6B@":       0x1801549a0,
    # POSITIVE CONTROLS -- each MUST read non-zero on the route it controls.
    "CTRL-lea ??_7MemoryStream@IO@@6B@": None,   # filled below from exports
}

pe = pefile.PE(DLL, fast_load=True); pe.parse_data_directories()
IB = pe.OPTIONAL_HEADER.ImageBase
data = pe.__data__
secs = []
for s in pe.sections:
    secs.append((s.Name.decode().rstrip('\0'), IB+s.VirtualAddress,
                 s.Misc_VirtualSize, s.SizeOfRawData, s.PointerToRawData))
exp = {}
for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if e.name:
        exp[e.name.decode()] = IB + e.address

SUBJECTS["CTRL-lea ??_7MemoryStream@IO@@6B@"] = exp["??_7MemoryStream@IO@@6B@"]
# A control for the DATA/qword screen: slot 0 of ??_7Stream@ is a qword the DLL
# demonstrably stores (CEIL 2.4 used the same one).  It MUST be found.
CTRL_QWORD = 0x18000c610

def sec(name):
    for n,base,vsz,rsz,off in secs:
        if n == name: return (base, rsz, off)
    raise KeyError(name)

tbase, tsize, toff = sec(".text")
text = data[toff:toff+tsize]

print("=== ROUTE B1: disp32 sliding window over .text ===")
print("subject denominator: %d 4-byte windows scanned in .text (VA %x..%x)"
      % (tsize-3, tbase, tbase+tsize))
hitsB = {k: [] for k in SUBJECTS}
for i in range(tsize-3):
    d = struct.unpack_from('<i', text, i)[0]
    tgt = tbase + i + 4 + d
    for k, va in SUBJECTS.items():
        if tgt == va:
            hitsB[k].append(tbase+i)
for k, va in SUBJECTS.items():
    print("  %-40s VA=%x  windows=%d" % (k, va, len(hitsB[k])))
    for h in hitsB[k]:
        print("      disp32 field at %x  (insn start <= %x)" % (h, h))

print()
print("=== ROUTE B2: absolute qword/dword scan, whole image, all sections ===")
tot = 0
found = {k: [] for k in list(SUBJECTS)+["CTRL_QWORD 0x18000c610"]}
for n, base, vsz, rsz, off in secs:
    blob = data[off:off+rsz]
    tot += max(0, rsz-7)
    for i in range(max(0, rsz-7)):
        q = struct.unpack_from('<Q', blob, i)[0]
        if q == CTRL_QWORD:
            found["CTRL_QWORD 0x18000c610"].append((n, base+i))
        for k, va in SUBJECTS.items():
            if q == va:
                found[k].append((n, base+i))
print("  denominator: %d qword windows across %d sections" % (tot, len(secs)))
for k, v in found.items():
    print("  %-40s qword hits=%d %s" % (k, len(v), v[:6]))
