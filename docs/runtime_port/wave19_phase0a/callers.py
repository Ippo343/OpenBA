"""rel32 CALL/JMP site scan for a set of target VAs, decoder-independent:
every byte offset i in .text where bytes[i]==0xE8 or 0xE9 and
VA(i)+5+disp32 == target.  Positive control: a target that MUST have callers
(operator delete-ish 0x180006a10) -- if that reads zero the scan is inert.
"""
import pefile, struct, sys
DLL = r"C:/GOG Games/Banished/Runtime-x64.dll"
pe = pefile.PE(DLL, fast_load=True)
IB = pe.OPTIONAL_HEADER.ImageBase
data = pe.__data__
for s in pe.sections:
    if s.Name.decode().rstrip('\0') == ".text":
        tb, off, rs = IB+s.VirtualAddress, s.PointerToRawData, s.SizeOfRawData
text = data[off:off+rs]
TARGETS = {
  "??0ReaderWriter ctor(copy) 180023c50": 0x180023c50,
  "??0ReaderWriter ctor       1800b9120": 0x1800b9120,
  "??1ReaderWriter dtor       1800b9150": 0x1800b9150,
  "IO::Stream ctor?           1800b99d0": 0x1800b99d0,
  "IO::Stream ?               1800b9a40": 0x1800b9a40,
  "IO::Stream ?               18000c6d0": 0x18000c6d0,
  "CONTROL free/delete        180006a10": 0x180006a10,
}
res = {k: [] for k in TARGETS}
for i in range(rs-5):
    b = text[i]
    if b in (0xE8, 0xE9):
        d = struct.unpack_from('<i', text, i+1)[0]
        t = tb + i + 5 + d
        for k, va in TARGETS.items():
            if t == va: res[k].append((tb+i, "CALL" if b==0xE8 else "JMP"))
print("denominator: %d bytes of .text scanned" % rs)
for k, v in res.items():
    print("%-40s sites=%-4d %s" % (k, len(v), [hex(a) for a,_ in v][:14]))
