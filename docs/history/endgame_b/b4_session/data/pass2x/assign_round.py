"""assign_round.py PACK.tsv ROUND PREFIX — write one assignment per agent of a packed round.

For agent N of the round: assign/<PREFIX><N>.tsv      the family rows (declaration, signatures, callers)
                          assign/<PREFIX><N>.files    the file set it owns (one path per line)
                          assign/<PREFIX><N>.lines.tsv every cast line of the inventory in those files
Run from the repo root. The pack is made by pack.py over the family tables (see PASS2X_BRIEF.md).
"""
import collections
import csv
import glob
import sys

D = 'docs/runtime_port/endgame_b/b4_session/data/pass2x/'
BS = chr(92)
pack, rnd, prefix = sys.argv[1], sys.argv[2], sys.argv[3]

fam = {}
hdr = None
for p in glob.glob(D + 'X*_families.tsv'):
    rd = csv.DictReader(open(p, encoding='utf-8', errors='replace'), delimiter='\t')
    hdr = hdr or rd.fieldnames
    for r in rd:
        k = (r.get('family') or '').strip()
        if k:
            fam.setdefault(k, []).append(r)

lines = []
lhdr = None
for p in glob.glob(D + 'X*.tsv'):
    if p.endswith('_families.tsv'):
        continue
    rd = csv.DictReader(open(p, encoding='utf-8', errors='replace'), delimiter='\t')
    lhdr = lhdr or rd.fieldnames
    for r in rd:
        r['file'] = (r.get('file') or '').replace(BS, '/')
        lines.append(r)

agents = collections.defaultdict(lambda: [[], set()])
for r in csv.DictReader(open(pack, encoding='utf-8'), delimiter='\t'):
    if r['round'] != rnd:
        continue
    a = agents[r['agent']]
    a[0].append(r['family'])
    a[1] |= set(f for f in r['files'].split(';') if f)

for n, (fams, files) in sorted(agents.items()):
    name = f'{prefix}{n}'
    with open(D + f'assign/{name}.tsv', 'w', encoding='utf-8', newline='') as fh:
        w = csv.DictWriter(fh, fieldnames=hdr, delimiter='\t', extrasaction='ignore')
        w.writeheader()
        for k in fams:
            w.writerows(fam.get(k, []))
    with open(D + f'assign/{name}.files', 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(sorted(files)) + '\n')
    mine = [r for r in lines if r['file'] in files]
    with open(D + f'assign/{name}.lines.tsv', 'w', encoding='utf-8', newline='') as fh:
        w = csv.DictWriter(fh, fieldnames=lhdr, delimiter='\t', extrasaction='ignore')
        w.writeheader()
        w.writerows(mine)
    kinds = collections.Counter(r['family'].split(':')[0] for r in mine)
    print(name, len(fams), 'families', len(files), 'files', len(mine), 'cast lines', dict(kinds))
