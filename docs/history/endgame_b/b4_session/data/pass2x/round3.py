"""round3.py DETAIL — round 3 of wave X: three agents by PARTITION of the tree (run from the repo root).

  X3-WORK  src/game/Work/ + src/game/Action/ + tests/unit/wrk_abi_test.cpp
  X3-GAME  the rest of code/ + standalone/
  X3-RT    runtime/ + engine/ + the rest of tests/
Writes assign/X3-<p>.lines.tsv: every counted cast of the partition with the reason tagged on its line
(`// LEFT: ...`), and prints the reason classes per partition.
"""
import collections
import re
import sys

D = 'docs/runtime_port/endgame_b/b4_session/data/pass2x/assign/'


def part(f):
    if f.startswith(('src/game/Work/', 'src/game/Action/')):
        return 'WORK'
    if f.startswith(('code/', 'standalone/')):
        return 'GAME'
    return 'RT'


def cls(t):
    tl = t.lower()
    if not t:
        return 'UNTAGGED'
    if 'not examined' in tl:
        return 'NOT-EXAMINED'
    if t.startswith('FLOOR?'):
        return 'FLOOR?'
    if t.startswith('SIG'):
        return 'SIG'
    if t.startswith('H5') or 'raw storage' in tl or 'raw triple' in tl or 'raw string' in tl:
        return 'H5'
    if 'forward' in tl:
        return 'FORWARD'
    if t.startswith('POD') or 'mirror' in tl:
        return 'POD'
    if 'erased' in tl:
        return 'ERASED'
    if 'const' in tl and 'find' in tl:
        return 'CONST'
    return 'OTHER'


src = {}
out = collections.defaultdict(list)
kinds = collections.defaultdict(collections.Counter)
for ln in open(sys.argv[1], encoding='utf-8', errors='replace'):
    m = re.match(r'^(\S+?):(\d+):\d+\t(\S+)\t(.*)', ln)
    if not m or m.group(3).startswith(('tier_i', 'cast_ok')):
        continue
    f, n = m.group(1), int(m.group(2))
    if f not in src:
        src[f] = open(f, encoding='utf-8', errors='replace').read().split('\n')
    t = ''
    for i in (n - 1, n - 2):
        if 0 <= i < len(src[f]) and '// LEFT' in src[f][i]:
            t = src[f][i].split('// LEFT', 1)[1].lstrip(': ').strip()
            break
    p = part(f)
    c = cls(t)
    kinds[p][c] += 1
    out[p].append(f'{f}\t{n}\t{c}\t{t[:200]}\t{m.group(4).strip()[:140]}\n')
for p in out:
    with open(D + f'X3-{p}.lines.tsv', 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('file\tline\tclass\tleft_tag\tsnippet\n')
        fh.writelines(out[p])
    print(p, len(out[p]), dict(kinds[p].most_common()))
