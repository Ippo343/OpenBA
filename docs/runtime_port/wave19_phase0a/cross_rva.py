"""cross.py's dword half, CORRECTED.

The original compared dwords against (VA & 0xffffffff) = 0x801549a0, which is
the low half of the VA.  A 32-bit reference in a PE is an **RVA**, 0x001549a0.
The screen was therefore blind to every RVA-form reference and could not have
said so.  Ghidra's xref list found the one it missed.

Scans all four shipped images for BOTH forms, and prints which form matched.
CONTROL: the RVA of ??_7Stream@ MUST be found in Runtime's export address
table (Ghidra says it is at 0x1801b21cc); a zero there means the screen is inert.
"""
import pefile, struct, collections
IMAGES = {
 "Runtime-x64.dll":    r"C:/GOG Games/Banished/Runtime-x64.dll",
 "Application-x64.exe":r"C:/GOG Games/Banished/Application-x64.exe",
 "VideoDX11-x64.dll":  r"C:/GOG Games/Banished/VideoDX11-x64.dll",
 "VideoDX9-x64.dll":   r"C:/GOG Games/Banished/VideoDX9-x64.dll",
}
SUBJ={"??_7ReaderWriter@IO@@6B@":0x1801553b0, "??_7Stream@IO@@6B@":0x1801549a0}
RT_BASE=0x180000000
for label,path in IMAGES.items():
    pe=pefile.PE(path,fast_load=True); pe.parse_data_directories()
    IB=pe.OPTIONAL_HEADER.ImageBase; data=pe.__data__
    ed=pe.DIRECTORY_ENTRY_EXPORT.struct if hasattr(pe,"DIRECTORY_ENTRY_EXPORT") else None
    eat_lo = IB+ed.AddressOfFunctions if ed else 0
    eat_hi = eat_lo+4*ed.NumberOfFunctions if ed else 0
    hits=collections.Counter(); where=collections.defaultdict(list)
    for s in pe.sections:
        o,rs,base=s.PointerToRawData,s.SizeOfRawData,IB+s.VirtualAddress
        blob=data[o:o+rs]
        for i in range(max(0,rs-3)):
            v=struct.unpack_from('<I',blob,i)[0]
            for n,va in SUBJ.items():
                if v==(va-RT_BASE):
                    a=base+i
                    tag="EXPORT ADDRESS TABLE" if eat_lo<=a<eat_hi else "*** NOT the EAT ***"
                    hits[n+" [RVA]"]+=1; where[n].append((hex(a),tag))
                elif v==(va&0xffffffff):
                    hits[n+" [VA-low32]"]+=1; where[n].append((hex(base+i),"VA-low32"))
    print("%-22s %s"%(label,dict(hits) or "no 32-bit reference in either form"))
    for n,v in where.items():
        for a,t in v: print("        %-14s %s   %s"%(n.split('@')[1] if '@' in n else n,a,t))
