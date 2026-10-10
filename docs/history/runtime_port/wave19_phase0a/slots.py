import pefile, struct, collections
DLL = r"C:/GOG Games/Banished/Runtime-x64.dll"
pe = pefile.PE(DLL, fast_load=True); pe.parse_data_directories()
IB = pe.OPTIONAL_HEADER.ImageBase
secs = [(s.Name.decode().rstrip('\0'), IB+s.VirtualAddress, s.Misc_VirtualSize, s.SizeOfRawData, s.PointerToRawData) for s in pe.sections]
data = pe.__data__
def sec_of(va):
    for n,base,vsz,rsz,off in secs:
        if base <= va < base+max(vsz,rsz): return n
    return None
def va2off(va):
    for n,base,vsz,rsz,off in secs:
        if base <= va < base+rsz: return off+(va-base)
    return None
def q(va):
    o=va2off(va); return struct.unpack_from('<Q',data,o)[0] if o is not None else None
exp={}
for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if e.name: exp.setdefault(IB+e.address, []).append(e.name.decode())
for name,va,n in [("ReaderWriter",0x1801553b0,60),("Stream",0x1801549a0,20)]:
    print("=== %s @ %x" % (name,va))
    for i in range(n):
        a=va+8*i; v=q(a)
        lbl = ",".join(exp.get(a,[]))
        print("  [%2d] %016x  sec=%-8s %s" % (i, v, sec_of(v), ("<<EXPORT@slot: "+lbl if lbl and i>0 else "")))
