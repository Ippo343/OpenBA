import collections
import os
import re
import sys

X = sys.argv[1]


def region(p):
    if p.startswith('code/Game/Component/'):
        return 'XA_component'
    if p.startswith(('code/Game/Work/', 'code/Game/Action/', 'code/Game/Entity/')):
        return 'XB_work_action_entity'
    if p.startswith('code/Game/'):
        return 'XC_game_dialog_player_tut'
    if p.startswith(('runtime/', 'engine/')):
        return 'XD_runtime'
    return 'XE_standalone'


out = collections.defaultdict(list)
files = collections.defaultdict(collections.Counter)
for ln in open(X + '/detail_all.txt', encoding='utf-8', errors='replace'):
    m = re.match(r'^(\S+?):(\d+):\d+\t(\S+)\t', ln)
    if not m or m.group(3).startswith(('tier_i', 'cast_ok')):
        continue
    r = region(m.group(1))
    out[r].append(ln)
    files[r][m.group(1)] += 1
for r in sorted(out):
    open(f'{X}/detail_{r}.txt', 'w', encoding='utf-8').writelines(out[r])
    print(r, len(out[r]), len(files[r]))
left = collections.defaultdict(list)
for root in ('code', 'runtime', 'standalone', 'engine'):
    for dp, dn, fn in os.walk(root):
        for n in fn:
            if not n.endswith(('.cpp', '.h')):
                continue
            p = os.path.join(dp, n).replace(os.sep, '/')
            for i, ln in enumerate(open(p, encoding='utf-8', errors='replace'), 1):
                if '// LEFT' in ln:
                    left[region(p)].append(f'{p}:{i}:{ln.strip()[:260]}\n')
for r in left:
    open(f'{X}/left_{r}.txt', 'w', encoding='utf-8').writelines(left[r])
    print(r, 'left', len(left[r]))
