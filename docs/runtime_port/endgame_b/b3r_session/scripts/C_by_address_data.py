#!/usr/bin/env python3
"""C_by_address_data.py -- block-0 inventory C (BY_ADDRESS_DATA) of the registry teardown.

Re-run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b3r_session/scripts/C_by_address_data.py
Writes docs/runtime_port/endgame_b/b3r_session/data/C_by_address_data.tsv and prints the
summary numbers the inventory quotes.

Questions (NEXT_SESSION.md 3b, 2 and 3):
  (2) every place a retail key (0x14.../0x18... address or a mangled name) reaches a
      resolver THROUGH A VARIABLE -- tables, wrappers fed by data, stored callbacks,
      identity compares, diagnostics, the DEFINE_EXE_VTABLE lines;
  (3) every lazy-cache spelling, per directory, and its thread-safety.

Method (code lines only, comments stripped by b3x_session/codegrep.strip):
  A. every call of a RESOLVER (ResolveAddr, ResolveExport, RtPortLookup*, RtResolveCached,
     ReadGlobal, WriteGlobal) whose key argument is NOT a literal;
  B. if that key is a PARAMETER of the enclosing function / lambda / macro, the enclosing
     callable is a WRAPPER; its call sites are scanned the same way (fixpoint). A literal
     at a wrapper call is a by-literal reader (inventories A/B own those); a non-literal
     key that is not a parameter is a DATA SITE -- this inventory's rows;
  C. array initialisers holding >= 2 retail addresses or mangled names (tables);
  D. registrations keyed by a variable (the rows a `Register*(0x` grep cannot see);
  E. resolver calls inside ==/!= (identity) and inside printf-family arguments (diagnostic);
  F. the lazy-cache census (section CACHE below).
Hand-read facts (source of values, thread, rows kept alive when not computable) are the
CURATED table below; each entry is anchored by a regex so line numbers are recomputed.
"""
import os
import re
import sys
from collections import Counter, defaultdict

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))))
sys.path.insert(0, os.path.join(REPO, 'docs', 'runtime_port', 'endgame_b', 'b3x_session'))
from codegrep import strip, EXCL  # noqa: E402
os.chdir(REPO)

ROOTS = ['code', 'engine', 'standalone', 'runtime', 'dx11', 'tests/unit']
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')
GENERATED = ('standalone/rt_export_addrs.inc', 'standalone/rt_data_mint_tables.inc')
OUT = os.path.join(REPO, 'docs', 'runtime_port', 'endgame_b', 'b3r_session', 'data',
                   'C_by_address_data.tsv')

RESOLVERS = {'ResolveAddr': 1, 'RtPortLookupAddr': 0, 'ResolveExport': 0,
             'RtPortLookupExport': 0, 'RtResolveCached': 0, 'ReadGlobal': 1,
             'WriteGlobal': 1, 'RuntimeExportIsRegistered': 0}
REGISTRARS = {'RegisterExeAddr': 0, 'RegisterRuntimeAddr': 0, 'RegisterRuntimeExport': 0,
              'RegisterDispatchSlot': 0}
LIT = re.compile(r'\(?\s*(?:\(\s*uintptr_t\s*\)\s*)?(0x[0-9a-fA-F]+[uUlL]*|"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*)\s*\)?')
ADDR = re.compile(r'\b0x(1[48][0-9a-fA-F]{7})[uUlL]*\b')
MANGLED = re.compile(r'"(\?[^"\s]*@[^"\s]*|_Z[^"\s]+)"')
IDENT = re.compile(r'[A-Za-z_]\w*$')
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'decltype', 'static_cast',
            'reinterpret_cast', 'const_cast', 'catch', 'defined'}


# ------------------------------------------------------------------ files
def load_files():
    files = {}
    for r in ROOTS:
        for dp, dn, fs in os.walk(os.path.join(REPO, r)):
            for f in fs:
                p = os.path.relpath(os.path.join(dp, f), REPO).replace(os.sep, '/')
                if any(e in p for e in EXCL) or not p.endswith(EXTS):
                    continue
                src = open(os.path.join(REPO, p), encoding='utf-8', errors='replace').read()
                files[p] = strip(src)
    return files


def line_of(src, pos):
    return src.count('\n', 0, pos) + 1


def call_args(src, i):
    """src[i] == '(' -> ([args], index of the closing paren)."""
    d = 0; cur = []; out = []; j = i; n = len(src)
    while j < n:
        c = src[j]
        if c == '"':
            k = j + 1
            while k < n and src[k] != '"':
                k += 2 if src[k] == '\\' else 1
            if d >= 1:
                cur.append(src[j:k + 1])
            j = k + 1
            continue
        if c in '([{':
            d += 1
            if d == 1:
                j += 1
                continue
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


def norm(s):
    return ' '.join(s.split())


def is_literal(arg):
    a = norm(arg)
    a = re.sub(r'^\(\s*uintptr_t\s*\)\s*', '', a)
    a = re.sub(r'^static_cast<[^>]*>\((.*)\)$', r'\1', a)
    return bool(re.fullmatch(r'0x[0-9a-fA-F]+[uUlL]*|"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*', a))


# ------------------------------------------------------------------ enclosing callable
FUNC_HDR = re.compile(r'(?:#\s*define\s+)?\b([A-Za-z_]\w*)\s*(?:<[^;(){}]*>)?\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*'
                      r'(?:const\s*)?(?:noexcept\s*)?(?:->\s*[^{;=]*)?\{')
LAMBDA_HDR = re.compile(r'\b([A-Za-z_]\w*)\s*=\s*\[[^\]]*\]\s*\(([^()]*)\)\s*(?:mutable\s*)?(?:->\s*[^{;]*)?\{')
MACRO_HDR = re.compile(r'#\s*define\s+([A-Za-z_]\w*)\(([^)]*)\)')


def param_names(plist):
    names = []
    for p in split_top(plist):
        p = p.split('=')[0].strip()
        m = re.search(r'([A-Za-z_]\w*)\s*(\[[^\]]*\])?$', p)
        names.append(m.group(1) if m else '')
    return names


def split_top(s):
    out = []; d = 0; cur = ''
    for c in s:
        if c in '(<[{': d += 1
        if c in ')>]}': d -= 1
        if c == ',' and d == 0:
            out.append(cur); cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur)
    return out


def enclosing(src, pos, ident):
    """(callable_name, param_index, kind) if `ident` is a parameter of the nearest enclosing
    function / lambda / macro before pos; else None."""
    best = None
    window = src[max(0, pos - 6000):pos]
    base = max(0, pos - 6000)
    for rx, kind in ((MACRO_HDR, 'macro'), (LAMBDA_HDR, 'lambda'), (FUNC_HDR, 'function')):
        for m in rx.finditer(window):
            name = m.group(1)
            if name in KEYWORDS:
                continue
            pn = param_names(m.group(2))
            if ident in pn:
                start = base + m.start()
                if kind == 'macro':
                    # the macro body must reach pos without an unescaped newline
                    seg = src[start:pos]
                    if any(not l.rstrip().endswith('\\') for l in seg.split('\n')[:-1]):
                        continue
                else:
                    # brace depth from the header's '{' to pos must stay > 0
                    b = base + m.end() - 1
                    d = 0; ok = True
                    for c in src[b:pos]:
                        if c == '{': d += 1
                        elif c == '}':
                            d -= 1
                            if d == 0: ok = False; break
                    if not ok:
                        continue
                if best is None or start > best[3]:
                    best = (name, pn.index(ident), kind, start)
    return best[:3] if best else None


# ------------------------------------------------------------------ A + B: wrappers
TEMPLATE_ARGS = r'(?:<(?:[^<>;{}]|<(?:[^<>;{}]|<[^<>;{}]*>)*>)*>)?'


_INC = {}


def include_closure(files, p, depth=4):
    """basenames of the headers p includes, transitively (by basename; good enough here)."""
    if p in _INC:
        return _INC[p]
    by_base = defaultdict(list)
    for f in files:
        by_base[f.rsplit('/', 1)[-1]].append(f)
    seen, todo = set(), [p]
    for _ in range(depth):
        nxt = []
        for f in todo:
            for h in re.findall(r'#\s*include\s*"([^"]+)"', files.get(f, '')):
                b = h.rsplit('/', 1)[-1]
                if b not in seen:
                    seen.add(b)
                    nxt.extend(by_base.get(b, []))
        todo = nxt
    _INC[p] = seen
    return seen


def scan_calls(files, defs):
    """yield (file, pos, callee, key_arg_text, def_file) for calls of the callables in
    defs {name: {def_file or '': key_index}}; '' = a global door (the resolvers).
    A call binds to a definition in the SAME file first, else to one in a header."""
    rx = re.compile(r'(?<![\w.])(?<!->)(?:\w+::)*\b(' + '|'.join(sorted(map(re.escape, defs), key=len, reverse=True))
                    + r')\s*' + TEMPLATE_ARGS + r'\s*\(')
    for p, src in files.items():
        if p in GENERATED:
            continue
        for m in rx.finditer(src):
            callee = m.group(1)
            d = defs[callee]
            if '' in d:
                df = ''
            elif p in d:
                df = p
            else:
                inc = include_closure(files, p)
                hdr = [f for f in d if f.endswith(('.h', '.hpp', '.inc')) and f.rsplit('/', 1)[-1] in inc]
                if not hdr:
                    continue                  # a file-local wrapper of another file
                df = hdr[0]
            pre = src[max(0, m.start() - 60):m.start()]
            if re.search(r'#\s*define\s+$', pre):
                continue
            paren = src.index('(', m.end() - 1) if src[m.end() - 1] != '(' else m.end() - 1
            a, e = call_args(src, paren)
            after = src[e + 1:e + 40].lstrip()
            if re.search(r'\b(?:void|bool|int|auto|inline|static|T|Fn|uintptr_t|const\s+\w+|\w+_t)\s*\**\s*(?:\w+::)*$', pre) \
                    and not re.search(r'(return|=|\(|,)\s*$', pre) \
                    and (after.startswith(('{', ';', '->', 'const', 'noexcept'))):
                continue                      # a declaration / definition, not a call
            k = d[df]
            if len(a) <= k:
                continue
            yield p, m.start(), callee, a[k], df


def build_wrappers(files, base=None):
    base = dict(RESOLVERS if base is None else base)
    defs = {n: {'': k} for n, k in base.items()}
    wrappers = {}            # (name, def_file) -> (file, line, kind, param_index, via)
    data_sites = []          # (file, line, callee, arg)
    literal_hits = defaultdict(list)   # (name, def_file) -> [(file, line, literal)]
    changed = True
    seen_sites = set()
    while changed:
        changed = False
        for p, pos, callee, arg, df in list(scan_calls(files, defs)):
            key = (p, pos, callee)
            if key in seen_sites:
                continue
            seen_sites.add(key)
            src = files[p]; ln = line_of(src, pos)
            a = norm(arg)
            if is_literal(a):
                literal_hits[(callee, df)].append((p, ln, a))
                continue
            if re.search(r'\b(?:uintptr_t|uint64_t|const\s+char\s*\*|char\s*\*|auto|int|ModuleT|T)\s+\w+$', a):
                continue                      # the wrapper's own parameter declaration
            m = re.search(r'([A-Za-z_]\w*)\s*\)?\s*$', re.sub(r'^\(+', '', a))
            ident = a.strip('() ') if IDENT.match(a.strip('() ')) else (m.group(1) if m and re.fullmatch(r'[\w\s()*&:<>]*', a) else None)
            enc = enclosing(src, pos, ident) if ident else None
            if enc and enc[0] not in base:
                slot = defs.setdefault(enc[0], {})
                if p not in slot:
                    slot[p] = enc[1]
                    wrappers[(enc[0], p)] = (p, line_of(src, src.rfind(enc[0], 0, pos)), enc[2], enc[1], callee)
                    changed = True
            elif enc:
                pass
            else:
                data_sites.append((p, ln, callee, a))
    return wrappers, data_sites, literal_hits, defs


# ------------------------------------------------------------------ C: tables
ARRAY_DEF = re.compile(r'(\w+)\s*\[[^\]]*\]\s*(?:\[[^\]]*\]\s*)*=\s*\{')


def scan_tables(files):
    out = []
    for p, src in files.items():
        if p in GENERATED or not p.endswith(('.cpp', '.h', '.hpp', '.c')):
            continue
        for m in ARRAY_DEF.finditer(src):
            i = m.end() - 1; d = 0; j = i
            while j < len(src):
                if src[j] == '{': d += 1
                elif src[j] == '}':
                    d -= 1
                    if d == 0: break
                j += 1
            body = src[i:j]
            body_ns = re.sub(r'"(?:[^"\\]|\\.)*"', lambda s: s.group(0) if MANGLED.fullmatch(s.group(0)) else '""', body)
            addrs = sorted(set(a.lower() for a in ADDR.findall(body_ns)))
            names = sorted(set(MANGLED.findall(body)))
            in_str = sorted(set(a.lower() for a in ADDR.findall(body)) - set(addrs))
            if len(addrs) >= 2 or len(names) >= 2 or len(in_str) >= 2:
                out.append((p, line_of(src, m.start()), m.group(1), addrs, names, in_str))
    return out


# ------------------------------------------------------------------ registries (rows)
REGISTRAR_WRAPPERS = {}
# directories whose code runs on the Engine::Loader thread as well as main (B35_RULES 5.2, B36_RULES 5):
# resource loads, UI/Graphics construction, Serialize on a save load, factory CreateNewObject.
LOADER_REACH = ('runtime/src/ui', 'runtime/src/graphics', 'runtime/src/io', 'runtime/src/object',
                'runtime/src/collection', 'runtime/src/video', 'runtime/src/save', 'dx11/',
                # GameUpdateLoop::Load runs SaveRestoreOut (every component / action / work
                # Serialize, every factory CreateNewObject) and the new-game world build here:
                'code/Game/Action', 'code/Game/Component', 'code/Game/Work', 'code/Game/Entity',
                'code/Game/Game', 'code/Game/Tutorial', 'code/Game/Goal')


def registry_rows(files):
    """Literal registrations: exe addrs, runtime addrs, runtime names; plus table-keyed ones."""
    exe, rta, rtn = set(), set(), set()
    for p, src in files.items():
        if p in GENERATED:
            continue
        for m in re.finditer(r'\b(RegisterExeAddr|RegisterDispatchSlot)\s*\(\s*(0x[0-9a-fA-F]+)', src):
            exe.add(int(m.group(2), 16))
        for m in re.finditer(r'\bRegisterRuntimeAddr\s*\(\s*(?:\(uintptr_t\)\s*)?(0x[0-9a-fA-F]+)', src):
            rta.add(int(m.group(1), 16))
        for m in re.finditer(r'\bRegisterRuntimeExport\s*\(\s*("(?:[^"\\]|\\.)*")', src):
            rtn.add(m.group(1)[1:-1])
    # table-keyed registrations read by hand (section D): {addr, "name"} kRows, float consts
    for p in ('runtime/src/object/class_operator_delete.cpp', 'runtime/src/ui/ui_operator_delete.cpp'):
        for a, n in re.findall(r'\{\s*(0x[0-9a-fA-F]+)ull\s*,\s*"([^"]+)"', files.get(p, '')):
            rta.add(int(a, 16)); rtn.add(n)
    for a in re.findall(r'\{\s*(0x[0-9a-fA-F]+)ULL\s*,', files.get('standalone/reg_rdata_float_consts.cpp', '')):
        exe.add(int(a, 16))
    # named-constant registrations: RegisterRuntimeExport(kX, ...) with const char* kX = "..."
    for p, src in files.items():
        consts = dict(re.findall(r'\b(k\w+)\s*(?:\[\s*\])?\s*=\s*("(?:[^"\\]|\\.)*")', src))
        for m in re.finditer(r'\bRegisterRuntimeExport\s*\(\s*(k\w+)', src):
            if m.group(1) in consts:
                rtn.add(consts[m.group(1)][1:-1])
    # registrations through a per-file registrar wrapper (a helper whose parameter reaches
    # Register*): its literal call sites are rows too
    rw, _rd, rlit, _defs = build_wrappers(files, REGISTRARS)
    for (callee, df), hits in rlit.items():
        if df == '':
            continue
        for _p, _ln, a in hits:
            a = re.sub(r'^\(\s*uintptr_t\s*\)\s*', '', a)
            if a.startswith('"'):
                rtn.add(a[1:-1])
            else:
                v = int(re.match(r'0x[0-9a-fA-F]+', a).group(0), 16)
                (exe if v >> 28 == 0x14 else rta).add(v)
    REGISTRAR_WRAPPERS.update(rw)
    mint = open(os.path.join(REPO, 'standalone', 'rt_data_mint_tables.inc'), encoding='utf-8',
                errors='replace').read()
    for n, a in re.findall(r'\{\s*(?:"([^"]*)"|nullptr),\s*(0x[0-9a-f]+)ull,\s*\d+,\s*&kStorageBytes', mint):
        rta.add(int(a, 16))
        if n:
            rtn.add(n)
    return exe, rta, rtn


def export_table():
    txt = open(os.path.join(REPO, 'standalone', 'rt_export_addrs.inc'), encoding='utf-8').read()
    return [(int(a, 16), n) for a, n in re.findall(r'\{\s*(0x[0-9a-f]+)ull,\s*"([^"]*)"\s*\}', txt)]


def keep_alive(keys, module, exe, rta, rtn, exp_by_addr, exp_by_name):
    """Registry rows a set of keys keeps alive: (count, detail)."""
    rows = set()
    for k in keys:
        if isinstance(k, str):
            if k in rtn:
                rows.add(('name', k))
            elif k in exp_by_name and exp_by_name[k] in rta:
                rows.add(('addr', exp_by_name[k])); rows.add(('mirror', k))
        elif module == 'exe' or (k >> 28) == 0x14:
            if k in exe:
                rows.add(('exe', k))
        else:
            if k in rta:
                rows.add(('addr', k))
            elif k in exp_by_addr and exp_by_addr[k] in rtn:
                rows.add(('name', exp_by_addr[k])); rows.add(('mirror', k))
    return rows


# ------------------------------------------------------------------ E: compares, diagnostics
PRINTF = re.compile(r'\b(printf|fprintf|snprintf|sprintf|std::printf|std::fprintf|TRACE\w*|LOG\w*)\s*\(')


def scan_compare_diag(files, defs):
    """call sites of a resolver or a BOUND wrapper that sit inside ==/!= or inside printf args."""
    spans = defaultdict(list)
    for p, src in files.items():
        if p in GENERATED:
            continue
        for m in PRINTF.finditer(src):
            a, e = call_args(src, m.end() - 1)
            spans[p].append((m.end(), e, line_of(src, m.start())))
    comp, diag = [], []
    for p, pos, callee, arg, df in scan_calls(files, defs):
        src = files[p]
        a = norm(arg)
        inside = next((sp for sp in spans[p] if sp[0] <= pos < sp[1]), None)
        if inside:
            diag.append((p, inside[2], callee, a[:120]))
            continue
        paren = src.index('(', pos)
        _a, e = call_args(src, paren)
        before = src[max(0, pos - 80):pos]
        after = src[e + 1:e + 40]
        if re.search(r'(==|!=)\s*(?:\(?\s*(?:reinterpret_cast<[^>]*>|\([^()]*\))?\s*\(?\s*)$', before)                 or re.match(r'\s*\)*\s*(==|!=)', after):
            comp.append((p, line_of(src, pos), callee, a[:80]))
    return comp, diag


# ------------------------------------------------------------------ F: caches
CACHE_PATTERNS = [
    ('RT_EXPORT_ONCE', r'\bRT_EXPORT_ONCE\s*\('),
    ('RT_DATA_SLOT', r'\bRT_DATA_SLOT\s*\('),
    ('RtResolveCached (direct)', r'\bRtResolveCached\s*\('),
    ('AtomicExport/AtomicAddr/AtomicByName/AtomicByAddr (uses)', r'\bAtomic(?:Export|Addr|ByName|ByAddr)\s*\('),
    ('std::atomic<void*> door slot (defs)', r'std::atomic<\s*void\s*\*\s*>\s+\w+\s*[{=(]'),
    ('function-local static = resolve', None),
    ('if (!x) x = resolve (hand-written lazy fill)', r'if\s*\(\s*(?:!\s*([\w.>-]+)|([\w.>-]+)\s*==\s*(?:nullptr|NULL|0))\s*\)\s*\{?\s*(?:\1|\2)\s*=\s*[^;]*\b(?:ResolveAddr|ResolveExport|RtResolveCached|RtPortLookup\w+)\b'),
    ('X_ResolveFuncs / _InitFuncs definitions', r'\bbool\s+\w+_(?:ResolveFuncs|InitFuncs)\s*\(\s*\)\s*\{|\bvoid\s+\w+_(?:ResolveFuncs|InitFuncs)\s*\(\s*\)\s*\{'),
    ('X_ResolveFuncs / _InitFuncs calls', r'(?<!bool )(?<!void )\b\w+_(?:ResolveFuncs|InitFuncs)\s*\(\s*\)\s*[;)]'),
    ('Ensure*() guard functions (bool-flag lazy fill)', r'\b(?:static\s+)?(?:bool|void|const\s+\w+\s*\*)\s+(Ensure\w*|EnsureResolved|ResolveOnce\w*)\s*\(\s*\)\s*\{'),
    ('guard flag set BEFORE the fills (if (f) return; f = true;)', r'if\s*\(\s*(\w*(?:[Rr]esolved|[Tt]ried|[Dd]one|[Ii]nit\w*))\s*\)\s*return[^;]*;\s*\1\s*=\s*true\s*;'),
    ('handle-struct field fill (g_x.f = resolve)', r'\b\w+\.\w+\s*=\s*(?:\([^()]*\)\s*|reinterpret_cast<[^>]*>\s*\(\s*)?(?:ResolveAddr|ResolveExport|Exp<|RtResolveCached)'),
]


def scan_caches(files, wrapper_names):
    per = defaultdict(Counter)
    sites = defaultdict(list)
    res_names = '|'.join(['ResolveAddr', 'ResolveExport', 'RtResolveCached', 'RtPortLookupAddr',
                          'RtPortLookupExport', 'ReadGlobal'] + sorted(wrapper_names))
    static_rx = re.compile(r'\bstatic\s+(?!inline\b)(?!constexpr\b)[^;{}()=]*?\b(\w+)\s*=\s*[^;]{0,300}?\b(?:' + res_names + r')\s*(?:<[^;(){}]*>)?\s*\(')
    for p, src in files.items():
        if p in GENERATED:
            continue
        d = p.split('/')[0] if p.split('/')[0] not in ('code', 'runtime', 'engine') else '/'.join(p.split('/')[:3 if p.startswith(('code/Game', 'runtime/src')) else 2])
        for label, rx in CACHE_PATTERNS:
            if rx is None:
                for m in static_rx.finditer(src):
                    per[label][d] += 1
                    sites[label].append((p, line_of(src, m.start())))
                continue
            for m in re.finditer(rx, src):
                if label.startswith('RtResolveCached') and ('rt_port.h' in p or 'rt_resolve_cache.cpp' in p):
                    continue
                per[label][d] += 1
                sites[label].append((p, line_of(src, m.start())))
    return per, sites


# ------------------------------------------------------------------ CURATED (hand-read)
# (file, anchor regex, kind, module, distinct(None=computed), source_of_values, rows(None=computed),
#  thread, note, keys(None=from anchor table)) -- every entry was read by hand.
CURATED = [
    # ---- tables whose element reaches a resolver (true data flow) ------------------------
    ('code/Game/Tutorial/tutorialstep.cpp', r'const ArrayLeg legs\[3\]', 'addr_table', 'exe', None,
     'const ArrayLeg legs[3] initialiser (3 ExternalPtrArray<...>::Serialize addrs) -> SerializePtrArrayFn(leg.serializeAddr) -> ResolveAddr(Exe)',
     None, 'loader',
     'TutorialStep::Serialize; runs on the loader thread on a save load. DIRECT: the three ExternalPtrArray<T>::Serialize bodies are nameable (template instances)'),
    ('standalone/dlgctrl_route.cpp', r'constexpr SlotSpec kSpecs\[\]', 'addr_table', 'runtime', None,
     'kSpecs[3] {dllAddr,name} -> ResolveAddr(Module::Runtime, kSpecs[s].dllAddr) at DlgCtrlRoute_Arm; result STORED in g_target[] and printed',
     None, 'main',
     'diagnostic instrument gated by Opt().dlgCtrlRoute; the three DialogController slot bodies are nameable members (OnOpened/OnClosed/OnSerializeSettings) -> DIRECT or delete the instrument'),
    ('dx11/src/devicedata.cpp', r'const Entry table\[\] = \{', 'name_table', 'runtime', None,
     'Entry{dst,mangled,iat}[21] walked by a loop -> Dx11ResolveRuntimeImport(e.mangled,e.iat) -> ResolveExport(name); fn ptr STORED in g_im (Imports handle struct); iat is a VideoDX11 IAT address (0x1800133xx) used only in the failure message',
     None, 'main',
     'lazy (g_imResolved, plain bool); first call at Device startup. Names are Runtime ctors/dtors of the Video *Data companions, Adapter/GraphicsInterface/DisplayMode, MemAlloc/MemFree: all nameable -> DIRECT'),
    ('runtime/src/ui/dialog.cpp', r'const char\* const kElementFindElement\s*=', 'name_table', 'runtime', 1,
     'named constant kElementFindElement -> ResolveExport(kElementFindElement) in DialogInstance::FindElement', None, 'both',
     'named-constant name (grep of the literal finds the definition, not the call). Element::FindElement is a class member -> DIRECT'),
    ('runtime/src/ui/dialog.cpp', r'const char\* const kElementFindElementType\s*=', 'name_table', 'runtime', 1,
     'named constant kElementFindElementType -> ResolveExport(...) in DialogInstance::FindElementType', None, 'both',
     'as above -> DIRECT'),
    ('standalone/modal_driver.cpp', r'constexpr uintptr_t kUpdateStandAloneUI\s*=', 'identity_compare', 'runtime', 1,
     'named constant 0x180120310 -> ResolveAddr(Runtime, kUpdateStandAloneUI) compared with fn ptrs STORED in MainLoop+0x90 by ShowDialog',
     None, 'main',
     'address compared, never called; the stored callback in MainLoop+0x90 is written by our ShowDialog port. DIRECT: compare with &UI::UserInterface::UpdateStandAloneUI (or the static our ShowDialog installs)'),
    ('standalone/scene_watch.cpp', r'RtPortLookupAddr\(0x180100f20\)', 'identity_compare', 'runtime', 1,
     'literal 0x180100f20 -> RtPortLookupAddr; cached in function-local s_addDrawItemsOurs; compared with the object vtable slot of Geometry::AddDrawItems (IsMeshInstance)',
     None, 'main',
     'identity token, not a call. The comment says the body is not registered but runtime/src/graphics/mesh.cpp registers RtGraphics_MeshInstance_AddDrawItems at this address. DIRECT: compare with &RtGraphics_MeshInstance_AddDrawItems / MeshInstance vtable'),
    ('code/Game/Component/citizenai_candidatelist.cpp', r'int CitizenAI_ValidateAndEnterState\(void\* selfV, int state\) \{', 'addr_table', 'exe', None,
     'STATE-MACHINE table: switch(state) assigns one of the CitizenAI state-handler addresses to a local, then ResolveAddr(Module::Exe, addr) and calls it (a null skips the state)',
     None, 'main',
     'the only behaviour-table dispatch through the registry; every case names a CitizenAIComponent state handler -> DIRECT (a switch of direct calls). A handler still unregistered is a live null-skip: converting it changes behaviour (titled commit)'),
    ('code/Game/Game/geometry_serialize.cpp', r'constexpr uintptr_t kSlot_Vector2Ser\s*=', 'addr_table', 'exe', 1,
     'named constant kSlot_Vector2Ser (PE import slot 0x1401526b8, Math::Vector2::Serialize) -> ResolveSlot(kSlot, cache, tried, tag) -> *(fn*)ResolveAddr(Exe)',
     None, 'loader', 'Geometry::Serialize on a save load. DIRECT: Math::Vector2::Serialize is a member'),
    ('runtime/src/graphics/wave24_lane_gfxmat.cpp', r'RT_GFXMAT_CORRUPT\(1\)\s*\?\s*0x1800616c0', 'addr_table', 'runtime', 2,
     'a negative-control probe: RT_GFXMAT_CORRUPT(n) swaps two literal addresses (0x1800616c0 / 0x180061880) at the door', None, 'both',
     'instrument inside a ported body; the swap is only armed in a probe build. DIRECT once the probe is deleted'),
    # ---- stored callbacks -----------------------------------------------------------------
    ('standalone/address_registry.cpp', r'static void RegisterDispatchSlot\(uintptr_t ghidra_addr', 'dispatch_slot', 'exe', None,
     'RegisterDispatchSlot(0x14015xxxx, (uintptr_t)ResolveExport("...")) at AddressRegistryInit: the runtime fn ptr is resolved ONCE and STORED in GetSlotStorage()[addr]; readers do *(fn_t*)ResolveAddr(Exe, slot)',
     None, 'main',
     'a stored callback: the slot VALUE is frozen at AddressRegistryInit (after RuntimePortInit), so a port registered later never reaches a slot reader. Each slot keeps alive BOTH its exe row and the runtime NAME row its initialiser resolves. Same helper in reg_player_place.cpp, reg_unreg_lane1.cpp, reg_unreg_lane3.cpp'),
    ('standalone/reg_player_place.cpp', r'RegisterExeAddr\(ghidra_addr, reinterpret_cast<uintptr_t>\(&slots\[ghidra_addr\]\)\)', 'dispatch_slot', 'exe', None,
     'file-local RegisterDispatchSlot copy (slot storage + stored fn ptr)', None, 'main', 'as address_registry.cpp'),
    ('standalone/reg_unreg_lane1.cpp', r'RegisterExeAddr\(ghidra_addr, reinterpret_cast<uintptr_t>\(&slots\[ghidra_addr\]\)\)', 'dispatch_slot', 'exe', None,
     'file-local RegisterDispatchSlot copy (slot storage + stored fn ptr)', None, 'main', 'as address_registry.cpp'),
    ('standalone/reg_unreg_lane3.cpp', r'RegisterExeAddr\(ghidra_addr, reinterpret_cast<uintptr_t>\(&slots\[ghidra_addr\]\)\)', 'dispatch_slot', 'exe', None,
     'file-local RegisterDispatchSlot copy; lane3 fills four slots with ?_typeInfo@Graphics@@ rows resolved by name', None, 'main', 'as address_registry.cpp'),
    ('runtime/src/engine/engine.cpp', r'device \+ MainLoopAddr::DeviceErrorFuncSlot', 'stored_callback', 'runtime', 0,
     'OUR static thunks &RtEngine_MainLoop_OnVideoError_static / _OnVideoRestart_static STORED into Video::Device+slot (offsets from MainLoopAddr, not addresses)',
     0, 'main', 'no registry row: the value is a direct C++ address; listed because it is a stored callback the brief asked about'),
    ('dx11/src/buffers.cpp', r'bool EnsureDeps\(\) \{', 'stored_callback', 'runtime', None,
     'RuntimeDeps g_deps handle struct filled by Exp<T>("mangled") (literal names) once; fn ptrs STORED; guarded by plain bool g_depsTried',
     None, 'both', 'buffers are created on the loader thread (MeshInstance/SetMesh) as well as main: a non-atomic first-call cache reachable off the main thread (B5-7 shape). DIRECT'),
    ('engine/funcs/engine_funcs.cpp', r'\bbool EngineFuncsInit\(\)', 'stored_callback', 'exe', None,
     '228 extern fn-pointer globals bound by INITIALISER at EngineFuncsInit (literal keys) and called everywhere',
     None, 'main', 'bound once on the main thread before any other thread exists: no race; freezes the answer at boot (after AddressRegistryInit). Inventory owner of question 5 counts the rows'),
    # ---- DEFINE_EXE_VTABLE (no retail address at all) -------------------------------------
    ('code/Game/Game/gameupdateloop.cpp', r'DEFINE_EXE_VTABLE\(GameUpdateLoop', 'vtable_define', 'exe', 0,
     'asm label _ZTV14GameUpdateLoop (compiler vtable group); readers: splash_loop_stubs.cpp vptr identity compare, newgamedialog.cpp printf', 0, 'main',
     'keeps NO registry row: the macro binds the compiler symbol, not a retail address (0x14015e2f0 is registered nowhere)'),
    ('standalone/menudialogloopt_stubs.cpp', r'DEFINE_EXE_VTABLE\(MenuDialogLoopT', 'vtable_define', 'exe', 0,
     'asm label _ZTV15MenuDialogLoopT; reader: game_main.cpp printf only', 0, 'main', 'keeps NO registry row'),
    ('standalone/splash_loop_stubs.cpp', r'DEFINE_EXE_VTABLE\(GameLoadDialogLoopT', 'vtable_define', 'exe', 0,
     'asm label _ZTV19GameLoadDialogLoopT; reader: game_main.cpp printf only', 0, 'main', 'keeps NO registry row (0x140158800 registered nowhere)'),
    ('code/Game/Goal/statgoal.cpp', r'DEFINE_EXE_VTABLE\(StatGoal', 'vtable_define', 'exe', 0,
     'asm label _ZTV8StatGoal; reader: main.cpp RunFactorySelfTest', 0, 'main', 'keeps NO registry row'),
    ('standalone/splash_loop_stubs.cpp', r'== GameUpdateLoop::Vtable\(\)', 'identity_compare', 'exe', 0,
     'vptr compared with GameUpdateLoop::Vtable() (compiler table)', 0, 'main', 'no registry row; already a direct reference'),
    # ---- registrations keyed by DATA (rows a Register*(0x grep cannot see) ---------------------
    ('standalone/reg_rdata_float_consts.cpp', r'const RdataFloat g_consts\[\]', 'addr_table', 'exe', None,
     'g_consts[25] {addr,bits} registered by a loop (RegisterExeAddr((uintptr_t)g_consts[i].addr, &g_storage[i]))', None, 'main',
     'REGISTRATION side: 25 STORAGE rows (.rdata floats) invisible to a RegisterExeAddr(0x grep; readers are literal ResolveAddr(Exe,0x14017xxxx) in code/Game'),
    ('runtime/src/object/class_operator_delete.cpp', r'const OpDelRow kRows\[\]', 'addr_table', 'runtime', None,
     'kRows[28] {addr,mangled} registered by a loop, BOTH doors, all to RtClass_OperatorDelete', None, 'main',
     'REGISTRATION side: 28 addr + 28 name rows invisible to a literal Register grep'),
    ('runtime/src/ui/ui_operator_delete.cpp', r'const OpDelRow kRows\[\]', 'addr_table', 'runtime', None,
     'kRows[60] {addr,mangled} registered by a loop, BOTH doors', None, 'main',
     'REGISTRATION side: 60 addr + 60 name rows invisible to a literal Register grep'),
    ('standalone/rt_data_mint.cpp', r'RegisterRuntimeAddr\(\(uintptr_t\)s\.dllVa', 'addr_table', 'runtime', 2,
     'kMintStorage[] (generated rt_data_mint_tables.inc) registered by a loop: symbol -> name door, dllVa -> address door', 2, 'main',
     'REGISTRATION side: the 2 minted storage rows (MemoryAllocator::_singleton, Debug::_disableHandler)'),
    ('code/Game/Work/buildroad.cpp', r'RegisterExeAddr\(BuildRoadAddr::CtorParams', 'addr_table', 'exe', 1,
     'named constant BuildRoadAddr::CtorParams as the registration key', 1, 'main', 'REGISTRATION side keyed by a named constant'),
    # ---- tables that DO NOT reach a resolver (listed so nobody re-counts them) ---------------
    ('dx11/funcs/dx11_seam.cpp', r'static const VtDesc kVtables\[', 'diagnostic', 'runtime', None,
     'VtDesc[12] ghidra field = VideoDX11-x64.dll vtable addresses (0x1800185xx..0x180018bb0), printed in the seam report only', 0, 'main',
     'NOT Runtime addresses: VideoDX11 shares the 0x180000000 preferred base. Resolves nothing; keeps no row'),
    ('code/Game/Dialog/tradeui.cpp', r'const Dep deps\[\] = \{', 'diagnostic', 'exe', None,
     'Dep{name string,fn} over engine_funcs globals; the addresses are inside strings, printed when a global is null', 0, 'main',
     'resolves nothing itself; the fn_ globals are engine_funcs initialiser bindings (question 5)'),
]


def curated_rows(files, tables, exe, rta, rtn, exp_by_addr, exp_by_name):
    rows = []
    tbl = {(t[0], t[1]): t for t in tables}
    for f, anchor, kind, module, distinct, source, nrows, thread, note in CURATED:
        src = files.get(f)
        if src is None:
            rows.append((f, 0, kind, module, distinct or 0, source, nrows or 0, thread, 'ANCHOR FILE MISSING ' + note)); continue
        m = re.search(anchor, src)
        if not m:
            rows.append((f, 0, kind, module, distinct or 0, source, nrows or 0, thread, 'ANCHOR NOT FOUND ' + note)); continue
        ln = line_of(src, m.start())
        keys = set()
        t = next((t for (p, l), t in tbl.items() if p == f and abs(l - ln) <= 1), None)
        if t:
            keys = {int(a, 16) for a in t[3]} | set(t[4])
        elif distinct is None and src[m.end() - 1:m.end()] == '{':
            # a function anchor: the keys are the addresses in its body (a switch table)
            i = m.end() - 1; d = 0; j = i
            while j < len(src):
                if src[j] == '{': d += 1
                elif src[j] == '}':
                    d -= 1
                    if d == 0: break
                j += 1
            keys = {int(a, 16) for a in ADDR.findall(re.sub(r'"(?:[^"\\]|\\.)*"', '""', src[i:j]))} | set(MANGLED.findall(src[i:j]))
        elif isinstance(distinct, int) and distinct > 0:
            for lit in re.finditer(r'(0x1[48][0-9a-fA-F]{7})|"(\?[^"]+)"', src[m.start():m.start() + 400]):
                if len(keys) >= distinct:
                    break
                keys.add(int(lit.group(1), 16) if lit.group(1) else lit.group(2))
        if 'devicedata' in f and t:
            keys = set(t[4])                      # the iat addresses are VideoDX11's: not keys
        if distinct is None:
            distinct = len(keys)
        if nrows is None:
            if kind == 'dispatch_slot' or 'EngineFuncsInit' in anchor:
                nrows = -1                        # computed below from the call sites
            else:
                nrows = len(keep_alive(keys, module, exe, rta, rtn, exp_by_addr, exp_by_name))
        rows.append((f, ln, kind, module, distinct, source, nrows, thread, note))
    return rows


def row_class(r):
    """The brief's row classes, applied to THIS inventory's rows (what the rows the site keeps
    alive become). 'none' = the site keeps no registry row."""
    f, kind, note = r[0], r[2], str(r[8])
    if kind == 'lazy_cache':
        return 'cache'
    if kind == 'vtable_define' or (kind == 'identity_compare' and str(r[6]) in ('0', '')):
        return 'none'
    if kind == 'diagnostic':
        return 'DEAD'
    if kind in ('dispatch_slot', 'stored_callback') or 'STATE-MACHINE' in str(r[5]):
        return 'CALLBACK'
    if f in ('standalone/reg_rdata_float_consts.cpp', 'standalone/rt_data_mint.cpp'):
        return 'STORAGE'
    return 'DIRECT'


# ------------------------------------------------------------------ main
def main():
    files = load_files()
    exe, rta, rtn = registry_rows(files)
    exp = export_table()
    exp_by_addr, exp_by_name = {}, {}
    dup_addr = Counter(a for a, _ in exp)
    for a, n in exp:
        exp_by_addr.setdefault(a, n)          # resolve_standalone.cpp emplace: FIRST name wins
        exp_by_name.setdefault(n, a)

    wrappers, data_sites, literal_hits, defs = build_wrappers(files)
    tables = scan_tables(files)
    wnames = {n for n, _f in wrappers}
    comp, diag = scan_compare_diag(files, defs)
    per, csites = scan_caches(files, wnames)
    # the keys that reach a door through a literal at a resolver or wrapper call (strict readers)
    strict_names, strict_addrs = set(), set()
    for (_c, _df), hits in literal_hits.items():
        for _p, _ln, a in hits:
            a = re.sub(r'^\(\s*uintptr_t\s*\)\s*', '', a)
            if a.startswith('"'):
                strict_names.add(re.sub(r'"\s*"', '', a)[1:-1])
            else:
                mm = re.match(r'0x[0-9a-fA-F]+', a)
                if mm:
                    strict_addrs.add(int(mm.group(0), 16))

    out = []
    header = ['site_file', 'site_line', 'kind', 'module', 'distinct_addresses', 'source_of_values',
              'rows_kept_alive', 'thread', 'note']

    # dispatch slots: every RegisterDispatchSlot literal keeps an exe row and the name it resolves
    ds = []
    for p, src in files.items():
        for m in re.finditer(r'\b(?:RegisterDispatchSlot|RegisterPlacePlayerSlot|RegisterLane3Slot)\s*\(\s*(0x[0-9a-fA-F]+)\s*,', src):
            a, e = call_args(src, m.end() - len(m.group(0)) + src[m.start():].index('('))
            nm = MANGLED.search(''.join(a[1:])) if len(a) > 1 else None
            ds.append((p, int(m.group(1), 16), nm.group(1) if nm else None))
    ds_names = {n for _, _, n in ds if n}

    cur = curated_rows(files, tables, exe, rta, rtn, exp_by_addr, exp_by_name)
    for r in cur:
        r = list(r)
        if r[6] == -1:
            if r[2] == 'dispatch_slot':
                mine = [d for d in ds if d[0] == r[0]]
                r[4] = len({d[1] for d in mine})
                r[6] = len({d[1] for d in mine}) + len({d[2] for d in mine if d[2] and d[2] in rtn}) \
                    + len({d[2] for d in mine if d[2] and d[2] not in rtn and d[2] in exp_by_name})
            else:
                efc = files.get('engine/funcs/engine_funcs.cpp', '')
                ks = {int(a, 16) for a in ADDR.findall(efc)} | set(MANGLED.findall(efc))
                r[4] = len(ks)
                r[6] = len(keep_alive(ks, 'mixed', exe, rta, rtn, exp_by_addr, exp_by_name))
        out.append(r)

    curated_keys = {(r[0], r[1]) for r in cur}

    # wrappers fed by DATA (non-parameter, non-literal keys) that the curated list does not cover
    curated_files = {'standalone/dlgctrl_route.cpp', 'standalone/modal_driver.cpp', 'runtime/src/ui/dialog.cpp',
                     'code/Game/Tutorial/tutorialstep.cpp', 'code/Game/Component/citizenai_candidatelist.cpp',
                     'code/Game/Game/geometry_serialize.cpp', 'runtime/src/graphics/wave24_lane_gfxmat.cpp'}
    for p, ln, callee, arg in data_sites:
        if p in ('standalone/resolve.h', 'runtime/funcs/rt_resolve_cache.cpp', 'tests/unit/rt_port_stub.cpp')                 or p in curated_files:
            continue                              # the doors' own internals / hand-read above
        ident = re.sub(r'^.*::', '', arg.strip())
        val = None
        if re.fullmatch(r'k\w+', ident):
            for f in [p] + [q for q in files if q.rsplit('/', 1)[-1] in include_closure(files, p)]:
                mm = re.search(r'\b' + ident + r'\s*(?:\[\s*\])?\s*=\s*(0x[0-9a-fA-F]+|"(?:[^"\\]|\\.)*")', files[f])
                if mm:
                    val = mm.group(1); break
        keys = set()
        if val:
            keys = {val[1:-1]} if val.startswith('"') else {int(re.match(r'0x[0-9a-fA-F]+', val).group(0), 16)}
        kind = 'name_table' if (keys and isinstance(next(iter(keys)), str)) or 'Export' in callee else 'addr_table'
        mod = 'runtime' if p.startswith(('runtime', 'dx11')) or 'Export' in callee else 'exe'
        out.append([p, ln, kind, mod, len(keys) if val else '',
                    f'{callee}({arg}) -- ' + (f'named constant {ident} = {val}' if val else 'key is a local/member'),
                    len(keep_alive(keys, mod, exe, rta, rtn, exp_by_addr, exp_by_name)) if val else '',
                    'both' if p.startswith(LOADER_REACH) else 'unknown',
                    'named constant: a literal grep finds the definition, not the resolve' if val else 'AUTO data site: read by hand'])

    # identity compares and diagnostics found automatically
    for p, ln, c, a in comp:
        if (p, ln) in curated_keys:
            continue
        keys = {int(x, 16) for x in ADDR.findall(a)} | set(MANGLED.findall(a))
        out.append([p, ln, 'identity_compare', 'exe' if any(isinstance(k, int) and k >> 28 == 0x14 for k in keys) else 'runtime',
                    len(keys), f'{c}({a}) compared, not called', len(keep_alive(keys, 'mixed', exe, rta, rtn, exp_by_addr, exp_by_name)),
                    'unknown', 'AUTO'])
    for p, ln, c, a in diag:
        keys = {int(x, 16) for x in ADDR.findall(a)} | set(MANGLED.findall(a))
        out.append([p, ln, 'diagnostic', 'exe' if any(isinstance(k, int) and k >> 28 == 0x14 for k in keys) else 'runtime',
                    len(keys), f'{c} inside a printf argument list', len(keep_alive(keys, 'mixed', exe, rta, rtn, exp_by_addr, exp_by_name)),
                    'unknown', 'AUTO: a resolve whose only use is to be printed'])

    # lazy-cache census rows (one per spelling per directory)
    for label, cnt in per.items():
        for d, n in sorted(cnt.items()):
            out.append([d, 0, 'lazy_cache', 'runtime' if d.startswith(('runtime', 'dx11')) else 'exe', '',
                        f'{label}: {n} site(s)', '', CACHE_THREAD.get(label, 'unknown'), CACHE_SAFETY.get(label, '')])

    for r in out:
        r[8] = f'class={row_class(r)}; ' + str(r[8])
    out.sort(key=lambda r: (r[2], r[0], int(r[1] or 0)))
    cls = Counter((row_class(r), r[0].split('/')[0] if not r[0].startswith(('code/Game', 'runtime/src'))
                   else '/'.join(r[0].split('/')[:3])) for r in out if r[2] != 'lazy_cache')
    print('row classes (non-cache rows) per directory:')
    for (c, d), n in sorted(cls.items()):
        print(f'    {c:10s} {d:28s} {n}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\t'.join(header) + '\n')
        for r in out:
            f.write('\t'.join(str(x).replace('\t', ' ') for x in r) + '\n')

    # ---------------- summary
    print(f'wrote {os.path.relpath(OUT, REPO)} ({len(out)} rows)')
    print(f'wrappers discovered (a resolver whose key is a parameter): {len(wrappers)}')
    lit_total = sum(len(v) for k, v in literal_hits.items())
    print(f'  literal-keyed calls through resolvers+wrappers: {lit_total}')
    print(f'data sites (key is neither a literal nor a parameter): {len(data_sites)}')
    for p, ln, c, a in data_sites:
        print(f'    {p}:{ln}  {c}({a})')
    print(f'tables with >=2 retail keys: {len(tables)}')
    for t in tables:
        print(f'    {t[0]}:{t[1]} {t[2]} addrs={len(t[3])} names={len(t[4])} addrs_in_strings={len(t[5])}')
    print(f'identity compares (auto): {len(comp)}   diagnostics (auto): {len(diag)}')
    print(f'dispatch-slot registrations: {len(ds)} ({len({d[1] for d in ds})} distinct slots, '
          f'{len(ds_names)} distinct names resolved into them)')
    print('cache census (sites per spelling):')
    for label, _ in CACHE_PATTERNS:
        c = per.get(label, Counter())
        nl = sum(1 for f, _l in csites.get(label, []) if f.startswith(LOADER_REACH))
        print(f'    {sum(c.values()):5d}  {label}   [{nl} in loader-reachable dirs]   '
              + ', '.join(f'{d}={n}' for d, n in c.most_common(8)))
    # the export table: rows the MIRROR actually serves
    body_all = {p: s for p, s in files.items() if p not in GENERATED}
    lit_names, lit_addrs = set(), set()
    for p, s in body_all.items():
        s2 = re.sub(r'\bRegister(?:RuntimeExport|RuntimeAddr|ExeAddr|DispatchSlot)\s*\([^;]*;', '', s)
        lit_names |= set(MANGLED.findall(s2))
        if not p.startswith('dx11/'):
            lit_addrs |= {int(a, 16) for a in ADDR.findall(re.sub(r'"(?:[^"\\]|\\.)*"', '""', s2))}
    need_n2a = [(a, n) for a, n in exp if a in lit_addrs and a not in rta and n in rtn]
    need_a2n = [(a, n) for a, n in exp if n in lit_names and n not in rtn and a in rta]
    any_reader = [(a, n) for a, n in exp if a in lit_addrs or n in lit_names]
    s_n2a = [(a, n) for a, n in exp if a in strict_addrs and a not in rta and n in rtn]
    s_a2n = [(a, n) for a, n in exp if n in strict_names and n not in rtn and a in rta]
    both_reg = [(a, n) for a, n in exp if a in rta and n in rtn]
    one_reg = [(a, n) for a, n in exp if (a in rta) != (n in rtn)]
    none_reg = [(a, n) for a, n in exp if a not in rta and n not in rtn]
    print(f'rt_export_addrs.inc: {len(exp)} rows; {sum(1 for c in dup_addr.values() if c > 1)} addresses carry '
          f'>1 name ({sum(c for c in dup_addr.values() if c > 1)} rows; the mirror keeps only the FIRST name)')
    print(f'  rows with a literal of either key outside registrations: {len(any_reader)}')
    print(f'  rows the MIRROR serves: name->addr {len(need_n2a)}, addr->name {len(need_a2n)}, '
          f'union {len(set(need_n2a) | set(need_a2n))}  (any literal counts as a reader: conservative)')
    print(f'  rows the MIRROR serves, strict (literal at a resolver/wrapper call): name->addr {len(s_n2a)}, '
          f'addr->name {len(s_a2n)}, union {len(set(s_n2a) | set(s_a2n))}')
    for a, n in sorted(set(need_n2a) | set(need_a2n)):
        print(f'      0x{a:x}  {n}  {"n->a" if (a, n) in need_n2a else "a->n"}')
    print(f'  rows with BOTH keys registered explicitly: {len(both_reg)}; exactly one key: {len(one_reg)} '
          f'(name only {sum(1 for a, n in one_reg if n in rtn)} -> the mirror adds that many ADDRESS entries; '
          f'address only {sum(1 for a, n in one_reg if a in rta)} -> that many NAME entries); neither: {len(none_reg)}')
    print(f'  registered keys with NO export row (never mirrored): names {len(rtn - set(exp_by_name))}, '
          f'addrs {len(rta - set(exp_by_addr))}')
    print(f'registered (literal + table + registrar wrappers): exe {len(exe)}, runtime addr {len(rta)}, runtime name {len(rtn)}')
    print(f'registrar wrappers (a helper whose parameter reaches Register*): {len(REGISTRAR_WRAPPERS)}')
    for (n, _f), w in sorted(REGISTRAR_WRAPPERS.items()):
        print(f'    {n}  {w[0]}:{w[1]} ({w[2]}, key arg {w[3]}, via {w[4]})')
    print('resolver wrappers:')
    for (n, f), w in sorted(wrappers.items(), key=lambda kv: kv[1][0]):
        print(f'    {n}  {w[0]}:{w[1]} ({w[2]}, key arg {w[3]}, via {w[4]}) literal calls={len(literal_hits.get((n, f), []))}')


CACHE_THREAD = {
    'RT_EXPORT_ONCE': 'both', 'RT_DATA_SLOT': 'both', 'RtResolveCached (direct)': 'both',
    'AtomicExport/AtomicAddr/AtomicByName/AtomicByAddr (uses)': 'both',
    'std::atomic<void*> door slot (defs)': 'both',
    'function-local static = resolve': 'both',
    'if (!x) x = resolve (hand-written lazy fill)': 'both',
    'X_ResolveFuncs / _InitFuncs definitions': 'both', 'X_ResolveFuncs / _InitFuncs calls': 'both',
    'Ensure*() guard functions (bool-flag lazy fill)': 'both',
    'guard flag set BEFORE the fills (if (f) return; f = true;)': 'both',
    'handle-struct field fill (g_x.f = resolve)': 'unknown',
}
CACHE_SAFETY = {
    'RT_EXPORT_ONCE': 'lambda-local `static void* = nullptr` (constant-init, no guard) + RtResolveCached: plain void* store, a formal data race the file calls benign; never caches null; never freezes a pre-init hit. Direct call: no cache',
    'RT_DATA_SLOT': 'same mechanism as RT_EXPORT_ONCE, for DATA rows (STORAGE)',
    'RtResolveCached (direct)': 'caller-owned void* cache; same contract; not atomic',
    'AtomicExport/AtomicAddr/AtomicByName/AtomicByAddr (uses)': 'std::atomic<void*> acquire/release: B6-5 compliant. element_common.h honours rt_resolve_cache.cpp section 6; the per-file copies differ (see inventory)',
    'std::atomic<void*> door slot (defs)': 'the storage of the atomic spelling',
    'function-local static = resolve': 'C++11 magic static (no -fno-threadsafe-statics in the Makefile): init is thread-safe but FREEZES the first answer, null included, for the life of the process',
    'if (!x) x = resolve (hand-written lazy fill)': 'plain pointer, not atomic: racy if reached from the loader thread; retries a null',
    'X_ResolveFuncs / _InitFuncs definitions': 'file-scope statics + plain bool flag; called lazily from bodies (CreateNewObject, Persist, Execute): reachable from the loader thread on a save load',
    'X_ResolveFuncs / _InitFuncs calls': 'the lazy call sites of the above',
    'Ensure*() guard functions (bool-flag lazy fill)': 'file-scope statics + plain bool; same race as X_ResolveFuncs',
    'guard flag set BEFORE the fills (if (f) return; f = true;)': 'WORST shape: a second thread sees the flag set and reads statics still null',
    'handle-struct field fill (g_x.f = resolve)': 'a struct of fn ptrs filled once (dx11 g_deps / g_im, engine handle structs)',
}

if __name__ == '__main__':
    main()
