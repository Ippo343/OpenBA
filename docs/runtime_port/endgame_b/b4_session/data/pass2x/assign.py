"""assign.py — cut the first wave-X assignments (run from the repo root).

X-GS  the code/Game tree-wide sweep: the virtual-slot parameter families (time, state machine,
      action stack) and the ValidateCell / FindCell / CalcDistance trio.
X-R1  the runtime sweep 1: the ExternalPtr serialize thunks and the RtObject mirrors.
X-RL  runtime lines that wait on nothing outside their file, in files neither sweep touches.
Prints the overlaps; an overlap of code files between X-GS and X-R1 is resolved in the prompts
(X-R1 never edits code/).
"""
import collections
import csv
import glob
import re

D = 'docs/runtime_port/endgame_b/b4_session/data/pass2x/'
BS = chr(92)
PATH = re.compile(r'((?:code|runtime|standalone|engine|tests)/[\w/\.\-]+\.(?:h|cpp))')


def fam_rows(pat):
    out = []
    for p in glob.glob(D + pat):
        out += list(csv.DictReader(open(p, encoding='utf-8', errors='replace'), delimiter='\t'))
    return out


def files_of(r):
    s = set()
    for f in (r.get('all_caller_files') or '').split(';'):
        f = re.sub(r'\(\d+\)', '', f).strip().replace(BS, '/')
        if PATH.match(f):
            s.add(f)
    for x in ('declared_at', 'defined_at'):
        for m in PATH.finditer(r.get(x) or ''):
            s.add(m.group(1))
    return s


def casts(rows):
    n = 0
    for r in rows:
        try:
            n += int(r['casts'] or 0)
        except (ValueError, TypeError):
            pass
    return n


allf = [r for r in fam_rows('X*_families.tsv') if (r.get('family') or '').strip()]
hdr = list(allf[0].keys())


def write(name, rows, fields=None):
    with open(D + 'assign/' + name, 'w', encoding='utf-8', newline='') as fh:
        w = csv.DictWriter(fh, fieldnames=fields or hdr, delimiter='\t', extrasaction='ignore')
        w.writeheader()
        w.writerows(rows)


GS = [r for r in allf
      if re.match(r'SLOT:(Component|Controller|Action|StateMachine|EntityList)', r['family'])
      or re.search(r'ValidateCell_a00f0|WorkGrid_FindCell_9ccf0|WorkBase_CalcDistance_8830|'
                   r'ActionExecutor_c4c50|EntityList::Update', r['family'])]
write('X-GS.tsv', GS)
gsf = set().union(*[files_of(r) for r in GS])
R1 = [r for r in allf if re.match(r'POD:(ExternalPtr-thunk|RtObject)', r['family'])]
write('X-R1.tsv', R1)
r1f = set().union(*[files_of(r) for r in R1])
both = gsf & r1f
print('X-GS', len(GS), 'families', casts(GS), 'casts', len(gsf), 'files')
print('X-R1', len(R1), 'families', casts(R1), 'casts', len(r1f), 'files')
print('overlap', len(both), sorted(both)[:14])

rows = list(csv.DictReader(open(D + 'XD_runtime.tsv', encoding='utf-8', errors='replace'), delimiter='\t'))
byf = collections.defaultdict(list)
for r in rows:
    r['file'] = r['file'].replace(BS, '/')
    byf[r['file']].append(r)
RL = []
for f, rs in byf.items():
    if f in r1f or f in gsf:
        continue
    RL += [r for r in rs if r['family'].split(':')[0] in ('LOCAL', 'H5', 'FIELD', 'FLOOR', 'OTHER', 'H8')]
write('X-RL.tsv', RL, list(rows[0].keys()))
print('X-RL', len(RL), 'lines', len(set(r['file'] for r in RL)), 'files',
      dict(collections.Counter(r['family'].split(':')[0] for r in RL)))
print(collections.Counter(r['file'] for r in RL).most_common(8))
