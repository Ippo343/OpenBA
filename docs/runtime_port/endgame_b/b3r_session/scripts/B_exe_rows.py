#!/usr/bin/env python3
"""B_exe_rows.py -- block-0 inventory B (EXE_ROWS) of the registry teardown.

Re-run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b3r_session/scripts/B_exe_rows.py

Writes docs/runtime_port/endgame_b/b3r_session/data/B_exe_rows.tsv (one row per
registered or read exe address) and prints the summary numbers the inventory
quotes. Reads the retail exe's PE import table and section table through pefile
(the same table tools/dispatch_slot_map.py reads); nothing else outside the repo.

Method (code lines only; comments stripped with b3x_session/codegrep.strip and
string contents blanked, so a printf/static_assert string is never a hit):
  * every 0x14xxxxxxx literal in code/ engine/ runtime/ standalone/ dx11/ tests/
    is found, and so is every USE of a named `constexpr/const uintptr_t NAME =
    0x14...` (the use then stands for the literal);
  * each occurrence is classified by the innermost call whose parentheses hold
    it (backward paren scan): a registration door (RegisterExeAddr and the four
    slot-storage wrappers), a direct ResolveAddr(Module::Exe, ...), a file-local
    by-address helper (HELPERS below, each read by hand: every one ends in
    ResolveAddr(Module::Exe, a)), a data table / switch that feeds a resolver
    later (DATA_READERS, read by hand), or something else (image-base
    arithmetic, a test compare) that is neither;
  * each reader is attributed to its outermost enclosing FUNCTION (brace-frame
    scan) and, when the statement assigns a namespace-scope pointer
    (`g_x = ...ResolveAddr(...)`, the engine_funcs initialiser binding), to that
    global. A reader is `live` when that function / global name occurs on a code
    line anywhere other than its own definition / assignment / declaration.
"""
import os, re, sys, bisect, collections

# Anchored on the script, not the current directory (paths in the table are repo-relative).
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), *(['..'] * 5)))
os.chdir(REPO)
sys.path.insert(0, os.path.join(REPO, 'docs', 'runtime_port', 'endgame_b', 'b3x_session'))
from codegrep import strip  # noqa: E402

OUT = os.path.join(REPO, 'docs', 'runtime_port', 'endgame_b', 'b3r_session', 'data', 'B_exe_rows.tsv')
EXE = r"C:\GOG Games\Banished\Application-x64.exe"
DIRS = ['code', 'engine', 'runtime', 'standalone', 'dx11', 'tests']
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')
EXCL = ('standalone/build', 'tests/unit/build', '.claude/worktrees', '/build/', '/build-')

# Registration doors: name -> index of the address argument. RegisterDispatchSlot
# exists twice (address_registry.cpp static, reg_unreg_lane1.cpp anon-namespace),
# RegisterLane3Slot (reg_unreg_lane3.cpp) and RegisterPlacePlayerSlot
# (reg_player_place.cpp) are copies of it: each stores the VALUE in a map and
# registers the ADDRESS OF THE MAP CELL, so the reader derefs it.
REG_DOORS = {'RegisterExeAddr': 'direct', 'RegisterDispatchSlot': 'slotcell',
             'RegisterLane3Slot': 'slotcell', 'RegisterPlacePlayerSlot': 'slotcell'}
# By-address helpers (file-local), each read by hand; all end in
# ResolveAddr(Module::Exe, <param>). file suffix -> set of helper names.
HELPERS = {
    'R', 'RSlot', 'ReadDispatchSlot', 'slot', 'Slot', 'Flt', 'ResolveSlot', 'RExe',
    'RF::Get', 'Get', 'resolveIAT', 'SlotFn', 'code', 'ResolveSlotPtr', 'ResolveFn',
    'loadExref', 'SerializeFieldViaSlot', 'SlotSerialize',
}
# Addresses that reach a resolver from DATA (a table / switch row), read by hand.
DATA_READERS = {
    'code/Game/Tutorial/tutorialstep.cpp': 'table of exe serialize-helper addresses, resolved by index',
    'code/Game/Component/citizenai_candidatelist.cpp': 'switch(state) -> addr -> ResolveAddr(Module::Exe, addr)',
}
NOT_READERS = {  # image-base arithmetic / diagnostics / a test compare: not a door
    'standalone/redzone_alloc.cpp', 'standalone/sampler.cpp', 'standalone/resolve_standalone.cpp',
    'tests/unit/alloc_door_test.cpp',
}
FLOAT_TABLE = 'standalone/reg_rdata_float_consts.cpp'
NAMED_USES = []
FN_LIVE = {}
FUNC_FILES = {}


def load_pe():
    import pefile
    pe = pefile.PE(EXE)
    imp = {}
    for d in pe.DIRECTORY_ENTRY_IMPORT:
        dll = d.dll.decode()
        for i in d.imports:
            imp[i.address] = (dll, i.name.decode() if i.name else f"#{i.ordinal}")
    secs = [(0x140000000 + s.VirtualAddress, 0x140000000 + s.VirtualAddress + s.Misc_VirtualSize,
             s.Name.rstrip(b'\0').decode()) for s in pe.sections]
    return imp, secs


def blank_strings(s):
    out = []; i = 0; n = len(s); st = None
    while i < n:
        c = s[i]
        if st is None:
            if c == '"' or (c == "'" and not (i > 0 and s[i - 1].isalnum())):
                st = c
            out.append(c); i += 1; continue
        if c == '\\' and i + 1 < n:
            out.append('  '); i += 2; continue
        if c == st or c == '\n':
            st = None; out.append(c); i += 1; continue
        out.append(' '); i += 1
    return ''.join(out)


def files():
    for d in DIRS:
        for dp, dn, fs in os.walk(d):
            for f in fs:
                p = os.path.join(dp, f).replace('\\', '/')
                if any(e in p for e in EXCL) or not p.endswith(EXTS):
                    continue
                yield p


LIT = re.compile(r'\b0x(14[0-9a-fA-F]{7})(?:ULL|ull|LL|U|u)?\b')
IDTAIL = re.compile(r'([A-Za-z_][A-Za-z_0-9:]*)\s*(<[^()]*>)?\s*$')
CTRL = re.compile(r'^(if|for|while|switch|catch|else|do|return|sizeof|decltype|static_assert|alignof)$')
NAMED = re.compile(r'\b(?:static\s+)?(?:inline\s+)?(?:constexpr|const)\s+(?:uintptr_t|uint64_t|std::uintptr_t|unsigned long long)\s+'
                   r'([A-Za-z_]\w*)\s*=\s*0x(14[0-9a-fA-F]{7})')


class Src:
    def __init__(self, path):
        self.path = path
        raw = open(path, encoding='utf-8', errors='replace').read()
        self.code = strip(raw)              # comments gone, strings kept
        self.t = blank_strings(self.code)   # strings blanked
        self.nl = [i for i, c in enumerate(self.t) if c == '\n']
        self.lines = self.code.split('\n')
        self.funcs = self._frames()

    def line(self, pos):
        return bisect.bisect_left(self.nl, pos) + 1

    def _frames(self):
        """Outermost function bodies: (start, end, name)."""
        t = self.t; stack = []; out = []; last = 0
        for i, c in enumerate(t):
            if c == '{':
                hdr = t[last:i]
                stack.append((i, hdr))
                last = i + 1
            elif c == '}':
                if stack:
                    s, hdr = stack.pop()
                    name = func_name(hdr)
                    if name and not any(func_name(h) for _, h in stack):
                        out.append((s, i, name))
                last = i + 1
            elif c == ';':
                last = i + 1
        out.sort()
        return out

    def enclosing(self, pos):
        k = bisect.bisect_right([f[0] for f in self.funcs], pos) - 1
        while k >= 0:
            s, e, n = self.funcs[k]
            if s <= pos <= e:
                return n
            k -= 1
        return None

    def call_ctx(self, pos):
        """(kind, name, argindex, callstart) of the innermost call holding pos."""
        t = self.t; depth = 0; brace = 0; i = pos - 1; commas = 0
        while i >= 0:
            c = t[i]
            if c in ')]':
                depth += 1
            elif c in '([':
                if depth == 0:
                    if c == '[':
                        return ('index', '', 0, i)
                    m = IDTAIL.search(t[max(0, i - 200):i])
                    return ('call', m.group(1) if m else '?', commas, i)
                depth -= 1
            elif c == '}':
                brace += 1
            elif c == '{':
                if brace == 0 and depth == 0:
                    return ('brace', '', commas, i)
                brace -= 1
            elif c == ',' and depth == 0 and brace == 0:
                commas += 1
            elif c == ';' and depth == 0 and brace == 0:
                return ('stmt', t[i + 1:pos].strip()[-80:], 0, i)
            i -= 1
        return ('top', '', 0, 0)

    def call_args(self, open_pos):
        """Argument texts (strings kept) of the call whose '(' is at open_pos."""
        c = self.code; depth = 0; args = []; cur = open_pos + 1
        i = open_pos
        while i < len(c):
            ch = c[i]
            if ch in '([{':
                depth += 1
            elif ch in ')]}':
                depth -= 1
                if depth == 0:
                    args.append(c[cur:i]); break
            elif ch == ',' and depth == 1:
                args.append(c[cur:i]); cur = i + 1
            elif ch == '"':
                j = i + 1
                while j < len(c) and c[j] != '"':
                    j += 2 if c[j] == '\\' else 1
                i = j
            i += 1
        return [re.sub(r'\s+', ' ', a).strip() for a in args]

    def assign_target(self, pos):
        """`NAME = ...<pos>` in the statement holding pos -> NAME, else None."""
        t = self.t; i = pos - 1; depth = 0
        while i >= 0 and t[i] not in ';{}':
            i -= 1
        stmt = t[i + 1:pos]
        m = re.match(r'\s*(?:(?:static|const|auto|[\w:<>*&]+)\s+)*?([A-Za-z_][\w:]*)\s*=[^=]', stmt)
        return m.group(1) if m else None


def func_name(hdr):
    h = hdr.strip()
    # a constructor's member-initialiser list: cut at the first top-level single ':'
    # that follows a closed parameter list (`X::X(a) : Base(a), f(0)`)
    d = 0
    for j, ch in enumerate(h):
        if ch in '(<':
            d += 1
        elif ch in ')>':
            d -= 1
        elif ch == ':' and d == 0 and h[j - 1:j] != ':' and h[j + 1:j + 2] != ':' and h[:j].rstrip().endswith(')'):
            h = h[:j].rstrip()
            break
    if not h or not h.endswith((')', 'const', 'override', 'noexcept', 'final')) and '->' not in h[-60:]:
        return None
    h = re.sub(r'\s*(const|override|noexcept|final|\s)*$', '', h)
    h = re.sub(r'\s*->\s*[\w:<>*& ]+$', '', h)
    if not h.endswith(')'):
        return None
    # find the '(' matching the last ')'
    d = 0
    for j in range(len(h) - 1, -1, -1):
        if h[j] == ')':
            d += 1
        elif h[j] == '(':
            d -= 1
            if d == 0:
                break
    pre = h[:j]
    if re.search(r'\]\s*$', pre):          # lambda
        return None
    m = IDTAIL.search(pre)
    if not m:
        return None
    n = m.group(1)
    if CTRL.match(n.split('::')[-1]) or n in ('namespace', 'struct', 'class', 'extern'):
        return None
    if re.search(r'\b(namespace|class|struct|enum|union)\b[^(]*$', pre):
        return None
    return n


def main():
    imp, secs = load_pe()

    def kind(a):
        if 0x140151000 <= a < 0x140153000 and a in imp:
            return 'import_slot'
        for s, e, n in secs:
            if s <= a < e:
                return 'exe_fn' if n == '.text' else 'data'
        return 'other'

    srcs = {p: Src(p) for p in files()}
    # ---- name index (code lines, strings blanked) for liveness ----
    word_lines = collections.defaultdict(list)   # word -> [(path, line)]
    WORD = re.compile(r'[A-Za-z_]\w*')
    for p, s in srcs.items():
        for ln, text in enumerate(s.t.split('\n'), 1):
            for w in set(WORD.findall(text)):
                word_lines[w].append((p, ln))

    for p, s in srcs.items():
        for _, _, n in s.funcs:
            FUNC_FILES.setdefault(n.split('::')[-1], set()).add(p)

    def refs(name, exclude):
        short = name.split('::')[-1]
        return [x for x in word_lines.get(short, []) if x not in exclude]

    # ---- function reachability (the "is the reader itself still called?" pass) ----
    # Nodes are function bodies (path, name). Roots: members (Class::M, reached
    # through a vtable or a header declaration), main/WinMain, and any function
    # named on a namespace-scope line that is not a prototype (a table, an
    # initialiser, a macro argument). An edge runs from the function whose body
    # names another function to that function. A reader is live when its
    # function is reachable from a root.
    frame_lines = {p: [(s.line(a), s.line(b), n) for a, b, n in s.funcs] for p, s in srcs.items()}
    frame_starts = {p: [x[0] for x in v] for p, v in frame_lines.items()}

    def enclosing_at(p, ln):
        k = bisect.bisect_right(frame_starts[p], ln) - 1
        while k >= 0:
            a, b, n = frame_lines[p][k]
            if a <= ln <= b:
                return n
            k -= 1
        return None

    tlines = {p: s.t.split('\n') for p, s in srcs.items()}
    nodes = {(p, n): a for p, v in frame_lines.items() for a, b, n in v}
    by_short = collections.defaultdict(set)
    for (p, n) in nodes:
        by_short[n.split('::')[-1]].add((p, n))
    roots = set(); edges = collections.defaultdict(set)
    for (p, n), a in nodes.items():
        short = n.split('::')[-1]
        if '::' in n or short in ('main', 'WinMain', 'wWinMain', 'wmain', 'DllMain'):
            roots.add((p, n)); continue
        own_files = {q for (q, _) in by_short[short]}
        proto = re.compile(r'^\s*(template\s*<[^>]*>\s*)?(extern\s+("C"\s*)?)?(static\s+|inline\s+)*'
                           r'[\w:<>*&,\s]+?[\s*&]' + re.escape(short) + r'\s*\(')
        for (q, l) in word_lines.get(short, []):
            if q == p and a - 3 <= l <= a:
                continue                       # its own definition header
            if q != p and q in own_files:
                continue                       # binds to that file's own definition
            e = enclosing_at(q, l)
            if e is None:
                line = tlines[q][l - 1] if l - 1 < len(tlines[q]) else ''
                if not proto.search(line):
                    roots.add((p, n))
            else:
                edges[(q, e)].add((p, n))
    live_fn = set(roots); todo = list(roots)
    while todo:
        x = todo.pop()
        for y in edges.get(x, ()):
            if y not in live_fn:
                live_fn.add(y); todo.append(y)
    FN_LIVE.update({k: (k in live_fn) for k in nodes})

    # ---- named constants ----
    named = {}   # short name -> [(addr, path, line, enclosing namespace or None)]
    for p, s in srcs.items():
        for m in NAMED.finditer(s.t):
            ns = None
            for nm in re.finditer(r'\bnamespace\s+(\w+)\s*\{', s.t[:m.start()]):
                if '}' not in s.t[nm.end():m.start()]:
                    ns = nm.group(1)
            named.setdefault(m.group(1), []).append((m.group(2).lower(), p, s.line(m.start()), ns))

    occ = []   # dict per occurrence
    def classify(s, pos, addr, via):
        ln = s.line(pos)
        k, name, argi, opos = s.call_ctx(pos)
        o = dict(addr=addr, path=s.path, line=ln, via=via, ctx=k, callee=name)
        if s.path == FLOAT_TABLE and k == 'brace':
            o['role'] = 'reg'; o['door'] = 'float_table'
            o['target'] = 'g_storage[i] (copy of the retail .rdata float)'
        elif k == 'call' and name in REG_DOORS and argi == 0:
            o['role'] = 'reg'; o['door'] = name
            args = s.call_args(opos)
            o['target'] = args[1] if len(args) > 1 else '?'
        elif k == 'call' and name == 'ResolveAddr':
            o['role'] = 'read'; o['door'] = 'ResolveAddr'
        elif k == 'call' and name in HELPERS:
            o['role'] = 'read'; o['door'] = 'helper:' + name
        elif s.path in DATA_READERS:
            o['role'] = 'read'; o['door'] = 'data:' + s.path.split('/')[-1]
        elif s.path in NOT_READERS:
            o['role'] = 'none'; o['door'] = 'not-a-door'
        elif NAMED.search(s.t[s.t.rfind('\n', 0, pos) + 1:pos + 12]):
            o['role'] = 'named'; o['door'] = 'constexpr'
        else:
            o['role'] = 'unknown'; o['door'] = f'{k}:{name}'
        if o['role'] == 'read':
            fn = s.enclosing(pos)
            o['func'] = fn or '(namespace scope)'
            tgt = s.assign_target(pos)
            o['global'] = None
            if tgt and fn and tgt.startswith(('g_', 'fn_', 's_', 'g', 'k')) and not re.match(r'^(slot|s|fp|p|a|addr|iat|dispatch)$', tgt):
                o['global'] = tgt
            # liveness
            ex = {(s.path, ln)}
            if o['global']:
                r = [x for x in refs(o['global'], ex)]
                # a file-static (s_) is looked up in its own file only
                gshort = re.escape(o['global'].split('::')[-1])
                if o['global'].startswith('s_') or re.search(r'\bstatic\s[^;=(]*\b' + gshort + r'\s*(=|;)', s.t):
                    r = [x for x in r if x[0] == s.path]
                # drop declaration lines (extern / typed definition with nullptr)
                r = [x for x in r if not re.search(r'(extern\s|\b' + re.escape(o['global'].split('::')[-1]) + r'\s*(=\s*nullptr|\{\s*\}|;))',
                                                    srcs[x[0]].t.split('\n')[x[1] - 1])]
                r = [x for x in r if enclosing_at(*x) is None or FN_LIVE.get((x[0], enclosing_at(*x)), True)]
                o['live'] = bool(r); o['live_by'] = 'global ' + o['global']
            elif fn:
                defline = s.line(next(f[0] for f in s.funcs if f[2] == fn))
                r = refs(fn, ex)
                # the header line may sit up to three lines above the brace
                r = [x for x in r if not (x[0] == s.path and defline - 3 <= x[1] <= defline)]
                # a name defined as a function body in more than one file (EnsureInit,
                # ResolveAll, ...) is a file-local: look in its own file only
                # (a member Class::M is reached through its class: tree-wide)
                if '::' not in fn and len(FUNC_FILES.get(fn, ())) > 1:
                    r = [x for x in r if x[0] == s.path]
                o['live'] = bool(r) and FN_LIVE.get((s.path, fn), True)
                o['live_by'] = 'fn ' + fn
            else:
                o['live'] = True; o['live_by'] = 'static-init'
        occ.append(o)

    for p, s in srcs.items():
        for m in LIT.finditer(s.t):
            classify(s, m.start(), m.group(1).lower(), 'literal')
    # uses of named constants
    # A namespaced constant (the XAddr documentation blocks) is used only when
    # spelled Ns::Name; a namespace-less one (kSlot_...) only in its own file.
    for short, defs in named.items():
        for addr, dp, dl, ns in defs:
            for (p, ln) in word_lines.get(short, []):
                if (p, ln) == (dp, dl):
                    continue
                if not ns and p != dp:
                    continue
                s = srcs[p]
                start = 0 if ln == 1 else s.nl[ln - 2] + 1
                end = s.nl[ln - 1] if ln - 1 < len(s.nl) else len(s.t)
                pat = (r'\b' + ns + r'::' + short + r'\b') if ns else (r'(?<!::)\b' + short + r'\b')
                m = re.search(pat, s.t[start:end])
                if m:
                    NAMED_USES.append((short, ns, addr, dp, dl, p, ln))
                    classify(s, start + m.end() - len(short), addr, 'named:' + (ns + '::' if ns else '') + short)
    n_named = sum(len(v) for v in named.values())
    print('# named constants', n_named, 'uses', len(NAMED_USES))
    for u in NAMED_USES:
        print('   ', u)

    # registration-function liveness
    for o in occ:
        if o['role'] == 'reg':
            s = srcs[o['path']]
            fn = s.enclosing(s.nl[o['line'] - 2] + 1 if o['line'] > 1 else 0) or None
            # enclosing by line start may miss; recompute by scanning funcs covering line
            fn = None
            for st, en, n in s.funcs:
                if s.line(st) <= o['line'] <= s.line(en):
                    fn = n
            if o['door'] == 'float_table':
                fn = 'RegisterRdataFloatConsts'   # the loop over the namespace-scope table
            o['regfn'] = fn
            if fn:
                defline = next(s.line(f[0]) for f in s.funcs if f[2] == fn)
                r = [x for x in refs(fn, set()) if not (x[0] == s.path and defline - 3 <= x[1] <= defline)]
                o['reglive'] = bool(r) and FN_LIVE.get((o['path'], fn), True)
            else:
                o['reglive'] = True

    # ---- per-address table ----
    by = collections.defaultdict(list)
    for o in occ:
        if o['role'] in ('reg', 'read'):
            by[o['addr']].append(o)

    def slot_value(o):
        m = re.search(r'ResolveExport\(\s*((?:"[^"]*"\s*)+)', o['target'])
        if m:
            return 'ResolveExport:' + ''.join(re.findall(r'"([^"]*)"', m.group(1)))
        return o['target']

    rows = []
    for a in sorted(by, key=lambda x: int(x, 16)):
        L = by[a]; ai = int(a, 16); ak = kind(ai)
        regs = [o for o in L if o['role'] == 'reg']
        reads = [o for o in L if o['role'] == 'read']
        sym = imp[ai][1] if ai in imp else ''
        live_regs = [o for o in regs if o['reglive']]
        win = (live_regs or regs)[-1] if regs else None
        live_reads = [o for o in reads if o['live']]
        data_reads = [o for o in reads if o['door'].startswith('data:')]
        target = slot_value(win) if win else ''
        tgt_short = re.sub(r'^.*?&\s*', '', target) if win else ''
        tgt_short = re.sub(r'[()]', '', re.sub(r'reinterpret_cast<[^>]*>|\(uintptr_t\)|\(void\s*\*\)', '', tgt_short)).strip()
        stubby = bool(re.search(r'Stub|NotImpl|NullFactory|Unimpl|Trap', target))
        # class
        if not regs:
            cls = 'STUB'
        elif not live_regs:
            cls = 'DEAD'
        elif not reads:
            cls = 'DEAD'
        elif not live_reads:
            cls = 'DEAD'
        elif data_reads:
            cls = 'CALLBACK'
        elif stubby:
            cls = 'STUB'
        elif ak == 'data' or win['door'] == 'float_table':
            cls = 'STORAGE'
        elif ak == 'import_slot':
            v = slot_value(win)
            cls = 'STORAGE' if is_data_symbol(sym) else 'DIRECT'
        else:
            cls = 'DIRECT'
        repl = ''
        if cls == 'DIRECT':
            if ak == 'import_slot':
                repl = demangle(sym)
            else:
                repl = tgt_short
        elif cls == 'STORAGE':
            repl = ('storage cell ' + (tgt_short if win['door'] != 'float_table' else 'g_storage[] (bits in the table)')
                    + ' in ' + win['path'] + (' holding ' + demangle(sym) if sym else ''))
        elif cls == 'CALLBACK':
            repl = 'table/switch row -> ' + tgt_short + ' (convert the table to function pointers)'
        rf = sorted({o['path'] for o in reads})
        rows.append(dict(
            key_addr='0x' + a, addr_kind=ak, runtime_symbol_if_slot=sym if ak == 'import_slot' else '',
            reg_file=win['path'] if win else '-', reg_line=str(win['line']) if win else '-',
            target=target, readers=str(len(reads)),
            reader_files=';'.join(rf[:8]) + (f';(+{len(rf) - 8})' if len(rf) > 8 else ''),
            cls=cls, direct_replacement=repl,
            # extra columns (right of the brief's ten)
            n_regs=str(len(regs)), live_readers=str(len(live_reads)),
            doors=';'.join(sorted({o['door'] for o in reads})),
            reader_funcs=';'.join(sorted({o['func'] + ('>' + o['global'] if o['global'] else '')
                                          + ('' if o['live'] else '[unref]') for o in reads})[:6]),
            reg_fn=(win.get('regfn') or '') + ('' if (not win or win['reglive']) else '[unref]') if win else '',
        ))
    # declaring header for DIRECT exe_fn replacements
    decl_index(rows, srcs)

    cols = ['key_addr', 'addr_kind', 'runtime_symbol_if_slot', 'reg_file', 'reg_line', 'target', 'readers',
            'reader_files', 'cls', 'direct_replacement', 'n_regs', 'live_readers', 'doors', 'reader_funcs', 'reg_fn']
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\t'.join(c if c != 'cls' else 'class' for c in cols) + '\n')
        for r in rows:
            f.write('\t'.join(r[c].replace('\t', ' ') for c in cols) + '\n')

    report(rows, occ, srcs, kind, imp)
    return rows, occ, srcs


def is_data_symbol(sym):
    # MSVC: ?name@scope@@<storage-class digit>... is data (2 static member, 3 global)
    if sym.startswith('??_7') or sym.startswith('??_R'):
        return True
    m = re.match(r'^\?[^?@][^@]*@(?:[^@]+@)*@([0-9])', sym)
    return bool(m)


OPS = {'0': '{ctor}', '1': '~{dtor}', '4': 'operator=', '8': 'operator==', '9': 'operator!=', 'A': 'operator[]',
       'D': 'operator*', 'G': 'operator-', 'H': 'operator+', 'Y': 'operator+=', 'Z': 'operator-=', 'X': 'operator*=',
       'K': 'operator/', '_0': 'operator/=', 'M': 'operator<', 'O': 'operator>', 'P': 'operator>=', 'N': 'operator<=',
       'B': 'operator{conv}', 'R': 'operator()'}


def demangle(sym):
    if not sym.startswith('?'):
        return sym
    s = sym[1:]
    op = None
    if s.startswith('?'):
        s = s[1:]
        code = s[:2] if s.startswith('_') else s[:1]
        op = OPS.get(code, 'op' + code); s = s[len(code):]
        name = None
    else:
        name, s = s.split('@', 1)
    scopes = []
    while s and not s.startswith('@'):
        if s.startswith('?$'):
            break
        sc, s = s.split('@', 1)
        scopes.append(sc)
    scopes.reverse()
    cls = scopes[-1] if scopes else ''
    if op:
        name = op.replace('{ctor}', cls).replace('{dtor}', cls)
    return '::'.join(scopes + [name])


def decl_index(rows, srcs):
    """For DIRECT exe_fn rows: where is the target declared? '(file-local, no declaration)' when the
    name is only defined in a .cpp (static / anon namespace) and declared in no header."""
    hdr_words = collections.defaultdict(set)
    for p, s in srcs.items():
        if p.endswith(('.h', '.hpp')):
            for w in set(re.findall(r'[A-Za-z_]\w*', s.t)):
                hdr_words[w].add(p)
    for r in rows:
        if r['cls'] != 'DIRECT':
            continue
        if r['addr_kind'] == 'import_slot':
            parts = r['direct_replacement'].split('::')
            cls = parts[-2] if len(parts) >= 2 else ''
            cand = [p for p in hdr_words.get(cls, ()) if p.startswith('runtime/types/')
                    and re.search(r'\b(class|struct)\s+(\w+\s+)?' + re.escape(cls) + r'\b[^;]*$', srcs[p].t, re.M)]
            ns = parts[0].lower() if parts else ''
            cand.sort(key=lambda p: (('/' + ns + '/') not in p, p))
            r['direct_replacement'] += ' @ ' + (cand[0] if cand else '(no runtime/types header found)')
        else:
            n = r['direct_replacement'].split('::')[-1].split('<')[0].strip()
            n = re.sub(r'\W.*$', '', n)
            hs = sorted(hdr_words.get(n, ()))
            defs = sorted(FUNC_FILES.get(n, ()))
            where = (' [defined ' + ';'.join(defs[:3]) + ']') if defs else ' [definition not found by name]'
            r['direct_replacement'] = (r['direct_replacement'] + ' @ ' + hs[0] + where) if hs else \
                (r['direct_replacement'] + ' @ no declaration exists yet' + where)
            r['_hdr'] = bool(hs); r['_def'] = defs


def report(rows, occ, srcs, kind, imp):
    print('# rows', len(rows))
    c = collections.Counter(r['cls'] for r in rows)
    print('# class', dict(c))
    print('# class x kind', dict(collections.Counter((r['cls'], r['addr_kind']) for r in rows)))
    regocc = [o for o in occ if o['role'] == 'reg']
    print('# registration occurrences', len(regocc), dict(collections.Counter(o['door'] for o in regocc)))
    unk = [o for o in occ if o['role'] == 'unknown']
    print('# unknown-context occurrences', len(unk))
    for o in unk[:40]:
        print('   ', o['path'], o['line'], o['addr'], o['door'])
    # per directory ResolveAddr(Module::Exe reader lines
    def d(p):
        parts = p.split('/')
        if parts[0] == 'code':
            return '/'.join(parts[:3])
        if parts[0] == 'runtime':
            return '/'.join(parts[:3])
        return parts[0]
    per = collections.defaultdict(lambda: collections.defaultdict(set))
    for o in occ:
        if o['role'] == 'read':
            k = kind(int(o['addr'], 16))
            per[d(o['path'])][('lines', o['door'] == 'ResolveAddr')].add((o['path'], o['line']))
            per[d(o['path'])][('addr', k)].add(o['addr'])
            per[d(o['path'])][('occ', k, o['door'] == 'ResolveAddr')].add((o['path'], o['line'], o['addr']))
    print('# per-dir: dir | RA lines | helper/data lines | distinct addrs slot/fn/data | RA occ slot/fn/data | helper occ slot/fn/data')
    for k in sorted(per):
        v = per[k]
        def n(*key): return len(v.get(key, ()))
        print(f"{k}\t{n('lines', True)}\t{n('lines', False)}\t"
              f"{n('addr', 'import_slot')}/{n('addr', 'exe_fn')}/{n('addr', 'data')}\t"
              f"{n('occ', 'import_slot', True)}/{n('occ', 'exe_fn', True)}/{n('occ', 'data', True)}\t"
              f"{n('occ', 'import_slot', False)}/{n('occ', 'exe_fn', False)}/{n('occ', 'data', False)}")
    # X_ResolveFuncs
    print('# X_ResolveFuncs')
    defs = []
    for p, s in srcs.items():
        for st, en, n in s.funcs:
            if re.search(r'_ResolveFuncs$', n):
                addrs = {o['addr'] for o in occ if o['path'] == p and o['role'] == 'read' and s.line(st) <= o['line'] <= s.line(en)}
                defs.append((n, p, s.line(st), len(addrs)))
    calls = collections.Counter()
    for p, s in srcs.items():
        for m in re.finditer(r'\b(\w+_ResolveFuncs)\s*\(', s.t):
            ln = s.line(m.start())
            lt = s.t[s.t.rfind('\n', 0, m.start()) + 1:m.start()]
            if re.search(r'\b(bool|void|int|static)\s+$', lt):
                continue          # a definition or a prototype, not a call
            calls[m.group(1)] += 1
    for n, p, l, na in sorted(defs):
        print(f'{n}\t{p}:{l}\taddrs={na}\tcalls={calls[n]}')
    print('# X_ResolveFuncs defs', len(defs), 'calls', sum(calls[n] for n, _, _, _ in defs))


if __name__ == '__main__':
    main()
