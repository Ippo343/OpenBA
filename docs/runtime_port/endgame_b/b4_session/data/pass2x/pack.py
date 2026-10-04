"""pack.py [BIG] [NAGENTS] [MAXFILES] — cut wave X from the family tables.

Families whose caller set is larger than BIG files are 'tree-wide sweeps' (run alone, one at a
time). The rest are packed into rounds: in one round NAGENTS clusters with pairwise-disjoint file
sets; a family joins the one cluster its files touch (or opens a new one); a family touching two
clusters, or overflowing MAXFILES, waits for the next round.
"""
import collections
import csv
import glob
import re
import sys

BIG = int(sys.argv[1]) if len(sys.argv) > 1 else 25
NAG = int(sys.argv[2]) if len(sys.argv) > 2 else 4
MAXF = int(sys.argv[3]) if len(sys.argv) > 3 else 70

fams = {}
for p in glob.glob(sys.argv[4]):
    for r in csv.DictReader(open(p, encoding='utf-8', errors='replace'), delimiter='\t'):
        k = (r.get('family') or '').strip()
        if not k:
            continue
        try:
            n = int(r['casts'] or 0)
        except (ValueError, TypeError):
            n = 0
        files = set()
        for f in (r.get('all_caller_files') or '').split(';'):
            f = re.sub(r'\(\d+\)', '', f).strip().replace('\\', '/')
            if re.match(sys.argv[5], f):
                files.add(f)
        for x in ('declared_at', 'defined_at'):
            for m in re.finditer(r'((?:code|runtime|standalone|engine|tests)/[\w/\.\-]+\.(?:h|cpp))', r.get(x) or ''):
                files.add(m.group(1))
        if k in fams:
            fams[k][0] += n
            fams[k][1] |= files
        else:
            fams[k] = [n, files]

HDR = re.compile(r'(exe_bodies\.h|_funcs\.h)$')


def cpp(files):
    # shared declaration headers do not make two families collide: the round's agents append to
    # them in separate sections and the coordinator merges; only .cpp / class headers count.
    return set(f for f in files if not HDR.search(f))


big = {k: v for k, v in fams.items() if len(cpp(v[1])) > BIG}
rest = {k: v for k, v in fams.items() if k not in big}
print(f'families {len(fams)}  casts {sum(v[0] for v in fams.values())}')
print(f'tree-wide (> {BIG} files): {len(big)} families, {sum(v[0] for v in big.values())} casts')
for k, v in sorted(big.items(), key=lambda kv: -kv[1][0]):
    print(f'   {v[0]:4d} casts {len(cpp(v[1])):4d} files  {k}')

todo = sorted(rest.items(), key=lambda kv: -kv[1][0])
rnd = 0
out = []
while todo and rnd < 6:
    rnd += 1
    clusters = []  # [casts, files, [families]]
    owner = {}
    wait = []
    for k, (n, f) in todo:
        f = cpp(f)
        hit = set(owner[x] for x in f if x in owner)
        if len(hit) > 1:
            wait.append((k, (n, f)))
            continue
        if len(hit) == 1:
            c = hit.pop()
            if len(clusters[c][1] | f) > MAXF:
                wait.append((k, (n, f)))
                continue
        else:
            if len(clusters) >= NAG:
                # join the smallest cluster only if it stays within budget
                c = min(range(len(clusters)), key=lambda i: len(clusters[i][1]))
                if len(clusters[c][1] | f) > MAXF:
                    wait.append((k, (n, f)))
                    continue
            else:
                clusters.append([0, set(), []])
                c = len(clusters) - 1
        clusters[c][0] += n
        clusters[c][1] |= f
        clusters[c][2].append(k)
        for x in f:
            owner[x] = c
    print(f'round {rnd}: ' + '  '.join(f'[{c[0]} casts, {len(c[1])} files, {len(c[2])} fams]' for c in clusters)
          + f'   waiting: {len(wait)} fams, {sum(w[1][0] for w in wait)} casts')
    for i, c in enumerate(clusters):
        for k in c[2]:
            out.append((rnd, i + 1, k, fams[k][0], ';'.join(sorted(cpp(fams[k][1])))))
    todo = wait
with open(sys.argv[6], 'w', encoding='utf-8', newline='') as fh:
    w = csv.writer(fh, delimiter='\t')
    w.writerow(['round', 'agent', 'family', 'casts', 'files'])
    w.writerows(out)
    for k, v in sorted(big.items(), key=lambda kv: -kv[1][0]):
        w.writerow(['SWEEP', 0, k, v[0], ';'.join(sorted(cpp(v[1])))])
