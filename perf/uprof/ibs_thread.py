import csv,subprocess,collections,sys
src,tid=sys.argv[1],sys.argv[2]; n=int(sys.argv[3]) if len(sys.argv)>3 else 40
exe=r'C:\code\GIT\untitled_rev_eng_project\build\cmake-perf\reimpl-x64.exe'
lines=open(src,encoding='utf-8',errors='replace').read().split('\n')
# find the detail block: a line starting with "Thread-<tid> (PID that is followed by a FUNCTION header
# The thread's summary row comes first (no FUNCTION header after it); the detail block is the
# occurrence that is followed, within a few lines, by a FUNCTION header. Found by name, not by
# a line offset (the offset moved between the 2026-10-12 and 2026-10-13 report layouts;
# the label became "Thread-<tid> (PID:<pid>)" with a colon on 2026-10-13 night).
start=None
for i,l in enumerate(lines):
    if (l.startswith(f'"Thread-{tid} (PID') or l.startswith(f'"Thread-{tid} (PID:')) and any(x.startswith('FUNCTION,') for x in lines[i+1:i+6]):
        start=i; break
if start is None: sys.exit('thread block not found')
hdr=None; rows=[]
for l in lines[start+1:]:
    if l.startswith('FUNCTION,'): hdr=next(csv.reader([l])); continue
    if l.startswith('"Thread-') : break
    if hdr and l.startswith('"'): rows.append(next(csv.reader([l])))
ci={h:i for i,h in enumerate(hdr)}
addrs=[r[0].split('!')[1] for r in rows if r[0].startswith('reimpl-x64.exe!')]
out=subprocess.run(['addr2line','-f','-C','-e',exe],input='\n'.join(addrs)+'\n',capture_output=True,text=True).stdout.split('\n')
fn={a:out[2*i] for i,a in enumerate(addrs)}
agg=collections.defaultdict(collections.Counter); tot=0
for r in rows:
    ops=float(r[ci['IBS_ALL_OPS']]); tot+=ops
    f=fn.get(r[0].split('!')[1],'?') if r[0].startswith('reimpl-x64.exe!') else '['+r[0].split('!')[0]+']'
    c=agg[f]; c['ops']+=ops; c['ld']+=float(r[ci['IBS_LOAD']]); c['t2r']+=float(r[ci['IBS_TAG_TO_RETIRE_CYCLES']])
    c['ldmisslat']+=float(r[ci['IBS_LD_L1_DC_MISS_LAT']]); c['ldmiss']+=float(r[ci['IBS_LOAD']])*float(r[ci['IBS_LD_L1_DC_MISS_RATE_%']])/100
print(f'thread {tid}: rows {len(rows)} ops {tot:.0f}')
print(f"{'function':58s} {'ops%':>6s} {'t2r/op':>7s} {'loads':>7s} {'L1miss%':>8s} {'misslat%':>9s}")
for f,c in sorted(agg.items(),key=lambda kv:-kv[1]['ops'])[:n]:
    print(f"{f[:58]:58s} {100*c['ops']/tot:6.2f} {c['t2r']/max(c['ops'],1):7.1f} {c['ld']:7.0f} {100*c['ldmiss']/max(c['ld'],1):8.2f} {100*c['ldmisslat']/max(c['t2r'],1):9.2f}")
