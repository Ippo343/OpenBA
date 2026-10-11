#!/usr/bin/env python3
"""F_all_spellings.py -- block 0 inventory F (B.4, NEXT_SESSION.md 3b.6).

Re-runs from the repo root with no arguments and rewrites
docs/runtime_port/endgame_b/b4_session/data/F_all_spellings.tsv: one row per file of PROD
(code runtime standalone dx11 engine) and tests/, every spelling of a reinterpretation counted
by ../castmetric.py (the metric itself), the `// B.3.x bridge` marker lines, and the
classification of every `static_cast` in the file (s_* columns). Prints the per-directory
summary and the summed metric for PROD and for tests.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import castmetric as CM  # noqa: E402

PROD = ('code', 'runtime', 'standalone', 'dx11', 'engine')
STATIC = ('arith', 'conv', 'hier', 'hier_ref', 'to_void', 'void_inner', 'void', 'cross',
          'ptr_unknown', 'null', 'uintptr_arith', 'uintptr_addr', 'discard', 'unparsed')
OUT = os.path.join(os.path.dirname(HERE), 'data', 'F_all_spellings.tsv')


def main():
    rows = CM.run(list(PROD) + ['tests'])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    hdr = ['file'] + list(CM.COLS) + ['static_cast'] + ['s_' + k for k in STATIC]
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\t'.join(hdr) + '\n')
        for rel, c, sc, hits in rows:
            nstatic = sum(sc.values())
            if not any(c[k] for k in CM.COLS) and not nstatic:
                continue
            f.write('\t'.join([rel] + [str(c[k]) for k in CM.COLS] + [str(nstatic)] +
                              [str(sc.get(k, 0)) for k in STATIC]) + '\n')
    for label, pred in (('PROD', lambda r: not r.startswith('tests/')),
                        ('tests', lambda r: r.startswith('tests/'))):
        tot = dict.fromkeys(CM.COLS, 0)
        for rel, c, sc, hits in rows:
            if pred(rel):
                for k in CM.COLS:
                    tot[k] += c[k]
        print(label, ' '.join(f'{k}={tot[k]}' for k in CM.COLS))
    print('wrote', os.path.relpath(OUT, CM.ROOT).replace('\\', '/'))


if __name__ == '__main__':
    main()
