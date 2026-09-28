# Conflict in runtime/src/object/deleting_dtor.cpp (DELETE-ONLY file): result = THEIRS (MERGE_HEAD) minus
# every row / wrapper / registration OURS (HEAD) no longer has. Structural, never line-matching.
import subprocess,re,sys
# HISTORICAL since ENDGAME B.3.9 OBJ W3: deleting_dtor.cpp is deleted, so no merge can conflict in it.
# mergegate.sh no longer calls this script (B.3.9 close); kept as the record of how the conflicts were resolved.
sys.exit("resolve_deldtor.py is HISTORICAL: runtime/src/object/deleting_dtor.cpp no longer exists")
P='runtime/src/object/deleting_dtor.cpp'
def show(ref): return subprocess.run(['git','show',f'{ref}:{P}'],capture_output=True,check=True).stdout.decode('utf-8','surrogateescape')
ours=show('HEAD'); theirs=show('MERGE_HEAD')
rows=lambda s:set(re.findall(r'RT_DELDTOR\((0x[0-9a-f]+)',s))
gone=rows(theirs)-rows(ours)
L=theirs.split('\n'); out=[]; i=0; n=[0,0,0]
while i<len(L):
    l=L[i]
    m=re.search(r'RT_DELDTOR\((0x[0-9a-f]+)',l)
    if m and m.group(1) in gone: n[0]+=1; i+=1; continue
    m=re.search(r'RegisterRuntimeAddr\((0x[0-9a-f]+),\s*\(void\*\)&RtDelDtor_',l)
    if m and m.group(1) in gone: n[2]+=1; i+=1; continue
    if re.match(r'void\* RtDelDtor_\w+\(void\* self, unsigned flags\) \{',l.rstrip('\r')) and i+2<len(L):
        m=re.search(r'Row<(0x[0-9a-f]+)>',L[i+1])
        if m and m.group(1) in gone and L[i+2].rstrip('\r')=='}':
            if out and out[-1].lstrip().startswith('//') and '~' in out[-1]: out.pop()
            n[1]+=1; i+=3; continue
    out.append(l); i+=1
s='\n'.join(out)
ok = n[0]==n[1]==n[2]==len(gone) and s.count('{')==s.count('}') and rows(s)==(rows(ours)&rows(theirs))
print(f'deldtor resolve: rows {n[0]} wrappers {n[1]} registrations {n[2]} of {len(gone)}; final rows {len(rows(s))}; ok={ok}')
if not ok: sys.exit(1)
open(P,'wb').write(s.encode('utf-8','surrogateescape'))
