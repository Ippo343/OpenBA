"""slice.py DETAIL — the wave-Y inventory inputs: every counted cast outside src/render/dx11/ with the reason tagged on
its line, split into four regions (run from the repo root)."""
import collections, re, sys
D = 'docs/runtime_port/endgame_b/b4_session/data/pass2y/'
def region(f):
    if f.startswith(('src/game/Component/', 'src/game/Entity/', 'src/game/Work/', 'src/game/Action/')) or f.endswith('wrk_abi_test.cpp'):
        return 'CW'
    if f.startswith(('src/game/Dialog/', 'src/game/Player/', 'standalone/')):
        return 'GAMEB'
    if f.startswith('code/'):
        return 'GAMEA'
    return 'RT'
src = {}
out = collections.defaultdict(list)
for ln in open(sys.argv[1], encoding='utf-8', errors='replace'):
    m = re.match(r'^(\S+?):(\d+):\d+\t(\S+)\t(.*)', ln)
    if not m or m.group(3).startswith(('tier_i', 'cast_ok')):
        continue
    f, n = m.group(1), int(m.group(2))
    if f not in src:
        src[f] = open(f, encoding='utf-8', errors='replace').read().split('\n')
    t = ''
    for i in (n - 1, n - 2):
        if 0 <= i < len(src[f]) and 'LEFT' in src[f][i] and ('//' in src[f][i] or '/*' in src[f][i]):
            t = re.split(r'LEFT:?', src[f][i], 1)[1].strip().strip('*/ ').strip()
            break
    out[region(f)].append(f'{f}\t{n}\t{m.group(3)}\t{t[:220]}\t{m.group(4).strip()[:140]}\n')
for r, rows in out.items():
    with open(D + f'lines_{r}.tsv', 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('file\tline\tkind\tleft_tag\tsnippet\n'); fh.writelines(rows)
    print(r, len(rows), 'casts in', len(set(x.split('\t')[0] for x in rows)), 'files')
