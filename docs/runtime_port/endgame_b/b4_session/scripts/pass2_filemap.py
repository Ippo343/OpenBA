#!/usr/bin/env python3
"""pass2_filemap.py — the SECOND-PASS file map (B.4), generated from the block-0 inventory.

Reads   b4_session/data/pass2/{A_citizen,B_component,C_work_action,D_dialog_player_tut,E_game,
        F_runtime,G_standalone}.tsv   (file, file_casts, blocker, casts, declared_in, note)
Writes  b4_session/data/pass2/filemap.tsv   file <TAB> wave <TAB> block <TAB> casts(wave-L/R work)
        b4_session/PASS2_FILEMAP.md         the block table, the header owners, the wave-X families

Rules (NEXT_SESSION 2.2, PASS2_BRIEF):
  * every file with a cast belongs to EXACTLY ONE block of wave L (code/Game, standalone) or
    wave R (runtime, engine); the script asserts it;
  * the tree-wide signature families run first, alone (wave S) and their casts are not counted
    into a block's work;
  * a header that must CHANGE (FIELD / NOCLASS / H5 rows name it in `declared_in`) has ONE owner:
    the block whose files hold the most casts waiting on it; every other block reads it and
    LEAVES what it cannot type;
  * blocks aim at <= 300 casts and <= 25 files.
Run from the repo root. Deterministic.
"""
import collections
import csv
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
P2 = os.path.join(HERE, '..', 'data', 'pass2')
OUT_MD = os.path.join(HERE, '..', 'PASS2_FILEMAP.md')
REGIONS = ['A_citizen', 'B_component', 'C_work_action', 'D_dialog_player_tut', 'E_game',
           'F_runtime', 'G_standalone']
WAVE_S = {'SIG:Serialize-stream': 'S1', 'SIG:Work-slots': 'S2', 'SIG:Action-slots': 'S2',
          'FIELD:InternalPtr-members-declared-as-T*': 'S2',
          'SIG:TableRegisterState/TableAddEvent': 'S3', 'CONST:FindOrLoad': 'S3'}
T1_FILES = {'standalone/tool_harness.cpp'}
MAXC, MAXF = 300, 25

rows = []
for r in REGIONS:
    for d in csv.DictReader(open(os.path.join(P2, r + '.tsv'), encoding='utf-8', errors='replace'),
                            delimiter='\t'):
        try:
            d['casts'] = int(d['casts'])
        except (TypeError, ValueError):
            continue
        d['file'] = d['file'].replace('\\', '/')
        if '/' not in d['file']:      # two inventories wrote bare file names
            cands = [p + d['file'] for p in (('standalone/',) if r == 'G_standalone' else
                                             ('code/Game/Game/', 'code/Game/Object/'))]
            hit = [c for c in cands if os.path.exists(c)]
            assert hit, (r, d['file'])
            d['file'] = hit[0]
        assert os.path.exists(d['file']), d['file']
        rows.append(d)

work = collections.Counter()          # file -> casts that are a block's work
swave = collections.Counter()         # wave-S key -> casts
for d in rows:
    if d['blocker'] in WAVE_S:
        swave[WAVE_S[d['blocker']]] += d['casts']
        work[d['file']] += 0
    elif d['file'] in T1_FILES:
        swave['T1'] += d['casts']
    else:
        work[d['file']] += d['casts']


def family(path):
    stem = os.path.basename(path).rsplit('.', 1)[0].lower()
    return stem.split('_')[0]


def group(path):
    """(wave, coarse group) of a file."""
    if path.startswith(('runtime/', 'engine/')):
        m = re.match(r'runtime/(?:src|types|funcs)/([a-z0-9]+)', path)
        ns = m.group(1) if m else 'misc'
        big = {'ui': 'R-UI', 'collection': 'R-COLL', 'io': 'R-IO', 'object': 'R-OBJ',
               'graphics': 'R-GFX', 'video': 'R-GFX'}
        return 'R', big.get(ns, 'R-SYS')
    if path.startswith('standalone/'):
        return 'L', 'SA'
    m = re.match(r'code/Game/([A-Za-z]+)/', path)
    d = m.group(1) if m else 'misc'
    if d == 'Component':
        f = family(path)
        if f == 'citizenai':
            return 'L', 'CAI'
        if f.startswith('citizen'):
            return 'L', 'CIT'
        return 'L', 'COMP'
    return 'L', {'Entity': 'ENT', 'Work': 'WORK', 'Action': 'ACT', 'Dialog': 'DLG', 'Player': 'PLAYER',
                 'Tutorial': 'TUT', 'Goal': 'TUT', 'Game': 'GAME', 'Object': 'GAME'}.get(d, 'MISC')


# pack: per coarse group, families in name order; a family is kept whole unless it exceeds MAXC
by_group = collections.defaultdict(lambda: collections.defaultdict(list))
for f in sorted(work):
    if f in T1_FILES:
        continue
    w, g = group(f)
    by_group[(w, g)][family(f)].append(f)

blocks = collections.OrderedDict()    # name -> list of files
assign = {}
for (w, g) in sorted(by_group):
    bins, cur, curc = [], [], 0
    for fam in sorted(by_group[(w, g)]):
        files = by_group[(w, g)][fam]
        famc = sum(work[f] for f in files)
        if famc > MAXC:               # a big family: its own bins, files in name order
            if cur:
                bins.append(cur)
                cur, curc = [], 0
            b, bc = [], 0
            for f in files:
                if b and (bc + work[f] > MAXC or len(b) >= MAXF):
                    bins.append(b)
                    b, bc = [], 0
                b.append(f)
                bc += work[f]
            if b:
                bins.append(b)
            continue
        if cur and (curc + famc > MAXC or len(cur) + len(files) > MAXF):
            bins.append(cur)
            cur, curc = [], 0
        cur += files
        curc += famc
    if cur:
        bins.append(cur)
    # merge a tiny trailing bin into its predecessor
    if len(bins) > 1 and sum(work[f] for f in bins[-1]) < 60 and \
            sum(work[f] for f in bins[-2]) + sum(work[f] for f in bins[-1]) <= MAXC + 60:
        last = bins.pop()
        bins[-1].extend(last)
    for i, b in enumerate(bins, 1):
        name = f'{g}-{i}' if len(bins) > 1 else g
        blocks[name] = (w, b)
        for f in b:
            assert f not in assign, f
            assign[f] = name

# header owners: declared_in of FIELD / NOCLASS / H5 rows
allfiles = {}
for root in ('code', 'runtime', 'standalone', 'engine'):
    for dp, dn, fn in os.walk(root):
        for n in fn:
            if n.endswith('.h'):
                allfiles.setdefault(n, []).append(os.path.join(dp, n).replace('\\', '/'))
need = collections.defaultdict(collections.Counter)      # header -> block -> casts
needkeys = collections.defaultdict(collections.Counter)  # header -> blocker -> casts
for d in rows:
    if d['blocker'].split(':')[0] not in ('FIELD', 'NOCLASS', 'H5') or d['blocker'] in WAVE_S:
        continue
    if d['file'] not in assign:
        continue
    for tok in re.findall(r'[\w/.]+\.h\b', d.get('declared_in') or ''):
        base = os.path.basename(tok)
        cands = allfiles.get(base, [])
        hit = [c for c in cands if c.endswith(tok)] or cands
        if len(hit) == 1:
            need[hit[0]][assign[d['file']]] += d['casts']
            needkeys[hit[0]][d['blocker']] += d['casts']

# wave X: SIG / FWD / ERASED / POD families by declaring header (cross-file signatures)
xfam = collections.defaultdict(lambda: [0, set(), collections.Counter()])
for d in rows:
    k = d['blocker'].split(':')[0]
    if k not in ('SIG', 'FWD', 'ERASED', 'POD') or d['blocker'] in WAVE_S or d['file'] in T1_FILES:
        continue
    dec = (d.get('declared_in') or '').strip()
    hs = [os.path.basename(t) for t in re.findall(r'[\w/.]+\.h\b', dec)]
    key = hs[0] if hs else '(file-local / no header)'
    if hs and 'exe_bodies.h' in hs[0]:
        m = re.search(r'(\w+)/exe_bodies\.h', dec)
        key = (m.group(1) + '/exe_bodies.h') if m else 'exe_bodies.h'
    xfam[key][0] += d['casts']
    xfam[key][1].add(d['file'])
    xfam[key][2][d['blocker']] += d['casts']

with open(os.path.join(P2, 'filemap.tsv'), 'w', encoding='utf-8', newline='\n') as f:
    f.write('file\twave\tblock\tcasts\n')
    for p in sorted(assign):
        f.write(f'{p}\t{blocks[assign[p]][0]}\t{assign[p]}\t{work[p]}\n')
    for p in sorted(T1_FILES):
        f.write(f'{p}\tS\tT1\t0\n')

L = ['# B.4 second-pass file map (generated by scripts/pass2_filemap.py; do not hand-edit)', '',
     'Source: the block-0 inventory of 2026-10-03 (`data/pass2/*.tsv`, one row per file and blocker).',
     'Every file with a cast is in exactly one block: `data/pass2/filemap.tsv`. A block owns its files;',
     'a header that must change has ONE owner (table 2); everything else is read-only for the block.', '',
     '## 0. Wave S — tree-wide signatures and the tool harness (first; merged and gated before any block below starts)', '',
     '| block | what | casts |', '|---|---|---:|',
     f"| `S1` | the `void*` stream of every `Serialize` / `Persist` -> `IO::Stream&` | {swave['S1']} |",
     f"| `S2` | `Work.h` / `Action.h` slot parameters, leaf `InternalPtr` members | {swave['S2']} |",
     f"| `S3` | `controller_state_table.h` handler types; `FindOrLoad` / `FindNext` const | {swave['S3']} |",
     f"| `T1` | `standalone/tool_harness.cpp` + `EditTool_HarnessAcquire` | {swave['T1']} |", '',
     '## 1. Waves L (code/Game, standalone) and R (runtime) — blocks by file, disjoint', '',
     '| block | wave | files | casts | largest files |', '|---|---|---:|---:|---|']
tot = 0
for name, (w, b) in blocks.items():
    c = sum(work[f] for f in b)
    tot += c
    top = sorted(b, key=lambda f: -work[f])[:4]
    L.append(f"| `{name}` | {w} | {len(b)} | {c} | " +
             ' · '.join(f"{os.path.basename(f)} {work[f]}" for f in top if work[f]) + ' |')
L += ['', f'Total: {tot} casts in {len(assign)} files, {len(blocks)} blocks '
          f'(+ wave S {sum(swave.values())}; `dx11/` 240 belongs to the dx11 seam step).', '',
      '## 2. Headers that must change, and their ONE owner', '',
      'Owner = the block whose files hold the most casts waiting on the header. Another block that needs',
      'the header LEAVES the read (original spelling + `// LEFT: <header> (<owner block>)`) and lists it;',
      'wave X picks those up. A header not in this table that a block finds it must change: ask the',
      'coordinator before editing.', '',
      '| header | owner | waiting casts (block: n) | blockers |', '|---|---|---|---|']
for h in sorted(need, key=lambda h: -sum(need[h].values())):
    c = need[h]
    # a header is owned inside its own wave: a runtime header by an R block, a code header by an L block
    hw = group(h)[0]
    same = [b for b in c if blocks[b][0] == hw]
    if not same:
        if hw == 'L':
            continue                  # a code header only runtime rows name: a mis-resolved basename
        cand = sorted(n for n, (w, fs) in blocks.items()
                      if w == 'R' and any(group(f)[1] == group(h)[1] for f in fs))
        owner = (cand[0] + ' (no R reader; its namespace block)') if cand else 'coordinator'
        L.append(f"| `{h}` | `{owner}` | " + ', '.join(f'{b}: {n}' for b, n in c.most_common(5)) + ' | ' +
                 '; '.join(f'{k} {n}' for k, n in needkeys[h].most_common(3)) + ' |')
        continue
    owner = sorted(same, key=lambda b: (-c[b], b))[0]
    L.append(f"| `{h}` | `{owner}` | " + ', '.join(f'{b}: {n}' for b, n in c.most_common(5)) + ' | ' +
             '; '.join(f'{k} {n}' for k, n in needkeys[h].most_common(3)) + ' |')
L += ['', '## 3. Wave X — cross-file signatures, forwarders, type-erased cores, POD mirrors (after L and R)', '',
      'Cut by DECLARING header once L / R are merged (the callers then hold typed values, so the rewire',
      'is mechanical). One agent per header family, owning the header and every caller; families whose',
      'caller sets overlap run in sequence. Forwarder inventory: `data/pass2/FORWARDERS.tsv`.', '',
      '| declaring header | casts | files | largest keys |', '|---|---:|---:|---|']
for k in sorted(xfam, key=lambda k: -xfam[k][0]):
    c, fs, ks = xfam[k]
    if c < 15:
        continue
    L.append(f"| `{k}` | {c} | {len(fs)} | " + '; '.join(f'{a} {n}' for a, n in ks.most_common(3)) + ' |')
small = sum(v[0] for k, v in xfam.items() if v[0] < 15)
L += [f'| (families under 15 casts) | {small} | | |', '']
open(OUT_MD, 'w', encoding='utf-8', newline='\n').write('\n'.join(L))
print(f'{len(blocks)} blocks, {len(assign)} files, {tot} casts; wave S {dict(swave)}')
for name, (w, b) in blocks.items():
    print(f'  {name:10s} {w} files {len(b):3d} casts {sum(work[f] for f in b)}')
