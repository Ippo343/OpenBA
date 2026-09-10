"""Cross-binary screen: does ANY shipped image (a) import either subject symbol
by name, or (b) hold either subject VA as a qword/dword value?

Positive controls, each of which MUST read non-zero:
  * import control  : ??_7MemoryStream@IO@@6B@ is not expected; instead we count
                      TOTAL Runtime imports per image and print it -- a zero
                      there would mean the screen read nothing.
  * qword control   : 0x18000c610 (slot 0 of ??_7Stream@) which the Runtime DLL
                      demonstrably stores.  MUST be found in Runtime.
"""
import pefile, struct, collections
IMAGES = {
 "Runtime-x64.dll":  r"C:/GOG Games/Banished/Runtime-x64.dll",
 "Application-x64.exe": r"C:/GOG Games/Banished/Application-x64.exe",
 "VideoDX11-x64.dll": r"C:/GOG Games/Banished/VideoDX11-x64.dll",
 "VideoDX9-x64.dll":  r"C:/GOG Games/Banished/VideoDX9-x64.dll",
}
SUBJ = {"??_7ReaderWriter@IO@@6B@": 0x1801553b0, "??_7Stream@IO@@6B@": 0x1801549a0}
CTRL = 0x18000c610
for label, path in IMAGES.items():
    try:
        pe = pefile.PE(path, fast_load=True); pe.parse_data_directories()
    except Exception as ex:
        print("%-22s COULD NOT OPEN: %s" % (label, ex)); continue
    data = pe.__data__
    imps = []
    nimp = 0
    for d in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        for i in d.imports:
            nimp += 1
            if i.name:
                n = i.name.decode()
                if n in SUBJ: imps.append(n)
    hits = collections.Counter()
    tot = 0
    for s in pe.sections:
        off, rs = s.PointerToRawData, s.SizeOfRawData
        blob = data[off:off+rs]
        tot += max(0, rs-7)
        for i in range(max(0, rs-7)):
            q = struct.unpack_from('<Q', blob, i)[0]
            if q == CTRL: hits["CTRL"] += 1
            for n, va in SUBJ.items():
                if q == va: hits[n] += 1
        for i in range(max(0, rs-3)):
            d32 = struct.unpack_from('<I', blob, i)[0]
            for n, va in SUBJ.items():
                if d32 == (va & 0xffffffff): hits[n+" [dword-lo]"] += 1
    print("%-22s imports_total=%-5d subject_imports=%s  qword_windows=%d  hits=%s"
          % (label, nimp, imps or "NONE", tot, dict(hits) or "NONE"))
