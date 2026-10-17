import csv,subprocess,collections,os,sys
# usage: ibs_agg.py <uprof-report.csv> [exe]   (exe defaults to this repo's build/cmake-perf/OpenBA.exe)
src=sys.argv[1]; exe=sys.argv[2] if len(sys.argv)>2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'build', 'cmake-perf', 'OpenBA.exe')
rows=[]; hdr=None; inF=False
for line in open(src,encoding='utf-8',errors='replace'):
    if 'HOTTEST FUNCTIONS' in line: inF=True; continue
    if inF and line.startswith('FUNCTION,'): hdr=next(csv.reader([line])); continue
    if inF and line.startswith('"'): rows.append(next(csv.reader([line])))
    elif inF and hdr and line.strip()=='': break
ci={h:i for i,h in enumerate(hdr)}
addrs=[r[0].split('!')[1] for r in rows if r[0].startswith('OpenBA.exe!')]
out=subprocess.run(['addr2line','-f','-C','-e',exe],input='\n'.join(addrs)+'\n',capture_output=True,text=True).stdout.split('\n')
fn={a:out[2*i] for i,a in enumerate(addrs)}
agg=collections.defaultdict(collections.Counter); tot=0; exetot=0
for r in rows:
    ops=float(r[ci['IBS_ALL_OPS']]); tot+=ops
    if not r[0].startswith('OpenBA.exe!'): f='['+r[0].split('!')[0]+']'
    else: f=fn.get(r[0].split('!')[1],'?'); exetot+=ops
    c=agg[f]; c['ops']+=ops; c['ld']+=float(r[ci['IBS_LOAD']]); c['t2r']+=float(r[ci['IBS_TAG_TO_RETIRE_CYCLES']])
    c['ldmisslat']+=float(r[ci['IBS_LD_L1_DC_MISS_LAT']]); c['ldmiss']+=float(r[ci['IBS_LOAD']])*float(r[ci['IBS_LD_L1_DC_MISS_RATE_%']])/100
    c['brmisp']+=float(r[ci['IBS_BR']])*float(r[ci['IBS_BR_MISP_RATE_%']])/100
print(f'rows {len(rows)}  ops in table {tot:.0f}  exe ops {exetot:.0f}')
print(f"{'function':58s} {'ops%':>6s} {'t2r/op':>7s} {'loads':>7s} {'L1miss%':>8s} {'misslat%':>9s} {'brmisp':>7s}")
for f,c in sorted(agg.items(),key=lambda kv:-kv[1]['ops'])[:int(sys.argv[2]) if len(sys.argv)>2 else 45]:
    print(f"{f[:58]:58s} {100*c['ops']/tot:6.2f} {c['t2r']/max(c['ops'],1):7.1f} {c['ld']:7.0f} {100*c['ldmiss']/max(c['ld'],1):8.2f} {100*c['ldmisslat']/max(c['t2r'],1):9.2f} {c['brmisp']:7.0f}")
