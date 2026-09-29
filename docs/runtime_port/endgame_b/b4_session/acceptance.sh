#!/usr/bin/env bash
# B.4 (the cast metric): acceptance, B4_RULES.md §6 items 1-6 (item 7, the gate and
# stepclose.sh, is the coordinator's).
# usage: acceptance.sh           run from the repo root; read-only; no build; under a minute.
# Prints one line per check, `NN <what> = <value> (target <t>)`, then
# `ACCEPTANCE: n line(s) off target`; exit 0 when n is 0.
#
#   §6.1  01-03  castmetric.py PROD total 0; CAST-OK per category at or under FLOOR.md's
#                ceiling; no BAD / STRAY marker
#   §6.2  04-06  no offset-accessor definition or call; `pun` only on CAST-OK byte_io /
#                float_bits lines
#   §6.3  07     bridge markers only on CAST-OK dx11_seam lines
#   §6.4  08-09  exe_bodies.h / *_funcs.h declarations and code/Game `void*`-first-parameter
#                functions are all named in b4_session/LEFT_FREE.md
#   §6.5  10-11  every field of a layout-asserted class has its static_assert(offsetof);
#                no Rt<Ns>:: POD mirror beside a real <Ns>:: class in runtime/types
#   §6.6  12     b3r_session/acceptance.sh stays at target
#
# 01-11 come from ONE python process that runs castmetric.py over PROD and reuses its
# comment-blanked sources; 12 runs alongside it. 08-11 are regex measures over the blanked
# code (no compiler): a count is a lower bound where the note on the line says so.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../../.." && pwd)"
cd "$ROOT" || exit 2
PY=python
B3R_OUT="$(mktemp)"
trap 'rm -f "$B3R_OUT"' EXIT
bash "$HERE/../b3r_session/acceptance.sh" > "$B3R_OUT" 2>&1 &
B3R_PID=$!

PYOUT="$("$PY" - "$HERE" <<'PYEOF'
import os, re, sys
HERE = sys.argv[1]
sys.path.insert(0, HERE)
import castmetric as CM

PROD = ('code', 'runtime', 'standalone', 'dx11', 'engine')
rows = CM.run(list(PROD))
tot = dict.fromkeys(CM.COLS, 0)
for rel, c, sc, hits in rows:
    for k in CM.COLS:
        tot[k] += c[k]
fails = 0


def line(nn, what, value, target, ok=None, note=''):
    global fails
    good = (value == target) if ok is None else ok
    if not good:
        fails += 1
    print(f'{nn} {what} = {value} (target {target})' + (f'   [{note}]' if note else ''))


# ---- §6.1
line('01', 'castmetric.py PROD total (unmarked casts that are not tier I)', tot['total'], 0,
     note=f'tier_i {tot["tier_i"]}, cast_ok {tot["cast_ok"]}')
ceiling = {}
fm = os.path.join(HERE, 'FLOOR.md')
if os.path.exists(fm):
    for ln in open(fm, encoding='utf-8'):
        m = re.match(r'^\|\s*`([a-z0-9_]+)`\s*\|\s*(\d+)\s*\|', ln)
        if m and m.group(1) in CM.CATEGORIES:
            ceiling[m.group(1)] = int(m.group(2))
percat, bycol, bad, stray = {}, {}, [], []
for rel, c, sc, hits in rows:
    fl = CM.FLOOR.get(rel, {})
    for (cat, col), n in fl.get('ok', {}).items():
        percat[cat] = percat.get(cat, 0) + n
        bycol[(cat, col)] = bycol.get((cat, col), 0) + n
    bad += [f'{rel}:{ln}' for ln, cat, why in fl.get('bad', [])]
    stray += [f'{rel}:{ln}' for ln, cat in fl.get('stray', [])]
over = sorted(f'{k} {v}>{ceiling.get(k, 0)}' for k, v in percat.items() if v > ceiling.get(k, 0))
line('02', 'CAST-OK categories above their FLOOR.md ceiling', len(over), 0,
     note=', '.join(over) if over else f'cast_ok {sum(percat.values())} of ceiling {sum(ceiling.values())}')
line('03', 'BAD MARKER + STRAY MARKER lines in PROD', len(bad) + len(stray), 0,
     note=' '.join((bad + stray)[:6]))

# ---- §6.2
okr = set()
for rel, c, sc, hits in rows:
    fl = CM.FLOOR.get(rel)
    if fl:
        okr |= {(rel, x) for x in fl['ok_lines']}
acc = {(r, n) for r, n, (a, b) in CM.INDEX[0].acc_defs
       if r.startswith(tuple(p + '/' for p in PROD))
       and not any((r, x) in okr for x in range(a, b + 1))}
line('04', 'offset-accessor definitions in PROD (not tier R)', len(acc), 0,
     note=' '.join(sorted(n for r, n in acc))[:120])
line('05', 'offset-accessor calls in PROD (accessor column)', tot['accessor'], 0)
pun_other = sum(n for (cat, col), n in bycol.items()
                if col == 'pun' and cat not in ('byte_io', 'float_bits'))
line('06', 'pun (memcpy / bit_cast of an address) outside CAST-OK byte_io / float_bits lines',
     tot['pun'] + pun_other, 0)

# ---- §6.3
nb = 0
for rel, (src, code) in CM._CACHE.items():
    if rel.startswith(tuple(p + '/' for p in PROD)):
        for ln in src.split('\n'):
            if CM._RE_BRIDGE.search(ln) and not re.search(r'CAST-OK:\s*dx11_seam\b', ln):
                nb += 1
line('07', 'bridge-marker lines outside CAST-OK: dx11_seam lines, PROD', nb, 0)

# ---- §6.4
lf = os.path.join(HERE, 'LEFT_FREE.md')
listed = set(re.findall(r'[A-Za-z_]\w*', open(lf, encoding='utf-8').read())) if os.path.exists(lf) else set()
KW = {'return', 'if', 'while', 'for', 'switch', 'sizeof', 'alignof', 'decltype', 'static_assert',
      'alignas', 'typeid', 'noexcept', 'operator', 'new', 'delete', 'case', 'else', 'do', 'throw',
      'defined', 'catch', 'using', 'typedef', 'template', 'goto', 'co_return'} | CM.BUILTIN | CM.QUAL
TYPEISH = re.compile(r'[A-Za-z_0-9*&>]$')
PARAMS = r'((?:[^;{}()]|\([^;{}()]*\))*)'
RE_DECL = re.compile(r'(?<![\w.:>~])([A-Za-z_]\w*)\s*\(' + PARAMS + r'\)\s*(?:const\s*)?(?:noexcept\s*)?;')


def declarer(code, start):
    """True when the identifier at `start` is a function NAME in a declaration: the token
    before it is a type (an identifier that is not a keyword, `*`, `&` or `>`)."""
    j = start - 1
    while j >= 0 and code[j] in ' \t\r\n':
        j -= 1
    if j < 0 or not TYPEISH.match(code[j]):
        return False
    k = j
    while k >= 0 and (code[k].isalnum() or code[k] == '_'):
        k -= 1
    tok = code[k + 1:j + 1]
    return not tok or tok not in {'return', 'case', 'else', 'throw', 'delete', 'new', 'goto',
                                  'co_return', 'do', 'sizeof', 'operator'}


def game_headers():
    for rel, (src, code) in sorted(CM._CACHE.items()):
        base = rel.rsplit('/', 1)[-1]
        if rel.startswith('code/Game/') and (base == 'exe_bodies.h' or base.endswith('_funcs.h')):
            yield rel, code


decls = set()
for rel, code in game_headers():
    code = re.sub(r'(?m)^\s*#.*$', '', code)
    for m in RE_DECL.finditer(code):
        n = m.group(1)
        if n not in KW and declarer(code, m.start(1)):
            decls.add(n)
unlisted = sorted(decls - listed)
line('08', 'exe_bodies.h / *_funcs.h declarations not named in LEFT_FREE.md', len(unlisted), 0,
     note=f'{len(decls)} declared' + ('' if os.path.exists(lf) else '; LEFT_FREE.md does not exist yet'))
RE_VOIDFIRST = re.compile(r'(?<![\w.:>~])([A-Za-z_]\w*)\s*\(\s*(?:const\s+)?void\s*(?:const\s*)?\*\s*'
                          r'(?:const\s*)?(?:[A-Za-z_]\w*)?\s*[,)]')
vfirst = set()
for rel, (src, code) in CM._CACHE.items():
    if rel.startswith('code/Game/'):
        for m in RE_VOIDFIRST.finditer(code):
            n = m.group(1)
            if n not in KW and declarer(code, m.start(1)):
                vfirst.add(n)
line('09', 'code/Game functions with a void* first parameter not named in LEFT_FREE.md',
     len(vfirst - listed), 0, note=f'{len(vfirst)} such functions')

# ---- §6.5
RE_OFFSETOF = re.compile(r'offsetof\s*\(\s*(?:::)?([\w:]+?(?:<[^()]*?>)?)\s*,\s*([A-Za-z_]\w*)')
RE_SIZEOF = re.compile(r'static_assert\s*\(\s*sizeof\s*\(\s*(?:::)?([\w:]+)\s*\)')
asserted_f, asserted_c = set(), set()
for rel, (src, code) in CM._CACHE.items():
    if not rel.startswith(tuple(p + '/' for p in PROD)):
        continue
    if 'offsetof' in code:
        for m in RE_OFFSETOF.finditer(code):
            cls = re.sub(r'<.*>', '', m.group(1)).split('::')[-1]
            asserted_f.add((cls, m.group(2)))
            asserted_c.add(cls)
    if 'sizeof' in code:
        for m in RE_SIZEOF.finditer(code):
            asserted_c.add(m.group(1).split('::')[-1])
RE_CLASS = re.compile(r'\b(?:struct|class)\s+(?:alignas\s*\([^)]*\)\s*)?(?:__attribute__\s*\(\([^)]*\)\)\s*)?'
                      r'([A-Za-z_]\w*)\s*(?:final\s*)?(?::[^;{()]*)?\{')
SKIP_STMT = re.compile(r'^\s*(?:using|typedef|friend|static|enum|template|struct|class|union|'
                       r'static_assert|virtual|explicit|inline|constexpr|operator)\b')


def fields(body):
    """Names of the data members declared at depth 0 of a class body (blanked code)."""
    out, cur, i, n = [], [], 0, len(body)
    while i < n:
        ch = body[i]
        if ch == '{':
            j = CM.match_paren(body, i, '{', '}')
            cur = []                 # a nested type / function body / brace init ends it
            i = (j if j > 0 else n) + 1
            continue
        if ch == ';':
            s = ''.join(cur)
            cur = []
            s = re.sub(r'\b(?:public|private|protected)\s*:', ' ', s)
            s = re.sub(r'\balignas\s*\([^()]*\)|__attribute__\s*\(\([^;]*?\)\)', ' ', s)
            i += 1
            if not s.strip() or SKIP_STMT.match(s):
                continue
            fp = re.search(r'\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\(', s)
            if fp:
                out.append(fp.group(1))
                continue
            if '(' in s:
                continue             # a member function
            s = re.sub(r'<[^<>]*(?:<[^<>]*>[^<>]*)*>', ' ', s)
            for part in s.split(','):
                part = part.split('=')[0]
                part = re.sub(r'\[[^\]]*\]', ' ', part)
                part = re.sub(r':\s*\d+\s*$', ' ', part)
                ids = re.findall(r'[A-Za-z_]\w*', part)
                if ids and ids[-1] not in KW:
                    out.append(ids[-1])
            continue
        cur.append(ch)
        i += 1
    return out


missing = 0
classes_seen = 0
for rel, (src, code) in CM._CACHE.items():
    if not rel.endswith(('.h', '.hpp')) or not rel.startswith(('runtime/types/', 'engine/types/', 'code/Game/', 'dx11/types/')):
        continue
    code = re.sub(r'(?m)^\s*#.*$', lambda m: ' ' * len(m.group(0)), code)
    for m in RE_CLASS.finditer(code):
        cls = m.group(1)
        if cls not in asserted_c:
            continue
        e = CM.match_paren(code, m.end() - 1, '{', '}')
        if e < 0:
            continue
        classes_seen += 1
        missing += sum(1 for f in fields(code[m.end():e]) if (cls, f) not in asserted_f)
line('10', 'fields of layout-asserted classes with no static_assert(offsetof)', missing, 0,
     note=f'{classes_seen} class bodies with a sizeof/offsetof assert; a lower bound')
defs = {}
for rel, (src, code) in CM._CACHE.items():
    if not rel.startswith('runtime/types/'):
        continue
    stack, i = [], 0
    for m in re.finditer(r'\bnamespace\s+([A-Za-z_]\w*)\s*\{|\b(?:struct|class)\s+([A-Za-z_]\w*)\s*'
                         r'(?:final\s*)?(?::[^;{()]*)?\{|[{}]', code):
        t = m.group(0)
        if m.group(1):
            stack.append(m.group(1))
        elif m.group(2):
            ns = next((s for s in reversed(stack) if s), '')
            defs.setdefault(m.group(2), set()).add(ns)
            stack.append(None)
        elif t == '{':
            stack.append(None)
        elif stack:
            stack.pop()
mirrors = sorted(f'{ns}::{x}' for x, nss in defs.items() for ns in nss
                 if re.match(r'^Rt[A-Z]', ns) and ns[2:] in nss)
line('11', 'Rt<Ns>:: POD mirrors beside a real <Ns>:: class in runtime/types', len(mirrors), 0)
print(f'#FAILS {fails}')
PYEOF
)"
PYRC=$?
printf '%s\n' "$PYOUT" | grep -v '^#FAILS '
fail="$(printf '%s\n' "$PYOUT" | sed -n 's/^#FAILS \([0-9]*\)$/\1/p')"
if [ "$PYRC" != 0 ] || [ -z "$fail" ]; then
  echo "01-11 the python pass failed (exit $PYRC): lines 01-11 count as 11 off target"
  fail=11
fi

wait "$B3R_PID"
B3R_OFF="$(sed -n 's/^acceptance: \([0-9]*\) line.*/\1/p' "$B3R_OUT" | tail -1)"
[ -n "$B3R_OFF" ] || B3R_OFF="(no result)"
echo "12 b3r_session/acceptance.sh lines off target = $B3R_OFF (target 0)"
if [ "$B3R_OFF" != "0" ]; then
  fail=$((fail+1))
  grep -E '^R[0-9]+b? +FAIL' "$B3R_OUT" | sed 's/^/     /'
fi
echo "ACCEPTANCE: $fail line(s) off target"
[ "$fail" = 0 ]
