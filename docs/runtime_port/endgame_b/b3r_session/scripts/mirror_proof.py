#!/usr/bin/env python3
r"""mirror_proof.py -- B3R W0-SM MIRROR: prove statically that deleting the
name<->address mirror (standalone/resolve_standalone.cpp + rt_export_addrs.inc)
changes nothing a reader can see.

Run from the repo root, no arguments (optional: --base REV, default eb6f8600,
the tree the mirror was deleted from):

    python docs/runtime_port/endgame_b/b3r_session/scripts/mirror_proof.py

What it does
  BEFORE = the base revision, read out of git (`git cat-file`), WITH the mirror.
  AFTER  = the working tree, WITHOUT the mirror.
  For each tree it extracts every explicit Runtime registration from CODE
  (comments stripped by b3x_session/codegrep.strip; tests/unit excluded, it is
  not linked into the exe; declarations excluded):
    RegisterRuntimeExport("literal" | kConst, fn)   RegisterRuntimeAddr(0x.. | (uintptr_t)0x.., fn)
    the two {addr, "name"} kRows loops (class_operator_delete / ui_operator_delete)
    the mint loop (standalone/rt_data_mint_tables.inc, while it exists)
  BEFORE's resolvable sets are then closed under resolve_standalone.cpp's mirror
  (the export table of the base revision's rt_export_addrs.inc: name -> its
  address; address -> the FIRST name emplace()d for it).
  A READER of a key is any code line (all roots, tests/unit included) holding the
  mangled name as a string literal, or the address as a hex literal in any
  spelling (`0x..`, `0x..ull`, `0x..ULL`, `(uintptr_t)0x..`, space-padded),
  outside a Register* call. Every spelling of BLOCK0_BRIEF's door list reduces
  to one of those two literal forms at its binding site; string contents are
  not read for addresses (a printf'd address is not a reader).

It asserts
  (1) for every key with a reader: resolvable(BEFORE) == resolvable(AFTER);
  (2) every key registered in AFTER only (the new explicit rows) was resolvable
      in BEFORE, and its body is the body the mirror handed out (the body of
      the counterpart key's explicit registration(s), which must be unique);
  (3) the [RtPort] runtime registry numbers: BEFORE (mint excluded -- the mint
      registers after the printf) must equal the gate's 1347 / 1817, which
      validates the extraction; AFTER is printed as the expected new line.
Exit 0 when all hold, 1 otherwise.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'docs', 'runtime_port', 'endgame_b', 'b3x_session'))
from codegrep import strip, EXCL  # noqa: E402

ROOTS = ('code', 'engine', 'standalone', 'runtime', 'dx11', 'tests/unit')
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')
GENERATED = ('standalone/rt_export_addrs.inc', 'standalone/rt_data_mint_tables.inc')
KNOWN_PRINT = (1347, 1817)          # [RtPort] runtime registry on the base (GATE_TABLE, B3X close)
MINT_TABLE = 'standalone/rt_data_mint_tables.inc'
EXPORT_TABLE = 'standalone/rt_export_addrs.inc'


def git(*args, inp=None):
    return subprocess.run(['git', '-C', REPO] + list(args), input=inp,
                          stdout=subprocess.PIPE, check=True).stdout


def load_rev(rev):
    names = git('ls-tree', '-r', '--name-only', rev, '--', *ROOTS).decode().split('\n')
    names = [n for n in names if n.endswith(EXTS) and not any(e in n for e in EXCL)]
    out = git('cat-file', '--batch', inp=''.join(f'{rev}:{n}\n' for n in names).encode())
    files, i = {}, 0
    for n in names:
        nl = out.index(b'\n', i)
        size = int(out[i:nl].split()[2])
        files[n] = out[nl + 1:nl + 1 + size].decode('utf-8', 'replace')
        i = nl + 1 + size + 1
    return files


def load_worktree():
    files = {}
    for r in ROOTS:
        for dp, _dn, fs in os.walk(os.path.join(REPO, r)):
            for f in fs:
                p = os.path.relpath(os.path.join(dp, f), REPO).replace(os.sep, '/')
                if p.endswith(EXTS) and not any(e in p for e in EXCL):
                    files[p] = open(os.path.join(REPO, p), encoding='utf-8', errors='replace').read()
    return files


def call_args(src, i):
    """src[i] == '(' -> ([args], index of the closing paren)."""
    d = 0; cur = []; out = []; j = i; n = len(src)
    while j < n:
        c = src[j]
        if c == '"':
            k = j + 1
            while k < n and src[k] != '"':
                k += 2 if src[k] == '\\' else 1
            cur.append(src[j:k + 1]); j = k + 1
            continue
        if c in '([{':
            d += 1
            if d == 1:
                j += 1; continue
        elif c in ')]}':
            d -= 1
            if d == 0:
                out.append(''.join(cur)); return out, j
        if c == ',' and d == 1:
            out.append(''.join(cur)); cur = []
        else:
            cur.append(c)
        j += 1
    return out, j


STR = r'"(?:[^"\\\n]|\\.)*"'
REG_CALL = re.compile(r'\bRegisterRuntime(Export|Addr)\s*\(')
HEX = re.compile(r'^\(?\s*(?:\(\s*uintptr_t\s*\)\s*|uintptr_t\s*\(\s*|static_cast<uintptr_t>\s*\(\s*)?'
                 r'0[xX]([0-9a-fA-F]+)[uUlL]*\s*\)?\s*$')
CONST = re.compile(r'\b(k\w+)\s*(?:\[\s*\])?\s*=\s*((?:' + STR + r'\s*)+)')
ROWS = re.compile(r'\{\s*0x([0-9a-fA-F]+)ull\s*,\s*(' + STR + r')\s*\}')
MINT_ROW = re.compile(r'\{\s*(?:(' + STR + r')|nullptr),\s*0x([0-9a-fA-F]+)ull,\s*\d+,\s*&kStorageBytes')


def cstr(lit):
    """a run of adjacent C string literals -> its value (no escapes occur in mangled names)."""
    return ''.join(m[1:-1] for m in re.findall(STR, lit))


def registrations(files):
    """[(kind 'name'|'addr', key, fn_expr, where)] -- every explicit Runtime registration."""
    regs, problems = [], []
    for p, raw in sorted(files.items()):
        if p.startswith('tests/') or p in GENERATED:
            continue
        src = strip(raw)
        consts = {m.group(1): cstr(m.group(2)) for m in CONST.finditer(src)}
        for m in REG_CALL.finditer(src):
            args, _end = call_args(src, m.end() - 1)
            if len(args) != 2:
                problems.append(f'{p}: odd Register call {args}')
                continue
            key, fn = ' '.join(args[0].split()), ' '.join(args[1].split())
            if re.match(r'^(const\s+)?(char|uintptr_t)\b', key):
                continue                          # a declaration / the definition
            where = f'{p}:{src.count(chr(10), 0, m.start()) + 1}'
            kind = 'name' if m.group(1) == 'Export' else 'addr'
            if kind == 'name' and key.startswith('"'):
                regs.append(('name', cstr(key), fn, where))
            elif kind == 'name' and key in consts:
                regs.append(('name', consts[key], fn, where))
            elif kind == 'addr' and HEX.match(key):
                regs.append(('addr', int(HEX.match(key).group(1), 16), fn, where))
            elif key in ('r.mangled', 'r.addr') and p.endswith('operator_delete.cpp'):
                for a, n in ROWS.findall(src):
                    regs.append(('name', cstr(n), fn, where) if key == 'r.mangled'
                                else ('addr', int(a, 16), fn, where))
            elif key in ('s.symbol', '(uintptr_t)s.dllVa') and p == 'standalone/rt_data_mint.cpp':
                mint = files.get(MINT_TABLE, '')
                for n, a in MINT_ROW.findall(mint):
                    if key == 's.symbol' and n:
                        regs.append(('name', cstr(n), 'mint:' + cstr(n), where + ' (mint)'))
                    elif key != 's.symbol':
                        regs.append(('addr', int(a, 16), 'mint:' + (cstr(n) if n else hex(int(a, 16))),
                                     where + ' (mint)'))
            else:
                problems.append(f'{where}: key not understood: RegisterRuntime{m.group(1)}({key}, {fn})')
    return regs, problems


STR_RUN = re.compile(r'(?:' + STR + r'\s*)+')
ADDR_LIT = re.compile(r'\b0[xX]0*(18[0-9a-fA-F]{7})[uUlL]*\b')
IDENT = re.compile(r'\b[A-Za-z_]\w*\b')
CONST_DEF = re.compile(r'\b(?:const|constexpr)\b[^;{}()]*?\b([A-Za-z_]\w*)\s*(?:\[\s*\])?\s*=\s*'
                       r'(?:\(\s*uintptr_t\s*\)\s*)?$')
NS_OPEN = re.compile(r'\bnamespace\s+([A-Za-z_]\w*)\s*\{')


def enclosing_namespace(src, pos):
    """innermost named namespace whose braces are still open at pos."""
    best = None
    for m in NS_OPEN.finditer(src, 0, pos):
        seg = re.sub(STR, '""', src[m.end():pos])
        if seg.count('{') - seg.count('}') >= 0:
            best = m.group(1)
    return best


def readers(files):
    """{key: [(where, bound, why)]}: every code-line literal of a mangled name (adjacent
    string literals joined) or of a 0x18xxxxxxx address (any suffix; string contents
    are not read), outside a Register* call. An occurrence is NOT bound (not a reader)
    only when it initialises a named constant that nothing names: the constant's
    identifier occurs once in the whole tree, or its enclosing namespace's does."""
    stripped = {p: strip(raw) for p, raw in files.items() if p not in GENERATED}
    ids = {}
    for s in stripped.values():
        for t in IDENT.findall(s):
            ids[t] = ids.get(t, 0) + 1
    occ = {}

    def verdict(src, pos):
        m = CONST_DEF.search(src[max(0, pos - 240):pos])
        if not m:
            return True, 'at a door, wrapper, table or cast'
        name = m.group(1)
        if ids.get(name, 0) <= 1:
            return False, f'constant {name} is named nowhere else'
        ns = enclosing_namespace(src, pos)
        if ns and ids.get(ns, 0) <= 1:
            return False, f'constant {ns}::{name}: namespace {ns} is named nowhere else'
        return True, f'constant {name} is named elsewhere'

    for p, src in sorted(stripped.items()):
        # blank the Register* calls (their keys are rows, not readers), keeping offsets
        for m in re.finditer(r'\bRegister(?:RuntimeExport|RuntimeAddr|ExeAddr|DispatchSlot)\s*\(', src):
            _a, end = call_args(src, m.end() - 1)
            src = src[:m.start()] + re.sub(r'[^\n]', ' ', src[m.start():end + 1]) + src[end + 1:]
        for m in STR_RUN.finditer(src):
            v = cstr(m.group(0))
            if re.fullmatch(r'(?:\?|_Z)\S*', v):
                b, why = verdict(src, m.start())
                occ.setdefault(v, []).append((f'{p}:{src.count(chr(10), 0, m.start()) + 1}', b, why))
        bare = re.sub(STR, lambda s: '"' + ' ' * (len(s.group(0)) - 2) + '"', src)
        # dx11's 0x18 literals are mostly VideoDX11 addresses (same image base);
        # they are counted anyway: a false reader only makes the proof stricter.
        for m in ADDR_LIT.finditer(bare):
            b, why = verdict(bare, m.start())
            occ.setdefault(int(m.group(1), 16), []).append(
                (f'{p}:{bare.count(chr(10), 0, m.start()) + 1}', b, why))
    return occ


LOOKUP = re.compile(r'^RtPortLookup(Export|Addr)\s*\(\s*((?:' + STR + r'\s*)+|0[xX][0-9a-fA-F]+)[uUlL]*\s*\)$')


def lookups_to_bodies(regs, problems):
    """standalone/reg_rt_mirror16.cpp registers a key with the value its counterpart
    key already holds (`RegisterRuntimeAddr(a, RtPortLookupExport("n"))`), which is
    what the mirror did. Replace such a body by the counterpart's explicit body; it
    must be registered exactly once, with one body."""
    out = []
    for kind, k, fn, w in regs:
        m = LOOKUP.match(fn)
        if m:
            ck = cstr(m.group(2)) if m.group(1) == 'Export' else int(m.group(2), 16)
            ckind = 'name' if m.group(1) == 'Export' else 'addr'
            bodies = {f for t, kk, f, _w in regs if t == ckind and kk == ck and not LOOKUP.match(f)}
            if len(bodies) != 1:
                problems.append(f'{w}: {fn} -- counterpart has {len(bodies)} bodies {sorted(bodies)}')
                continue
            fn = bodies.pop()
        out.append((kind, k, fn, w))
    return out, problems


def export_table(files_base):
    txt = files_base.get(EXPORT_TABLE, '')
    rows = [(int(a, 16), n) for a, n in re.findall(r'\{\s*0x([0-9a-f]+)ull,\s*"([^"]*)"\s*\}', txt)]
    if not rows:
        sys.exit(f'no {EXPORT_TABLE} in the base revision')
    by_name, by_addr = {}, {}
    for a, n in rows:
        by_name.setdefault(n, a)
        by_addr.setdefault(a, n)            # emplace(): the FIRST name wins
    return by_name, by_addr


def resolvable(regs, mirror=None):
    n = {k for kind, k, _f, _w in regs if kind == 'name'}
    a = {k for kind, k, _f, _w in regs if kind == 'addr'}
    mn, ma = set(), set()
    if mirror:
        by_name, by_addr = mirror
        ma = {by_name[k] for k in n if k in by_name} - a
        mn = {by_addr[k] for k in a if k in by_addr} - n
    return n | mn, a | ma, mn, ma


def main():
    base = 'eb6f8600'
    if '--base' in sys.argv:
        base = sys.argv[sys.argv.index('--base') + 1]
    fb, fa = load_rev(base), load_worktree()
    mirror = export_table(fb)
    by_name, by_addr = mirror
    rb, pb = lookups_to_bodies(*registrations(fb))
    ra, pa = lookups_to_bodies(*registrations(fa))
    ok = True
    for p in pb + pa:
        print('EXTRACTION PROBLEM:', p); ok = False

    Nb, Ab, mirN, mirA = resolvable(rb, mirror)
    Na, Aa, _x, _y = resolvable(ra)
    mint_n = {k for kind, k, f, _w in rb if kind == 'name' and f.startswith('mint:')}
    mint_a = {k for kind, k, f, _w in rb if kind == 'addr' and f.startswith('mint:')}
    mint_na = {k for kind, k, f, _w in ra if kind == 'name' and f.startswith('mint:')}
    mint_aa = {k for kind, k, f, _w in ra if kind == 'addr' and f.startswith('mint:')}

    print(f'base {base}: explicit names {len({k for t, k, *_ in rb if t == "name"})}, '
          f'explicit addrs {len({k for t, k, *_ in rb if t == "addr"})}; '
          f'the mirror adds {len(mirN)} names, {len(mirA)} addrs')
    pre = (len(Nb - mint_n), len(Ab - mint_a))
    print(f'  [RtPort] runtime registry (mint excluded) computed: {pre[0]} exports, {pre[1]} addrs'
          f'   gate: {KNOWN_PRINT[0]} / {KNOWN_PRINT[1]}   {"MATCH" if pre == KNOWN_PRINT else "MISMATCH"}')
    if pre != KNOWN_PRINT:
        ok = False
    post = (len(Na - mint_na), len(Aa - mint_aa))
    print(f'worktree: [RtPort] runtime registry expected: {post[0]} exports, {post[1]} addrs '
          f'(delta {post[0] - pre[0]:+d} / {post[1] - pre[1]:+d})')

    occ = readers(fb)
    for k, v in readers(fa).items():     # a reader in either tree counts
        occ.setdefault(k, []).extend(v)
    rn = {k for k, v in occ.items() if isinstance(k, str) and any(b for _w, b, _y in v)}
    raddr = {k for k, v in occ.items() if isinstance(k, int) and any(b for _w, b, _y in v)}
    print(f'readers (bound, either tree): {len(rn)} names, {len(raddr)} addresses; '
          f'literal-only (dead constants): '
          f'{sum(1 for k, v in occ.items() if not any(b for _w, b, _y in v))}')
    lostN = sorted(k for k in rn if (k in Nb) != (k in Na))
    lostA = sorted(k for k in raddr if (k in Ab) != (k in Aa))
    print(f'(1) keys with a reader whose resolvability changed: names {len(lostN)}, addrs {len(lostA)}')
    for k in lostN:
        print(f'      NAME {k}: before {k in Nb}, after {k in Na}'); ok = False
    for k in lostA:
        print(f'      ADDR 0x{k:x}: before {k in Ab}, after {k in Aa}'); ok = False
    # keys whose resolvability changed and that have ONLY dead-constant literals
    for k in sorted((k for k in occ if k not in rn and k not in raddr
                     and ((k in Nb) != (k in Na) if isinstance(k, str) else (k in Ab) != (k in Aa))),
                    key=str):
        label = k if isinstance(k, str) else f'0x{k:x}'
        print(f'      unread, resolvability changed: {label}')
        for w, _b, why in sorted(set(occ[k])):
            print(f'          {w}: {why}')

    # (2) rows new in AFTER: each must have been a mirror row with a unique body
    newN = {k for t, k, *_ in ra if t == 'name'} - {k for t, k, *_ in rb if t == 'name'}
    newA = {k for t, k, *_ in ra if t == 'addr'} - {k for t, k, *_ in rb if t == 'addr'}
    print(f'(2) explicit rows new in the worktree: {len(newN)} names, {len(newA)} addrs')
    for kind, keys in (('name', newN), ('addr', newA)):
        for k in sorted(keys):
            if kind == 'name':      # served by the address door's mirror: the name's own address
                src_key, src_kind, was = by_name.get(k), 'addr', k in mirN
            else:                   # served by the name door's mirror: the address's first name
                src_key, src_kind, was = by_addr.get(k), 'name', k in mirA
            bodies_b = {f for t, kk, f, _w in rb if t == src_kind and kk == src_key}
            bodies_a = {f for t, kk, f, _w in ra if t == kind and kk == k}
            label = k if kind == 'name' else f'0x{k:x}'
            good = was and len(bodies_b) == 1 and bodies_a == bodies_b
            print(f'      {"ok " if good else "BAD"} {kind} {label}: mirrored before={was}; '
                  f'mirror body {sorted(bodies_b)}; new body {sorted(bodies_a)}')
            ok &= good
    # mirror rows the worktree does NOT re-register: must have no reader
    dropN = sorted(mirN - Na)
    dropA = sorted(mirA - Aa)
    print(f'mirror rows dropped (no reader): {len(dropN)} names, {len(dropA)} addrs')
    bad = [k for k in dropN if k in rn] + [k for k in dropA if k in raddr]
    if bad:
        print('      DROPPED WITH A READER:', bad); ok = False
    print('PROOF', 'HOLDS' if ok else 'FAILS')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
