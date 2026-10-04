"""round2.py NAGENTS MAXFILES — cut wave X round 2 from the TREE (run from the repo root).

Input: `castmetric.py code runtime standalone engine --detail` on stdin-file argv[3].
For every counted cast, the `// LEFT: SIG <name>` tag on its line (or the line above) names the
declaration it waits on. A family = one such name; its files = every .cpp / .h under
code/ standalone/ runtime/ engine/ tests/ that names it (word match; the shared declaration
headers exe_bodies.h / *_funcs.h do not make two families collide).

Agents get pairwise-disjoint file sets: a family joins the one cluster its files touch, opens a
new one while there is room, or waits (a family touching two clusters is listed as WAIT).
Every other cast in a file belongs to the agent that owns the file; files with casts that no
cluster claimed are dealt round-robin by directory to the smallest cluster.
Writes assign/X-G2-<n>.files, .families.tsv, .lines.tsv and prints the cut.
"""
import collections
import os
import re
import sys

NAG = int(sys.argv[1])
MAXF = int(sys.argv[2])
DETAIL = sys.argv[3]
D = 'docs/runtime_port/endgame_b/b4_session/data/pass2x/assign/'
ROOTS = ('code', 'standalone', 'runtime', 'engine', 'tests')
SHARED = re.compile(r'(exe_bodies\.h|_funcs\.h)$')

src = {}
for root in ROOTS:
    for dp, dn, fn in os.walk(root):
        for n in fn:
            if n.endswith(('.cpp', '.h', '.inc')):
                p = os.path.join(dp, n).replace(os.sep, '/')
                src[p] = open(p, encoding='utf-8', errors='replace').read()
lines = {p: t.split('\n') for p, t in src.items()}

casts = []
for ln in open(DETAIL, encoding='utf-8', errors='replace'):
    m = re.match(r'^(\S+?):(\d+):\d+\t(\S+)\t(.*)', ln)
    if not m or m.group(3).startswith(('tier_i', 'cast_ok')):
        continue
    casts.append((m.group(1), int(m.group(2)), m.group(3), m.group(4).strip()))


def tag(f, n):
    L = lines.get(f, [])
    for i in (n - 1, n - 2, n):
        if 0 <= i < len(L) and '// LEFT' in L[i]:
            return L[i].split('// LEFT', 1)[1].lstrip(': ').strip()
    return ''


fam_casts = collections.Counter()
cast_tag = {}
for f, n, k, s in casts:
    t = tag(f, n)
    cast_tag[(f, n)] = t
    m = re.match(r'SIG\s+([A-Za-z_][\w:]*)', t)
    if m:
        fam_casts[m.group(1)] += 1

# files naming each family (word match on the last component of a qualified name)
words = collections.defaultdict(set)
want = {name: name.split('::')[-1] for name in fam_casts}
rx = re.compile(r'\b(' + '|'.join(sorted(set(re.escape(w) for w in want.values()), key=len, reverse=True)) + r')\b') if want else None
if rx:
    for p, t in src.items():
        if SHARED.search(p):
            continue
        for w in set(rx.findall(t)):
            words[w].add(p)
fam_files = {name: set(words[w]) for name, w in want.items()}

clusters = []
owner = {}
wait = []
for name, n in fam_casts.most_common():
    f = fam_files[name]
    if not f or len(f) > MAXF:
        wait.append((name, n, len(f)))
        continue
    hit = set(owner[x] for x in f if x in owner)
    if len(hit) > 1:
        wait.append((name, n, len(f)))
        continue
    if hit:
        c = hit.pop()
        if len(clusters[c][1] | f) > MAXF:
            wait.append((name, n, len(f)))
            continue
    elif len(clusters) < NAG:
        clusters.append([0, set(), []])
        c = len(clusters) - 1
    else:
        c = min(range(NAG), key=lambda i: len(clusters[i][1]))
        if len(clusters[c][1] | f) > MAXF:
            wait.append((name, n, len(f)))
            continue
    clusters[c][0] += n
    clusters[c][1] |= f
    clusters[c][2].append((name, n))
    for x in f:
        owner[x] = c

# deal the unclaimed files that still hold casts, by directory, to the smallest cluster
by_dir = collections.defaultdict(set)
for f, n, k, s in casts:
    if f not in owner:
        by_dir[os.path.dirname(f)].add(f)
for d in sorted(by_dir, key=lambda d: -len(by_dir[d])):
    c = min(range(len(clusters)), key=lambda i: len(clusters[i][1]))
    clusters[c][1] |= by_dir[d]
    for x in by_dir[d]:
        owner[x] = c

per = collections.Counter(owner.get(f, -1) for f, n, k, s in casts)
for i, c in enumerate(clusters):
    name = f'X-G2-{i + 1}'
    with open(D + name + '.files', 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(sorted(c[1])) + '\n')
    with open(D + name + '.families.tsv', 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('family\tcasts\tfiles\n')
        for nm, n in c[2]:
            fh.write(f'{nm}\t{n}\t{"; ".join(sorted(fam_files[nm]))}\n')
    with open(D + name + '.lines.tsv', 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('file\tline\tkind\tleft_tag\tsnippet\n')
        for f, n, k, s in casts:
            if owner.get(f) == i:
                fh.write(f'{f}\t{n}\t{k}\t{cast_tag[(f, n)]}\t{s[:160]}\n')
    dirs = collections.Counter('/'.join(x.split('/')[:3]) for x in c[1])
    print(name, f'{len(c[2])} SIG families ({c[0]} casts), {len(c[1])} files, {per[i]} casts in them;',
          ', '.join(f'{d} {n}' for d, n in dirs.most_common(5)))
with open(D + 'X-G2-WAIT.tsv', 'w', encoding='utf-8', newline='\n') as fh:
    fh.write('family\tcasts\tfiles\n')
    for nm, n, nf in wait:
        fh.write(f'{nm}\t{n}\t{nf}\n')
print('WAIT (spans two agents or too wide):', len(wait), 'families,', sum(w[1] for w in wait), 'casts')
print('unowned casts:', per[-1])
