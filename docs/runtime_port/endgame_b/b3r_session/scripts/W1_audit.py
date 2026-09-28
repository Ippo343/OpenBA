#!/usr/bin/env python3
"""W1_audit.py -- the independent audit of wave 1 of the registry teardown (B3R).

Re-run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b3r_session/scripts/W1_audit.py

Writes docs/runtime_port/endgame_b/b3r_session/data/W1_audit.tsv (part A: every code line on the
checked-out tree that matches a door spelling, classified) and prints parts B and F.

Trees: the working tree (part A), and `git archive` copies of 537f8c39 (end of W0), 65b800d4 (start
of the step) and HEAD in a temporary directory (parts B and F). Nothing outside the repo is read
except the retail exe's import table, through tools/dispatch_slot_map.py (as B_exe_rows.py does).

Method (W1_AUDIT.md section 3):
  * code lines only: comments stripped by b3r_session/codegrep.strip (strings kept); hex keys are
    matched with string contents blanked;
  * a REGISTERED key is the key argument of RegisterRuntimeExport / RegisterRuntimeAddr (literal,
    split literal, file constant, `r.field` table loop), RegisterExeAddr (literal, the
    BuildRoadAddr::CtorParams constant, the reg_rdata_float_consts g_consts loop) and the three
    slot-cell wrappers RegisterDispatchSlot / RegisterLane3Slot / RegisterPlacePlayerSlot;
  * a READER of a key is any code line, outside a registration's KEY argument, holding the name as a
    (concatenated) string literal, the address as a 0x literal in any suffix / case, or an exe PE
    import slot 0x140151xxx / 0x140152xxx that the retail import table maps to the name.
"""
import collections, csv, io, os, re, subprocess, sys, tarfile, tempfile, importlib.util

sys.dont_write_bytecode = True
ROOT = os.getcwd()
sys.path.insert(0, os.path.join(ROOT, 'docs/runtime_port/endgame_b/b3r_session'))
from codegrep import strip  # noqa: E402

OUT = 'docs/runtime_port/endgame_b/b3r_session/data/W1_audit.tsv'
DIRS = ['code', 'runtime', 'standalone', 'engine', 'dx11', 'tests']
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')
EXCL = ('standalone/build', 'tests/unit/build', '.claude/worktrees', '/build/', '/build-')
W0_END, STEP_START = '537f8c39', '65b800d4'
STRLIT = r'"(?:\\.|[^"\\\n])*"'
HEX9 = re.compile(r'\b0[xX](1[48][0-9a-fA-F]{7})(?:ull|ULL|uLL|ul|UL|u|U|ll|LL)?\b')


def walk(root):
    out = []
    for d in DIRS:
        for dp, dn, fs in os.walk(os.path.join(root, d)):
            for f in fs:
                p = os.path.relpath(os.path.join(dp, f), root).replace('\\', '/')
                if any(e in '/' + p for e in EXCL) or not p.endswith(EXTS):
                    continue
                out.append(p)
    return sorted(out)


def blank(s):
    return re.sub(STRLIT, lambda m: '"' + ' ' * (len(m.group(0)) - 2) + '"', s)


def lit_join(expr):
    return ''.join(x[1:-1] for x in re.findall(STRLIT, expr))


def call_args(s, i):
    depth = 0; j = i; args = []; cur = []; st = None
    while j < len(s):
        c = s[j]
        if st:
            cur.append(c)
            if c == '\\':
                cur.append(s[j + 1]); j += 2; continue
            if c == st:
                st = None
            j += 1; continue
        if c == '"':
            st = c; cur.append(c)
        elif c in '([{':
            depth += 1
            if depth > 1:
                cur.append(c)
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                args.append(''.join(cur).strip()); return args, j
            cur.append(c)
        elif c == ',' and depth == 1:
            args.append(''.join(cur).strip()); cur = []
        else:
            cur.append(c)
        j += 1
    return args, j


REG_RT = re.compile(r'\bRegisterRuntime(Export|Addr)\s*\(')
REG_EXE = re.compile(r'\b(RegisterExeAddr|RegisterDispatchSlot|RegisterLane3Slot|RegisterPlacePlayerSlot)\s*\(')
HEXKEY = re.compile(r'^\s*(?:\(\s*uintptr_t\s*\)\s*)?0x([0-9a-fA-F]+)(?:ull|ULL|u|U|ll|LL)?\s*$')


def registrations(root):
    """[(kind, key, file, line, fill)] -- kind rtname / rtaddr / exe / exeslot."""
    rows = []
    for p in walk(root):
        s = strip(open(os.path.join(root, p), encoding='utf-8', errors='replace').read())
        line = lambda pos: s.count('\n', 0, pos) + 1
        consts = None
        for rx, fam in ((REG_RT, 'rt'), (REG_EXE, 'exe')):
            for m in rx.finditer(s):
                if re.search(r'\b(void|int)\s*\*?\s*$', s[max(0, m.start() - 30):m.start()]):
                    continue
                args, end = call_args(s, m.end() - 1)
                if not args or re.match(r'^(const\s+)?(char|uintptr_t|void)\b', args[0]):
                    continue
                k = args[0]; fill = re.sub(r'\s+', ' ', args[1]) if len(args) > 1 else ''
                if fam == 'rt' and m.group(1) == 'Export':
                    v = lit_join(k) if re.fullmatch(r'\s*(?:' + STRLIT + r'\s*)+', k) else None
                    if v is None and re.fullmatch(r'\w+', k):
                        if consts is None:
                            consts = {c.group(1): lit_join(c.group(2)) for c in re.finditer(
                                r'\b(\w+)\s*(?:\[\s*\])?\s*=\s*((?:' + STRLIT + r'\s*)+)[;,]', s)}
                        v = consts.get(k)
                    if v is None and re.fullmatch(r'\w+\.\w+', k):
                        for a, n in re.findall(r'\{\s*0x(18[0-9a-fA-F]{7})\w*\s*,\s*((?:' + STRLIT + r'\s*)+)', s):
                            rows.append(('rtname', lit_join(n), p, line(m.start()), fill))
                        continue
                    if v is not None:
                        rows.append(('rtname', v, p, line(m.start()), fill))
                    continue
                kind = 'rtaddr' if fam == 'rt' else ('exe' if m.group(1) == 'RegisterExeAddr' else 'exeslot')
                hm = HEXKEY.match(k)
                if hm:
                    rows.append((kind, int(hm.group(1), 16), p, line(m.start()), fill))
                elif fam == 'rt' and re.fullmatch(r'(?:\(\s*uintptr_t\s*\)\s*)?\w+\.\w+', k):
                    for a, n in re.findall(r'\{\s*0x(18[0-9a-fA-F]{7})\w*\s*,\s*((?:' + STRLIT + r'\s*)+)', s):
                        rows.append(('rtaddr', int(a, 16), p, line(m.start()), fill))
                elif 'g_consts' in k:
                    for a in re.findall(r'\{\s*0x(14[0-9a-fA-F]{7})', s):
                        rows.append(('exe', int(a, 16), p, line(m.start()), 'g_storage[i]'))
                elif k == 'BuildRoadAddr::CtorParams':
                    rows.append(('exe', 0x1400e4b50, p, line(m.start()), fill))
                # else: the wrappers' own bodies (`RegisterExeAddr(ghidra_addr, ...)`)
    return rows


def keysets(rows):
    d = collections.defaultdict(set)
    for kind, key, *_ in rows:
        d['exe' if kind in ('exe', 'exeslot') else kind].add(key)
    return d


def archive(rev, dest):
    data = subprocess.run(['git', 'archive', rev] + DIRS, capture_output=True, check=True).stdout
    with tarfile.open(fileobj=io.BytesIO(data)) as t:
        t.extractall(dest, filter='data') if sys.version_info >= (3, 12) else t.extractall(dest)


def load_slots():
    spec = importlib.util.spec_from_file_location('dsm', os.path.join(ROOT, 'tools/dispatch_slot_map.py'))
    dsm = importlib.util.module_from_spec(spec); spec.loader.exec_module(dsm)
    return {a: nm for a, (dll, nm) in dsm.slotmap.items() if dll.lower().startswith('runtime')}


# ---------------------------------------------------------------------------
# Part B -- rows registered at the end of W0 and not now, and their readers now
# ---------------------------------------------------------------------------
def part_b(t0, t1, slot2name):
    k0, k1 = keysets(registrations(t0)), keysets(registrations(t1))
    gone = {kind: k0[kind] - k1[kind] for kind in ('rtname', 'rtaddr', 'exe')}
    new = {kind: k1[kind] - k0[kind] for kind in ('rtname', 'rtaddr', 'exe')}
    slots = {a: nm for a, nm in slot2name.items() if nm in gone['rtname']}
    hits = []
    for p in walk(t1):
        s = strip(open(os.path.join(t1, p), encoding='utf-8', errors='replace').read())
        lines = s.split('\n')
        ln = lambda pos: s.count('\n', 0, pos) + 1
        # registration KEY arguments are not readers
        keyspans = []
        for rx in (REG_RT, REG_EXE):
            for m in rx.finditer(s):
                c = s.find(',', m.end())
                keyspans.append((m.start(), c if c > 0 else m.end()))
        inkey = lambda pos: any(a <= pos <= b for a, b in keyspans)
        for m in re.finditer(r'(?:' + STRLIT + r'\s*)+', s):
            v = lit_join(m.group(0))
            if '@' not in v:
                continue
            for nm in gone['rtname']:
                if nm == v or (len(nm) > 8 and nm in v):
                    hits.append(('name' if nm == v else 'name-in-text', nm, p, ln(m.start()),
                                 lines[ln(m.start()) - 1].strip()[:160]))
        for m in HEX9.finditer(blank(s)):
            if inkey(m.start()):
                continue
            a = int(m.group(1), 16)
            for kind in ('rtaddr', 'exe'):
                if a in gone[kind]:
                    hits.append((kind, hex(a), p, ln(m.start()), lines[ln(m.start()) - 1].strip()[:160]))
            if a in slots:
                hits.append(('slot->' + slots[a], hex(a), p, ln(m.start()), lines[ln(m.start()) - 1].strip()[:160]))
        for m in re.finditer(r'0x180000000\w*\s*[+]|\bkRuntimeBase\b', blank(s)):
            hits.append(('base+offset', '', p, ln(m.start()), lines[ln(m.start()) - 1].strip()[:160]))
    return k0, k1, gone, new, slots, hits


# ---------------------------------------------------------------------------
# Part A -- every remaining door line on the working tree, classified
# ---------------------------------------------------------------------------
SPELLINGS = [
    ('ResolveAddr(Exe)', r'\bResolveAddr\s*\(\s*Module::Exe'),
    ('ResolveAddr(Runtime)', r'\bResolveAddr\s*\(\s*Module::Runtime'),
    ('ResolveAddr', r'\bResolveAddr\s*\('),
    ('ResolveExport', r'\bResolveExport\s*\('),
    ('RT_EXPORT_ONCE', r'\bRT_EXPORT_ONCE\b'),
    ('RtAddr<>', r'\bRtAddr\s*<'),
    ('RtResolveCached', r'\bRtResolveCached\b'),
    ('AtomicExport/AtomicAddr', r'\bAtomic(?:Export|Addr)\b'),
    ('RtPortLookup', r'\bRtPortLookup\w*\b|\bRuntimeExportIsRegistered\b'),
    ('ReadGlobal/WriteGlobal', r'\b(?:Read|Write)Global\b'),
    ('RT_DATA_SLOT', r'\bRT_DATA_SLOT\b'),
    ('file-local helper', r'\b(?:RSlot|ResolveSlot\w*|ResolveFn|loadExref|resolveIAT|SlotFn|ByAddr|RtAddrFn|ArrayDtor|PlainArrayDtor|ArraySetSize|SetSizeWithFill|ReadDispatchSlot)\b'),
    ('resolver fn', r'\b\w+_ResolveFuncs\b|\b\w+_InitFuncs\b|\bEnsure\w+\b'),
    ('Register*', r'\bRegister(?:Exe|Runtime)\w*\s*\(|\bRegister\w*Slot\w*\s*\('),
]
MACHINERY_FILES = {'standalone/resolve.h', 'standalone/resolve_standalone.cpp', 'runtime/funcs/rt_port.h',
                   'runtime/funcs/rt_resolve_cache.cpp', 'standalone/address_registry.h'}
INSTRUMENT_FILES = {'standalone/dlgctrl_route.cpp', 'standalone/scene_watch.cpp', 'standalone/redzone_alloc.cpp'}
STORAGE_GLOBALS = {'g_pGameWorldSingleton', 'g_pKeyMapSingleton', 'g_pGameSettingsPtr', 'g_pInputConfigSingletonExref',
                   'g_pGameSingleton', 'g_pNewGameSettings', 'g_pMemAllocSingleton'}
LEFTOVER_GLOBALS = {'g_fUVScale', 'g_pAgeMultiplier', 'g_pNotifyTimerDuration', 'fn_EdgeMesh_TriangleVertex_Ctor',
                    'fn_EdgeMesh_EdgeVertex_Ctor', 'fn_ModDialog_ArrayPush'}
STUB_KEYS = {0x140002280}
ROAD_ASTAR = 0x14009ce00


def engine_globals():
    h = strip(open('engine/funcs/engine_funcs.h', encoding='utf-8').read())
    names = set(re.findall(r'^\s*extern\s+[^;()]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*;', h, re.M))
    names |= set(re.findall(r'^\s*extern\s+[^;]*\(\s*\*\s*(\w+)\s*\)', h, re.M))
    names.discard('C')
    src = strip(open('engine/funcs/engine_funcs.cpp', encoding='utf-8').read())
    bind = {}
    for n in names:
        m = re.search(r'(?<![\w.])' + re.escape(n) + r'\s*=\s*([^;]*?(?:ResolveAddr|ResolveExport)[^;]*);', src)
        if m:
            h9 = HEX9.search(m.group(1)); st = re.search(STRLIT, m.group(1))
            bind[n] = hex(int(h9.group(1), 16)) if h9 else (lit_join(m.group(1)) if st else '?')
    return names, bind


def body_of(src, name):
    m = re.search(r'\b' + re.escape(name) + r'\s*\([^;{)]*\)[^;{]*\{', src)
    if not m:
        return None
    i = m.end() - 1; d = 0
    for j in range(i, len(src)):
        if src[j] == '{': d += 1
        elif src[j] == '}':
            d -= 1
            if d == 0:
                return src[i:j + 1]
    return None


def resolver_kind(src, name):
    """'RESOLVES' when the definition still holds a door; 'RETURNS-TRUE' / 'NO-DOOR' otherwise."""
    b = body_of(src, name)
    if b is None:
        return None
    if re.search(r'Resolve(?:Addr|Export)\s*\(|RtAddr\s*<|RT_EXPORT_ONCE|RtResolveCached|RtPortLookup|\bfn_\w+\s*=|RSlot\s*\(', b):
        keys = {int(h, 16) for h in re.findall(r'0[xX](1[48][0-9a-fA-F]{7})', b)}
        if keys == {ROAD_ASTAR}:
            return 'ROAD-ASTAR'
        return 'RESOLVES'
    if re.fullmatch(r'\{\s*return\s+true\s*;\s*\}', b):
        return 'RETURNS-TRUE'
    return 'NO-DOOR'


def part_a(reg_now, slot2name):
    names, bind = engine_globals()
    grx = re.compile(r'\b(' + '|'.join(sorted(names, key=len, reverse=True)) + r')\b')
    reg_keys = keysets(reg_now)
    # readers of every registered key (for the W2-ROW rows): literal / name / slot sweep
    key_readers = collections.Counter()
    files = walk('.')
    srcs = {p: strip(open(p, encoding='utf-8', errors='replace').read()) for p in files}
    for p, s in srcs.items():
        spans = []
        for rx in (REG_RT, REG_EXE):
            for m in rx.finditer(s):
                c = s.find(',', m.end()); spans.append((m.start(), c if c > 0 else m.end()))
        inkey = lambda pos: any(a <= pos <= b for a, b in spans)
        for m in HEX9.finditer(blank(s)):
            if not inkey(m.start()):
                key_readers[hex(int(m.group(1), 16))] += 1
        for m in re.finditer(r'(?:' + STRLIT + r'\s*)+', s):
            if not inkey(m.start()):
                v = lit_join(m.group(0))
                if v.startswith('?'):
                    key_readers[v] += 1
    # an engine_funcs binding counts as a reader only while its global has a use outside engine/funcs
    guses = collections.Counter()
    for p, s in srcs.items():
        if p.startswith('engine/funcs/engine_funcs') or p in INSTRUMENT_FILES:
            continue   # an instrument's read does not keep a binding alive for the product
        shadows = set(re.findall(r'\b(?:const|constexpr)\s+\w+\s+((?:g_p|g_f|fn_)\w+)\s*=', s))
        for ln in blank(s).split('\n'):
            for g in set(grx.findall(ln)) - shadows:
                guses[g] += 1

    rows = []
    for p in files:
        s = srcs[p]; sb = blank(s)
        lines = s.split('\n'); blines = sb.split('\n')
        # lines covered by a registration call (a slot fill written on the next line is part of it)
        reglines = set()
        for rx in (REG_RT, REG_EXE):
            for m in rx.finditer(s):
                if re.search(r'\b(void|int)\s*\*?\s*$', s[max(0, m.start() - 30):m.start()]):
                    continue
                a, e = call_args(s, m.end() - 1)
                reglines.update(range(s.count('\n', 0, m.start()) + 1, s.count('\n', 0, e) + 2))
        for i, bl in enumerate(blines, 1):
            full = lines[i - 1]
            spell = None
            for nm, rx in SPELLINGS:
                if re.search(rx, bl):
                    spell = nm; break
            glob = None
            if not spell and not p.startswith('engine/funcs/engine_funcs.h'):
                gm = grx.search(bl)
                if gm:
                    spell = 'engine_funcs global'; glob = gm.group(1)
            if not spell:
                continue
            h9 = HEX9.search(bl); st = re.search(STRLIT, full)
            key = hex(int(h9.group(1), 16)) if h9 else (lit_join(st.group(0)) if st and '@' in st.group(0) else '')
            if glob:
                key = key or bind.get(glob, '')
            if spell != 'Register*' and i in reglines and not p.startswith('engine/'):
                cls, why = 'W2-ROW', 'the fill of a slot registration (a reader of that name row)'
            else:
                cls, why = classify(p, i, bl, full, spell, key, glob, s, bind, guses, key_readers, reg_keys)
            rows.append((p, i, spell, key, cls, why, full.strip()[:160]))
    return rows


def classify(p, i, bl, full, spell, key, glob, src, bind, guses, key_readers, reg_keys):
    if p.startswith('tests/'):
        return 'TEST', 'unit test'
    if p in MACHINERY_FILES:
        return 'W2-MACHINERY', 'the resolver / registry itself'
    if p in INSTRUMENT_FILES:
        return 'INSTRUMENT', 'R11: inspects or rewires the registry / slot cells'
    if spell == 'Register*':
        if re.search(r'\b(?:void|int|bool|static)\s+Register\w*\s*\(', bl) or re.search(r'^\s*(?:extern\s+)?\w+\s+Register\w*\s*\([^)]*\)\s*;', bl):
            return 'W2-MACHINERY', 'a registration wrapper / register function declaration'
        k = key
        n = key_readers.get(k, 0) if k else 0
        if p == 'standalone/reg_rt_mirror16.cpp':
            return 'W2-ROW', 'mirror row; its name row is gone, so it registers NULL (no reader of the address)'
        return 'W2-ROW', ('key has %d reader line(s) outside registrations' % n) if k else 'registration'
    if p.startswith('engine/funcs/engine_funcs'):
        g = next((g for g in bind if re.search(r'(?<![\w.])' + re.escape(g) + r'\s*=', bl)), None)
        if g is None:
            gm = re.search(r'^\s*(\w+)\s*=', bl)
            g = gm.group(1) if gm else None
        if key and key.startswith('0x') and int(key, 16) in STUB_KEYS:
            return 'STUB', 'EngineFuncsInit binding of an unregistered exe address (PORT GAP owner)'
        if g and guses.get(g, 0):
            if g in STORAGE_GLOBALS:
                return 'STORAGE', f'binding of {g} ({guses[g]} use lines outside engine/funcs)'
            return 'LEFTOVER', f'binding of {g} ({guses[g]} use lines outside engine/funcs)'
        if 'dispatch' in bl and 'ResolveAddr' in bl:
            return 'W2-MACHINERY', 'EngineFuncsInit slot-cell read whose global has no reader'
        return 'W2-MACHINERY', 'EngineFuncsInit binding whose global has no reader outside engine/funcs (EZ deletes it)'
    if glob:
        if re.search(r'\b(?:const|constexpr)\s+\w+\s+' + glob + r'\s*=', bl) or \
                re.search(r'\bconst\s+\w+\s+' + glob + r'\b', src[:src.find('\n'.join(src.split('\n')[i - 1:i]))]):
            return 'NOT-A-DOOR', f'a file-local constant that shadows the engine_funcs name {glob}'
        if p.endswith('/exe_bodies.h') and glob not in STORAGE_GLOBALS:
            return 'LEFTOVER', f'{glob} inside an inline body of exe_bodies.h'
        if glob in STORAGE_GLOBALS:
            if glob == 'g_pMemAllocSingleton' and p == 'standalone/main.cpp':
                return 'STORAGE', 'the self-test refresh of the snapshot (R7 behaviour commit 2 is still owed)'
            return 'STORAGE', f'read of {glob}'
        if glob in LEFTOVER_GLOBALS:
            return 'LEFTOVER', f'{glob} is on the LEFTOVERS list'
        return 'MISSED', f'{glob} still read'
    if key == hex(ROAD_ASTAR):
        return 'LEFTOVER', 'road A* 0x14009ce00 reader (signature not settled, known_port_issues 9)'
    if spell == 'resolver fn':
        fn = re.search(r'\b(\w+_ResolveFuncs|\w+_InitFuncs|Ensure\w+)\b', bl).group(1)
        kind = None
        for q in (p, p.replace('_funcs.h', '.cpp')):
            if os.path.exists(q):
                kind = resolver_kind(strip(open(q, encoding='utf-8', errors='replace').read()), fn)
                if kind:
                    break
        if kind is None:
            # a declaration or call of a resolver defined elsewhere: find its definition
            for q, s2 in _defs(fn):
                kind = resolver_kind(s2, fn)
                if kind:
                    break
        if kind == 'RETURNS-TRUE':
            return 'LEFTOVER', f'{fn}: resolver stub that only returns true'
        if kind == 'ROAD-ASTAR':
            return 'LEFTOVER', f'{fn}: resolves only the road A* 0x14009ce00'
        if kind == 'RESOLVES':
            return 'MISSED', f'{fn} still resolves'
        return 'NOT-A-DOOR', f'{fn}: a lazy init that resolves nothing (type ids / real work)'
    if spell == 'file-local helper':
        if re.search(r'\bSlotFn\b', bl) and 'tests/' in p:
            return 'TEST', 'vtable slot type in a test'
        if re.search(r'\bArraySetSize\b', bl) and p in ('runtime/src/audio/audio.cpp', 'runtime/src/object/external_load.cpp'):
            return 'NOT-A-DOOR', 'a real template / body named like a door helper; no resolve inside'
        return 'MISSED', 'file-local door helper'
    if spell.startswith('ResolveAddr') or spell in ('ResolveExport', 'RT_EXPORT_ONCE', 'RtAddr<>', 'RtResolveCached', 'AtomicExport/AtomicAddr', 'RtPortLookup', 'ReadGlobal/WriteGlobal', 'RT_DATA_SLOT'):
        if 'RegisterDispatchSlot' in bl or re.search(r'Register\w*\s*\(', '\n'.join(src.split('\n')[max(0, i - 2):i + 1])):
            return 'W2-ROW', 'the fill of a slot registration'
        if key and key.startswith('0x') and int(key, 16) not in reg_keys['exe'] | reg_keys['rtaddr']:
            return 'STUB', 'resolve of an unregistered key'
        return 'MISSED', 'door resolve outside the machinery'
    return 'MISSED', 'unclassified'


_DEFCACHE = {}


def _defs(fn):
    if fn not in _DEFCACHE:
        out = []
        for p in walk('.'):
            if p.endswith(('.cpp', '.h')):
                s = open(p, encoding='utf-8', errors='replace').read()
                if re.search(r'\b' + re.escape(fn) + r'\s*\([^;{)]*\)[^;{]*\{', s):
                    out.append((p, strip(s)))
        _DEFCACHE[fn] = out
    return _DEFCACHE[fn]


# ---------------------------------------------------------------------------
# Part F -- the numbers for the close
# ---------------------------------------------------------------------------
F_PATTERNS = [
    ('RegisterRuntimeExport(', r'\bRegisterRuntimeExport\s*\('),
    ('RegisterRuntimeAddr(', r'\bRegisterRuntimeAddr\s*\('),
    ('RegisterExeAddr(', r'\bRegisterExeAddr\s*\('),
    ('ResolveAddr(Module::Exe', r'\bResolveAddr\s*\(\s*Module::Exe'),
    ('ResolveAddr(Module::Runtime', r'\bResolveAddr\s*\(\s*Module::Runtime'),
    ('ResolveExport(', r'\bResolveExport\s*\('),
    ('RT_EXPORT_ONCE', r'\bRT_EXPORT_ONCE\b'),
    ('RtAddr<', r'\bRtAddr\s*<'),
    ('X_ResolveFuncs / _InitFuncs (defs+calls+decls)', r'\b\w+_(?:ResolveFuncs|InitFuncs)\s*\('),
    ('reinterpret_cast', r'\breinterpret_cast\b'),
    ('// B.3.x bridge marker', None),
    ('PORT GAP marker', None),
    ('STANDALONE DEVIATION marker', None),
]


def count_tree(root):
    res = {nm: collections.Counter() for nm, _ in F_PATTERNS}
    res['engine_funcs extern decls'] = collections.Counter()
    res['engine_funcs use lines'] = collections.Counter()
    names = set()
    hp = os.path.join(root, 'engine/funcs/engine_funcs.h')
    if os.path.exists(hp):
        h = strip(open(hp, encoding='utf-8').read())
        names = set(re.findall(r'^\s*extern\s+[^;()]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*;', h, re.M))
        names |= set(re.findall(r'^\s*extern\s+[^;]*\(\s*\*\s*(\w+)\s*\)', h, re.M))
        names.discard('C')
        res['engine_funcs extern decls']['engine'] = len(names)
    grx = re.compile(r'\b(?:' + '|'.join(sorted(names)) + r')\b') if names else None
    for p in walk(root):
        top = p.split('/')[0]
        raw = open(os.path.join(root, p), encoding='utf-8', errors='replace').read()
        code = strip(raw).split('\n')
        for ln in code:
            b = blank(ln)
            for nm, rx in F_PATTERNS:
                if rx and re.search(rx, b):
                    res[nm][top] += 1
            if grx and not p.startswith('engine/funcs/') and grx.search(b):
                res['engine_funcs use lines'][top] += 1
        for ln in raw.split('\n'):
            if re.search(r'//.*B\.3\.[\w.]*\s*bridge', ln):
                res['// B.3.x bridge marker'][top] += 1
            if 'PORT GAP' in ln:
                res['PORT GAP marker'][top] += 1
            if 'STANDALONE DEVIATION' in ln:
                res['STANDALONE DEVIATION marker'][top] += 1
    regs = [f for f in os.listdir(os.path.join(root, 'standalone')) if re.match(r'reg_.*\.cpp$', f)]
    ar = os.path.join(root, 'standalone/address_registry.cpp')
    extra = {'standalone/reg_*.cpp files': len(regs),
             'address_registry.cpp lines': sum(1 for _ in open(ar, encoding='utf-8', errors='replace'))}
    return res, extra


def main():
    slot2name = load_slots()
    with tempfile.TemporaryDirectory() as tmp:
        t0, t1, t65 = (os.path.join(tmp, x) for x in ('w0end', 'tip', 'start'))
        for rev, d in ((W0_END, t0), ('HEAD', t1), (STEP_START, t65)):
            os.makedirs(d); archive(rev, d)
        k0, k1, gone, new, slots, hits = part_b(t0, t1, slot2name)
        f65, e65 = count_tree(t65)
        f1, e1 = count_tree(t1)
        reg_now = registrations(t1)
    print('== Part B: registered keys at %s -> HEAD' % W0_END)
    for kind in ('rtname', 'rtaddr', 'exe'):
        print(f'  {kind:7s} {len(k0[kind]):5d} -> {len(k1[kind]):5d}   deleted {len(gone[kind])}   added {len(new[kind])}')
    print(f'  import slots mapping to a deleted name: {len(slots)}')
    print(f'  reader candidates of deleted keys (read each by hand): {len(hits)}')
    for h in hits:
        print('   ', '\t'.join(map(str, h)))

    rows = part_a(reg_now, slot2name)
    with open(OUT, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['file', 'line', 'spelling', 'key', 'class', 'reason', 'code'])
        for r in rows:
            w.writerow(r)
    c = collections.Counter(r[4] for r in rows)
    print('\n== Part A: %d code lines -> %s' % (len(rows), OUT))
    for k, v in c.most_common():
        print(f'  {k:13s} {v}')
    for r in rows:
        if r[4] == 'MISSED':
            print('  MISSED', f'{r[0]}:{r[1]}', r[2], r[3], r[5], '|', r[6][:100])

    print('\n== Part F: code lines per directory, %s -> HEAD' % STEP_START)
    for nm in list(f1):
        a = sum(f65[nm].values()); b = sum(f1[nm].values())
        per = ' '.join(f'{d}:{f65[nm].get(d, 0)}->{f1[nm].get(d, 0)}' for d in DIRS if f65[nm].get(d, 0) or f1[nm].get(d, 0))
        print(f'  {nm:48s} {a:6d} -> {b:6d}   {per}')
    for nm in e1:
        print(f'  {nm:48s} {e65[nm]:6d} -> {e1[nm]:6d}')


if __name__ == '__main__':
    main()
