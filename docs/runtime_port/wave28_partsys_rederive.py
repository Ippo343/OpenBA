# WAVE 28 LANE PARTSYS — lane-private re-derivation (integrator correction 2).
# Re-derives, from C:\GOG Games\Banished\Runtime-x64.dll on disk:
#   (a) the FULL .pdata chain for ?Update@ParticleSystemInstance@, following
#       UNW_FLAG_CHAININFO, because one entry is only a LOWER bound;
#   (b) every float constant the body loads, as a bit pattern;
#   (c) the RNG masks, counted out of the raw bytes rather than out of a
#       disassembler listing.
import pefile, struct

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
pe = pefile.PE(DLL, fast_load=True)
pe.parse_data_directories()
base = pe.OPTIONAL_HEADER.ImageBase

# ---- (a) .pdata chain -------------------------------------------------------
d = pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]
pdata = pe.get_data(d.VirtualAddress, d.Size)
rows = []
for i in range(len(pdata) // 12):
    s, e, u = struct.unpack_from("<III", pdata, i * 12)
    rows.append((s, e, u))
rows.sort()

START = 0x1800a5600 - base
chain = []
cur = START
for (s, e, u) in rows:
    if s == cur:
        ui = pe.get_data(u, 4)
        flags = ui[0] >> 3
        chain.append((s, e, u, flags))
        cur = e
print("PDATA CHAIN for ?Update@ParticleSystemInstance@ (0x1800a5600)")
for (s, e, u, f) in chain:
    print("   %s..%s  size %s  unwind %s  flags %s%s" %
          (hex(base + s), hex(base + e), hex(e - s), hex(base + u), hex(f),
           "  CHAININFO" if f & 4 else ""))
first = chain[0]
end = chain[-1][1]
print("   FIRST ENTRY ALONE says size %s" % hex(first[1] - first[0]))
print("   FULL CHAIN  says   %s..%s = %s bytes (last byte %s)" %
      (hex(base + START), hex(base + end), hex(end - START), hex(base + end - 1)))

# ---- (b) float constants ----------------------------------------------------
print("\nFLOAT CONSTANTS (bit patterns straight out of .rdata)")
for va in (0x180161658, 0x18016165c, 0x18016167c, 0x180161694, 0x18016169c,
           0x1801616d8, 0x180161710, 0x180161718, 0x1801617c0):
    b = pe.get_data(va - base, 4)
    u, = struct.unpack("<I", b)
    f, = struct.unpack("<f", b)
    print("   %s  0x%08x  %r" % (hex(va), u, f))

# ---- (c) RNG masks, counted out of the CODE BYTES ---------------------------
# `25 <imm32>` is AND EAX,imm32. The two shipped tempering masks appear as
# literal little-endian immediates inside the body's byte range.
code = pe.get_data(START, end - START)
print("\nRNG MASKS counted in the body's raw bytes 0x1800a5600..%s" % hex(base + end))
for name, mask in (("mask1 (<<7)", 0xff3a58ad), ("mask2 (<<15)", 0xffffdf8c)):
    needle = b"\x25" + struct.pack("<I", mask)
    hits = []
    off = code.find(needle)
    while off != -1:
        hits.append(base + START + off)
        off = code.find(needle, off + 1)
    print("   %-13s 0x%08x  AND EAX,imm32 sites: %d" % (name, mask, len(hits)))
    print("      " + " ".join(hex(h) for h in hits))
# the textbook masks, as a NEGATIVE control: they must NOT appear
for name, mask in (("textbook 0x9d2c5680", 0x9d2c5680), ("textbook 0xefc60000", 0xefc60000)):
    needle = b"\x25" + struct.pack("<I", mask)
    print("   NEGATIVE CONTROL %-22s sites: %d" % (name, code.count(needle)))
# shift amounts, as a second witness on the pairing
for name, pat in (("SHR EAX,11", b"\xc1\xe8\x0b"), ("SHL EAX,7", b"\xc1\xe0\x07"),
                  ("SHL EAX,15", b"\xc1\xe0\x0f"), ("SHR EAX,18", b"\xc1\xe8\x12"),
                  ("CMP EDX,0x270", b"\x81\xfa\x70\x02\x00\x00")):
    print("   %-14s sites: %d" % (name, code.count(pat)))
