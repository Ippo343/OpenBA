#!/usr/bin/env python3
"""classes.py -- the CLASS CENSUS for the normal-virtual cutover (NEXT_SESSION.md §3b questions 3 and 5).

Run from the repo root:
    python docs/runtime_port/endgame_b/b3x_session/classes.py --tsv       one row per DeletingDtor class
    python docs/runtime_port/endgame_b/b3x_session/classes.py --summary   the counts + UNRESOLVED list
    python docs/runtime_port/endgame_b/b3x_session/classes.py --other     every non-STD body, quoted
    python docs/runtime_port/endgame_b/b3x_session/classes.py --nondd     polymorphic roots whose slot 0 is NOT DeletingDtor
    python docs/runtime_port/endgame_b/b3x_session/classes.py --macros    #defines whose body emits DeletingDtor / a class
    python docs/runtime_port/endgame_b/b3x_session/classes.py --crosscheck  partc.py's 902 lines vs what this parser attributed

A regex/brace parser, NOT a C++ front end. Comments are stripped with codegrep.strip, string/char literal
contents are blanked, preprocessor lines are blanked (so BOTH arms of an #if are scanned), and invocations of
macros whose body declares a class / a virtual / DeletingDtor / an access label are expanded textually in
place (one line, the invocation's newlines kept after it, so line numbers hold). Everything the parser could
not pin down is printed as UNRESOLVED -- read that list before trusting a number.
"""
import os, re, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from codegrep import strip

ROOTS = ('code', 'runtime', 'standalone', 'engine', 'dx11', 'tests')
EXTS = ('.h', '.hpp', '.cpp', '.inc')
DD_RE = re.compile(r'\bDeletingDtor\s*\(\s*(?:unsigned|uint32_t|int|uint8_t)\b')   # == partc.py's pattern

UNRESOLVED = []   # (kind, where, text)


def unres(kind, where, text):
    UNRESOLVED.append((kind, where, text))


# ----------------------------------------------------------------------------------------------------------
# 1. Load + preprocess
# ----------------------------------------------------------------------------------------------------------
def walk():
    for r in ROOTS:
        for dp, dn, fs in os.walk(r):
            q = dp.replace('\\', '/')
            dn[:] = sorted(x for x in dn if not (x.startswith('build') or x == 'obj')
                           and '.claude/worktrees' not in (q + '/' + x))
            for f in sorted(fs):
                if f.endswith(EXTS):
                    yield (q + '/' + f)


STR_RE = re.compile(r'"(?:\\.|[^"\\\n])*"')
CHR_RE = re.compile(r"(?<![\w'])'(?:\\.|[^'\\\n])+'")


def blank_literals(s):
    s = STR_RE.sub('""', s)
    return CHR_RE.sub("' '", s)


DEFINE_RE = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(\w+)(\([^)]*\))?(.*)$', re.S)


def split_pp(src):
    """Blank every preprocessor line (continuations included); return (code, [(name, params, body, line)])."""
    lines = src.split('\n'); out = []; defs = []; i = 0
    while i < len(lines):
        l = lines[i]
        if l.lstrip().startswith('#'):
            start = i; buf = [l]
            while buf[-1].rstrip().endswith('\\') and i + 1 < len(lines):
                i += 1; buf.append(lines[i])
            text = '\n'.join(x.rstrip()[:-1] if x.rstrip().endswith('\\') else x for x in buf)
            m = DEFINE_RE.match(text)
            if m:
                params = None
                if m.group(2) is not None:
                    params = [p.strip() for p in m.group(2)[1:-1].split(',') if p.strip()]
                defs.append((m.group(1), params, m.group(3).replace('\n', ' '), start + 1))
            out.extend([''] * len(buf))
        else:
            out.append(l)
        i += 1
    return '\n'.join(out), defs


FILES = {}      # path -> processed text (after expansion)
RAW = {}        # path -> stripped text (comments gone), split into lines
MACROS = collections.defaultdict(list)   # name -> [(params, body, file, line)]
INTERESTING_BODY = re.compile(r'DeletingDtor|\b(?:class|struct)\s+\w+[^;]*\{|\bvirtual\b|\boverride\b|'
                              r'\b(?:public|protected|private)\s*:(?!:)|operator\s+(?:delete|new)|~\w+\s*\(')

pre = {}
for p in walk():
    s = strip(open(p, encoding='utf-8', errors='replace').read())
    RAW[p] = s.split('\n')
    code, defs = split_pp(blank_literals(s))
    pre[p] = code
    for (n, params, body, ln) in defs:
        MACROS[n].append((params, body, p, ln))


def pick_macro(name, path):
    c = MACROS.get(name)
    if not c: return None
    same = [m for m in c if m[2] == path]
    if same: return same[-1]
    bodies = {(tuple(m[0]) if m[0] is not None else None, m[1].strip()) for m in c}
    if len(bodies) == 1: return c[0]
    return 'AMBIG'


EXPANDABLE = {n for n, c in MACROS.items() if any(INTERESTING_BODY.search(b) for (_, b, _, _) in c)}
MACRO_EMITS_DD = sorted({(n, m[2], m[3]) for n, c in MACROS.items() for m in c if DD_RE.search(m[1])})
MACRO_EMITS_CLASS = sorted({(n, m[2], m[3]) for n, c in MACROS.items() for m in c
                            if re.search(r'\b(?:class|struct)\s+\w+[^;{]*\{', m[1])})
MACRO_USES = collections.Counter()


def split_args(s):
    args = []; depth = 0; cur = []
    for ch in s:
        if ch in '([{<' and ch != '<': depth += 1
        elif ch in ')]}': depth -= 1
        if ch == ',' and depth == 0:
            args.append(''.join(cur)); cur = []
        else:
            cur.append(ch)
    args.append(''.join(cur))
    return [a.strip() for a in args]


def expand_once(text, path):
    names = [n for n in EXPANDABLE if n in text]
    if not names: return text, False
    pat = re.compile(r'\b(' + '|'.join(sorted(map(re.escape, names), key=len, reverse=True)) + r')\b')
    out = []; pos = 0; changed = False
    for m in pat.finditer(text):
        if m.start() < pos: continue
        name = m.group(1); mac = pick_macro(name, path)
        if mac is None: continue
        if mac == 'AMBIG':
            unres('macro-ambiguous', f'{path}:{text.count(chr(10), 0, m.start()) + 1}', name); continue
        params, body, mfile, mline = mac
        end = m.end()
        if params is not None:
            j = end
            while j < len(text) and text[j] in ' \t\n': j += 1
            if j >= len(text) or text[j] != '(':
                continue
            depth = 0; k = j
            while k < len(text):
                if text[k] == '(': depth += 1
                elif text[k] == ')':
                    depth -= 1
                    if depth == 0: break
                k += 1
            args = split_args(text[j + 1:k]); end = k + 1
            exp = body
            if params and params[-1] == '...':
                named = params[:-1]; va = ', '.join(args[len(named):])
                mp = dict(zip(named, args)); mp['__VA_ARGS__'] = va
            else:
                mp = dict(zip(params, args + [''] * (len(params) - len(args))))
            if mp:
                exp = re.sub(r'#\s*(\w+)', lambda mm: '""' if mm.group(1) in mp else mm.group(0), exp)
                exp = re.sub(r'\b(' + '|'.join(map(re.escape, mp)) + r')\b', lambda mm: mp[mm.group(1)], exp)
            exp = re.sub(r'\s*##\s*', '', exp)
        else:
            exp = body
        invoked = text[m.start():end]
        out.append(text[pos:m.start()])
        out.append(' ' + exp + ' ' + '\n' * invoked.count('\n'))
        MACRO_USES[name] += 1
        pos = end; changed = True
    out.append(text[pos:])
    return ''.join(out), changed


for p, code in pre.items():
    for _ in range(3):
        code, ch = expand_once(code, p)
        if not ch: break
    FILES[p] = code
# Which lines of each file hold an expandable macro invocation (line granularity is what the report needs).
MACRO_LINES = collections.defaultdict(dict)   # path -> {line: macro}
for p, code in pre.items():
    for n in EXPANDABLE:
        if n not in code: continue
        for m in re.finditer(r'\b' + re.escape(n) + r'\b', code):
            MACRO_LINES[p].setdefault(code.count('\n', 0, m.start()) + 1, n)


def area(path):
    return 'tests' if path.startswith('tests/') else 'product'


# ----------------------------------------------------------------------------------------------------------
# 2. Scope scan
# ----------------------------------------------------------------------------------------------------------
class Scope:
    __slots__ = ('kind', 'name', 'head', 'start', 'end', 'parent', 'children', 'file', 'line', 'path', 'id')


# What may precede a class head in the same statement: access labels, attributes, and an UNTERMINATED macro
# invocation (`DEFINE_TYPE(X, ...)` has no trailing `;`, so the next class head shares its segment).
ACCESS_LEAD = re.compile(r'^\s*(?:(?:public|private|protected)\s*:(?!:)\s*|\[\[[^\]]*\]\]\s*|template\s*<>\s*|'
                         r'[A-Z][A-Z0-9_]+\s*\([^()]*(?:\([^()]*\)[^()]*)*\)\s*)*', re.S)
CLASS_HEAD = re.compile(r'^(?P<tpl>template\s*<.*>\s*)?(?:typedef\s+)?(?P<kw>class|struct|union)\s+'
                        r'(?:(?:alignas\s*\([^)]*\)|__attribute__\s*\(\(.*?\)\)|__declspec\s*\([^)]*\)|[A-Z_]+_API)\s+)*'
                        r'(?P<name>[A-Za-z_]\w*(?:\s*::\s*[A-Za-z_]\w*)*)\s*(?P<spec><[^{]*?>)?\s*(?P<final>final)?\s*'
                        r'(?::(?P<bases>[^{]*))?$', re.S)
NS_HEAD = re.compile(r'^(?:inline\s+)?namespace(?:\s+(?P<name>[\w:]+))?\s*$')
EXTERN_C = re.compile(r'^extern\s*""\s*$')

ALL_SCOPES = []


def scan(path, text):
    root = Scope(); root.kind = 'file'; root.name = None; root.head = ''; root.start = -1; root.end = len(text)
    root.parent = None; root.children = []; root.file = path; root.line = 1; root.path = (); root.id = len(ALL_SCOPES)
    ALL_SCOPES.append(root)
    stack = [root]; seg = 0; line = 1; i = 0; n = len(text)
    paren = 0
    while i < n:
        c = text[i]
        if c == '\n': line += 1
        elif c == '(': paren += 1
        elif c == ')': paren = max(0, paren - 1)
        elif c == ';' and paren == 0: seg = i + 1
        elif c == '{':
            head = ' '.join(text[seg:i].split())
            top = stack[-1]
            sc = Scope(); sc.head = head; sc.start = i; sc.end = None; sc.parent = top; sc.children = []
            sc.file = path; sc.line = line; sc.id = len(ALL_SCOPES); sc.name = None
            h = ACCESS_LEAD.sub('', head)
            m = CLASS_HEAD.match(h) if paren == 0 else None   # local classes (inside a function) included
            nm = NS_HEAD.match(h)
            if nm and top.kind in ('file', 'ns', 'linkage'):
                sc.kind = 'ns'; sc.name = nm.group('name') or '(anon)'
                sc.path = top.path + ((() if sc.name == '(anon)' else tuple(sc.name.split('::'))))
            elif EXTERN_C.match(h) and top.kind in ('file', 'ns'):
                sc.kind = 'linkage'; sc.path = top.path
            elif m and not re.search(r'\benum\b', h) and '=' not in h.split(':')[0] and '(' not in m.group('name'):
                sc.kind = 'class'; sc.name = re.sub(r'\s+', '', m.group('name'))
                sc.path = top.path + tuple(sc.name.split('::'))
            else:
                sc.kind = 'other'; sc.path = top.path
            top.children.append(sc); ALL_SCOPES.append(sc); stack.append(sc); seg = i + 1
            if sc.kind != 'other': paren = 0
        elif c == '}':
            if len(stack) > 1:
                sc = stack.pop(); sc.end = i
            else:
                unres('brace-imbalance', f'{path}:{line}', 'extra }')
            seg = i + 1
        i += 1
    if len(stack) > 1:
        unres('brace-imbalance', f'{path}:{stack[-1].line}', f'{len(stack) - 1} unclosed scope(s), innermost head: {stack[-1].head[:80]}')
    return root


ROOTSCOPES = {p: scan(p, t) for p, t in FILES.items()}


def own_text(sc):
    """The scope's own-level text with each child replaced by {@k} (k = index into sc.children)."""
    t = FILES[sc.file]; out = []; pos = sc.start + 1 if sc.kind != 'file' else 0
    for k, ch in enumerate(sc.children):
        out.append(t[pos:ch.start]); out.append('{@%d}' % k); pos = (ch.end + 1) if ch.end is not None else len(t)
    out.append(t[pos:sc.end if sc.end is not None else len(t)])
    return ''.join(out)


def body_text(sc):
    return FILES[sc.file][sc.start + 1:sc.end]


def line_of(path, idx):
    return FILES[path].count('\n', 0, idx) + 1


# ----------------------------------------------------------------------------------------------------------
# 3. Classes and their members
# ----------------------------------------------------------------------------------------------------------
class Cls:
    pass


CLASSES = []
BY_QNAME = collections.defaultdict(list)


def statements(sc):
    """Yield (text, child_or_None, access, char_index_in_file) at class level, in order."""
    ot = own_text(sc)
    m = CLASS_HEAD.match(ACCESS_LEAD.sub('', sc.head))
    access = 'public' if m and m.group('kw') in ('struct', 'union') else 'private'
    # tokenise into statements ending with ';' or a child block (a child block may be followed by ';')
    toks = re.split(r'(;|\{@\d+\})', ot)
    cur = ''
    for tk in toks:
        if tk == ';':
            if cur.strip():
                s, access = eat_access(cur, access)
                if s.strip(): yield (s, None, access)
            cur = ''
        elif tk.startswith('{@'):
            ch = sc.children[int(tk[2:-1])]
            s, access = eat_access(cur, access)
            yield (s, ch, access)
            cur = ''
        else:
            cur += tk
    if cur.strip():
        s, access = eat_access(cur, access)
        if s.strip(): yield (s, None, access)


ACC_RE = re.compile(r'\b(public|private|protected)\s*:(?!:)')


def eat_access(s, access):
    last = None
    for m in ACC_RE.finditer(s):
        last = m
    if last:
        access = last.group(1); s = s[last.end():]
    return s, access


def parse_bases(txt):
    if not txt: return []
    out = []
    for b in split_args(txt):
        b = re.sub(r'\b(public|protected|private|virtual)\b', '', b).strip()
        if b: out.append(b)
    return out


for sc in ALL_SCOPES:
    if sc.kind != 'class': continue
    m = CLASS_HEAD.match(ACCESS_LEAD.sub('', sc.head))
    c = Cls(); c.sc = sc; c.file = sc.file; c.line = sc.line
    c.qname = '::'.join(sc.path); c.short = sc.path[-1]
    c.kw = m.group('kw'); c.template = bool(m.group('tpl')); c.spec = (m.group('spec') or '').strip()
    c.bases_raw = parse_bases(m.group('bases'))
    c.area = area(sc.file)
    c.macro = MACRO_LINES[sc.file].get(sc.line)
    if c.macro and c.macro not in {x[0] for x in MACRO_EMITS_CLASS}: c.macro = None
    c.dd_decl = None; c.dd_def = None; c.dd_shape = None; c.dd_body = None; c.dd_free = None; c.dd_param = None
    c.dd_access = None; c.dd_virtual_kw = False
    c.dtor = None          # (access, kind, where, virtual)
    c.dtor_def = None
    c.opdel = []; c.opnew = []
    c.first_virtual = None
    c.virtuals = []; c.overrides = 0
    for (s, ch, acc) in statements(sc):
        ss = ' '.join(s.split())
        if DD_RE.search(ss):
            ln = line_of(sc.file, ch.start) if ch else None
            if c.dd_decl is None:
                c.dd_access = acc
                c.dd_virtual_kw = bool(re.search(r'\bvirtual\b', ss))
                pm = re.search(r'DeletingDtor\s*\(\s*(?:unsigned|uint32_t|int|uint8_t)\s*(\w*)\s*\)', ss)
                c.dd_param = pm.group(1) if pm else ''
                if ch is not None:
                    c.dd_decl = (sc.file, line_of(sc.file, ch.start))
                    c.dd_def = (sc.file, c.dd_decl[1], 'inline')
                    c.dd_body = ' '.join(body_text(ch).split())
                elif re.search(r'=\s*0\s*$', ss):
                    c.dd_decl = (sc.file, None); c.dd_shape = 'PURE'
                else:
                    c.dd_decl = (sc.file, None)
            else:
                unres('dd-duplicate-decl', f'{sc.file}:{sc.line}', c.qname)
        if re.search(r'\bvirtual\b', ss) or re.search(r'\boverride\b', ss) or (ch is None and re.search(r'\)\s*(?:const\s*)?override', ss)):
            fm = re.search(r'(~?\w+)\s*\([^()]*(?:\([^()]*\)[^()]*)*\)\s*(?:const)?\s*(?:noexcept)?\s*(?:override|final|=\s*0|$)', ss)
            fname = fm.group(1) if fm else ss[-40:]
            if re.search(r'\boverride\b', ss): c.overrides += 1
            if re.search(r'\bvirtual\b', ss):
                c.virtuals.append(fname)
                if c.first_virtual is None: c.first_virtual = fname
        dm = re.search(r'(?:^|[\s:])~\s*' + re.escape(c.short) + r'\s*\(', ss)
        if dm and 'operator' not in ss:
            if ch is not None: kind = 'user-provided(inline)'
            elif re.search(r'=\s*default', ss): kind = '=default'
            elif re.search(r'=\s*delete', ss): kind = '=delete'
            else: kind = 'declared'
            c.dtor = (acc, kind, bool(re.search(r'\bvirtual\b', ss)))
            if ch is not None: c.dtor_def = (sc.file, line_of(sc.file, ch.start))
            elif kind == '=default': c.dtor_def = (sc.file, None)
        if re.search(r'\boperator\s+delete\b', ss):
            c.opdel.append((sc.file, None if ch is None else line_of(sc.file, ch.start), ss[:120], ch is not None))
        if re.search(r'\boperator\s+new\b', ss):
            c.opnew.append((sc.file, None, ss[:120], ch is not None))
    CLASSES.append(c)
    BY_QNAME[c.qname].append(c)


def find_line_in_scope(sc, pat):
    t = FILES[sc.file]
    for m in pat.finditer(t, sc.start, sc.end):
        # the match must be at the class's own level
        idx = m.start()
        if any(ch.start < idx < (ch.end or 0) for ch in sc.children): continue
        return line_of(sc.file, idx)
    return None


for c in CLASSES:
    if c.dd_decl and c.dd_decl[1] is None:
        c.dd_decl = (c.file, find_line_in_scope(c.sc, DD_RE))
    c.opdel = [(f, l if l else find_line_in_scope(c.sc, re.compile(r'operator\s+delete')), s, inl) for (f, l, s, inl) in c.opdel]
    c.opnew = [(f, l if l else find_line_in_scope(c.sc, re.compile(r'operator\s+new')), s, inl) for (f, l, s, inl) in c.opnew]


# ----------------------------------------------------------------------------------------------------------
# 4. Name resolution
# ----------------------------------------------------------------------------------------------------------
def prefer(cands, ctx_file):
    if len(cands) <= 1: return cands[0] if cands else None
    same = [c for c in cands if c.file == ctx_file]
    if len(same) >= 1: return same[0]
    ar = area(ctx_file)
    a = [c for c in cands if c.area == ar] or cands
    # product: prefer headers; a class defined twice in product code is reported
    hdr = [c for c in a if c.file.endswith(('.h', '.hpp'))] or a
    if len({x.file for x in hdr}) > 1:
        unres('multiply-defined', ctx_file, f'{hdr[0].qname}: ' + ', '.join(f'{x.file}:{x.line}' for x in hdr[:4]))
    return hdr[0]


def resolve(name, scope_path, ctx_file):
    name = re.sub(r'<.*>', '', name).strip()
    name = re.sub(r'\s+', '', name)
    if name.startswith('::'):
        return prefer(BY_QNAME.get(name[2:], []), ctx_file), 'exact'
    parts = tuple(name.split('::'))
    for k in range(len(scope_path), -1, -1):
        q = '::'.join(scope_path[:k] + parts)
        if q in BY_QNAME: return prefer(BY_QNAME[q], ctx_file), 'scoped'
    suff = [q for q in BY_QNAME if q == name or q.endswith('::' + name)]
    if len(suff) == 1: return prefer(BY_QNAME[suff[0]], ctx_file), 'suffix'
    if len(suff) > 1:
        # prefer candidates in the same area
        cands = [x for q in suff for x in BY_QNAME[q]]
        same = [x for x in cands if x.file == ctx_file]
        if same: return same[0], 'suffix-samefile'
        return None, 'ambiguous:' + ','.join(suff[:5])
    return None, 'unknown'


for c in CLASSES:
    c.bases = []
    for b in c.bases_raw:
        r, how = resolve(b, c.sc.path[:-1], c.file)
        if r is c: r = None; how = 'self'
        c.bases.append((b, r, how))


def has_dd(c, seen=None):
    seen = seen or set()
    if id(c) in seen: return False
    seen.add(id(c))
    if c.dd_decl: return True
    return any(r is not None and has_dd(r, seen) for (_, r, _) in c.bases)


def polymorphic(c, seen=None):
    seen = seen or set()
    if id(c) in seen: return False
    seen.add(id(c))
    if c.virtuals or c.dd_decl: return True
    return any(r is not None and polymorphic(r, seen) for (_, r, _) in c.bases)


def dd_base(c):
    """The base through which c inherits DeletingDtor (first such), or None."""
    for (b, r, how) in c.bases:
        if r is not None and has_dd(r): return r
    return None


def root_of(c):
    seen = set(); cur = c
    while True:
        if id(cur) in seen: return cur
        seen.add(id(cur))
        b = dd_base(cur)
        if b is None: return cur
        cur = b


def poly_root(c):
    seen = set(); cur = c
    while True:
        if id(cur) in seen: return cur
        seen.add(id(cur))
        nxt = None
        for (b, r, how) in cur.bases:
            if r is not None and polymorphic(r): nxt = r; break
        if nxt is None: return cur
        cur = nxt


# ----------------------------------------------------------------------------------------------------------
# 5. Out-of-class definitions (DeletingDtor, ~T, operator delete / new)
# ----------------------------------------------------------------------------------------------------------
OOC_DD = re.compile(r'((?:::)?[A-Za-z_]\w*(?:\s*<[^;{}]*?>)?(?:\s*::\s*[A-Za-z_]\w*(?:\s*<[^;{}]*?>)?)*)\s*::\s*DeletingDtor\s*\(\s*(?:unsigned|uint32_t|int|uint8_t)\s*(\w*)\s*\)')
OOC_DTOR = re.compile(r'((?:::)?[A-Za-z_]\w*(?:\s*::\s*[A-Za-z_]\w*)*)\s*::\s*~\s*(\w+)\s*\(\s*(?:void)?\s*\)')
OOC_OPDEL = re.compile(r'((?:::)?[A-Za-z_]\w*(?:\s*::\s*[A-Za-z_]\w*)*)\s*::\s*operator\s+(delete|new)\s*\(')

DEF_LINES = set()   # (file, line) attributed to a DeletingDtor decl/def
OOC_DEFS = []


def enclosing_path(sc):
    p = sc.parent
    while p is not None and p.kind not in ('ns', 'file', 'class', 'linkage'): p = p.parent
    return p.path if p is not None else ()


def resolve_qualifier(q, sc):
    q = re.sub(r'\s+', '', q)
    q = re.sub(r'<[^<>]*>', '', q)
    return resolve(q, enclosing_path(sc), sc.file)


for sc in ALL_SCOPES:
    if sc.kind != 'other' or sc.parent.kind not in ('file', 'ns', 'linkage'): continue
    h = sc.head
    m = OOC_DD.search(h)
    if m:
        cl, how = resolve_qualifier(m.group(1), sc)
        ln = sc.line
        rec = (sc.file, ln, m.group(1), cl, how)
        OOC_DEFS.append(rec)
        if cl is None:
            unres('dd-def-unmatched', f'{sc.file}:{ln}', f'{m.group(1)}::DeletingDtor ({how})')
        else:
            if cl.dd_def and cl.dd_def[2] != 'inline':
                unres('dd-def-duplicate', f'{sc.file}:{ln}', f'{cl.qname} already defined at {cl.dd_def[0]}:{cl.dd_def[1]}')
            elif cl.dd_def and cl.dd_def[2] == 'inline':
                unres('dd-def-duplicate', f'{sc.file}:{ln}', f'{cl.qname} also inline at {cl.dd_def[0]}:{cl.dd_def[1]}')
            else:
                cl.dd_def = (sc.file, ln, 'ooc'); cl.dd_body = ' '.join(body_text(sc).split())
                cl.dd_param = m.group(2) or cl.dd_param
        continue
    m = OOC_DTOR.search(h)
    if m and m.group(1).split('::')[-1] == m.group(2):
        cl, how = resolve_qualifier(m.group(1), sc)
        if cl is not None and cl.dtor_def is None: cl.dtor_def = (sc.file, sc.line)
        continue
    m = OOC_OPDEL.search(h)
    if m:
        cl, how = resolve_qualifier(m.group(1), sc)
        if cl is not None:
            (cl.opdel if m.group(2) == 'delete' else cl.opnew).append((sc.file, sc.line, 'DEFINITION ' + ' '.join(h.split())[:120], True))

# `= default` dtors declared out of class: `X::~X() = default;`
for p, t in FILES.items():
    for m in re.finditer(r'((?:[A-Za-z_]\w*::)+)~\s*(\w+)\s*\(\s*\)\s*=\s*default\s*;', t):
        q = m.group(1)[:-2]
        if q.split('::')[-1] != m.group(2): continue
        cl, how = resolve(q, (), p)
        if cl is not None and cl.dtor_def is None: cl.dtor_def = (p, line_of(p, m.start()))


# ----------------------------------------------------------------------------------------------------------
# 6. Shape classification
# ----------------------------------------------------------------------------------------------------------
def classify(c):
    if not c.dd_decl: return 'INHERITED'
    if c.dd_shape == 'PURE': return 'PURE'
    b = c.dd_body
    if b is None: return 'NODEF'
    compact = re.sub(r'\s+', '', b)
    P = re.escape(c.dd_param) if c.dd_param else r'\w+'
    this_arg = r'(?:(?:static_cast|reinterpret_cast)<[^>]*>\(this\)|\(void\*\)this|this)'
    lf = re.search(r'if\(\(?' + P + r'&(?:1u?|0x1u?)\)?(?:!=0u?)?\)\{?([\w:]+)\(', compact)
    c.dd_free_loose = lf.group(1) if lf else None
    if re.search(P + r'&(?:0x)?2u?\b', compact) or re.search(r'&(?:0x)?2u?\)', compact):
        # MSVC's VECTOR deleting dtor: `if (flags & 2) { count at this-8; ~T() per element; free(this-8) on &1;
        # return this-8; }` followed by the scalar arm, which must itself be STD.
        m = re.match(r'if\(\(?' + P + r'&(?:2u?|0x2u?)\)?\)\{(.*?)->~(\w+)\(\);(.*?)if\(\(?' + P +
                     r'&(?:1u?|0x1u?)\)?\)\{?([\w:]+)\(([^;]*)\);\}?return[^;]*;\}(.*)$', compact)
        if m:
            c.vec_free = m.group(4); tail = m.group(6)
            em = re.search(r'\*(0x[0-9a-fA-F]+|\d+)', m.group(1) + m.group(2))
            c.vec_elem = em.group(1) if em else '?'
            mt = re.fullmatch(r'this->(?:(?:::)?\w+::)*~(\w+)\(\);'
                              r'if\(\(?' + P + r'&(?:1u?|0x1u?)\)?(?:!=0u?)?\)\{?([\w:]+(?:<[^>]*>)?)\(' + this_arg + r'\);\}?'
                              r'return(?:' + this_arg + r');', tail)
            if mt and mt.group(1) == c.short and m.group(2) == c.short:
                c.dd_free = mt.group(2)
                if c.vec_free != mt.group(2): c.dd_note = f'vector arm frees with {c.vec_free}'
                return 'VEC'
            c.dd_note = 'vector arm parsed; scalar arm not STD' if not mt else f'destroys ~{mt.group(1)} / ~{m.group(2)}'
            return 'VEC-OTHER'
        c.dd_note = 'vector arm not in the standard shape'
        return 'VEC-OTHER'
    m = re.fullmatch(r'this->(?:(?:::)?\w+::)*~(\w+)\(\);'
                     r'if\(\(?' + P + r'&(?:1u?|0x1u?)\)?(?:!=0u?)?\)\{?([\w:]+(?:<[^>]*>)?)\(' + this_arg + r'\);\}?'
                     r'return(?:' + this_arg + r');', compact)
    if m:
        c.dd_free = m.group(2)
        if m.group(1) != c.short:
            c.dd_note = f'destroys ~{m.group(1)} not ~{c.short}'
            return 'OTHER'
        return 'STD'
    return 'OTHER'


for c in CLASSES:
    c.dd_note = ''; c.dd_free_loose = None; c.vec_elem = ''; c.vec_free = None
    c.in_scope = has_dd(c)
for c in CLASSES:
    c.shape = classify(c) if c.in_scope else None
    if c.in_scope and c.dd_decl and c.dd_body is None and c.dd_shape != 'PURE':
        unres('dd-decl-no-def', f'{c.file}:{c.dd_decl[1]}', c.qname)


MANGLED = re.compile(r'"(\?[^"]*@@[^"]*)"')


def free_target(spelling):
    """What a FreeFn spelling finally calls: the first mangled export literal in its PRODUCT definition (a
    function body `void NAME(void* ...) {`, or a function-pointer global assigned from ResolveExport). The unit
    build's `std::free` twins (inside `#if defined(BANISHED_UNIT_TEST)`) are skipped by taking the definition
    whose next 600 characters name an export."""
    if spelling in free_target.cache: return free_target.cache[spelling]
    last = spelling.split('::')[-1]
    pats = [re.compile(r'\bvoid\s+' + re.escape(last) + r'\s*\(\s*void\s*\*\s*\w*\s*\)\s*\{'),
            re.compile(r'\b' + re.escape(last) + r'\s*=\s*reinterpret_cast\s*<[^>]*>\s*\(')]
    hits = []
    for p, lines in RAW.items():
        if p.startswith('tests/'): continue
        t = '\n'.join(lines)
        if last not in t: continue
        for k, pat in enumerate(pats):
            for m in pat.finditer(t):
                # the body only: to the matching `}` (definition) or the `;` (assignment)
                d = 1; e = m.end()
                if k == 0:
                    while e < len(t) and d:
                        d += {'{': 1, '}': -1}.get(t[e], 0); e += 1
                else:
                    e = t.find(';', e)
                mm = MANGLED.search(t, m.end(), e)
                if mm: hits.append((mm.group(1), f'{p}:{t.count(chr(10), 0, m.start()) + 1}'))
    tg = sorted({h[0] for h in hits})
    res = (' | '.join(tg) if tg else 'NOT RESOLVED', ', '.join(sorted({h[1] for h in hits}))[:200])
    free_target.cache[spelling] = res
    return res


free_target.cache = {}


def eff_free(c, seen=None):
    """The FreeFn the class's slot 0 uses (own STD body, else the nearest base that has one)."""
    seen = seen or set()
    if id(c) in seen: return None
    seen.add(id(c))
    if c.dd_free: return c.dd_free
    if c.dd_decl and c.shape not in ('INHERITED', 'PURE'): return None if c.shape != 'STD' else c.dd_free
    b = dd_base(c)
    return eff_free(b, seen) if b is not None else None


def chain_opdel(c, seen=None):
    seen = seen or set()
    if id(c) in seen: return None
    seen.add(id(c))
    if c.opdel: return c
    for (_, r, _) in c.bases:
        if r is not None:
            x = chain_opdel(r, seen)
            if x: return x
    return None


SC = [c for c in CLASSES if c.in_scope]
for c in SC:
    c.root = root_of(c)
    b = dd_base(c)
    c.base_free = eff_free(b) if b is not None else None
    c.own_free = c.dd_free
    for (bn, r, how) in c.bases:
        if r is None and how not in ('self',):
            # an unresolved base is only interesting when it could carry the dtor
            unres('base-unresolved', f'{c.file}:{c.line}', f'{c.qname} : {bn} ({how})')
    if c.template: unres('template-class', f'{c.file}:{c.line}', f'{c.qname} {c.spec} (template; instantiations not enumerated)')
    if c.macro: unres('macro-class', f'{c.file}:{c.line}', f'{c.qname} via {c.macro}')
    dbs = [r for (_, r, _) in c.bases if r is not None and has_dd(r)]
    if len(dbs) > 1: unres('multiple-dd-bases', f'{c.file}:{c.line}', f'{c.qname}: ' + ', '.join(x.qname for x in dbs))
    elif dbs and c.bases and c.bases[0][1] is not dbs[0]:
        unres('dd-base-not-first', f'{c.file}:{c.line}', f'{c.qname}: DeletingDtor via {dbs[0].qname}, first base {c.bases[0][0]}')

# A class OUTSIDE the census whose base did not resolve but which overrides something may be a missed member
# (e.g. `: public T`, a typedef'd base, a base defined in a file the scan does not read).
for c in CLASSES:
    if c.in_scope or not c.overrides: continue
    for (bn, r, how) in c.bases:
        if r is None:
            unres('base-unresolved', f'{c.file}:{c.line}', f'{c.qname} : {bn} ({how}; {c.overrides} override(s), NOT counted)')


def where(f, l):
    return f'{f}:{l}' if l else (f or '')


def row(c):
    base = c.bases_raw[0] if c.bases_raw else ''
    dd_decl = where(*c.dd_decl) if c.dd_decl else ''
    dd_def = (where(c.dd_def[0], c.dd_def[1]) + (' (inline)' if c.dd_def[2] == 'inline' else ' (ooc)')) if c.dd_def else ''
    if c.dtor:
        dt = f'{c.dtor[0]} {c.dtor[1]}' + (' virtual' if c.dtor[2] else '')
    else:
        dt = 'absent'
    dtd = where(*c.dtor_def) if c.dtor_def else ''
    od = chain_opdel(c)
    opd = ('own ' if od is c else f'via {od.qname} ') + where(od.opdel[0][0], od.opdel[0][1]) + \
          (' DEFINED' if any(x[3] for x in od.opdel) else ' decl-only') if od else 'none'
    opn = where(c.opnew[0][0], c.opnew[0][1]) if c.opnew else ''
    return [c.qname, where(c.file, c.line), base, c.root.qname, 'yes' if c.dd_decl else 'no', dd_decl, dd_def,
            c.shape, c.dd_free or '', c.base_free or '', dt, dtd, opd, opn, c.area,
            c.macro or '', 'template' if c.template else '', c.vec_elem or '', c.dd_note]


COLS = ['class', 'defined_at', 'direct_base', 'root', 'declares_dd', 'dd_decl', 'dd_def', 'shape', 'free_fn',
        'base_free_fn', 'dtor', 'dtor_def', 'class_operator_delete', 'class_operator_new', 'area', 'via_macro',
        'template', 'vec_elem', 'note']


# ----------------------------------------------------------------------------------------------------------
# 7. Non-DD polymorphic roots (exempt candidates)
# ----------------------------------------------------------------------------------------------------------
def nondd():
    roots = collections.OrderedDict()
    for c in CLASSES:
        if not polymorphic(c): continue
        r = poly_root(c)
        if r.dd_decl and (r.first_virtual in (None, 'DeletingDtor')): continue
        if r.first_virtual == 'DeletingDtor': continue
        roots.setdefault(id(r), (r, []))[1].append(c)
    return list(roots.values())


# ----------------------------------------------------------------------------------------------------------
# 8. Cross-check against partc.py's pattern
# ----------------------------------------------------------------------------------------------------------
def crosscheck():
    attributed = set()
    for c in CLASSES:
        if c.dd_decl and c.dd_decl[1]: attributed.add((c.dd_decl[0], c.dd_decl[1]))
    for (f, l, q, cl, how) in OOC_DEFS:
        attributed.add((f, l))
    in_define = set()
    for n, lst in MACROS.items():
        for (params, body, f, l) in lst:
            if 'DeletingDtor' in body:
                k = l
                while True:
                    in_define.add((f, k))
                    if not RAW[f][k - 1].rstrip().endswith('\\'): break
                    k += 1
    total = 0; miss = []; macro_lines = 0
    for p, lines in RAW.items():
        for i, l in enumerate(lines, 1):
            if DD_RE.search(l):
                total += 1
                if (p, i) in in_define and (p, i) not in attributed: macro_lines += 1; continue
                if (p, i) not in attributed: miss.append((p, i, l.strip()))
    crosscheck.macro_lines = macro_lines
    return total, attributed, miss


# ----------------------------------------------------------------------------------------------------------
def summary():
    prod = [c for c in SC if c.area == 'product']; tst = [c for c in SC if c.area == 'tests']
    print(f'classes that declare or inherit DeletingDtor(unsigned): {len(SC)}  (product {len(prod)}, tests {len(tst)})')
    print(f'  of which declare it themselves: {sum(1 for c in SC if c.dd_decl)}  (product {sum(1 for c in prod if c.dd_decl)}, tests {sum(1 for c in tst if c.dd_decl)})')
    print('\nroots (hierarchies) -> classes  [product / tests]  (root file)')
    byroot = collections.defaultdict(list)
    for c in SC: byroot[id(c.root)].append(c)
    items = sorted(byroot.values(), key=lambda v: -len(v))
    for v in items:
        r = v[0].root; np = sum(1 for c in v if c.area == 'product'); nt = len(v) - np
        dirs = collections.Counter(c.file.split('/')[0] + ('/' + c.file.split('/')[1] if c.file.startswith(('code/', 'runtime/')) else '') for c in v)
        print(f'  {r.qname:45s} {len(v):4d}  [{np} / {nt}]  {r.file}:{r.line}  dirs: ' + ', '.join(f'{d} {n}' for d, n in dirs.most_common(6)))
    print('\nshape (product / tests):')
    for s, n in collections.Counter(c.shape for c in SC).most_common():
        print(f'  {s:10s} {n:4d}  [{sum(1 for c in prod if c.shape == s)} / {sum(1 for c in tst if c.shape == s)}]')
    print('\nFreeFn spellings in STD / VEC bodies (scalar arm) (product / tests) -> what the spelling finally calls:')
    for f, n in collections.Counter(c.dd_free for c in SC if c.dd_free).most_common():
        tg, at = free_target(f)
        print(f'  {f:40s} {n:4d}  [{sum(1 for c in prod if c.dd_free == f)} / {sum(1 for c in tst if c.dd_free == f)}]  -> {tg}  ({at})')
    loose = collections.Counter(c.dd_free_loose for c in SC if c.shape in ('OTHER', 'VEC-OTHER') and c.dd_free_loose)
    if loose:
        print('FreeFn called on the &1 arm of OTHER / VEC-OTHER bodies:')
        for f, n in loose.most_common():
            print(f'  {f:40s} {n:4d}  -> {free_target(f)[0]}  ({free_target(f)[1]})')
    vf = collections.Counter(c.vec_free for c in SC if c.vec_free and c.vec_free != c.dd_free)
    if vf: print(f'VEC bodies whose vector arm frees with a different spelling than the scalar arm: {dict(vf)}')
    diff = [c for c in SC if c.own_free and c.base_free and c.own_free != c.base_free]
    difft = [c for c in diff if free_target(c.own_free)[0] != free_target(c.base_free)[0]]
    print(f'\nclasses whose FreeFn SPELLING differs from their base\'s: {len(diff)}; whose free TARGET differs: {len(difft)}')
    for c in difft: print(f'  TARGET DIFFERS {c.qname}  {c.file}:{c.line}  own={c.own_free} base={c.base_free}')
    for c in diff[:0]: print(f'  {c.qname}  {c.file}:{c.line}  own={c.own_free} base={c.base_free}')
    pp = [c for c in SC if c.dtor and c.dtor[0] != 'public']
    print(f'\nclasses with a protected/private dtor: {len(pp)}  (product {sum(1 for c in pp if c.area == "product")})'
          f'  by access: {dict(collections.Counter(c.dtor[0] for c in pp))}')
    print(f'classes with NO dtor declared: {sum(1 for c in SC if not c.dtor)}; with a public dtor: {sum(1 for c in SC if c.dtor and c.dtor[0] == "public")}')
    print(f'classes that already have a VIRTUAL dtor: {sum(1 for c in SC if c.dtor and c.dtor[2])}')
    nod = [c for c in SC if not chain_opdel(c)]
    print(f'\nclasses with NO class-scope operator delete in their chain: {len(nod)}  (product {sum(1 for c in nod if c.area == "product")})')
    wod = [c for c in SC if chain_opdel(c)]
    print(f'classes WITH one: {len(wod)}; of those, the operator delete found is DEFINED (not decl-only): '
          f'{sum(1 for c in wod if any(x[3] for x in chain_opdel(c).opdel))}')
    total, att, miss = crosscheck()
    print(f'\ncross-check: partc pattern lines {total}; attributed to a class decl/def {total - len(miss) - crosscheck.macro_lines}; '
          f'inside a #define body (counted at each expansion instead) {crosscheck.macro_lines}; unattributed {len(miss)}')
    print(f'  = {sum(1 for c in SC if c.dd_decl)} in-class declarations ({sum(1 for c in SC if c.macro or (c.dd_decl and MACRO_LINES[c.file].get(c.dd_decl[1]) in {x[0] for x in MACRO_EMITS_DD}))} of them emitted by a macro expansion, '
          f'whose #define lines are the {crosscheck.macro_lines} above) + {len(OOC_DEFS)} out-of-class definitions')
    print(f'macros whose body emits DeletingDtor: {len(MACRO_EMITS_DD)}; macros that emit a class: {len(MACRO_EMITS_CLASS)}')
    print_unresolved()


def print_unresolved():
    seen = set(); u = []
    for x in UNRESOLVED:
        if x not in seen: seen.add(x); u.append(x)
    print(f'\nUNRESOLVED ({len(u)}):')
    for k, n in collections.Counter(x[0] for x in u).most_common():
        print(f'  {k}: {n}')
    for (k, w, t) in u:
        print(f'  UNRESOLVED {k:20s} {w}  {t}')


def main():
    a = sys.argv[1:]
    if '--tsv' in a:
        print('\t'.join(COLS))
        for c in sorted(SC, key=lambda c: (c.root.qname, c.area, c.qname)):
            print('\t'.join(str(x) for x in row(c)))
    elif '--other' in a:
        for c in SC:
            if c.shape in ('OTHER', 'VEC-OTHER', 'NODEF') or ('--all' in a and c.shape == 'VEC'):
                d = where(c.dd_def[0], c.dd_def[1]) if c.dd_def else where(*c.dd_decl)
                print(f'{c.shape}\t{c.area}\t{c.qname}\t{d}\t{c.dd_note}\t{(c.dd_body or "")[:400]}')
    elif '--nondd' in a:
        for (r, members) in nondd():
            print(f'ROOT {r.qname}\t{r.file}:{r.line}\tslot0={r.first_virtual}\tvirtual dtor={bool(r.dtor and r.dtor[2])}\t{len(members)} class(es)\t{r.area}')
            for c in members:
                if c is not r: print(f'    {c.qname}\t{c.file}:{c.line}\t{c.area}')
    elif '--macros' in a:
        print('# macros whose body contains DeletingDtor')
        for (n, f, l) in MACRO_EMITS_DD: print(f'{n}\t{f}:{l}\tuses={MACRO_USES[n]}')
        print('# macros whose body defines a class/struct')
        for (n, f, l) in MACRO_EMITS_CLASS: print(f'{n}\t{f}:{l}\tuses={MACRO_USES[n]}')
    elif '--crosscheck' in a:
        total, att, miss = crosscheck()
        print(f'# partc pattern lines {total}; unattributed {len(miss)}')
        for (p, i, l) in miss: print(f'{p}:{i}: {l[:160]}')
    elif '--unresolved' in a:
        print_unresolved()
    else:
        summary()


if __name__ == '__main__':
    main()
