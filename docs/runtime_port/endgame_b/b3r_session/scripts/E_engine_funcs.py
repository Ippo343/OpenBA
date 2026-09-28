#!/usr/bin/env python3
"""E_engine_funcs.py -- block-0 inventory E (ENGINE_FUNCS), B.3.R registry teardown.

Re-run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b3r_session/scripts/E_engine_funcs.py
Writes docs/runtime_port/endgame_b/b3r_session/data/E_engine_funcs.tsv and prints
the summary numbers the inventory quotes.

What it does (read-only on the tree):
 1. Parses engine/funcs/engine_funcs.h (comments stripped with b3x codegrep's
    strip()) into top-level items: externs, typedefs, function declarations,
    inline functions, the one `static inline auto&` alias, forward declarations.
 2. Parses EngineFuncsInit() in engine_funcs.cpp and records, BY INITIALISER, what
    each global is bound to: ResolveExport("mangled"), ResolveAddr(Module::Runtime,
    0x18...), ResolveAddr(Module::Exe, 0x14...) (a code/data address or, through a
    `dispatch` local, a PE import slot), or a direct `&Function` / lambda.
 3. Resolves each binding through the registries the way the standalone resolver
    does at boot: RegisterExeAddr / RegisterDispatchSlot / RegisterLane3Slot /
    RegisterPlacePlayerSlot rows for exe addresses; RegisterRuntimeExport (by name)
    and RegisterRuntimeAddr (by the address rt_export_addrs.inc pairs with the name)
    for runtime names; rt_data_mint_tables.inc for minted storage. It then finds the
    registered body's definition and reads it for stub markers.
 4. Counts every CODE line (comments stripped, strings kept) that names each item,
    per directory, excluding: its own declaration, its definition in the .cpp, its
    binding statement(s) and the _DEBUG dump printfs inside EngineFuncsInit, and any
    file that declares its OWN symbol of the same name (a shadow, reported).
"""
import os, re, sys, collections

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', '..'))
os.chdir(REPO)
sys.path.insert(0, os.path.join(REPO, 'docs', 'runtime_port', 'endgame_b', 'b3x_session'))
from codegrep import strip  # noqa: E402  (the counting rule of the brief)

HDR = 'engine/funcs/engine_funcs.h'
CPP = 'engine/funcs/engine_funcs.cpp'
OUT = 'docs/runtime_port/endgame_b/b3r_session/data/E_engine_funcs.tsv'
SCAN_DIRS = ['code', 'runtime', 'standalone', 'engine', 'dx11', 'tests', 'tools']
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')
EXCL = ('standalone/build', 'tests/unit/build', '.claude/worktrees', '/build/', '/build-')
INC_DIRS = ['.', 'engine/types', 'engine/funcs', 'code/Game', 'runtime/types', 'runtime/funcs',
            'dx11/types', 'dx11/funcs', 'standalone']   # Makefile SA_INCLUDES, in order


def dir_bucket(p):
    if p.startswith('code/Game/'): return 'code_game'
    if p.startswith('runtime/'): return 'runtime'
    if p.startswith('standalone/'): return 'standalone'
    if p.startswith('engine/'): return 'engine'
    if p.startswith('dx11/'): return 'dx11'
    if p.startswith('tests/'): return 'tests'
    return 'other'


# ---------------------------------------------------------------- the tree
FILES = {}      # path -> (raw_lines, stripped_lines, stripped_text)
for root in SCAN_DIRS:
    for dp, dn, fs in os.walk(root):
        for f in fs:
            p = os.path.join(dp, f).replace('\\', '/')
            if any(e in p for e in EXCL) or not p.endswith(EXTS):
                continue
            try:
                raw = open(p, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            st = strip(raw)
            FILES[p] = (raw.split('\n'), st.split('\n'), st)


def lines_matching(rx, paths=None):
    for p in (paths or FILES):
        for i, l in enumerate(FILES[p][1], 1):
            if rx.search(l):
                yield p, i, l


# ---------------------------------------------------------------- 1. header
hraw, hlines, htext = FILES[HDR]


def lineno(text, idx):
    return text.count('\n', 0, idx) + 1


def top_level_items(text):
    """Yield (start_idx, chunk) for each depth-0 statement / function definition."""
    i, n, depth, start = 0, len(text), 0, 0
    while i < n:
        c = text[i]
        if depth == 0 and c == '#':          # preprocessor line
            j = text.find('\n', i); j = n if j < 0 else j
            yield i, text[i:j]; i = j + 1; start = i; continue
        if c == '"' or c == "'":
            q = c; i += 1
            while i < n and text[i] != q:
                i += 2 if text[i] == '\\' else 1
            i += 1; continue
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                yield start, text[start:i + 1]; start = i + 1
        elif c == ';' and depth == 0:
            yield start, text[start:i + 1]; start = i + 1
        i += 1


RX_TYPEDEF_FP = re.compile(r'typedef\s+(.+?)\(\s*\*\s*(\w+)\s*\)\s*\((.*?)\)\s*;', re.S)
items = []      # dicts
for idx, chunk in top_level_items(htext):
    s = chunk.strip()
    if not s or s.startswith('#'):
        continue
    ln = lineno(htext, idx + (len(chunk) - len(chunk.lstrip())))
    flat = ' '.join(s.split())
    m = RX_TYPEDEF_FP.match(flat)
    if m:
        items.append(dict(name=m.group(2), kind='typedef', ret=m.group(1).strip(), args=m.group(3), line=ln, text=flat))
        continue
    m = re.match(r'extern\s+"C"\s+(.+?)\b(\w+)\s*\((.*)\)\s*;$', flat)
    if m:
        items.append(dict(name=m.group(2), kind='extern "C" function declaration', ret=m.group(1).strip(), args=m.group(3), line=ln, text=flat))
        continue
    m = re.match(r'extern\s+(.+?)\s*\b(\w+)\s*;$', flat)
    if m:
        items.append(dict(name=m.group(2), kind='extern', vtype=m.group(1).strip(), line=ln, text=flat))
        continue
    m = re.match(r'class\s+(\w+)\s*;$', flat)
    if m:
        items.append(dict(name=m.group(1), kind='forward class declaration', line=ln, text=flat))
        continue
    m = re.match(r'static\s+inline\s+auto\s*&\s*(\w+)\s*=\s*(\w+)\s*;$', flat)
    if m:
        items.append(dict(name=m.group(1), kind='alias reference', target=m.group(2), line=ln, text=flat))
        continue
    m = re.match(r'inline\s+(.+?)\b(\w+)\s*\(([^)]*)\)\s*\{', flat)
    if m:
        endln = lineno(htext, idx + len(chunk))
        items.append(dict(name=m.group(2), kind='inline function', ret=m.group(1).strip(), args=m.group(3), line=ln, endline=endln, text=flat[:160]))
        continue
    m = re.match(r'(.+?)\b(\w+)\s*\(([^)]*)\)\s*;$', flat)
    if m:
        items.append(dict(name=m.group(2), kind='function declaration', ret=m.group(1).strip(), args=m.group(3), line=ln, text=flat))
        continue
    items.append(dict(name='?', kind='UNPARSED', line=ln, text=flat[:120]))

BY_NAME = collections.OrderedDict()
for it in items:
    BY_NAME.setdefault(it['name'], it)
EXTERNS = [it for it in items if it['kind'] == 'extern']
TYPEDEFS = {it['name']: it for it in items if it['kind'] == 'typedef'}

# ---------------------------------------------------------------- 2. the .cpp
craw, clines, ctext = FILES[CPP]
m = re.search(r'bool\s+EngineFuncsInit\s*\(\s*\)\s*\{', ctext)
init_start = m.end()
d, k = 1, init_start
while d:
    if ctext[k] == '{': d += 1
    elif ctext[k] == '}': d -= 1
    k += 1
init_end = k
INIT_L0, INIT_L1 = lineno(ctext, init_start), lineno(ctext, init_end)
body = ctext[init_start:init_end - 1]

RX_ADDR = re.compile(r'ResolveAddr\(\s*Module::(Exe|Runtime)\s*,\s*(0x[0-9a-fA-F]+)\s*\)')
RX_EXPORT = re.compile(r'ResolveExport\(\s*"([^"]+)"\s*\)')
bindings = collections.defaultdict(list)   # name -> [dict(by, to, line, deref)]
binding_lines = collections.defaultdict(set)
vestigial = []                              # resolves whose result is dropped
dispatch_addr = None
pos = 0
for stmt in re.split(r';', body):
    s_idx = init_start + pos
    pos += len(stmt) + 1
    flat = ' '.join(stmt.split())
    l0 = lineno(ctext, s_idx + (len(stmt) - len(stmt.lstrip())))
    l1 = lineno(ctext, s_idx + len(stmt))
    md = re.search(r'void\*\*\s*dispatch\s*=.*?' + RX_ADDR.pattern, flat)
    if md:
        dispatch_addr = md.group(2).lower()
        if not re.search(r'\bfn_\w+\s*=', ctext[s_idx + len(stmt):s_idx + len(stmt) + 250].split('}')[0]):
            vestigial.append((dispatch_addr, l0))
        continue
    ma = re.search(r'(?:^|[\s{)])(\w+)\s*=\s*(?!=)(.*)$', flat)
    if not ma:
        continue
    name, rhs = ma.group(1), ma.group(2)
    if name not in BY_NAME and not name.startswith('s_fn_'):
        continue
    rec = dict(line=l0)
    if 'dispatch' in rhs and dispatch_addr:
        rec.update(by='exe_slot', to=dispatch_addr)
    elif RX_EXPORT.search(rhs):
        rec.update(by='export', to=RX_EXPORT.search(rhs).group(1))
    elif RX_ADDR.search(rhs):
        mm = RX_ADDR.search(rhs)
        rec.update(by='exe_addr' if mm.group(1) == 'Exe' else 'rt_addr', to=mm.group(2).lower(),
                   deref=rhs.count('*reinterpret_cast') > 0)
    elif rhs.startswith('[]'):
        rec.update(by='other', to='lambda (thunk onto fn_MemFreeStatic)')
    elif re.search(r'&\s*(\w+)', rhs):
        rec.update(by='other', to='&' + re.search(r'&\s*(\w+)', rhs).group(1))
    else:
        rec.update(by='other', to=rhs[:60])
    if 'if (!' + name + ')' in flat or 'if (!' in flat.split(name)[0]:
        rec['fallback'] = True
    bindings[name].append(rec)
    for L in range(l0, l1 + 1):
        binding_lines[name].add(L)

# every ResolveAddr / ResolveExport line inside the init body, for the "vestigial" list
dump_lines = set()
in_debug = False
for L in range(INIT_L0, INIT_L1 + 1):
    raw = craw[L - 1]
    if raw.strip().startswith('#ifdef _DEBUG'): in_debug = True
    elif raw.strip().startswith('#endif'): in_debug = False
    elif in_debug: dump_lines.add(L)

cpp_defs = {}   # name -> line of `T name = nullptr;` definition in the .cpp
for i, l in enumerate(clines, 1):
    mm = re.match(r'^\s*(?:static\s+)?[\w:<>]+[\s\*]+(\w+)\s*=\s*nullptr\s*;', l)
    if mm:
        cpp_defs[mm.group(1)] = i


def nostr(l):
    """A hit inside a string literal is not a reader (brief, counting rules)."""
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', l)

# ---------------------------------------------------------------- 3. registries
RT_ADDR_OF, RT_NAME_OF = {}, {}
for l in FILES['standalone/rt_export_addrs.inc'][1]:
    mm = re.search(r'\{\s*(0x[0-9a-fA-F]+)ull\s*,\s*"([^"]+)"', l)
    if mm:
        RT_ADDR_OF[mm.group(2)] = int(mm.group(1), 16); RT_NAME_OF[int(mm.group(1), 16)] = mm.group(2)
MINT = {}
for l in FILES['standalone/rt_data_mint_tables.inc'][1]:
    mm = re.search(r'\{\s*"([^"]+)"\s*,\s*(0x[0-9a-fA-F]+)ull', l)
    if mm:
        MINT[int(mm.group(2), 16)] = mm.group(1)

REG_FILES = [p for p in FILES if not p.endswith('.inc') and not p.startswith(('tests/', 'tools/'))]


def addr_rx(a):
    return r'0x0*' + ('%x' % a) + r'(?:ull|ULL|u|U)?\b'


def next_text(p, i, n=3):
    return ' '.join(FILES[p][1][i - 1:i - 1 + n])


def arg_after_addr(txt, a):
    mm = re.search(addr_rx(a) + r'\s*,\s*(.*)', txt, re.I)
    if not mm: return ''
    t, depth, out = mm.group(1), 0, ''
    for ch in t:
        if ch == '(': depth += 1
        elif ch == ')':
            if depth == 0: break
            depth -= 1
        out += ch
    return ' '.join(out.split())


def target_of(argtxt):
    mm = RX_EXPORT.search(argtxt)
    if mm: return ('export', mm.group(1))
    mm = re.search(r'&\s*([\w:]+)', argtxt)
    if mm: return ('fn', mm.group(1))
    mm = re.search(r'(\w+)\s*$', argtxt)
    return ('expr', argtxt) if argtxt else ('?', '')


def exe_registrations(a):
    rx = re.compile(r'(Register\w*\s*\(\s*' + addr_rx(a) + r')|^\s*\{\s*' + addr_rx(a) + r'\s*,', re.I)
    out = []
    for p in REG_FILES:
        if p in (HDR, CPP): continue
        for i, l in enumerate(FILES[p][1], 1):
            if rx.search(l):
                t = next_text(p, i)
                out.append(dict(file=p, line=i, arg=arg_after_addr(t, a), text=' '.join(l.split())))
    return out


def rt_name_registrations(n):
    out = []
    lit = '"' + n + '"'
    for p in REG_FILES:
        if p in (HDR, CPP): continue
        L = FILES[p][1]
        for i, l in enumerate(L, 1):
            if lit in l:
                ctx = ' '.join(L[max(0, i - 2):i + 2])
                if re.search(r'RegisterRuntimeExport\s*\(\s*$', L[i - 2] if i > 1 else '') or \
                   re.search(r'RegisterRuntimeExport\s*\(\s*' + re.escape(lit), l):
                    t = ' '.join(L[i - 1:i + 2])
                    mm = re.search(re.escape(lit) + r'\s*,\s*(.*)', t)
                    arg = mm.group(1) if mm else ''
                    out.append(dict(file=p, line=i, arg=' '.join(arg.split())[:120], text=' '.join(l.split())))
                elif re.search(r'^\s*\{\s*' + re.escape(lit), l):
                    out.append(dict(file=p, line=i, arg=' '.join(l.split())[:120], text='table row'))
    return out


def rt_addr_registrations(a):
    rx = re.compile(r'RegisterRuntimeAddr\s*\(\s*' + addr_rx(a), re.I)
    out = []
    for p in REG_FILES:
        for i, l in enumerate(FILES[p][1], 1):
            if rx.search(l):
                out.append(dict(file=p, line=i, arg=arg_after_addr(next_text(p, i), a), text=' '.join(l.split())))
    return out


DEF_CACHE = {}


def find_definition(sym, prefer=None):
    """First definition (a body follows) of free function / member `sym` (last :: component).
    The registering file is searched first; a qualified name only matches a file that
    spells `qual::base` or opens `namespace qual`."""
    key = (sym, prefer)
    if key in DEF_CACHE: return DEF_CACHE[key]
    base = sym.split('::')[-1]
    qual = sym.split('::')[-2] if '::' in sym else None
    rx = re.compile(r'\b' + re.escape(base) + r'\s*\(')
    res = None
    order = ([prefer] if prefer in FILES else []) + [q for q in FILES if q != prefer]
    for p in order:
        raw, L, T = FILES[p]
        if p.startswith(('tests/', 'tools/')): continue
        if base not in T: continue
        if qual and (qual + '::' + base) not in T and not re.search(r'namespace\s+' + re.escape(qual) + r'\b', T):
            continue
        for i, l in enumerate(L, 1):
            if not rx.search(l): continue
            pre = l[:rx.search(l).start()]
            if re.search(r'(=|return|\(|,|->|\.|!|&&|\|\|)\s*$', pre.rstrip()) and not pre.rstrip().endswith('::'):
                continue
            if not re.search(r'^[\s\w\*&:<>,"]*$', pre):
                continue
            look = ' '.join(L[i - 1:i + 6])
            after = look[look.find(base):]
            b, s = after.find('{'), after.find(';')
            if b >= 0 and (s < 0 or b < s):
                # body extent
                txt = '\n'.join(L[i - 1:])
                j = txt.find('{', txt.find(base)); dd = 0; k2 = j
                while k2 < len(txt):
                    if txt[k2] == '{': dd += 1
                    elif txt[k2] == '}':
                        dd -= 1
                        if dd == 0: break
                    k2 += 1
                nbody = txt[j:k2 + 1].count('\n') + 1
                rawbody = '\n'.join(raw[i - 1:i - 1 + nbody])
                marker = bool(re.search(r'NOT IMPLEMENTED|PORT GAP|\bSTUB\b|not ported|unported', rawbody, re.I))
                # a STUB is a short body that says so; a long body carrying a PORT GAP
                # constant or a NOT PORTED leg is a real port with a hole (noted, not STUB)
                stubby = marker and nbody <= 12
                head = txt[:j]
                plist = head[head.find('(') + 1:head.rfind(')')]
                depth, nparam = 0, (0 if not plist.strip() or plist.strip() == 'void' else 1)
                for ch in plist:
                    if ch in '(<': depth += 1
                    elif ch in ')>': depth -= 1
                    elif ch == ',' and depth == 0: nparam += 1
                res = dict(file=p, line=i, sig=' '.join(l.split())[:150], nlines=nbody, stub=stubby,
                           partial=marker and not stubby, nparam=nparam,
                           member=('::' + base + '(') in l and not (qual and re.search(r'namespace\s+' + re.escape(qual) + r'', T)))
                break
        if res: break
    DEF_CACHE[key] = res
    return res


def header_declaring(sym):
    base = sym.split('::')[-1]
    rx = re.compile(r'\b' + re.escape(base) + r'\s*\([^;{]*\)\s*(const)?\s*;')
    hits = []
    for p, (raw, L, T) in FILES.items():
        if not p.endswith('.h') or p == HDR or base not in T: continue
        if any(rx.search(l) for l in L):
            hits.append(p)
    return hits


def demangle_simple(n):
    """Enough of the MSVC scheme for the 27 names this file binds."""
    m = re.match(r'\?\?([0-9A-Z])(\w+)@(\w+)@@', n)
    ops = {'0': 'ctor', '1': 'dtor', '4': 'operator=', 'D': 'operator*', 'Y': 'operator+=', 'M': 'operator<', 'H': 'operator+'}
    if m:
        cls, ns = m.group(2), m.group(3)
        return '%s::%s::%s' % (ns, cls, ops.get(m.group(1), '??' + m.group(1)))
    m = re.match(r'\?(\w+)@(\w+)@(\w+)@@', n)
    if m:
        return '%s::%s::%s' % (m.group(3), m.group(2), m.group(1))
    return n


def resolve_exe(a):
    regs = exe_registrations(a)
    if not regs:
        return dict(resolves='NULL (unregistered exe address: boot logs NOT IMPLEMENTED)', regs=[], target=None)
    tk, tv = target_of(regs[-1]['arg'])
    r = dict(regs=regs)
    if tk == 'export':
        rr = resolve_export(tv)
        r.update(resolves='slot -> ' + tv + ' -> ' + rr['resolves'], target=rr.get('target'), export=tv)
    elif tk == 'fn':
        dfn = find_definition(tv, regs[-1]['file'])
        where = '%s:%d' % (dfn['file'], dfn['line']) if dfn else '?'
        r.update(resolves='%s:%s' % (dfn['file'] if dfn else '?', tv), target=tv, defn=dfn, where=where)
    else:
        r.update(resolves='storage/expr: ' + tv[:80], target=None)
    return r


def resolve_export(n):
    regs = rt_name_registrations(n)
    a = RT_ADDR_OF.get(n)
    aregs = rt_addr_registrations(a) if a else []
    allr = regs + aregs
    r = dict(regs=allr, addr=a)
    for rg in allr:
        tk, tv = target_of(rg['arg'])
        if tk == 'fn':
            dfn = find_definition(tv, rg['file'])
            r.update(resolves='%s:%s' % (dfn['file'] if dfn else rg['file'], tv), target=tv, defn=dfn)
            return r
    if allr:
        r.update(resolves='registered (%s:%d, value `%s`)' % (allr[0]['file'], allr[0]['line'], allr[0]['arg'][:60]), target=None)
    else:
        r.update(resolves='NULL (no RegisterRuntimeExport / RegisterRuntimeAddr row: RtUnresolved at boot)', target=None)
    return r


def resolve_rt_addr(a, deref):
    if a in MINT:
        return dict(resolves='MINT storage row %s' % MINT[a], target=None, regs=[], mint=True)
    regs = rt_addr_registrations(a)
    n = RT_NAME_OF.get(a)
    if n and not regs:
        rr = resolve_export(n); rr['export'] = n; return rr
    r = dict(regs=regs, export=n)
    if regs:
        tk, tv = target_of(regs[0]['arg'])
        if tk == 'fn':
            dfn = find_definition(tv, regs[0]['file'])
            r.update(resolves='%s:%s' % (dfn['file'] if dfn else regs[0]['file'], tv), target=tv, defn=dfn)
        else:
            r.update(resolves='registered (%s:%d, value `%s`, export %s)' % (regs[0]['file'], regs[0]['line'], regs[0]['arg'], n), target=None)
    else:
        r.update(resolves='NULL', target=None)
    return r


# ---------------------------------------------------------------- 4. uses
def shadow_files(name):
    """Files other than engine/funcs that declare their own symbol `name`."""
    # an `extern` redeclaration names the SAME symbol: it is a use, not a shadow
    rx = re.compile(r'^\s*(?!extern\b)(?:static\s+|inline\s+|constexpr\s+|const\s+)*'
                    r'(?:[\w:]+(?:<[^;=]*>)?[\s\*&]+)\b' + re.escape(name) + r'\s*(?:=[^=]|;|\[|\()')
    rx_td = re.compile(r'(?:typedef\b.*\(\s*\*\s*' + re.escape(name) + r'\s*\)|using\s+' + re.escape(name) + r'\s*=|typedef\b[^;]*\b' + re.escape(name) + r'\s*;)')
    out = set()
    for p, (raw, L, T) in FILES.items():
        if p.startswith('engine/funcs/') or name not in T: continue
        for l in L:
            if re.search(r'&&|\|\||\?', l):
                continue   # an expression continuation, not a declaration
            if rx_td.search(l) or (rx.search(l) and not re.search(r'\breturn\b|^\s*(?:if|else|while|for)\b', l)
                                   and not re.search(r'\b' + re.escape(name) + r'\s*\([^)]*\)\s*;', l)):
                out.add(p); break
            if rx.search(l) and re.search(r'\b(fn_\w+_t|void\s*\*|auto)\s+' + re.escape(name) + r'\s*=', l):
                out.add(p); break
    return out


def uses_of(name, it):
    rx = re.compile(r'\b' + re.escape(name) + r'\b')
    sh = shadow_files(name) if not it['kind'].startswith('function') else set()
    res = []
    for p, (raw, L, T) in FILES.items():
        if name not in T or p in sh: continue
        if it['kind'] == 'forward class declaration' and p != HDR:
            continue   # the class itself is real; only this header's own spellings need the forward declaration
        for i, l in enumerate(L, 1):
            l = nostr(l)
            if not rx.search(l): continue
            if p == HDR:
                if i == it['line']: continue
                if it['kind'] == 'inline function' and it['line'] <= i <= it.get('endline', i): continue
            if p == CPP:
                if i == cpp_defs.get(name): continue
                if i in binding_lines.get(name, ()): continue
                if i in dump_lines: continue
                if INIT_L0 <= i <= INIT_L1 and re.search(r'reinterpret_cast<\s*' + re.escape(name) + r'\s*>', l): continue
            res.append((p, i, l.strip(), nostr(L[i]) if i < len(L) else ''))
    return res, sh


def use_kind(name, l, nxt=''):
    """call | guard (readiness test) | write (the global is reassigned) | copy (value
    cached into another variable / passed as a callback) | addr (taken as void* for a
    printf or a readiness table) | other."""
    n = re.escape(name)
    if re.search(r'\b' + n + r'\s*$', l) and nxt.lstrip().startswith('('):
        return 'call'
    if re.search(r'\b' + n + r'\s*\(', l) or re.search(r'\(\s*\*\s*' + n + r'\s*\)\s*\(', l):
        return 'call'
    if re.search(r'(?<![\w.>])' + n + r'\s*=[^=]', l) or re.search(r'\*\s*' + n + r'\s*=[^=]', l):
        return 'write'
    if re.search(r'(\bif\b|\bwhile\b|&&|\|\||\?|!\s*' + n + r'\b|==\s*nullptr|!=\s*nullptr)', l) \
            or nxt.lstrip().startswith('?'):
        return 'guard'
    if re.search(r'\(\s*(const\s+)?void\s*\*\s*\)\s*' + n + r'\b|reinterpret_cast<\s*void\s*\*\s*>\(\s*' + n + r'\s*\)', l):
        return 'addr'
    if re.search(r'=\s*(\([^)]*\)\s*|reinterpret_cast<[^>]*>\(\s*)*' + n + r'\b', l) or re.search(r'reinterpret_cast<\w+>\(\s*' + n + r'\s*\)', l):
        return 'copy'
    return 'other'


RESOLVE_RX = r'(ResolveAddr\s*\(\s*Module::(?:Exe|Runtime)\s*,|RtAddr\s*<[^>]*>\s*\(|RtAddrFn\s*\(|ByAddr\s*\(|RtResolveCached\s*\()\s*'


def other_readers(by, to):
    """Code lines OUTSIDE engine/funcs that resolve the same address / name themselves
    (spellings 1, 3, 8, 10 of the brief). The row cannot die until these convert too."""
    if by in ('exe_addr', 'exe_slot', 'rt_addr'):
        a = int(to, 16)
        rx = re.compile(RESOLVE_RX + addr_rx(a), re.I)
        rx2 = re.compile(r'constexpr[^;]*=\s*' + addr_rx(a), re.I)
    elif by == 'export':
        rx = re.compile(r'(ResolveExport|RT_EXPORT_ONCE\s*\(\s*\w+\s*,)\s*\(?\s*"' + re.escape(to) + '"')
        rx2 = None
    else:
        return []
    out = []
    for p, (raw, L, T) in FILES.items():
        if p.startswith('engine/funcs/'): continue
        for i, l in enumerate(L, 1):
            if rx.search(l) or (rx2 and rx2.search(l)):
                out.append('%s:%d' % (p, i))
            elif by == 'export' and ('"' + to + '"') in l and i > 1 and re.search(r'(ResolveExport|RT_EXPORT_ONCE)\s*\([^)]*$', L[i - 2]):
                out.append('%s:%d' % (p, i))
    return out


# ---------------------------------------------------------------- 5. includes
def resolve_include(frm, inc):
    cand = [os.path.join(os.path.dirname(frm), inc)] + [os.path.join(d, inc) for d in INC_DIRS]
    for c in cand:
        c = os.path.normpath(c).replace('\\', '/')
        if os.path.isfile(c): return c
    return None


INCLUDERS = collections.defaultdict(list)
for p, (raw, L, T) in FILES.items():
    for l in raw:
        mm = re.match(r'\s*#\s*include\s*"([^"]+)"', l)
        if mm:
            tgt = resolve_include(p, mm.group(1))
            if tgt and tgt.startswith('engine/'):
                INCLUDERS[tgt].append(p)

# ---------------------------------------------------------------- read by hand
# Facts the patterns cannot see, each read from the code (inventory section 4/5).
# (class override or None, note). A class override is used only where reading the
# code contradicts the mechanical class.
MIS = 'MISNAMED (binding disagrees with the name): '
HAND = {
    'fn_MemAlloc': (None, 'bound to ?Allocate@MemoryAllocator (the singleton-first MEMBER), not ?MemAlloc; the registered value is '
                    'SwapAllocate (standalone/alloc_cutover_arm.cpp, anonymous namespace) handed over by RtAllocSwapArmCutover; '
                    'standalone/redzone_alloc.cpp:340 REWRITES the global to ShimAlloc under BANISHED_REDZONE, so a direct call '
                    'bypasses the redzone: convert only after the shim hooks a named entry'),
    'fn_MemFreeStatic': (None, 'rtalloc::MemFree; standalone/redzone_alloc.cpp:341 REWRITES it to ShimFree (see fn_MemAlloc); '
                         '11 Work files cache it into their own s_fn_MemFree / s_fn_MemFreeStatic'),
    'fn_MemFree': (None, 'never resolved: EngineFuncsInit assigns a lambda forwarding to fn_MemFreeStatic (singleton arg dead); '
                   'direct form is System::MemoryAllocator::MemFree(p)'),
    'fn_IsDerivedFrom': (None, 'EngineFuncsInit REPLACES the resolved export with IsDerivedFrom_NullSafe (null -> false, an SA '
                         'deviation documented in engine_funcs.cpp). p->IsDerivedFrom(id) drops the null-safety: a BEHAVIOUR '
                         'CHANGE unless each site is proven non-null or a free null-safe helper keeps it'),
    'fn_InternalBasePtr_Ctor': (None, 'every guard is `if (fn_InternalBasePtr_Ctor) new (p) Object::InternalBasePtr(...)`: the '
                                'pointer is a readiness test on a placement-new it never calls; conversion = delete the test'),
    'fn_InternalBasePtr_DeletePtr': (None, 'readiness tests around member DeletePtr() calls, plus 3 copies into s_fn_IBP_DeletePtr '
                                     '(dropoffrawmaterial / performwork / removenaturalresource)'),
    'fn_IBPTreeInsert': (None, 'the `if (!fn_IBPTreeInsert)` fallback to IBPTreeInsert_CppImpl (entitylist.cpp) never fires: '
                         '0x140021a30 is registered by RegisterWorkBstBatch inside AddressRegistryInit, before EngineFuncsInit; '
                         'the EntityList tree runs WorkBST_RegisterWork_1a30 and the entitylist.cpp body is dead'),
    'fn_IBPTreeRemove': (None, 'as fn_IBPTreeInsert: fallback IBPTreeRemove_CppImpl never fires (0x14001a6e0 registered)'),
    'fn_Entity_FindAudioComponent': ('STUB', '0x140002280 is registered nowhere: resolves NULL and EngineFuncsInit prints the boot '
                                     'line `NOT IMPLEMENTED: exe function at 0x140002280`; debugui.cpp guards it'),
    'fn_ScenarioDialog_Ctor': (None, 'not a door: bound to &ScenarioDialog_Ctor directly'),
    'fn_ModDialog_ArrayPush': (None, 'not a door: bound to fn_ModDialog_ArrayPush58_CppImpl, which is DEFINED in engine_funcs.cpp '
                               'and must move out with the file'),
    'fn_Game_DestroyEntity': (None, MIS + 'header says FUN_1400c3ec0 (entityList, entity); the initialiser is 0x140091e60 = '
                              'RawMaterialComponent_RegisterRawMaterial'),
    'fn_AudioObjectGroup_Ctor': (None, MIS + '0x1400c3720 is ComponentEntity_CtorFromDescription (same as fn_EntityFactory_CreateEntity)'),
    'fn_AudioScene_RegisterObject': (None, MIS + '0x1400c48a0 is EntityList_RegisterEntity_Impl (same as fn_EntityList_RegisterEntity)'),
    'fn_StorageComponent_GetCapacity': (None, MIS + '0x140128570 is PastureComponent_ComputeCapacity'),
    'fn_StorageComponent_SpawnItem': (None, MIS + '0x140128670 is Pasture_SpawnLivestock'),
    'fn_Building_AddBuyToStorage': (None, MIS + '0x14005ace0 is TradeComponent_SpawnPurchasedLivestock'),
    'fn_CitizenList_RequestWorker': (None, MIS + '0x14008d100 is FindAvailableBuilding'),
    'fn_RadiusCalc': (None, MIS + '0x140055430 is Player_HighlightAdd_55430'),
    'fn_RadiusDisplay_Remove': (None, MIS + '0x1400b0d00 is DecalComponent_TerrainDecalRemove'),
    'fn_Toolbar_ShowGroup': (None, 'same address as fn_SetEditMode (0x140102b60 -> Toolbar_ShowGroup_Impl); the header also calls '
                             'FUN_140102b60 "SetEditMode" (EntityUI_SetEditMode_Impl, a third body for it)'),
    'fn_SetEditMode': (None, MIS + '0x140102b60 is Toolbar_ShowGroup_Impl (see fn_Toolbar_ShowGroup)'),
    'fn_SetFollowBuilding': (None, MIS + '0x140101a80 is Toolbar_Utility_Reg (the header already says so)'),
    'fn_CalcMaxBuilders': (None, MIS + '0x14012e5c0 is WorkComponent_CalcMaxWorkers (tutorial_base.cpp); fn_StorageCountQuery, '
                           'declared in this header, is a SECOND body for the same retail function (workcomponent.cpp)'),
    'fn_StorageCountQuery': (None, 'second body for 0x14012e5c0; the registered one is WorkComponent_CalcMaxWorkers'),
    'fn_ResidenceComponent_GetOccupantCount': (None, MIS + '0x140123070 is Pf_RecursiveChildSum_123070 (= fn_PositionComponent_Count)'),
    'fn_ResidenceComponent_GetSpawnPoint': (None, MIS + '0x140122e20 is Pf_EntityResolvePos_2e20 (three globals, three typedefs)'),
    'fn_Entity_GetPosition': (None, 'same body as fn_ResidenceComponent_GetSpawnPoint / fn_Entity_GetPositionForIndex'),
    'fn_Entity_GetPositionForIndex': (None, 'same body as fn_Entity_GetPosition'),
    'fn_PositionComponent_Count': (None, 'same body as fn_ResidenceComponent_GetOccupantCount'),
    'fn_Entity_FindComponent_Position': (None, MIS + '0x140002ed0 finds InteractComponent (WorkBase_FindComponent_2ed0)'),
    'fn_FindComponent_Position': (None, MIS + 'finds BuildComponent (entity_findcomponent.cpp:60)'),
    'fn_FindComponent_Build': (None, MIS + 'finds MapComponent (the header says so)'),
    'fn_Entity_FindComponent_MatDesc': (None, MIS + '0x140003050 finds WorkComponent (= fn_FindComponent_WorkC, fn_FindStorageLocationComp)'),
    'fn_FindStorageLocationComp': (None, MIS + '0x140003050 finds WorkComponent'),
    'fn_ExternalPtr_Serialize': (None, 'three globals, three retail functions (0x14000bd30 / 0x14000bf40 / 0x140002340), ONE registered '
                                 'body Reimpl_ExternalPtr_Serialize_StringTable ("stream-identical", address_registry.cpp:1195)'),
    'fn_ExternalPtr_Serialize2': (None, 'see fn_ExternalPtr_Serialize'),
    'fn_ExternalPtr_BuildSerialize': (None, 'see fn_ExternalPtr_Serialize; also passed as a callback to SerializeCompRefTree '
                                      '(citizenlist_serialize.cpp:661)'),
    'fn_ExternalPtr_Serialize_Weather': (None, 'same address and body as fn_WeatherExternalSerialize'),
    'fn_TreeRecursiveFree': (None, 'same address and body as fn_BST_FreeNodes'),
    'fn_BST_Insert': (None, 'same address and body as fn_CitizenAI_RegisterWork / fn_IBPTreeInsert'),
    'fn_Map_CanPathTo': (None, 'same address as fn_Map_CheckPassable, different typedef'),
    'fn_FindStorageDestination': (None, 'same address as fn_FindStorageForResource, different typedef'),
    'fn_Game_ReloadWithSave': (None, 'same address and body as fn_LoadScenarioFromSave'),
    'g_pGameSingleton': (None, 'the SAME cell as g_pGameWorldSingleton (0x1401c3dd8 -> g_storage_GameWorldSingleton)'),
    'g_pGameWorldSingleton': (None, 'cell g_storage_GameWorldSingleton (address_registry.cpp:1149); also g_pGameSingleton'),
    'g_pNewGameSettings': (None, 'the SAME cell as g_pGameSettingsPtr (0x1401c3de0), typed void** here and int32_t** there'),
    'g_pGameSettingsPtr': (None, 'cell g_storage_GameSettingsPtr (address_registry.cpp:1150); also g_pNewGameSettings'),
    'g_pKeyMapSingleton': (None, 'cell g_storage_KeyMapSingleton (address_registry.cpp:1148)'),
    'g_pInputConfigSingletonExref': (None, 'import slot 0x140151738 registered to &g_StandaloneKeyMapMid, a hand-built 3-level chain '
                                     'ending in the KeyMap; the header says the global was RENAMED at the B.3.7 close -- it was not'),
    'g_pAgeMultiplier': (None, 'constant 5.0f (k_rdata_172198): becomes constexpr'),
    'g_pNotifyTimerDuration': (None, 'constant 30.0f (k_bits_1721cc): becomes constexpr'),
    'g_fUVScale': (None, 'constant 1.0f (s_fConst_1_0, gameupdateloop_load.cpp:1684): becomes constexpr'),
    'g_pMemAllocSingleton': (None, 'the minted storage row ?_singleton@MemoryAllocator (question 4): becomes the class static'),
    'fn_SceneAddObject': (None, 'bound through a dispatch-slot read; the only mentions left are comments in tool_base_place.cpp'),
    'fn_SceneRemoveObject': (None, 'see fn_SceneAddObject'),
    'fn_MeshInstance_UpdateHierarchy': (None, 'see fn_SceneAddObject'),
    'fn_TutorialStepExecute': ('DIRECT', 'TutWave_StepExecute is a real port with one NOT PORTED leg (tutorial completion)'),
}


# ---------------------------------------------------------------- 6. rows
ABI_RET = re.compile(r'\b(retval|retbuf|retBuf|retStr|retIBP|outIBP|retNode|retvalIBP|outBuf|outEntityIBP|outNodePtr)\b')


def abi_flag(it, td, by, target_member):
    if not td: return ''
    args = td['args']
    f = []
    if ABI_RET.search(args):
        if by in ('export', 'exe_slot', 'rt_addr') and target_member:
            f.append('retslot: the typedef carries MSVC this-then-ret order; the real member returns by value -- NOT a rename')
        elif by in ('export', 'exe_slot', 'rt_addr'):
            f.append('retslot in typedef (runtime member returning by value): convert to the member call, NOT a rename')
        else:
            f.append('retslot in typedef (exe C-shaped body takes the same explicit slot: rename is safe if the body signature matches)')
    if re.search(r'\bString\b|InternalPtr|Filename', args):
        f.append('by-value class parameter')
    return '; '.join(f)


rows = []
DUMP = []   # every counted use, for --dump
summary = collections.Counter()
typedef_users = collections.defaultdict(list)
for it in EXTERNS:
    typedef_users[it['vtype']].append(it['name'])

for it in items:
    if it['kind'] in ('UNPARSED',): continue
    name = it['name']
    if it['kind'] == 'typedef' and typedef_users.get(name):
        continue   # reported on its extern's row
    uses, shadows = uses_of(name, it)
    by_dir = collections.Counter(dir_bucket(u[0]) for u in uses)
    kinds = collections.Counter(use_kind(name, u[2], u[3]) for u in uses)
    # an alias's uses count toward its target
    alias_uses = []
    for a in items:
        if a['kind'] == 'alias reference' and a.get('target') == name:
            au, _ = uses_of(a['name'], a)
            alias_uses = au
    td = TYPEDEFS.get(it.get('vtype', '')) if it['kind'] == 'extern' else (it if it['kind'] == 'typedef' else None)
    bnd = bindings.get(name, [])
    by = ','.join(sorted(set(b['by'] for b in bnd))) if bnd else ''
    bound_to = ' | '.join(b['to'] + (' (fallback)' if b.get('fallback') else '') for b in bnd)
    resolves, target, defn, member, regnote = '', None, None, False, ''
    if it['kind'] == 'extern':
        primary = [b for b in bnd if not b.get('fallback')] or bnd
        b = primary[0] if primary else None
        if b is None:
            resolves = 'NEVER BOUND (stays nullptr)'
        elif b['by'] in ('exe_addr', 'exe_slot'):
            r = resolve_exe(int(b['to'], 16)); resolves = r['resolves']; target = r.get('target'); defn = r.get('defn')
            if len(r['regs']) > 1:
                regnote = 'registered %d times: %s' % (len(r['regs']), ' ; '.join('%s:%d %s' % (x['file'], x['line'], x['arg'][:50]) for x in r['regs']))
            elif r['regs']:
                regnote = '%s:%d' % (r['regs'][0]['file'], r['regs'][0]['line'])
            if r.get('export'):
                member = True
        elif b['by'] == 'export':
            r = resolve_export(b['to']); resolves = r['resolves']; target = r.get('target'); defn = r.get('defn'); member = True
            regnote = ' ; '.join('%s:%d' % (x['file'], x['line']) for x in r['regs'][:3])
        elif b['by'] == 'rt_addr':
            r = resolve_rt_addr(int(b['to'], 16), b.get('deref')); resolves = r['resolves']; target = r.get('target'); defn = r.get('defn')
            member = bool(r.get('export'))
            regnote = ' ; '.join('%s:%d' % (x['file'], x['line']) for x in r.get('regs', [])[:3]) + (' export=%s' % r.get('export') if r.get('export') else '')
        else:
            tgt = b['to'].lstrip('&')
            defn = find_definition(tgt) if re.match(r'^\w+$', tgt) else None
            resolves = ('%s:%s' % (defn['file'], tgt)) if defn else b['to']
            target = tgt if defn else None
            if len(bnd) > 1:
                resolves += ' (fallback bodies: %s)' % ', '.join(x['to'] for x in bnd[1:])
        if defn and defn.get('stub'):
            resolves += ' [STUB-MARKED BODY]'
    elif it['kind'] in ('function declaration', 'extern "C" function declaration'):
        defn = find_definition(name)
        resolves = '%s:%d (defined; %d lines)' % (defn['file'], defn['line'], defn['nlines']) if defn else 'NO DEFINITION FOUND'
        target = name if defn else None
        if defn:   # the definition itself is not a use
            uses = [u for u in uses if not (u[0] == defn['file'] and u[1] == defn['line'])]
            by_dir = collections.Counter(dir_bucket(u[0]) for u in uses)
            kinds = collections.Counter(use_kind(name, u[2], u[3]) for u in uses)
    elif it['kind'] == 'inline function':
        resolves = 'inline body in engine_funcs.h:%d-%d' % (it['line'], it['endline'])
    elif it['kind'] == 'alias reference':
        resolves = 'alias of ' + it['target']

    # -------- classification (see the inventory, section 2)
    n_uses = len(uses) + len(alias_uses)
    kind = it['kind']
    vt = it.get('vtype', '')
    is_data = kind == 'extern' and not vt.startswith('fn_')
    if n_uses == 0:
        cls = 'DEAD'
    elif is_data:
        cls = 'STORAGE'
    elif kind == 'extern' and (resolves.startswith('NULL') or 'STUB-MARKED' in resolves or resolves.startswith('NEVER')):
        cls = 'STUB'
    else:
        cls = 'DIRECT'
    repl = ''
    b0 = bnd[0] if bnd else None
    ex = ''
    if b0 and b0['by'] == 'export': ex = b0['to']
    elif b0 and b0['by'] == 'exe_slot': ex = resolve_exe(int(b0['to'], 16)).get('export', '')
    elif b0 and b0['by'] == 'rt_addr': ex = RT_NAME_OF.get(int(b0['to'], 16), '')
    if cls == 'DIRECT' and kind == 'extern':
        if ex:
            repl = 'member/static of the real class: %s (door body today: %s)' % (demangle_simple(ex), target or '?')
        elif target:
            hs = header_declaring(target)
            repl = '%s(...)  [%s]' % (target, ('declared in ' + hs[0]) if hs else ('NO HEADER declares it; defined in ' + (defn['file'] if defn else '?')))
    elif cls == 'DIRECT' and kind == 'function declaration':
        hs = header_declaring(name)
        repl = 'already a real function; move this declaration to %s' % (hs[0] if hs else ('a header beside ' + (defn['file'] if defn else '?')))
    elif cls == 'DIRECT' and kind == 'extern "C" function declaration':
        hs = header_declaring(name)
        repl = 'already a real function; declaration moves to %s' % (hs[0] if hs else ('a header beside ' + (defn['file'] if defn else '?')))
    elif cls == 'DIRECT' and kind == 'inline function':
        repl = 'move the body next to its users (%s)' % ', '.join(sorted(set(u[0] for u in uses)))[:200]
    elif cls == 'DIRECT' and kind == 'forward class declaration':
        repl = 'dies with the declarations that use it'
    elif cls == 'STORAGE':
        repl = 'class static / constexpr: see inventory section 5'
    # ---- ABI
    abi = ''
    if td and kind in ('extern', 'typedef'):
        f = []
        ctor = bool(ex) and ex.startswith('??0')
        if ABI_RET.search(td['args']) and not ctor:
            if ex:
                f.append('RETSLOT: typedef spells the hidden return pointer explicitly (MSVC this-then-ret); the real member returns by value -- a direct call is NOT a rename')
            else:
                f.append('retslot spelled explicitly; the registered exe body is C-shaped with the same slot -- rename-safe when the arity matches')
        if re.search(r'\b(String|InternalPtr|Filename)\b', td['args']):
            f.append('by-value class parameter')
        tn = 0 if not td['args'].strip() else td['args'].count(',') + 1
        if defn and defn.get('nparam') is not None and defn['nparam'] != tn and not ex:
            f.append('ARITY: typedef %d params, registered body %d' % (tn, defn['nparam']))
        abi = '; '.join(f)
    # ---- notes
    notes = []
    if name in HAND:
        notes.append('READ: ' + HAND[name][1])
        cls = HAND[name][0] or cls
    if shadows: notes.append('shadowed (own symbol of this name) in: ' + ', '.join(sorted(shadows)))
    if alias_uses: notes.append('%d uses through alias fn_SM_CancelWork' % len(alias_uses))
    if kinds['call'] == 0 and kinds['guard'] > 0 and kind == 'extern' and not is_data:
        notes.append('GUARD-ONLY: never called, only tested for non-null')
    if kinds['write']: notes.append('%d WRITE(S) to the global outside its binding' % kinds['write'])
    if regnote: notes.append('reg: ' + regnote)
    if defn:
        notes.append('body %s:%d (%d lines, %s params)%s%s' % (defn['file'], defn['line'], defn['nlines'], defn.get('nparam'),
                     ' member' if defn.get('member') else '', ' PARTIAL: body carries a PORT GAP / NOT PORTED marker' if defn.get('partial') else ''))
    if kind == 'extern' and td:
        others = [u for u in uses_of(td['name'], td)[0] if not (u[0] == CPP and INIT_L0 <= u[1] <= INIT_L1)]
        others = [u for u in others if not (u[0] == HDR and re.match(r'\s*extern\s', u[2]))]
        others = [u for u in others if not (u[0] == CPP and re.match(r'^\s*' + re.escape(td['name']) + r'\s+\w+\s*=\s*nullptr', u[2]))]
        if others:
            notes.append('typedef %s used elsewhere %d: %s' % (td['name'], len(others), ', '.join(sorted(set('%s:%d' % (u[0], u[1]) for u in others)))[:300]))
        else:
            notes.append('typedef %s dies with it' % td['name'])
    orr = []
    for bb in bnd:
        orr += other_readers(bb['by'], bb['to'])
    rows.append(dict(
        name=name, kind=kind + (' (data)' if is_data else (' (fn ptr)' if kind == 'extern' else '')),
        typedef=(td['name'] if td and kind == 'extern' else ''), bound_by=by, bound_to=bound_to,
        resolves_to=resolves, uses_total=n_uses,
        uses_code_game=by_dir['code_game'], uses_runtime=by_dir['runtime'], uses_standalone=by_dir['standalone'],
        uses_engine=by_dir['engine'], uses_dx11=by_dir['dx11'], uses_tests=by_dir['tests'],
        guards=kinds['guard'], cls=cls, direct_replacement=repl, abi_flag=abi,
        calls=kinds['call'], writes=kinds['write'], copies=kinds['copy'], addr_taken=kinds['addr'], other_uses=kinds['other'],
        other_readers_same_binding=('%d %s' % (len(orr), ' '.join(orr)[:600])) if orr else '0',
        header_line=it['line'],
        use_sites=' '.join(sorted('%s:%d' % (u[0], u[1]) for u in uses))[:1500],
        notes=' | '.join(notes)))
    DUMP.extend((name, cls, u[0], u[1], use_kind(name, u[2], u[3]), u[2]) for u in uses)
    summary[(kind if kind != 'extern' else ('extern data' if is_data else 'extern fnptr'), cls)] += 1

COLS = ['name', 'kind', 'typedef', 'bound_by', 'bound_to', 'resolves_to', 'uses_total', 'uses_code_game',
        'uses_runtime', 'uses_standalone', 'uses_engine', 'uses_dx11', 'uses_tests', 'guards', 'cls',
        'direct_replacement', 'abi_flag', 'calls', 'writes', 'copies', 'addr_taken', 'other_uses',
        'other_readers_same_binding', 'header_line', 'use_sites', 'notes']
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\t'.join(c if c != 'cls' else 'class' for c in COLS) + '\n')
    for r in rows:
        f.write('\t'.join(str(r[c]).replace('\t', ' ') for c in COLS) + '\n')

if '--dump' in sys.argv:   # optional: every counted use line, one per row, for reading by hand
    with open(sys.argv[sys.argv.index('--dump') + 1], 'w', encoding='utf-8') as f:
        for d in DUMP: f.write('%s\t%s\t%s:%d\t%s\t%s\n' % d)

# ---------------------------------------------------------------- 7. summary
print('items parsed: %d  (externs %d, typedefs %d)' % (len(items), len(EXTERNS), len(TYPEDEFS)))
print('unparsed:', [i for i in items if i['kind'] == 'UNPARSED'])
print('rows written:', len(rows), '->', OUT)
for k in sorted(summary): print('  %-45s %-8s %d' % (k[0], k[1], summary[k]))
never = [e['name'] for e in EXTERNS if e['name'] not in bindings]
print('externs never bound in EngineFuncsInit:', never)
print('vestigial dispatch resolves (result dropped):', vestigial)
ecount = collections.Counter()
for e in EXTERNS:
    for b in bindings.get(e['name'], []): ecount[b['by']] += 1
print('bindings by door:', dict(ecount))
addr_names = collections.defaultdict(list)
for e in EXTERNS:
    for b in bindings.get(e['name'], []):
        if b['by'] in ('exe_addr', 'exe_slot', 'rt_addr'): addr_names[b['to']].append(e['name'])
print('addresses bound by more than one global:')
for a, ns in sorted(addr_names.items()):
    if len(ns) > 1: print('  ', a, ns)
print('includers of engine/ files (direct, per dir):')
for tgt in sorted(set(list(INCLUDERS) + ['engine/funcs/engine_funcs.h'] + ['engine/types/' + x for x in os.listdir('engine/types') if x.endswith('.h')] + ['engine/types/ui/' + x for x in os.listdir('engine/types/ui')])):
    c = collections.Counter(dir_bucket(p) for p in INCLUDERS.get(tgt, []))
    nl = len(FILES[tgt][0]) - (1 if FILES[tgt][0][-1] == '' else 0) if tgt in FILES else 0
    fwd = [l for l in FILES[tgt][1] if re.match(r'\s*#\s*include', l)] if tgt in FILES else []
    code = [l for l in FILES[tgt][1] if l.strip() and not l.strip().startswith('#')] if tgt in FILES else []
    shape = 'forwarder (%s)' % fwd[0].split('include')[1].strip() if fwd and not code else ('%d code lines' % len(code))
    print('  %-44s lines=%-5d includers=%-4d %-60s %s' % (tgt, nl, len(INCLUDERS.get(tgt, [])), dict(c), shape))
# who includes engine_funcs.h, by header vs source (a header that includes it spreads it)
hdr_inc = sorted(p for p in INCLUDERS['engine/funcs/engine_funcs.h'] if p.endswith('.h'))
print('headers that include engine_funcs.h (%d):' % len(hdr_inc), ' '.join(hdr_inc))
# rows and uses per class, per directory
print('per class: rows / uses by dir')
cc = collections.defaultdict(collections.Counter)
for r in rows:
    k = r['cls']
    cc[k]['rows'] += 1
    for d in ('code_game', 'runtime', 'standalone', 'engine', 'dx11', 'tests'):
        cc[k][d] += r['uses_' + d]
    cc[k]['guards'] += r['guards']; cc[k]['calls'] += r['calls']
for k in sorted(cc): print('  %-8s %s' % (k, dict(cc[k])))
fe = [r for r in rows if r['kind'].startswith('extern')]
print('externs only: %s' % dict(collections.Counter(r['cls'] for r in fe)))
print('externs by binding door x class:')
for r in fe: pass
print('  ', dict(collections.Counter((r['bound_by'], r['cls']) for r in fe)))
print('guard-only externs:', [r['name'] for r in fe if r['calls'] == 0 and r['guards'] > 0 and 'fn ptr' in r['kind']])
print('misnamed (hand-read):', sum(1 for r in rows if 'MISNAMED' in r['notes']))
print('files with any use (all rows), per dir:', dict(collections.Counter(dir_bucket(f) for f in set(d[2] for d in DUMP))))
print('files with an extern use, per dir:', dict(collections.Counter(dir_bucket(f) for f in set(d[2] for d in DUMP if d[0] in set(r['name'] for r in fe)))))
print('EngineFuncsInit callers:', [(p, i) for p, i, l in lines_matching(re.compile(r'\bEngineFuncsInit\s*\(\s*\)')) if p != HDR and not (p == CPP)])
