#!/usr/bin/env python3
"""filemap.py -- cut PROD into W1 blocks with DISJOINT FILE SETS (B4_RULES section 5).

Run from the repo root, no arguments. Writes
  docs/runtime_port/endgame_b/b4_session/data/filemap.tsv   (file, block, metric total)
  docs/runtime_port/endgame_b/b4_session/B4_FILEMAP.md      (the blocks, their sizes)

A block is a slice of one module directory. Files are grouped into FAMILIES by stem (the file name
up to its first '_' or '.', lower-cased: citizen_*.cpp, citizenai_*.cpp, CitizenComponent.h and
citizencomponent*.cpp are kept together by the ALIAS table), families are sorted by name and packed
greedily up to TARGET casts. A class header belongs to the block of its family. Headers owned by
W0 (H9) are listed as such: a W1 block reads them, it does not edit them.
"""
import csv, os, re, subprocess, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
SESSION = os.path.dirname(HERE)
TARGET = 1800

# module directory -> block prefix
MODULES = [
    ('code/Game/Component', 'COMP'), ('code/Game/Dialog', 'DLG'), ('code/Game/Work', 'WORK'),
    ('code/Game/Entity', 'WORK'), ('code/Game/Action', 'ACT'), ('code/Game/Game', 'GAME'),
    ('code/Game/Player', 'PLAYER'), ('code/Game/Tutorial', 'TUT'), ('code/Game/Goal', 'TUT'),
    ('code/Game/Object', 'TUT'),
    ('runtime/src/ui', 'R-UI'), ('runtime/types/ui', 'R-UI'),
    ('runtime/src/graphics', 'R-GFX'), ('runtime/types/graphics', 'R-GFX'),
    ('runtime/src/video', 'R-VIDEO'), ('runtime/types/video', 'R-VIDEO'),
    ('runtime/src/io', 'R-IO'), ('runtime/types/io', 'R-IO'), ('runtime/src/save', 'R-IO'),
    ('runtime/types/save', 'R-IO'), ('runtime/src/object', 'R-IO'), ('runtime/types/object', 'R-IO'),
    ('standalone', 'STANDALONE'), ('dx11', 'DX11'), ('engine', 'R-CORE'),
]
DEFAULT_RUNTIME = {'engine': 'R-ENG', 'application': 'R-ENG', 'achievement': 'R-ENG',
                   'input': 'R-ENG', 'audio': 'R-ENG'}
NOSPLIT = {'R-UI': 2, 'R-GFX': 1, 'R-VIDEO': 1, 'R-IO': 1, 'R-ENG': 1, 'R-CORE': 1,
           'STANDALONE': 1, 'DX11': 1, 'TUT': 1}

# families that must stay together although their stems differ
ALIAS = {
    'citizen': 'citizen', 'citizencomponent': 'citizen', 'citizenai': 'citizen',
    'citizenaicomponent': 'citizen', 'citizenaidescription': 'citizen',
    'citizendescription': 'citizen', 'citizenbehavior': 'citizen',
    'tradeui': 'trade', 'tradecomponent': 'trade', 'tradedescription': 'trade',
    'tradework': 'trade', 'trade': 'trade',
    'gameupdateloop': 'gameupdateloop',
}


def metric():
    out = subprocess.run([sys.executable, os.path.join(SESSION, 'castmetric.py'), 'code', 'runtime',
                          'standalone', 'dx11', 'engine', '--by-file', '--tsv'],
                         capture_output=True, text=True, check=True).stdout
    m = {}
    hdr = None
    for line in out.splitlines():
        c = line.rstrip('\r').split('\t')
        if c[0] in ('file', 'dir'):
            hdr = c
            continue
        if hdr and hdr[0] == 'file' and len(c) == len(hdr):
            m[c[0]] = int(c[hdr.index('total')])
    return m


def tracked():
    out = subprocess.run(['git', 'ls-files', 'code', 'runtime', 'standalone', 'dx11', 'engine'],
                         capture_output=True, text=True, check=True).stdout
    return [f for f in out.splitlines()
            if re.search(r'\.(cpp|h|inl|inc)$', f) and not re.search(r'(^|/)(build|build-[a-z]+)/', f)]


def module_of(f):
    best = None
    for d, b in MODULES:
        if f.startswith(d + '/') and (best is None or len(d) > len(best[0])):
            best = (d, b)
    if best:
        return best[1]
    m = re.match(r'runtime/(?:src|types)/([a-z0-9]+)/', f)
    if m:
        return DEFAULT_RUNTIME.get(m.group(1), 'R-CORE')
    if f.startswith('runtime/'):
        return 'R-CORE'
    return 'OTHER'


def family(f):
    stem = os.path.basename(f).lower()
    stem = re.split(r'[_.]', stem)[0]
    return ALIAS.get(stem, stem)


def main():
    m = metric()
    files = tracked()
    per = collections.defaultdict(lambda: collections.defaultdict(list))
    for f in files:
        per[module_of(f)][family(f)].append(f)
    rows = []
    blocks = collections.OrderedDict()
    for mod in sorted(per):
        fams = per[mod]
        total = sum(m.get(f, 0) for fl in fams.values() for f in fl)
        nblocks = NOSPLIT.get(mod) or max(1, round(total / TARGET))
        target = total / nblocks
        idx, acc = 0, 0
        letters = 'abcdefghijklmnopqrstuvwxyz'
        for fam in sorted(fams):
            ft = sum(m.get(f, 0) for f in fams[fam])
            if acc and acc + ft / 2 > target and idx < nblocks - 1:
                idx += 1
                acc = 0
            acc += ft
            name = mod if nblocks == 1 else '%s-%s' % (mod, letters[idx])
            b = blocks.setdefault(name, {'files': 0, 'casts': 0, 'fams': []})
            b['files'] += len(fams[fam]); b['casts'] += ft
            b['fams'].append((fam, ft))
            for f in sorted(fams[fam]):
                rows.append((f, name, m.get(f, 0)))
    with open(os.path.join(SESSION, 'data', 'filemap.tsv'), 'w', encoding='utf-8', newline='\n') as o:
        o.write('file\tblock\tmetric\n')
        for r in sorted(rows):
            o.write('%s\t%s\t%d\n' % r)
    with open(os.path.join(SESSION, 'B4_FILEMAP.md'), 'w', encoding='utf-8', newline='\n') as o:
        o.write('# B.4 file map (generated by scripts/filemap.py; do not hand-edit)\n\n')
        o.write('Every PROD file belongs to exactly one block: `data/filemap.tsv`. A block owns its\n'
                'files and the class headers among them. Headers finished by W0 (B4_RULES H9) are\n'
                'READ by every block and edited by none without the coordinator.\n\n')
        o.write('| block | files | metric | first / last family | largest families |\n|---|---:|---:|---|---|\n')
        tot = 0
        for name, b in blocks.items():
            big = sorted(b['fams'], key=lambda x: -x[1])[:6]
            o.write('| `%s` | %d | %d | %s … %s | %s |\n' % (
                name, b['files'], b['casts'], b['fams'][0][0], b['fams'][-1][0],
                ' · '.join('%s %d' % x for x in big if x[1])))
            tot += b['casts']
        o.write('\nTotal metric in the map: %d in %d files, %d blocks.\n' % (tot, len(rows), len(blocks)))
    print('blocks', len(blocks), 'files', len(rows), 'metric', sum(r[2] for r in rows))
    for name, b in blocks.items():
        print('%-14s %4d files %6d' % (name, b['files'], b['casts']))


if __name__ == '__main__':
    main()
