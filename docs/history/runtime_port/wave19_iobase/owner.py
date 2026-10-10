# Which .pdata function contains each address, and what is it exported as?
import pefile,struct,sys,bisect
P=r"C:\GOG Games\Banished\Runtime-x64.dll"
pe=pefile.PE(P); b=pe.OPTIONAL_HEADER.ImageBase
exp={}
for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if e.name: exp[b+e.address]=e.name.decode()
d=pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]
raw=pe.get_data(d.VirtualAddress,d.Size)
funcs=[]
for i in range(0,len(raw)-11,12):
    s,e,u=struct.unpack_from('<III',raw,i)
    if s==0: break
    funcs.append((b+s,b+e))
funcs.sort(); starts=[f[0] for f in funcs]
for a in sys.argv[1:]:
    va=int(a,16)
    i=bisect.bisect_right(starts,va)-1
    if i>=0 and funcs[i][0]<=va<funcs[i][1]:
        s,e=funcs[i]
        print(f"{va:#x}  in .pdata fn {s:#x}..{e:#x}   {exp.get(s,'<NOT EXPORTED>')}")
    else:
        print(f"{va:#x}  NO .pdata entry (leaf frame)   nearest export: {exp.get(va,'-')}")
