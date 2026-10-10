"""b11pair.py BASE TIP MAPFILE tu-substring... [--detail] -- b11cmp with a by-hand rename map.
(Written by block COMP-A, 2026-10-10; kit copy -- B8_RULES.md section 7 C1.)

MAPFILE: lines `old_bare_name -> new_bare_name` (demangled, no parameter list).
Per object: every function is compared with relocation targets reduced to their bare demangled
name and the map applied to the BASE side. A base function whose bare name is in the map is
paired with the tip function of the mapped name (RENAMED-IDENTICAL / RENAMED-SAME /
RENAMED-DIFF); a mapped base function with no tip counterpart in the object is ONLY-BASE
(a deleted forwarder, or a body that lives in another object). Static-init functions
(_GLOBAL__sub_I_*) are paired by position.
"""
import collections, os, sys, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'b4_session'))
import b11cmp as B

base, tip, mapfile = sys.argv[1:4]
subs = [a for a in sys.argv[4:] if not a.startswith('--')]
detail = '--detail' in sys.argv
rmap = {}
for ln in open(mapfile, encoding='utf-8'):
    if '->' in ln:
        a, b = [x.strip() for x in ln.split('->')]
        rmap[a] = b

_dm = {}
def dem(syms):
    need = [s for s in syms if s not in _dm]
    if need:
        for s, d in zip(need, B.demangle(need)):
            _dm[s] = B.bare(d)
    return [_dm[s] for s in syms]

def norm(f, side):
    syms = set()
    for t in f['text']:
        if t.startswith('  R '):
            syms.add(t[4:])
    syms |= {c for c in f['calls'] if not c.startswith(('call ', 'jmp '))}
    syms = sorted(syms)
    m = dict(zip(syms, dem(syms)))
    if side == 'base':
        m = {k: rmap.get(v, v) for k, v in m.items()}
    text = [('  R ' + m.get(t[4:], t[4:])) if t.startswith('  R ') else t for t in f['text']]
    calls = [m.get(c, c) for c in f['calls']]
    return text, calls

tot = collections.Counter()
for n in sorted(os.listdir(tip)):
    if not n.endswith('.o') or (subs and not any(s in n for s in subs)):
        continue
    bp, tp = os.path.join(base, n), os.path.join(tip, n)
    if not os.path.exists(bp):
        print(f'## {n}: ONLY IN TIP'); continue
    fb, ft = B.parse(bp), B.parse(tp)
    bnames = dict(zip(sorted(fb), dem(sorted(fb))))
    tnames = dict(zip(sorted(ft), dem(sorted(ft))))
    tby = collections.defaultdict(list)
    for s, d in tnames.items():
        tby[d].append(s)
    rows = []
    used_t = set()
    gb = sorted(s for s in fb if s.startswith('_GLOBAL__sub_I_'))
    gt = sorted(s for s in ft if s.startswith('_GLOBAL__sub_I_'))
    gpair = dict(zip(gb, gt)) if len(gb) == len(gt) else {}
    for s in sorted(fb):
        if s in ft and s not in gpair:
            t, kind = s, ''
        elif s in gpair:
            t, kind = gpair[s], 'RENAMED-'
        else:
            nb = rmap.get(bnames[s], None)
            cands = [c for c in tby.get(nb, []) if c not in fb] if nb else []
            if len(cands) == 1:
                t, kind = cands[0], 'RENAMED-'
            else:
                rows.append(('ONLY-BASE', s + (f'  [{bnames[s]} -> {nb}: not in this object]' if nb else ''), ''))
                continue
        used_t.add(t)
        xb, cb = norm(fb[s], 'base')
        xt, ct = norm(ft[t], 'tip')
        label = s if not kind else f'{bnames[s]} -> {tnames[t]}'
        if xb == xt:
            rows.append((kind + 'IDENTICAL', label, ''))
            continue
        why = []
        if cb != ct:
            why.append('calls: ' + ' | '.join(cb[:40]) + '  ==>  ' + ' | '.join(ct[:40]))
        if fb[s]['stores'] != ft[t]['stores']:
            why.append('stores(width,disp): ' + B.cdiff(fb[s]['stores'], ft[t]['stores']))
        if fb[s]['cmps'] != ft[t]['cmps']:
            why.append('compares(width,disp,imm): ' + B.cdiff(fb[s]['cmps'], ft[t]['cmps']))
        rows.append((kind + ('DIFF' if why else 'SAME'), label, '\n      '.join(why)))
    for t in sorted(set(ft) - used_t):
        rows.append(('ONLY-TIP', t + f'  [{tnames[t]}]', ''))
    c = collections.Counter(r[0] for r in rows)
    tot.update(c)
    print(f'## {n}: ' + ' '.join(f'{k} {v}' for k, v in sorted(c.items())))
    for v, fn, why in rows:
        if v not in ('IDENTICAL',) and (detail or v.endswith(('DIFF',)) or v.startswith('ONLY')):
            print(f'  {v} {fn}' + (f'\n      {why}' if why else ''))
print('# TOTAL ' + ' '.join(f'{k} {v}' for k, v in sorted(tot.items())))
