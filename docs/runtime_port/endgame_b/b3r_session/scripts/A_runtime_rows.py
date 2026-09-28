#!/usr/bin/env python3
"""A_runtime_rows.py -- block-0 inventory A (RUNTIME_ROWS) of the registry teardown.

Re-run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b3r_session/scripts/A_runtime_rows.py

Writes docs/runtime_port/endgame_b/b3r_session/data/A_runtime_rows.tsv (one line per
registration, table loops expanded) and prints the summary numbers the inventory quotes.

Method (see INVENTORY_A_RUNTIME_ROWS.md section 2):
  * every file under runtime/ standalone/ engine/ dx11/ code/ is read with comments
    stripped by b3x_session/codegrep.py's strip() (strings kept);
  * a registration is a RegisterRuntimeExport( / RegisterRuntimeAddr( CALL (declarations
    and the three definitions skipped), args split at top level; a key that is an
    identifier is looked up as a file-local `const char* kX = "..."`, a key `r.field`
    expands to every {0x18..., "..."} pair of the file's table (the loop spelling);
  * the two keys are paired through standalone/rt_export_addrs.inc (the mirror);
  * a READER is any occurrence, outside the registrations and the generated .inc files,
    of the row's mangled name as a (concatenated) string literal, of its 0x18... address
    as a code literal, or of an exe PE import slot 0x140151218..0x140152c18 whose
    imported name (tools/dispatch_slot_map.py) is the row's name;
  * a reader is LIVE when the function or the named binding (a global / static / named
    constant / table) it sits in is referenced anywhere else in code.
"""
import os, re, sys, collections, importlib.util
sys.dont_write_bytecode = True

ROOT = os.getcwd()
sys.path.insert(0, os.path.join(ROOT, 'docs/runtime_port/endgame_b/b3x_session'))
from codegrep import strip, EXCL  # noqa: E402

OUT = 'docs/runtime_port/endgame_b/b3r_session/data/A_runtime_rows.tsv'
DIRS = ['runtime', 'standalone', 'engine', 'dx11', 'code']
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')
GENERATED = {'standalone/rt_export_addrs.inc', 'standalone/rt_data_mint_tables.inc'}
SLOT_LO, SLOT_HI = 0x140151218, 0x140152c18


def walk(dirs):
    out = []
    for r in dirs:
        for dp, dn, fs in os.walk(r):
            for f in fs:
                p = os.path.join(dp, f).replace('\\', '/')
                if any(e in p for e in EXCL) or not p.endswith(EXTS):
                    continue
                out.append(p)
    return sorted(out)


FILES = walk(DIRS)
SRC = {}
for p in FILES:
    SRC[p] = strip(open(p, encoding='utf-8', errors='replace').read())


def blank_strings(s):
    """Same length, string/char literal CONTENTS replaced by spaces."""
    return re.sub(r'"(?:\\.|[^"\\\n])*"', lambda m: '"' + ' ' * (len(m.group(0)) - 2) + '"', s)


NOSTR = {p: blank_strings(s) for p, s in SRC.items()}


class LineIdx:
    def __init__(self, s):
        self.starts = [0] + [m.end() for m in re.finditer('\n', s)]

    def line(self, pos):
        import bisect
        return bisect.bisect_right(self.starts, pos)


LIDX = {p: LineIdx(s) for p, s in SRC.items()}

# ---------------------------------------------------------------------------
# rt_export_addrs.inc: the name <-> address mirror
# ---------------------------------------------------------------------------
INC = open('standalone/rt_export_addrs.inc', encoding='utf-8').read()
NAME2ADDR, ADDR2NAME = {}, {}
for a, n in re.findall(r'\{\s*(0x[0-9a-fA-F]+)ull,\s*"([^"]+)"\s*\}', INC):
    NAME2ADDR[n] = int(a, 16)
    ADDR2NAME[int(a, 16)] = n

# ---------------------------------------------------------------------------
# Block structure: which function / binding encloses a position
# ---------------------------------------------------------------------------
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'catch', 'else', 'do',
            'case', 'throw', 'new', 'delete', 'decltype', 'static_assert', 'alignof',
            'defined', 'noexcept', 'operator', 'alignas', 'co_return', 'typeid'}


def blocks(p):
    """List of (open_pos, close_pos, kind, name) for every {...} whose parents are all
    namespace / class / extern blocks: kind in ns, type, func, init."""
    s = NOSTR[p]
    res = []
    stack = []  # (open_pos, kind, name, toplevel)
    last_boundary = 0
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == '{':
            header = s[last_boundary:i]
            h = re.sub(r'\s+', ' ', header).strip()
            parent_top = all(k in ('ns', 'type') for (_, k, _, _) in stack)
            kind, name = 'other', ''
            if parent_top:
                if re.search(r'\bnamespace\b', h) or re.match(r'^extern\s*"[^"]*"$', h):
                    kind = 'ns'
                    mn = re.search(r'\bnamespace\s+([\w:]+)\s*$', h)
                    name = mn.group(1) if mn else ''
                elif re.search(r'\b(class|struct|union|enum)\b[^()=]*$', h):
                    kind = 'type'
                    m = re.search(r'\b(?:class|struct|union|enum)\s+(?:\w+\s+)*?(\w+)\s*(?:final\s*)?(?::[^{]*)?$', h)
                    name = m.group(1) if m else ''
                elif '=' in h and not re.search(r'\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:->.*)?$', h) \
                        or re.search(r'\w\s*(\[[^\]]*\])?\s*$', h) and not h.endswith(')') and '(' not in h:
                    kind = 'init'
                    m = re.search(r'(\w+)\s*(?:\[[^\]]*\])*\s*=?\s*$', h)
                    name = m.group(1) if m else ''
                else:
                    kind = 'func'
                    # name: last identifier (with qualifier) before the parameter list
                    hh = re.sub(r'\s*:\s*[^:].*$', '', h) if re.search(r'\)\s*:\s*\w', h) else h
                    m = None
                    depth = 0
                    j = len(hh) - 1
                    # find the '(' matching the last ')'
                    while j >= 0:
                        if hh[j] == ')':
                            depth += 1
                        elif hh[j] == '(':
                            depth -= 1
                            if depth == 0:
                                break
                        j -= 1
                    if j > 0:
                        m = re.search(r'((?:\w+\s*::\s*)*~?\w+(?:\s*<[^()]*>)?)\s*$', hh[:j])
                    name = re.sub(r'\s+', '', m.group(1)) if m else ''
                    name = re.sub(r'<.*>$', '', name)
            if kind == 'func' and name:
                scope = [nm for (_, k, nm, _) in stack if k in ('ns', 'type') and nm]
                QUALNAME[(p, i)] = '::'.join(scope + [name])
            stack.append((i, kind, name, parent_top))
            last_boundary = i + 1
        elif c == '}':
            if stack:
                o, kind, name, top = stack.pop()
                if top:
                    res.append((o, i, kind, name))
            last_boundary = i + 1
        elif c == ';':
            last_boundary = i + 1
        i += 1
    return res


QUALNAME = {}   # (file, open_pos) -> namespace/class-qualified function name
BLOCKS = {p: blocks(p) for p in FILES}


def enclosing(p, pos):
    """Innermost top-level func/init block containing pos, else ('file', '')."""
    best = None
    for o, c, kind, name in BLOCKS[p]:
        if o < pos < c and kind in ('func', 'init'):
            if best is None or o > best[0]:
                best = (o, c, kind, name)
    if best:
        return best[2], best[3], best[0]
    return 'file', '', 0


def enclosing_type(p, pos):
    best = None
    for o, c, kind, name in BLOCKS[p]:
        if o < pos < c and kind == 'type':
            if best is None or o > best[0]:
                best = (o, name)
    return best[1] if best else ''


# ---------------------------------------------------------------------------
# Identifier references (for liveness)
# ---------------------------------------------------------------------------
TOK = collections.defaultdict(list)   # ident -> [(file, pos)]
for p in FILES:
    for m in re.finditer(r'\b[A-Za-z_]\w*\b', NOSTR[p]):
        TOK[m.group(0)].append((p, m.start()))


def stmt_prefix(s, pos):
    j = pos - 1
    while j >= 0 and s[j] not in ';{}':
        j -= 1
    return s[j + 1:pos]


TYPEISH = re.compile(r'^[\w:\*&<>,\s]*$')


def is_decl_or_write(p, pos, name):
    s = NOSTR[p]
    pre = stmt_prefix(s, pos)
    after = s[pos + len(name):pos + len(name) + 40].lstrip()
    bare = re.sub(r'(?:\w+\s*::\s*)+$', '', pre.rstrip())        # drop A::B:: qualifier
    bare = re.sub(r'#\s*\w+.*\n', '', bare).strip()
    words = re.findall(r'\w+', bare)
    if bare and TYPEISH.match(bare) and words and words[-1] not in KEYWORDS \
            and not any(w in ('return', 'case', 'throw', 'else', 'delete', 'new') for w in words):
        return True                            # `type Name(` / `type Name =` / `type Name;`
    if not bare and re.match(r'=(?!=)', after):
        return True                            # `Name = ...;`  (a write, not a use)
    if re.match(r'^\s*(?:template\s*<[^>]*>\s*)?$', bare) and after.startswith('(') \
            and re.search(r'::\s*$', pre):
        # `A::B(...)` at statement start: out-of-class definition with ctor-like shape
        s2 = s[pos:pos + 400]
        m = re.match(r'\w+\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?::[^;{]*)?\{', s2)
        if m:
            return True
    return False


_live_cache = {}
REG_SPANS = {}
REG_TARGETS = set()   # short names of every registration target (filled after parsing)


def uses(name, exclude_span=None, only_file=None):
    """Code references of an identifier that are neither its declaration/definition nor
    a plain write, nor the key of a Register*() call. exclude_span = (file, lo, hi) of
    the defining block itself; only_file limits the count to one translation unit."""
    n = 0
    files = set()
    for p, pos in TOK.get(name, ()):
        if only_file and p != only_file:
            continue
        if exclude_span and p == exclude_span[0] and exclude_span[1] <= pos <= exclude_span[2]:
            continue
        if any(a <= pos <= b for a, b in REG_SPANS.get(p, ())):
            continue
        if is_decl_or_write(p, pos, name):
            continue
        n += 1
        files.add(p)
    return n, files


def func_live(p, name, open_pos):
    if not name:
        return True, 'no-name'
    short = re.split(r'::', name)[-1]
    if short.startswith('~') or short in ('main', 'WinMain', 'wWinMain', 'DllMain'):
        return True, 'dtor/main'
    key = (p, name, open_pos)
    if key in _live_cache:
        return _live_cache[key]
    close = next((c for o, c, k, nm in BLOCKS[p] if o == open_pos), open_pos)
    s = NOSTR[p]
    hdr = open_pos - len(stmt_prefix(s, open_pos))          # the definition's own header
    n, fs = uses(short, (p, hdr, close))
    r = (n > 0, f'{n} uses')
    if n == 0 and '::' in name:
        # a member reached only through its vtable: declared virtual / override
        rx = re.compile(r'\bvirtual\b[^;{}]*\b' + re.escape(short) + r'\s*\(|\b' + re.escape(short)
                        + r'\s*\([^;{}]*\)[^;{}]*\b(?:override|final)\b')
        if any(rx.search(t) for t in NOSTR.values()):
            r = (True, 'virtual (0 by-name uses)')
    if n == 0 and r[0] is False and short in REG_TARGETS:
        # a door body reached ONLY through its own registration: as live as that row
        r = (None, 'only via its row -> ' + short)
    _live_cache[key] = r
    return r


# ---------------------------------------------------------------------------
# Registrations
# ---------------------------------------------------------------------------
REG = re.compile(r'\bRegisterRuntime(Export|Addr)\s*\(')


def call_args(s, i):
    depth = 0
    j = i
    args, cur = [], []
    st = None
    while j < len(s):
        c = s[j]
        if st:
            cur.append(c)
            if c == '\\':
                cur.append(s[j + 1])
                j += 2
                continue
            if c == st:
                st = None
            j += 1
            continue
        if c == '"':
            st = c
            cur.append(c)
        elif c in '([{':
            depth += 1
            if depth > 1:
                cur.append(c)
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                args.append(''.join(cur).strip())
                return args, j
            cur.append(c)
        elif c == ',' and depth == 1:
            args.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(c)
        j += 1
    return args, j


def lit_value(expr):
    lits = re.findall(r'"((?:\\.|[^"\\])*)"', expr)
    if lits and re.fullmatch(r'\s*(?:"(?:\\.|[^"\\])*"\s*)+', expr):
        return ''.join(lits)
    return None


def file_consts(p):
    s = SRC[p]
    d = {}
    for m in re.finditer(r'\b(\w+)\s*(?:\[\s*\])?\s*=\s*((?:"(?:\\.|[^"\\])*"\s*)+)[;,]', s):
        d[m.group(1)] = ''.join(re.findall(r'"((?:\\.|[^"\\])*)"', m.group(2)))
    return d


def file_table_pairs(p):
    """{addr, "name"} rows of the file's tables, and {"name", addr, ...} rows of the
    generated .inc it includes (the mint's spelling)."""
    pairs = [(int(a, 16), n) for a, n in
             re.findall(r'\{\s*(0x18[0-9a-fA-F]{7})(?:ull|ULL|u|U)?\s*,\s*"([^"]+)"', SRC[p])]
    for inc in re.findall(r'#\s*include\s+"([^"]+\.inc)"', open(p, encoding='utf-8', errors='replace').read()):
        q = os.path.join(os.path.dirname(p), inc).replace('\\', '/')
        if q in SRC and q != 'standalone/rt_export_addrs.inc':
            pairs += [(int(a, 16), n) for n, a in
                      re.findall(r'\{\s*"([^"]+)"\s*,\s*(0x18[0-9a-fA-F]{7})(?:ull|ULL)?', SRC[q])]
    return pairs


def target_of(expr):
    t = re.sub(r'\s+', ' ', expr).strip()
    t = re.sub(r'^\(\s*void\s*\*\s*\)\s*', '', t)
    m = re.match(r'^reinterpret_cast\s*<\s*void\s*\*\s*>\s*\((.*)\)$', t)
    if m:
        t = m.group(1).strip()
    t = re.sub(r'^\(\s*void\s*\*\s*\)\s*', '', t)
    return t


rows = []   # dicts
for p in FILES:
    s = SRC[p]
    consts = None
    for m in REG.finditer(s):
        pre = s[max(0, m.start() - 40):m.start()]
        if re.search(r'\b(void|int)\s*\*?\s*$', pre):
            continue   # declaration / definition
        door = 'E' if m.group(1) == 'Export' else 'A'
        args, end = call_args(s, m.end() - 1)
        line = LIDX[p].line(m.start())
        tgt = target_of(args[1]) if len(args) > 1 else '?'
        keys = []
        k = args[0]
        if door == 'E':
            v = lit_value(k)
            if v is None and re.fullmatch(r'\w+', k):
                consts = consts if consts is not None else file_consts(p)
                v = consts.get(k)
                if v is not None:
                    keys.append(('name', v, 'const ' + k))
            elif v is not None:
                keys.append(('name', v, 'literal'))
            if v is None and re.fullmatch(r'\w+\.\w+', k):
                for a, n in file_table_pairs(p):
                    keys.append(('name', n, 'table ' + k))
            elif v is None:
                keys.append(('name', None, 'expr ' + k))
        else:
            mm = re.fullmatch(r'\s*(?:\(\s*uintptr_t\s*\)\s*)?(0x[0-9a-fA-F]+)(?:ull|ULL|u|U|ll|LL)?\s*', k)
            if mm:
                keys.append(('addr', int(mm.group(1), 16), 'literal'))
            elif re.fullmatch(r'(?:\(\s*uintptr_t\s*\)\s*)?\w+\.\w+', k):
                for a, n in file_table_pairs(p):
                    keys.append(('addr', a, 'table ' + k))
            else:
                keys.append(('addr', None, 'expr ' + k))
        ekind, ename, eopen = enclosing(p, m.start())
        for kind, val, how in keys:
            r = dict(door=door, file=p, line=line, target=tgt, how=how,
                     regfn=QUALNAME.get((p, eopen), ename), regnode=(p, eopen))
            if kind == 'name':
                r['name'] = val
                r['addr'] = NAME2ADDR.get(val) if val else None
            else:
                r['addr'] = val
                r['name'] = ADDR2NAME.get(val) if val else None
            rows.append(r)

# ---------------------------------------------------------------------------
# The exe import slot table
# ---------------------------------------------------------------------------
spec = importlib.util.spec_from_file_location('dsm', os.path.join(ROOT, 'tools/dispatch_slot_map.py'))
dsm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dsm)
SLOT2NAME = {a: nm for a, (dll, nm) in dsm.slotmap.items() if dll.lower().startswith('runtime')}
NAME2SLOTS = collections.defaultdict(list)
for a, nm in SLOT2NAME.items():
    NAME2SLOTS[nm].append(a)

# ---------------------------------------------------------------------------
# Reader sites
# ---------------------------------------------------------------------------
reg_names = {r['name'] for r in rows if r['name']}
reg_addrs = {r['addr'] for r in rows if r['addr']}

# spans that ARE registrations (their literals are keys, not readers)
reg_spans = collections.defaultdict(list)
table_files = set()
for p in FILES:
    s = SRC[p]
    for m in REG.finditer(s):
        pre = s[max(0, m.start() - 40):m.start()]
        if re.search(r'\b(void|int)\s*\*?\s*$', pre):
            continue
        args, end = call_args(s, m.end() - 1)
        reg_spans[p].append((m.start(), end))
        if args and re.fullmatch(r'\w+\.\w+', args[0]):
            table_files.add(p)
REG_SPANS = reg_spans
REG_TARGETS.update(re.sub(r'^&', '', r['target']).split('::')[-1] for r in rows)
# RegisterDispatchSlot(0x1401..., ...): the slot literal is a registration of the EXE
# slot, not a read of it; the ResolveExport inside it IS a reader of the runtime row.
disp_spans = collections.defaultdict(list)
SLOTREG = r'\b(?:RegisterDispatchSlot|RegisterLane3Slot|RegisterPlacePlayerSlot|RegisterExeAddr)\s*\(\s*(0x14015[12][0-9a-fA-F]{3})\b'
DISP_FILLED = {}
SLOT_DIRECT = {}   # slot -> fill expression, for slots filled with OUR body, not through the registry
for p in FILES:
    s = SRC[p]
    for m in re.finditer(SLOTREG, s):
        if re.search(r'\b(void)\s*$', s[max(0, m.start() - 20):m.start()]):
            continue
        disp_spans[p].append((m.start(1), m.end(1)))
        DISP_FILLED[int(m.group(1), 16)] = (p, LIDX[p].line(m.start()))
        if 'ResolveExport' not in s[m.end():s.find(';', m.end())]:
            SLOT_DIRECT[int(m.group(1), 16)] = re.sub(r'\s+', ' ', s[m.end():s.find(';', m.end())]).strip(' ,)')

DOORS = [
    ('RT_EXPORT_ONCE', r'RT_EXPORT_ONCE\s*\('), ('RT_DATA_SLOT', r'RT_DATA_SLOT\s*\('),
    ('RtResolveCached', r'RtResolveCached\s*\('), ('RtPortLookupExport', r'RtPortLookupExport\s*\('),
    ('RtPortLookupAddr', r'RtPortLookupAddr\s*\('), ('RuntimeExportIsRegistered', r'RuntimeExportIsRegistered\s*\('),
    ('ResolveExport', r'ResolveExport\s*\('), ('ResolveAddr(Runtime)', r'ResolveAddr\s*\(\s*Module::Runtime'),
    ('ResolveAddr(Exe)', r'ResolveAddr\s*\(\s*Module::Exe'), ('RtAddr<>', r'\bRtAddr\s*<'),
    ('RtAddrFn', r'\bRtAddrFn\b'), ('ArrayDtor', r'\bArrayDtor\s*\('), ('PlainArrayDtor', r'PlainArrayDtor\s*\('),
    ('ArraySetSize', r'ArraySetSize\s*\('), ('SetSizeWithFill', r'SetSizeWithFill\s*\('),
    ('ByAddr', r'\bByAddr\s*\('), ('AtomicExport', r'AtomicExport\s*\('), ('AtomicAddr', r'AtomicAddr\s*\('),
    ('RSlot', r'\bRSlot\b'), ('ResolveSlotPtr', r'ResolveSlotPtr\b'), ('ResolveSlot', r'\bResolveSlot\s*\('),
    ('RegisterDispatchSlot', r'RegisterDispatchSlot\s*\('),
]


def door_of(p, pos):
    """The nearest door token before pos within the same statement; else the binding shape."""
    s = NOSTR[p]
    pre = stmt_prefix(s, pos)
    # a table row `{ ..., lit }` inside an initializer
    best = None
    for nm, rx in DOORS:
        for m in re.finditer(rx, pre):
            if best is None or m.start() > best[0]:
                best = (m.start(), nm)
    if best:
        return best[1]
    # function-call wrappers: nearest `ident(` / `ident<...>(` whose paren is still open
    depth = 0
    j = len(pre) - 1
    while j >= 0:
        c = pre[j]
        if c == ')':
            depth += 1
        elif c == '(':
            if depth == 0:
                m = re.search(r'([A-Za-z_][\w:]*)\s*(?:<[^()]*>)?\s*$', pre[:j])
                if m and m.group(1) not in ('reinterpret_cast', 'static_cast', 'const_cast', 'uintptr_t',
                                            'if', 'while', 'return', 'sizeof'):
                    return 'call:' + m.group(1)
            else:
                depth -= 1
        j -= 1
    ekind, ename, _ = enclosing(p, pos)
    if ekind == 'init':
        return 'table:' + ename
    m = re.search(r'(\w+)\s*(?:\[[^\]]*\])?\s*=\s*[^=]*$', pre)
    if m:
        return 'bind:' + m.group(1)
    if re.search(r'\bcase\s*$', pre):
        return 'case'
    line = s[s.rfind(chr(10), 0, pos) + 1:pos]
    if line.lstrip().startswith('{'):
        return 'table-row:' + (ename or '?')       # a row of a local table walked by a loop
    return 'other'


def binding_of(p, pos):
    """(kind, name) of the named thing a reader site feeds: a namespace-scope or static
    variable / constant / table assigned from it, or the enclosing function."""
    s = NOSTR[p]
    ekind, ename, eopen = enclosing(p, pos)
    if ekind == 'init':
        return 'table', ename, eopen
    pre = stmt_prefix(s, pos)
    m = re.match(r'^\s*(?:static\s+|inline\s+|constexpr\s+|const\s+|extern\s+)*[\w:<>\*&\s,]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*=[^=]', pre)
    if ekind == 'file' and m:
        return 'global', m.group(1), 0
    if ekind == 'func':
        m2 = re.match(r'^\s*(?:(?:this\s*->\s*)?)(\w+)\s*=[^=]', pre)   # write to an outer name
        if m2 and not re.match(r'^\s*(auto|const|static)\b', pre):
            nm = m2.group(1)
            # a local declared earlier in the same function stays with the function
            fsrc = s[eopen:pos]
            if not re.search(r'[\w>\*&]\s+\*?' + re.escape(nm) + r'\s*[=;]', fsrc):
                return 'var', nm, eopen
        m3 = re.match(r'^\s*static\s+[\w:<>\*&\s,]*?\b(\w+)\s*=', pre)
        if m3:
            return 'fstatic', m3.group(1), eopen
    return ekind, ename, eopen


def in_spans(spans, pos):
    return any(a <= pos <= b for a, b in spans)


readers = collections.defaultdict(list)   # ('name', v) / ('addr', v) / ('slot', v) -> sites
unknown_doors = collections.Counter()
for p in FILES:
    if p in GENERATED:
        continue
    s = SRC[p]
    # string literals, adjacent ones concatenated
    for m in re.finditer(r'(?:"(?:\\.|[^"\\\n])*"\s*)+', s):
        val = ''.join(re.findall(r'"((?:\\.|[^"\\\n])*)"', m.group(0)))
        if not val.startswith('?') or (val not in reg_names and val not in NAME2ADDR):
            continue
        if in_spans(reg_spans[p], m.start()) or p in table_files and re.match(r'\s*\}', s[m.end():m.end() + 3]) \
                and re.search(r'\{\s*0x18[0-9a-fA-F]{7}\w*\s*,\s*$', s[max(0, m.start() - 40):m.start()]):
            continue
        readers[('name', val)].append((p, m.start()))
    for m in re.finditer(r'\b0x(1[48][0-9a-fA-F]{7})(?:ull|ULL|u|U|ll|LL)?\b', NOSTR[p]):
        v = int(m.group(1), 16)
        if in_spans(reg_spans[p], m.start()):
            continue
        if 0x180000000 <= v < 0x190000000:
            # VideoDX11-x64.dll shares the 0x180000000 preferred base: a literal under
            # dx11/ is a VideoDX11 address unless it goes through a Runtime door.
            if p.startswith('dx11/') and 'Module::Runtime' not in stmt_prefix(NOSTR[p], m.start()):
                continue
            if v in reg_addrs or v in ADDR2NAME:
                if p in table_files and re.match(r'\w*\s*,\s*"', s[m.end():m.end() + 6]):
                    continue
                readers[('addr', v)].append((p, m.start()))
        elif SLOT_LO <= v <= SLOT_HI:
            if in_spans(disp_spans[p], m.start()):
                continue
            readers[('slot', v)].append((p, m.start()))

# A literal at file scope that feeds no binding is a macro's provenance argument
# (RT_SER_SCALAR / RT_PKG2_SCALAR_GETTER take the mangled name and never use it):
# not a reader.
NOT_READERS = []
for k in list(readers):
    keep = []
    for p, pos in readers[k]:
        if binding_of(p, pos)[0] == 'file':
            NOT_READERS.append((p, LIDX[p].line(pos), k))
            continue
        keep.append((p, pos))
    readers[k] = keep

# ---------------------------------------------------------------------------
# Per-site info, liveness
# ---------------------------------------------------------------------------
SITE = {}
SITE_DEP = {}   # (file, pos) -> the door body whose row decides this site's liveness


def site_info(p, pos, key):
    k = (p, pos)
    if k in SITE:
        return SITE[k]
    door = door_of(p, pos)
    bkind, bname, bopen = binding_of(p, pos)
    live, why = True, ''
    mm = re.search(SLOTREG, stmt_prefix(NOSTR[p], pos))
    if mm and key[0] == 'name':
        # a name resolved INTO an exe import slot: live iff some code reads that slot
        door = 'slotfill:' + door
        slot = int(mm.group(1), 16) if mm else None
        sr = [site_info(pp, ps, ('slot', slot)) for pp, ps in readers.get(('slot', slot), [])] if slot else []
        n = sum(x['live'] for x in sr)
        live, why = n > 0, f'slot 0x{slot:x} has {n}/{len(sr)} live readers' if slot else 'slot?'
    elif bkind in ('table', 'global', 'var', 'fstatic'):
        local = p.endswith(('.cpp', '.c')) and bkind in ('table', 'global', 'fstatic') and not any(
            re.search(r'\bextern\b[^;]*\b' + re.escape(bname) + r'\s*;', NOSTR[q]) for q, _ in TOK.get(bname, ())[:50])
        n, fs = uses(bname, (p, pos - 2000, pos + 2000) if bkind == 'table' else None,
                     only_file=p if local else None)
        nsb = [(o, c, nm) for o, c, k, nm in BLOCKS[p] if k == 'ns' and nm and o < pos < c]
        if bkind == 'global' and p.endswith(('.h', '.hpp')) and nsb:
            # a constant in a header namespace (EngineAddr::Startup): count the QUALIFIED
            # uses, plus bare uses inside that namespace block
            o, c, nsname = max(nsb)
            last = nsname.split('::')[-1]
            rxq = re.compile(r'\b' + re.escape(last) + r'\s*::\s*' + re.escape(bname) + r'\b')
            n = sum(len(rxq.findall(t)) for t in NOSTR.values())
            n += sum(1 for q, ps in TOK.get(bname, ()) if q == p and o < ps < c and ps != pos
                     and not is_decl_or_write(q, ps, bname))
        live, why = n > 0, f'{bkind} {bname}: {n} uses'
        if bkind == 'fstatic' and live is False:
            pass
    elif bkind == 'func':
        live, why = func_live(p, bname, bopen)
        why = f'func {bname}: {why}'
        if live is None:
            SITE_DEP[(p, pos)] = why.rsplit('-> ', 1)[-1]
    else:
        why = bkind
    lineno = LIDX[p].line(pos)
    SITE[k] = dict(file=p, line=lineno, door=door, bkind=bkind, bname=bname, live=live, why=why,
                   pos_file=p, pos=pos)
    if door.startswith(('call:', 'bind:', 'table:', 'other', 'case')):
        unknown_doors[door.split(':')[0] + ':' + door.split(':', 1)[-1] if ':' in door else door] += 1
    return SITE[k]


# ---------------------------------------------------------------------------
# Targets: definitions and kinds
# ---------------------------------------------------------------------------
DEFS = collections.defaultdict(list)   # short name -> [(file, qualname, params, enclosing type)]
for p in FILES:
    for o, c, kind, name in BLOCKS[p]:
        if kind == 'func' and name:
            s = NOSTR[p]
            hdr = s[max(0, o - 600):o]
            mm = re.search(re.escape(name.split('::')[-1]) + r'\s*\(([^{};]*)\)\s*[^{};()]*$', hdr)
            params = re.sub(r'\s+', ' ', mm.group(1)).strip() if mm else '?'
            DEFS[name.split('::')[-1]].append((p, name, params, enclosing_type(p, o)))
CLASSNAMES = set()
for p in FILES:
    for m in re.finditer(r'\b(?:class|struct)\s+(\w+)\s*(?:final\s*)?[:{]', NOSTR[p]):
        CLASSNAMES.add(m.group(1))

STUBRX = re.compile(r'NOT IMPLEMENTED|PORT GAP|NotImplemented|RtStub|UNPORTED', re.I)


def member_kind(n):
    """MSVC access/storage code after the scope's '@@': instance / virtual / static /
    global / data, or '' for an unnamed (internal) row."""
    if not n or not n.startswith('?'):
        return ''

    def qname(i):          # a scope: name@ / digit backref / template, ended by '@'
        while i < len(n):
            if n[i] == '@':
                return i + 1
            if n[i].isdigit():
                i += 1
            elif n.startswith('?$', i):
                i = n.index('@', i) + 1
                while i < len(n) and n[i] != '@':
                    i = typ(i)
                i += 1
            else:
                i = n.index('@', i) + 1
        return i

    def typ(i):            # one template argument type
        c = n[i]
        if c in 'VUT':
            return qname(i + 1)
        if c == 'W':
            return qname(i + 2)
        if c in 'PQAR':
            j = i + 1
            while j < len(n) and n[j] in 'EI':
                j += 1
            return typ(j + 1)
        if c == '_' or c == '$':
            return i + 2 if c == '_' else i + 3
        return i + 1

    try:
        if n.startswith('??_') or n.startswith('??$'):
            return ''
        i = 3 if n.startswith('??') else n.index('@') + 1
        i = qname(i)
        c = n[i]
    except (ValueError, IndexError):
        return ''
    if c in 'ABIJQR':
        return 'instance'
    if c in 'EFMNUV':
        return 'virtual'
    if c in 'CDKLST':
        return 'static'
    if c in 'YZ':
        return 'global'
    return 'data'


def target_kind(t, regfile, mangled=None):
    if not t.startswith('&'):
        if t.endswith('.storage'):
            return 'storage (mint cell)', ''
        return 'param/expr', ''
    q = t[1:].strip()
    q = re.sub(r'<.*>', '', q)
    parts = [x for x in q.split('::') if x]
    short = parts[-1] if parts else q
    defs = DEFS.get(short, [])
    params = ''
    body_stub = False
    # prefer a definition in the registering file, then any whose qualname matches
    pick = None
    for d in defs:
        if d[0] == regfile and (len(parts) == 1 or d[1].endswith('::'.join(parts[-2:])) or d[3] == parts[-2]):
            pick = d
            break
    if not pick:
        for d in defs:
            if len(parts) == 1 or d[1].endswith('::'.join(parts[-2:])) or (len(parts) > 1 and d[3] == parts[-2]):
                pick = d
                break
    if pick:
        params = pick[2]
        p = pick[0]
        o = next((o for o, c, k, nm in BLOCKS[p] if k == 'func' and nm == pick[1]), None)
        if o is not None:
            c = next(c for oo, c, k, nm in BLOCKS[p] if oo == o)
            body = SRC[p][o:c]
            if STUBRX.search(body) and len(body) < 1500:
                body_stub = True
    if len(parts) >= 2 and (parts[-2] in CLASSNAMES or (pick and pick[3])):
        kind = 'static member'
    elif len(parts) >= 2:
        kind = 'namespace fn'
    elif pick and pick[3]:
        kind = 'static member (unqualified)'
    else:
        mk = member_kind(mangled)
        voidself = re.match(r'^(?:const\s+)?void\s*\*', params) is not None
        if mk in ('instance', 'virtual'):
            kind = 'C-door, void* self' if voidself else 'C-door, typed self'
        elif mk in ('static', 'global'):
            kind = 'free fn for a ' + mk
        elif mk == 'data':
            kind = 'free fn for DATA?'
        else:
            kind = 'free fn (internal body)'
    if body_stub:
        kind += ' [stub-marked]'
    return kind, params


# ---------------------------------------------------------------------------
# Emit
# ---------------------------------------------------------------------------
def fmt_addr(a):
    return f'0x{a:x}' if a else ''


def qualified_from_mangled(n):
    """?Member@Class@Ns@@... -> Ns::Class::Member (backrefs 0-9 resolved)."""
    if not n or not n.startswith('?'):
        return ''
    special = {'?0': '<ctor>', '?1': '~<dtor>', '?2': 'operator new', '?3': 'operator delete',
               '?4': 'operator=', '?8': 'operator==', '?9': 'operator!=', '?A': 'operator[]',
               '?D': 'operator*', '?G': 'operator-', '?H': 'operator+', '?X': 'operator*=',
               '?Y': 'operator+=', '?Z': 'operator-=', '?K': 'operator/', '?_7': 'vftable'}
    body = n[1:]
    op = None
    for k in sorted(special, key=len, reverse=True):
        if body.startswith(k):
            op = special[k]
            body = body[len(k):]
            break
    segs = []
    i = 0
    if op is None:
        m = re.match(r'([^@]+)@', body)
        if not m:
            return ''
        segs.append(m.group(1))
        i = m.end()
    names = []
    while i < len(body) and body[i] != '@':
        if body[i].isdigit():
            ref = int(body[i])
            allnames = segs + names
            names.append(allnames[ref] if ref < len(allnames) else '?')
            i += 1
            continue
        if body.startswith('?$', i):
            j = body.find('@', i)
            names.append(body[i + 2:j] + '<>')
            k = body.find('@@', j)
            i = (k + 2) if k >= 0 else len(body)
            continue
        j = body.find('@', i)
        if j < 0:
            break
        names.append(body[i:j])
        i = j + 1
    scope = list(reversed(names))
    if op:
        cls = scope[-1] if scope else ''
        member = op.replace('<ctor>', cls).replace('<dtor>', cls)
    else:
        member = segs[0]
    return '::'.join(scope + [member])


HDR_DIRS = ['runtime/types', 'runtime/funcs']
HDRTXT = {}
for p in FILES:
    if any(p.startswith(d + '/') for d in HDR_DIRS):
        HDRTXT[p] = NOSTR[p]


def header_declares(qual):
    """Is the member / function named in some runtime/types or runtime/funcs header?"""
    if not qual:
        return ''
    parts = qual.split('::')
    member = parts[-1]
    cls = parts[-2] if len(parts) > 1 else ''
    if member.startswith('operator') or member.startswith('~'):
        rx = re.compile(re.escape(member).replace('\\ ', r'\s*') + r'\s*\(')
    else:
        rx = re.compile(r'\b' + re.escape(member) + r'\s*\(')
    for p, s in HDRTXT.items():
        if not rx.search(s):
            continue
        if cls and not re.search(r'\b(?:class|struct)\s+' + re.escape(cls) + r'\b', s):
            continue
        return p
    return ''


# Rows whose live reader is an address or a name held in DATA and resolved later.
# Filled from the hand reading recorded in INVENTORY_A_RUNTIME_ROWS.md section 4.
CALLBACK_ROWS = set()

# Fixpoint: a site inside a door body that nothing calls by name is live iff one of
# the rows registering that body is live.
ROWSITES = []
for r in rows:
    nm, ad = r['name'], r['addr']
    sites = [(p, pos, ('name', nm)) for p, pos in readers.get(('name', nm), [])] if nm else []
    sites += [(p, pos, ('addr', ad)) for p, pos in readers.get(('addr', ad), [])] if ad else []
    for sl in [x for x in NAME2SLOTS.get(nm, []) if x in DISP_FILLED and x not in SLOT_DIRECT] if nm else []:
        sites += [(p, pos, ('slot', sl)) for p, pos in readers.get(('slot', sl), [])]
    ROWSITES.append([site_info(p, pos, k) for p, pos, k in sites])
live_targets = set()
while True:
    new = set()
    for r, ss in zip(rows, ROWSITES):
        if any(x['live'] is True or (x['live'] is None and SITE_DEP.get((x['pos_file'], x['pos'])) in live_targets)
               for x in ss):
            new.add(re.sub(r'^&', '', r['target']).split('::')[-1])
    if new == live_targets:
        break
    live_targets = new
for x in SITE.values():
    if x['live'] is None:
        x['live'] = SITE_DEP.get((x['pos_file'], x['pos'])) in live_targets
        x['why'] += ' (live)' if x['live'] else ' (that row is dead)'

out = []
dup_name = collections.Counter(r['name'] for r in rows if r['name'] and r['door'] == 'E')
dup_addr = collections.Counter(r['addr'] for r in rows if r['addr'] and r['door'] == 'A')
dup_key_door = collections.Counter((r['door'], r['name'] if r['door'] == 'E' else r['addr']) for r in rows)
stats = collections.Counter()
for r in rows:
    nm, ad = r['name'], r['addr']
    by_name = [site_info(p, pos, ('name', nm)) for p, pos in readers.get(('name', nm), [])] if nm else []
    by_addr = [site_info(p, pos, ('addr', ad)) for p, pos in readers.get(('addr', ad), [])] if ad else []
    by_slot = []
    for sl in [x for x in NAME2SLOTS.get(nm, []) if x in DISP_FILLED and x not in SLOT_DIRECT] if nm else []:
        by_slot += [site_info(p, pos, ('slot', sl)) for p, pos in readers.get(('slot', sl), [])]
    live = [x for x in by_name + by_addr + by_slot if x['live']]
    tkind, params = target_kind(r['target'], r['file'], nm)
    qual = qualified_from_mangled(nm) if nm else ''
    # class
    if r['target'].endswith('.storage') or (nm and re.match(r'\?_(singleton|disableHandler|typeInfo|firstTypeInfo)', nm)):
        cls = 'STORAGE'
    elif 'stub-marked' in tkind:
        cls = 'STUB'
    elif not live:
        cls = 'DEAD'
    else:
        cls = 'DIRECT'
    # CALLBACK is assigned by READING (see CALLBACK_ROWS), never by a door shape.
    if cls == 'DIRECT' and (nm in CALLBACK_ROWS or ad in CALLBACK_ROWS):
        cls = 'CALLBACK'
    repl = ''
    if cls == 'DIRECT':
        hd = header_declares(qual) if qual else ''
        body = r['target'][1:] if r['target'].startswith('&') else r['target']
        if tkind.startswith('static member') or tkind == 'namespace fn':
            repl = body + ('  [decl: ' + hd + ']' if hd else '  [no header decl]')
        elif tkind.startswith('free fn (internal'):
            repl = body + '  [internal body, no retail name: call it directly]'
        else:
            repl = (qual or body) + ('  [decl: ' + hd + ']' if hd else '  [no header decl]') \
                + ('  (today via C-door ' + body + ')' if tkind.startswith('C-door') else '')
    rf = []
    for x in by_name + by_addr + by_slot:
        tag = f"{x['file']}:{x['line']}" + ('' if x['live'] else '(dead)')
        if tag not in rf:
            rf.append(tag)
    dups = []
    if nm and dup_name[nm] > 1:
        dups.append(f'name x{dup_name[nm]}')
    if ad and dup_addr[ad] > 1:
        dups.append(f'addr x{dup_addr[ad]}')
    notes = []
    if r['door'] == 'E' and nm and nm not in NAME2ADDR:
        notes.append('name not in rt_export_addrs.inc')
    if r['door'] == 'A' and ad and ad not in ADDR2NAME:
        notes.append('internal (not an export)')
    doors = collections.Counter(x['door'] for x in by_name + by_addr + by_slot)
    out.append([
        nm or '', fmt_addr(ad), r['file'], str(r['line']), r['target'], tkind,
        f"{len(by_name)}/{sum(x['live'] for x in by_name)}",
        f"{len(by_addr)}/{sum(x['live'] for x in by_addr)}",
        f"{len(by_slot)}/{sum(x['live'] for x in by_slot)}",
        ';'.join(rf[:8]) + (f';+{len(rf) - 8}' if len(rf) > 8 else ''),
        cls, repl,
        r['door'], r['how'], r['regfn'], ','.join(dups), ','.join(notes),
        ','.join(f'{k}x{v}' for k, v in doors.most_common()), params[:80],
    ])
    stats[cls] += 1

HDR = ['key_name', 'key_addr', 'reg_file', 'reg_line', 'target', 'target_kind',
       'readers_by_name', 'readers_by_addr', 'readers_by_exe_slot', 'reader_files', 'class',
       'direct_replacement', 'door', 'key_spelling', 'reg_function', 'registered_more_than_once',
       'notes', 'reader_doors', 'target_params']
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\t'.join(HDR) + '\n')
    for o in out:
        f.write('\t'.join(x.replace('\t', ' ') for x in o) + '\n')

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
print(f'registrations: {len(rows)} (E {sum(r["door"] == "E" for r in rows)}, A {sum(r["door"] == "A" for r in rows)})')
print('class:', dict(stats))
bydir = collections.Counter()
for o in out:
    d = '/'.join(o[2].split('/')[:3]) if o[2].startswith('runtime/src') else o[2].split('/')[0]
    bydir[(d, o[10])] += 1
dirs = sorted({d for d, c in bydir})
print('per-dir:')
for d in dirs:
    print('  ', d, {c: bydir[(d, c)] for c in ('DEAD', 'DIRECT', 'STORAGE', 'STUB', 'CALLBACK') if bydir[(d, c)]})
print('target kinds:', collections.Counter(o[5] for o in out).most_common())
print('unknown door spellings:', unknown_doors.most_common(60))
print('distinct names:', len({r['name'] for r in rows if r['name']}), 'distinct addrs:', len({r['addr'] for r in rows if r['addr']}))
print('names not in inc:', sorted({r['name'] for r in rows if r['door'] == 'E' and r['name'] and r['name'] not in NAME2ADDR}))
print('internal addrs (not exports):', len({r['addr'] for r in rows if r['door'] == 'A' and r['addr'] and r['addr'] not in ADDR2NAME}))
print('unresolved keys:', [(r['file'], r['line'], r['how']) for r in rows if not r['name'] and not r['addr']])
print('same key+door registered >1:', sorted(((k, v) for k, v in dup_key_door.items() if v > 1), key=lambda x: -x[1])[:40])

# ---------------------------------------------------------------------------
# Reader census (every reader site of a registered row, one line each)
# ---------------------------------------------------------------------------
slot_sites = sum(len(v) for k, v in readers.items() if k[0] == 'slot')
slot_rt = sum(len(v) for k, v in readers.items() if k[0] == 'slot' and k[1] in SLOT2NAME)
slot_reg = sum(len(v) for k, v in readers.items() if k[0] == 'slot' and SLOT2NAME.get(k[1]) in reg_names)
print(f'exe import-slot literal sites: {slot_sites}; naming a Runtime import: {slot_rt}; '
      f'whose import is a registered row name: {slot_reg}')
print(f'slots filled by a Register*Slot call: {len(DISP_FILLED)}')
live_sites = [v for v in SITE.values()]
print(f'reader sites of registered rows: {len(live_sites)}, live {sum(v["live"] for v in live_sites)}')
if '--sites' in sys.argv:
    for v in sorted(live_sites, key=lambda v: (v['file'], v['line'])):
        print('SITE', v['file'], v['line'], v['door'], v['bkind'], v['bname'], v['live'], v['why'], sep='\t')

# ---------------------------------------------------------------------------
# *_PortInit attribution: which call of RuntimePortInit() reaches each registering
# function. Call graph over function bodies, by qualified name, following only
# callees named like a registration function (Register / PortInit / Init); first
# claimant in RuntimePortInit()'s call order wins.
# ---------------------------------------------------------------------------
NODES = collections.defaultdict(list)   # short name -> [(file, open, qualname)]
for (p, o), q in QUALNAME.items():
    NODES[q.split('::')[-1]].append((p, o, q))
REGISH = re.compile(r'Register|PortInit|Init$|Init[A-Z]')


def callees(node):
    p, o = node
    c = next(cc for oo, cc, k, nm in BLOCKS[p] if oo == o)
    me = QUALNAME.get(node, '')
    myns = me.rsplit('::', 1)[0] if '::' in me else ''
    out = set()
    for m in re.finditer(r'(?<![\w.>])((?:\w+::)*[A-Za-z_]\w*)\s*\(', NOSTR[p][o:c]):
        txt = m.group(1)
        short = txt.split('::')[-1]
        if not REGISH.search(short) or short in ('RegisterRuntimeExport', 'RegisterRuntimeAddr'):
            continue
        cands = [(pp, oo) for pp, oo, q in NODES.get(short, ()) if q == txt or q.endswith('::' + txt)]
        if len(cands) > 1 and '::' not in txt:
            same = [(pp, oo) for pp, oo in cands if QUALNAME[(pp, oo)].rsplit('::', 1)[0] == myns or pp == p]
            cands = same or cands
        out.update(cands)
    return out


rpi = next((o, c) for o, c, k, nm in BLOCKS['standalone/resolve_standalone.cpp'] if nm == 'RuntimePortInit')
roots = []
for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\(\s*\)\s*;', NOSTR['standalone/resolve_standalone.cpp'][rpi[0]:rpi[1]]):
    if m.group(1) not in roots and REGISH.search(m.group(1)) and m.group(1) != 'RtResolveMarkPortInitDone':
        roots.append(m.group(1))
owner = {}
for r0 in roots:
    todo = [(pp, oo) for pp, oo, q in NODES.get(r0, ())]
    seen = set(todo)
    while todo:
        n0 = todo.pop()
        owner.setdefault(n0, r0)
        for g in callees(n0):
            if g not in seen:
                seen.add(g)
                todo.append(g)
per_root = collections.Counter()
per_root_fn = collections.defaultdict(set)
unowned = collections.Counter()
for r in rows:
    o = owner.get(r['regnode'])
    r['root'] = o or ''
    if o:
        per_root[o] += 1
        per_root_fn[o].add(r['regfn'])
    else:
        unowned[(r['file'], r['regfn'])] += 1
print('rows per RuntimePortInit() call (in call order): rows / registering functions')
for r0 in roots:
    print(f'   {r0}: {per_root[r0]} / {len(per_root_fn[r0])}')
print('registering functions NOT reached from RuntimePortInit():')
for (f, fn), n in sorted(unowned.items()):
    callers = [(pp, LIDX[pp].line(pos)) for pp, pos in TOK.get(fn.split('::')[-1], ())
               if not is_decl_or_write(pp, pos, fn.split('::')[-1])]
    print(f'   {f} {fn}: {n} rows; name referenced at {callers[:4]}')

# ---------------------------------------------------------------------------
# Logical rows (a name and its mirrored address are ONE row), duplicates, conflicts
# ---------------------------------------------------------------------------
logical = collections.defaultdict(list)
for r, o in zip(rows, out):
    key = r['name'] or f"0x{r['addr']:x}"
    logical[key].append((r, o))
print(f'logical rows (name+mirrored address = one): {len(logical)}')
both = sum(1 for k, v in logical.items() if {r["door"] for r, o in v} == {'E', 'A'})
print(f'   registered through both doors: {both}; name only: '
      f'{sum(1 for k, v in logical.items() if {r["door"] for r, o in v} == {"E"})}; address only: '
      f'{sum(1 for k, v in logical.items() if {r["door"] for r, o in v} == {"A"})}')
lcls = collections.Counter()
for k, v in logical.items():
    lcls[v[0][1][10]] += 1
print('   logical rows per class:', dict(lcls))
conf = [(k, sorted({r['target'] for r, o in v})) for k, v in logical.items() if len({r['target'] for r, o in v}) > 1]
print(f'   rows whose registrations name DIFFERENT targets: {len(conf)}')
for k, ts in conf:
    print('      ', k, ts, [(r['file'], r['line'], r['door']) for r, o in logical[k]])
multi = [(k, len(v)) for k, v in logical.items() if len([1 for r, o in v if r['door'] == 'E']) > 1
         or len([1 for r, o in v if r['door'] == 'A']) > 1]
print(f'   rows registered more than once through the same door: {len(multi)}')
for k, n in multi:
    print('      ', k, [(r['file'], r['line'], r['door'], r['target']) for r, o in logical[k]])

# ---------------------------------------------------------------------------
# Block planning numbers: live reader sites of DIRECT rows by reader file / directory,
# and registration lines by registering file.
# ---------------------------------------------------------------------------
def area(f):
    parts = f.split('/')
    if parts[0] == 'runtime' and len(parts) > 2 and parts[1] == 'src':
        return 'runtime/src/' + parts[2]
    if parts[0] == 'runtime':
        return 'runtime/' + parts[1]
    if parts[0] == 'code' and len(parts) > 2:
        return 'code/Game/' + parts[2] if parts[1] == 'Game' else 'code/' + parts[1]
    return parts[0]


site_rows = collections.defaultdict(set)
for r, o, ss in zip(rows, out, ROWSITES):
    if o[10] != 'DIRECT':
        continue
    for x in ss:
        if x['live']:
            site_rows[(x['file'], x['line'], x['door'])].add(o[0] or o[1])
by_area = collections.defaultdict(lambda: [set(), 0, set()])
for (f, ln, door), keys in site_rows.items():
    a = by_area[area(f)]
    a[0].add(f)
    a[1] += 1
    a[2] |= keys
print('live reader sites of DIRECT rows, by reader area: files / sites / rows')
for a in sorted(by_area, key=lambda a: -by_area[a][1]):
    print(f'   {a}: {len(by_area[a][0])} / {by_area[a][1]} / {len(by_area[a][2])}')
doors = collections.Counter(d.split(':')[0] for (f, ln, d) in site_rows)
print('   by door:', doors.most_common())
reg_area = collections.defaultdict(lambda: collections.Counter())
reg_files = collections.defaultdict(lambda: collections.Counter())
for o in out:
    reg_area[area(o[2])][o[10]] += 1
    reg_files[o[2]][o[10]] += 1
print('registration lines by registering area (class counts), files:')
for a in sorted(reg_area):
    nf = len({f for f in reg_files if area(f) == a})
    print(f'   {a}: {dict(reg_area[a])}  files={nf}')
alldead = [f for f, c in reg_files.items() if set(c) == {'DEAD'}]
print(f'registering files whose every row is DEAD: {len(alldead)} of {len(reg_files)}')
print(f'file-scope literals that are not readers (macro provenance args): {len(NOT_READERS)}', sorted({p for p, l, k in NOT_READERS}))
dk = collections.Counter()
for k, v in logical.items():
    o = v[0][1]
    if o[10] == 'DIRECT':
        kinds = {x[1][5] for x in v}
        hd = any('[decl:' in x[1][11] for x in v)
        dk[(sorted(kinds)[0], 'header decl' if hd else 'no header decl')] += 1
print('DIRECT logical rows by target kind / header declaration:')
for (kd, hd), n in sorted(dk.items(), key=lambda x: -x[1]):
    print(f'   {kd} | {hd}: {n}')
lk = collections.Counter()
for k, v in logical.items():
    lk[(v[0][1][10], 'named' if v[0][0]['name'] else 'internal')] += 1
print('logical rows by class and named/internal:', dict(lk))
ls = collections.Counter()
for k, v in logical.items():
    o = v[0][1]
    ls[(o[10], 'exe-slot' if o[8] != '0/0' else '', 'by-name' if o[6] != '0/0' else '', 'by-addr' if o[7] != '0/0' else '')] += 1
print('logical rows by class and reader door kinds (any reader, live or not):')
for k2, n in sorted(ls.items(), key=lambda x: -x[1]):
    print('   ', k2, n)
print('exe import slots READ by code but filled by no Register*Slot call (resolve to 0 today;')
print('converting such a reader to a direct call is a BEHAVIOUR CHANGE):')
for sl in sorted(k[1] for k in readers if k[0] == 'slot'):
    if sl in DISP_FILLED:
        continue
    ss = [site_info(p, pos, ('slot', sl)) for p, pos in readers[('slot', sl)]]
    nm = SLOT2NAME.get(sl, '?')
    print(f'   0x{sl:x} {nm} registered-row={nm in reg_names} live={sum(x["live"] for x in ss)}/{len(ss)} '
          + ';'.join(f"{x['file']}:{x['line']}" for x in ss))
print('exe import slots filled DIRECTLY with our body (their readers bypass the registry, so they')
print('keep no Runtime row alive; they are still readers to convert):')
for sl, e in sorted(SLOT_DIRECT.items()):
    ss = readers.get(('slot', sl), [])
    print(f'   0x{sl:x} {SLOT2NAME.get(sl, "?")} <- {e}; {len(ss)} reader site(s): '
          + ';'.join(f'{p}:{LIDX[p].line(pos)}' for p, pos in ss))

# ---------------------------------------------------------------------------
# Proposed blocks (disjoint FILE sets): each owns its files' registrations AND readers.
# ---------------------------------------------------------------------------
BLOCKS_PLAN = [
    ('A1 ui', ['runtime/src/ui']),
    ('A2 video+graphics', ['runtime/src/video', 'runtime/src/graphics']),
    ('A3 io+save', ['runtime/src/io', 'runtime/src/save']),
    ('A4 runtime core', ['runtime/src/system', 'runtime/src/object', 'runtime/src/collection', 'runtime/src/crt',
                         'runtime/src/math', 'runtime/src/volume', 'runtime/src/engine', 'runtime/src/application',
                         'runtime/src/input', 'runtime/src/audio', 'runtime/src/achievement', 'runtime/src/picking',
                         'runtime/src/pathfinding', 'runtime/src/collision2d', 'runtime/src/utility', 'runtime/types',
                         'runtime/funcs']),
    ('A5 exe-side readers', ['code', 'engine', 'standalone', 'dx11']),
]


def block_of(f):
    for name, prefixes in BLOCKS_PLAN:
        if any(f == pf or f.startswith(pf + '/') for pf in prefixes):
            return name
    return '?'


bstat = collections.defaultdict(lambda: dict(files=set(), dead=0, direct=0, storage=0, sites=0, xrows=set()))
for r, o, ss in zip(rows, out, ROWSITES):
    b = block_of(r['file'])
    bs = bstat[b]
    if o[10] in ('DEAD', 'DIRECT', 'STORAGE'):
        bs[o[10].lower()] += 1
        bs['files'].add(r['file'])
    if o[10] == 'DIRECT':
        for x in ss:
            if x['live']:
                rb = block_of(x['file'])
                bstat[rb]['files'].add(x['file'])
                if rb != b:
                    bstat[rb]['xrows'].add(o[0] or o[1])
live_sites_by_block = collections.Counter(block_of(f) for (f, ln, d) in site_rows)
print('proposed blocks: registering+reader files / DEAD reg lines / DIRECT reg lines / live reader sites '
      '/ rows read here but registered in another block')
for name, _ in BLOCKS_PLAN:
    bs = bstat[name]
    print(f'   {name}: files={len(bs["files"])} dead={bs["dead"]} direct={bs["direct"]} storage={bs["storage"]} '
          f'sites={live_sites_by_block[name]} cross-block-rows={len(bs["xrows"])}')
