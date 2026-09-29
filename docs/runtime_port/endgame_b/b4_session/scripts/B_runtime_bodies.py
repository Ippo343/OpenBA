#!/usr/bin/env python3
"""B_runtime_bodies.py -- B.4 block 0, inventory B (runtime bodies). Read-only.

Re-run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b4_session/scripts/B_runtime_bodies.py
Writes  docs/runtime_port/endgame_b/b4_session/data/B_runtime_bodies.tsv
Reads   docs/runtime_port/endgame_b/b4_session/data/B_runtime_exports.tsv (Runtime-x64.dll's
        4,926 mangled exports with their retail addresses, dumped once from Ghidra's
        list_exports, program="Runtime-x64.dll"; the script never talks to Ghidra).
Prints  the per-directory summaries the inventory quotes (reinterpret_cast causes, bridge
        markers, POD mirrors, class counts).

Rows of the table:
  kind=c_body          every free function in SCOPE whose name is Rt<Ns>_..., or which is
                       extern "C", or whose first parameter is a void* / a pointer named like a
                       receiver (self, this_, obj, ...).  Anonymous-namespace / static helpers
                       are included and marked linkage=internal.
  kind=decl_undefined  a member function DECLARED in a class of runtime/types (or engine/types,
                       dx11/types) with no definition anywhere in PROD (out of class, in class,
                       or =default/=delete/=0).
"""
import os, re, sys, collections

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..'))
os.chdir(ROOT)
B4 = 'docs/runtime_port/endgame_b/b4_session'
OUT = f'{B4}/data/B_runtime_bodies.tsv'
EXPORTS = f'{B4}/data/B_runtime_exports.tsv'

SCOPE = ['runtime/src', 'runtime/funcs', 'runtime/types', 'standalone', 'engine', 'dx11']
TYPE_DIRS = ['runtime/types', 'engine/types', 'dx11/types']
PROD = ['code', 'runtime', 'standalone', 'dx11', 'engine']
TESTS = ['tests']
EXCL = ('/standalone/build', '/tests/unit/build', '/.claude/worktrees', '/build/', '/build-')
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')


# ----------------------------------------------------------------------------- files
def walk(dirs):
    out = []
    for r in dirs:
        for dp, dn, fs in os.walk(r):
            dn.sort()
            for f in sorted(fs):
                p = os.path.join(dp, f).replace('\\', '/')
                if any(e in '/' + p for e in EXCL) or not p.endswith(EXTS):
                    continue
                out.append(p)
    return out


def strip_comments(src):
    """Comments -> blanks (line count kept). Strings kept."""
    out = []; i = 0; n = len(src); st = None
    while i < n:
        c = src[i]
        if st is None:
            if src.startswith('//', i):
                j = src.find('\n', i); j = n if j < 0 else j
                i = j; continue
            if src.startswith('/*', i):
                j = src.find('*/', i + 2); j = n if j < 0 else j + 2
                out.append('\n' * src.count('\n', i, j)); i = j; continue
            if c == '"' or c == "'":
                if c == '"' and i > 0 and src[i - 1] == 'R':
                    m = re.match(r'"([^(\s]*)\(', src[i:])
                    if m:
                        end = src.find(')' + m.group(1) + '"', i)
                        end = n if end < 0 else end + len(m.group(1)) + 2
                        out.append(src[i:end]); i = end; continue
                if c == "'" and i > 0 and src[i - 1].isalnum():
                    out.append(c); i += 1; continue
                st = c; out.append(c); i += 1; continue
            out.append(c); i += 1
        else:
            if c == '\\' and i + 1 < n:
                out.append(src[i:i + 2]); i += 2; continue
            if c == st or c == '\n':
                st = None
            out.append(c); i += 1
    return ''.join(out)


def blank_strings_and_pp(code):
    """For structure scanning: string/char literal contents -> spaces, preprocessor lines -> blank."""
    code = re.sub(r'"(?:\\.|[^"\\\n])*"', lambda m: m.group(0) if m.group(0) == '"C"' else '"' + ' ' * (len(m.group(0)) - 2) + '"', code)
    code = re.sub(r"(?<![A-Za-z0-9])'(?:\\.|[^'\\\n])+'", lambda m: "'" + ' ' * (len(m.group(0)) - 2) + "'", code)
    lines = code.split('\n'); cont = False
    for k, l in enumerate(lines):
        if cont or l.lstrip().startswith('#'):
            cont = l.rstrip().endswith('\\')
            lines[k] = ''
    return '\n'.join(lines)


CACHE = {}


def load(p):
    if p not in CACHE:
        raw = open(p, encoding='utf-8', errors='replace').read()
        code = strip_comments(raw)
        CACHE[p] = (raw, code, blank_strings_and_pp(code))
    return CACHE[p]


# ----------------------------------------------------------------------------- scanner
FUNC_TAIL = re.compile(r'\)\s*(?:const|noexcept(?:\([^)]*\))?|override|final|volatile|&&|&|\s|->\s*[\w:<>,\*&\s]+|'
                       r'__attribute__\s*\(\(.*?\)\))*$', re.S)
CTOR_INIT = re.compile(r'\)\s*(?:noexcept\s*)?:\s*[\w:<>]+\s*[({]', re.S)
CTRL = {'if', 'for', 'while', 'switch', 'catch', 'return', 'sizeof', 'decltype', 'alignof', 'static_assert',
        'defined', '__attribute__', '__declspec', 'alignas'}
TYPE_WORDS = {'void', 'int', 'char', 'bool', 'float', 'double', 'long', 'short', 'unsigned', 'signed', 'auto',
              'const', 'return', 'operator', 'new', 'delete', 'template', 'typename', 'using', 'typedef',
              'static', 'virtual', 'inline', 'explicit', 'friend', 'constexpr', 'extern'} | CTRL


def split_params(s):
    out = []; d = 0; cur = ''
    for c in s:
        if c in '(<[{':
            d += 1
        elif c in ')>]}':
            d -= 1
        if c == ',' and d == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def parse_declarator(header):
    """header = text before '{' of a function definition. -> (qualname, params_text) or None."""
    h = header
    # find the parameter list: the first top-level '(' that follows an identifier / operator token
    depth = 0; angle = 0
    for m in re.finditer(r'(operator\s*(?:\(\)|\[\]|[^\s(]+)|~?[A-Za-z_]\w*(?:\s*::\s*~?[A-Za-z_]\w*|\s*::\s*operator\s*(?:\(\)|\[\]|[^\s(]+))*)\s*\(', h):
        pre = h[:m.start()]
        if pre.count('(') != pre.count(')'):
            continue
        name = re.sub(r'\s+', '', m.group(1))
        last = name.split('::')[-1]
        if last in CTRL or last in ('__attribute__', '__declspec', 'alignas', 'decltype', 'noexcept'):
            continue
        # matching paren
        i = m.end() - 1; d = 0
        for j in range(i, len(h)):
            if h[j] == '(':
                d += 1
            elif h[j] == ')':
                d -= 1
                if d == 0:
                    return name, h[i + 1:j], pre, h[j + 1:]
        return None
    return None


class Scan:
    """One pass over a file: namespace stack, class bodies, function definitions."""

    def __init__(self, path):
        self.path = path
        raw, code, s = load(path)
        self.funcs = []     # dicts: name(qual as written), ns (list), cls (enclosing class or None), params, line, body_start, body_end, externC, internal, header
        self.classes = []   # dicts: name, ns, line, body_start, body_end, kind
        self._scan(s)

    def _scan(self, s):
        stack = []  # entries: (type, name, externC, anon)
        last = 0; i = 0; n = len(s)
        line_at = lambda pos: s.count('\n', 0, pos) + 1
        pending_externC = False
        while i < n:
            c = s[i]
            if c == ';':
                last = i + 1
            elif c == '}':
                if stack:
                    ent = stack.pop()
                    if ent[0] in ('func', 'class'):
                        ent[3]['body_end'] = i
                last = i + 1
            elif c == '{':
                header = s[last:i]
                hs = header.strip()
                ctx = [e[0] for e in stack]
                at_ns = all(t == 'ns' for t in ctx)
                in_class = bool(stack) and stack[-1][0] == 'class'
                kind = 'other'; info = None
                if re.search(r'\bnamespace\b\s*[\w:]*\s*$', hs) or re.search(r'extern\s*"\s*C\s*"\s*$', hs):
                    kind = 'ns'
                    nm = re.search(r'\bnamespace\b\s*([\w:]*)\s*$', hs)
                    info = {'name': nm.group(1) if nm else '', 'externC': bool(re.search(r'extern\s*"', hs)),
                            'anon': bool(nm) and nm.group(1) == ''}
                    stack.append(('ns', info['name'], info['externC'], info))
                elif (at_ns or in_class) and re.search(r'\b(class|struct|union)\b\s+(?:__attribute__\s*\(\(.*?\)\)\s*)?(?:alignas\s*\([^)]*\)\s*)?([A-Za-z_]\w*)(?:\s+final)?\s*(?::[^;{]*)?$', hs, re.S) \
                        and not re.search(r'=\s*$', hs) and '(' not in re.sub(r'(alignas|__attribute__)\s*\(\(?.*?\)\)?', '', hs.split(':')[0]):
                    m = re.search(r'\b(class|struct|union)\b\s+(?:__attribute__\s*\(\(.*?\)\)\s*)?(?:alignas\s*\([^)]*\)\s*)?([A-Za-z_]\w*)', hs, re.S)
                    ns = self._ns(stack)
                    outer = [e[1] for e in stack if e[0] == 'class']
                    info = {'name': m.group(2), 'ns': ns, 'outer': outer, 'line': line_at(i), 'body_start': i + 1,
                            'kind': m.group(1)}
                    self.classes.append(info)
                    stack.append(('class', m.group(2), False, info))
                elif (at_ns or in_class) and '(' in hs and not re.search(r'=\s*$', hs) and \
                        (FUNC_TAIL.search(hs) or CTOR_INIT.search(hs)) and not re.match(r'(?:return|else|do|try)\b', hs):
                    pd = parse_declarator(hs)
                    if pd:
                        name, params, pre, tail = pd
                        # skip macro-looking invocations at namespace scope: ALLCAPS( ... ) {
                        info = {'name': name, 'ns': self._ns(stack), 'cls': stack[-1][1] if in_class else None,
                                'cls_info': stack[-1][3] if in_class else None,
                                'params': params, 'line': line_at(last + len(header) - len(header.lstrip())),
                                'def_line': line_at(i), 'body_start': i + 1,
                                'externC': any(e[2] for e in stack) or bool(re.search(r'extern\s*"C"', pre)),
                                'internal': any(e[0] == 'ns' and e[3].get('anon') for e in stack) or bool(re.search(r'\bstatic\b', pre)) and not in_class,
                                'pre': pre}
                        self.funcs.append(info)
                        kind = 'func'
                        stack.append(('func', name, False, info))
                        # skip body quickly
                        d = 1; j = i + 1
                        while j < n and d:
                            if s[j] == '{':
                                d += 1
                            elif s[j] == '}':
                                d -= 1
                            j += 1
                        info['body_end'] = j - 1
                        stack.pop()
                        i = j; last = j
                        continue
                    else:
                        stack.append(('other', '', False, {}))
                else:
                    stack.append(('other', '', False, {}))
                last = i + 1
            i += 1

    @staticmethod
    def _ns(stack):
        return [e[1] for e in stack if e[0] == 'ns' and e[1]]


# ----------------------------------------------------------------------------- exports / demangle
OPS = {'0': None, '1': '~', '2': 'operator new', '3': 'operator delete', '4': 'operator=', '8': 'operator==',
       '9': 'operator!=', 'A': 'operator[]', 'B': 'operator T', 'C': 'operator->', 'D': 'operator*',
       'G': 'operator-', 'H': 'operator+', 'K': 'operator/', 'M': 'operator<', 'N': 'operator<=',
       'O': 'operator>', 'P': 'operator>=', 'R': 'operator()', 'X': 'operator*=', 'Y': 'operator+=',
       'Z': 'operator-=', '_0': 'operator/=', '_G': 'scalar deleting dtor', '_E': 'vector deleting dtor',
       '_7': 'vftable', '_4': 'operator&='}


def demangle(m):
    """-> (class_qual 'Ns::Class' or '', member, access_code) best effort."""
    s = m
    names = []
    if s.startswith('??'):
        code = s[2:4] if s[2] == '_' else s[2]
        s = s[2 + len(code):]
        op = OPS.get(code, 'op' + code)
    elif s.startswith('?'):
        op = None; code = ''
        j = s.find('@', 1)
        names.append(s[1:j]); s = s[j + 1:]
    else:
        return '', m, ''
    scopes = []
    while s and not s.startswith('@'):
        if s[0].isdigit():
            k = int(s[0])
            scopes.append(names[k] if k < len(names) else '?'); s = s[1:]
            continue
        if s.startswith('?$'):
            # template scope: ?$Name@args@ -- take the name, skip to the next '@@'
            j = s.find('@', 2); nm = s[2:j]
            e = s.find('@@', j)
            scopes.append(nm + '<>'); names.append(nm)
            s = s[e + 2:] if e >= 0 else ''
            continue
        j = s.find('@')
        if j < 0:
            break
        scopes.append(s[:j]); names.append(s[:j]); s = s[j + 1:]
    acc = s[1:2] if s.startswith('@') else ''
    if op is None and code == '':
        member = names[0]; cls_parts = scopes
    else:
        cls_parts = scopes
        cname = scopes[0] if scopes else ''
        if code == '0':
            member = cname
        elif code == '1':
            member = '~' + cname
        else:
            member = op
    cls = '::'.join(reversed(cls_parts))
    return cls, member, acc


def access_kind(acc):
    if acc in ('Y', 'Z'):
        return 'global'
    if acc in ('C', 'D', 'K', 'L', 'S', 'T'):
        return 'static'
    if acc in ('E', 'F', 'M', 'N', 'U', 'V'):
        return 'virtual'
    if acc in ('A', 'B', 'I', 'J', 'Q', 'R'):
        return 'member'
    return ''


def load_exports():
    by_addr = collections.defaultdict(list); by_cls_member = collections.defaultdict(list); classes = set()
    for ln in open(EXPORTS, encoding='utf-8').read().split('\n')[1:]:
        if not ln.strip():
            continue
        a, mg = ln.split('\t')
        cls, mem, acc = demangle(mg)
        rec = {'addr': a.lower(), 'mangled': mg, 'cls': cls, 'member': mem, 'kind': access_kind(acc)}
        by_addr[a.lower()].append(rec)
        if cls:
            classes.add(cls)
            by_cls_member[(cls.split('::')[-1], mem)].append(rec)
    return by_addr, by_cls_member, classes


# ----------------------------------------------------------------------------- main
RECV_NAMES = re.compile(r'^(self\w*|this_?\w*|thisPtr|obj|objv|me|inst|instance|owner|elem|element|dlg|dialog|ctx)$')


def first_param(params):
    ps = split_params(params)
    if not ps or ps[0] in ('void', ''):
        return '', ''
    p = re.sub(r'=.*$', '', ps[0]).strip()
    m = re.match(r'(.*?)([A-Za-z_]\w*)?\s*$', p, re.S)
    typ, nm = (m.group(1) or '').strip(), (m.group(2) or '')
    if not typ or typ.endswith('::'):
        typ, nm = p, ''
    return re.sub(r'\s+', ' ', typ), nm


RT_NAME = re.compile(r'^Rt([A-Z][A-Za-z0-9]*?)_([A-Za-z][A-Za-z0-9]*)_(\w+)$')
ADDR_RE = re.compile(r'(?<![0-9A-Fa-fx_])(?:0x)?(18[0-9a-fA-F]{7})(?![0-9A-Fa-f])')
MANGLED_RE = re.compile(r'(\?\??[A-Za-z0-9_$?@]+@@[A-Za-z0-9_$@?]*)')
NS_MAP = {'Ui': 'UI', 'Io': 'IO', 'IO': 'IO', 'UI': 'UI'}


COLL_ARRAY = re.compile(r'(Array|SetSize|Assign|Destroy\w*Range|Destroy\w*Array|Resize|Grow|ElemDestroy|CopyCtor|ListCtor|'
                        r'^Destroy[A-Z]\w*(Element|Entry|Transforms|Strings|Pair)s?$|^RtColl_)')
COLL_TREE = re.compile(r'(Retrace|Rotate|Erase\d|AllocNode|NodeAlloc|NodeFree|Subtree|Successor|Map(Find|Insert|Clear)|SetFree|NodeChild)')


def infer_class(short, ftyp, class_idx, exp_cls_short, dir_ns=""):
    """Class of a C body that carries no export evidence. -> (cls, member, confidence, evidence)."""
    real = lambda nm: [q for (q, hp, ci) in class_idx.get(nm, []) if not q.startswith('Rt')]
    # 1) <Ns>_<Class>_<Member> / <Class>_<Member> with a known class
    parts = short.split('_')
    for k in range(len(parts) - 1, 0, -1):
        cand = parts[k - 1]
        if cand in exp_cls_short or real(cand):
            opts = sorted(exp_cls_short.get(cand) or real(cand))
            ns_hint = parts[k - 2].lower() if k >= 2 else ''
            ns_hint = {'ui': 'ui', 'io': 'io'}.get(ns_hint, ns_hint).replace('rt', '', 1) if ns_hint.startswith('rt') else ns_hint
            q = next((o for o in opts if ns_hint and o.split('::')[0].lower() == ns_hint), next((o for o in opts if o.split('::')[0].lower() == dir_ns), opts[0]))
            return q, '_'.join(parts[k:]), 'medium', 'name <Class>_<Member> names a known class'
    # 2) the first parameter's pointee type is a known class
    t = re.sub(r'\b(const|volatile|struct|class)\b', '', ftyp).replace('*', '').replace('&', '').strip()
    t = t.split('::')[-1].strip()
    if t and t not in ('void', 'char', 'uint8_t', 'int', 'float') and (t in exp_cls_short or real(t)):
        opts = sorted(exp_cls_short.get(t) or real(t)); q = next((o for o in opts if o.split('::')[0].lower() == dir_ns), opts[0])
        return q, short, 'medium', 'first parameter is ' + ftyp
    # 3) Collection template instantiations (never exported: MSVC inlined / instanced them per T)
    if COLL_TREE.search(short):
        return 'Collection::Set/Map<T> (AVL instance)', short, 'low', 'name = AVL tree / map internals'
    if COLL_ARRAY.search(short):
        return 'Collection::Array<T> (instance)', short, 'low', 'name = Array<T> internals'
    return '', '', 'none', ''


def role_of(p, short, deleg, body_lines, ev_cls, conf, unexported):
    if p.startswith('standalone/'):
        return 'standalone_tool'
    if deleg == 'C->member' and body_lines <= 6:
        return 'door'
    if deleg == 'member->C':
        return 'c_body_behind_member'
    if deleg == 'both_have_bodies':
        return 'duplicate_body'
    if ev_cls.startswith('Collection::') and 'instance' in ev_cls:
        return 'collection_instance'
    if not ev_cls and body_lines <= 4:
        return 'accessor_helper'
    return 'c_body'


def main():
    by_addr, by_cls_member, exp_classes = load_exports()
    exp_cls_short = collections.defaultdict(set)
    for c in exp_classes:
        exp_cls_short[c.split('::')[-1]].add(c)

    prod_files = walk(PROD); test_files = walk(TESTS)
    scans = {p: Scan(p) for p in prod_files}

    # ---- class index (headers in the type dirs and runtime/src headers)
    class_idx = collections.defaultdict(list)   # short name -> [(qual, file, info)]
    for p, sc in scans.items():
        for c in sc.classes:
            if 'body_end' not in c:
                continue
            q = '::'.join(c['ns'] + c['outer'] + [c['name']])
            class_idx[c['name']].append((q, p, c))

    def class_members(p, c):
        """names of member functions declared / defined inside a class body (depth 1)."""
        s = load(p)[2][c['body_start']:c['body_end']]
        # drop nested bodies
        flat = []; d = 0
        for ch in s:
            if ch == '{':
                d += 1
                if d == 1:
                    flat.append('{')
                continue
            if ch == '}':
                d -= 1
                if d == 0:
                    flat.append('};')
                continue
            if d == 0:
                flat.append(ch)
        flat = ''.join(flat)
        decls = {}
        for stmt in re.split(r';', flat):
            st = stmt.strip()
            if '(' not in st:
                continue
            st2 = re.sub(r'^(?:public|private|protected)\s*:\s*', '', st)
            pd = parse_declarator(st2)
            if not pd:
                continue
            name, params, pre, tail = pd
            nm = name.split('::')[-1]
            if nm in TYPE_WORDS or re.fullmatch(r'[A-Z0-9_]+', nm) or '(*' in st2.split(name)[0][-3:]:
                continue
            if re.search(r'\busing\b|\btypedef\b|\bfriend\b', pre):
                continue
            if re.search(r'\(\s*\*', st2[:st2.find('(') + 3]):
                continue
            inline = '{' in tail
            special = bool(re.search(r'=\s*(0|default|delete)\s*$', tail.replace('{', '').replace('}', '').strip())) if not inline else False
            decls.setdefault(nm, []).append({'inline': inline, 'special': special, 'text': st2[:160]})
        return decls

    # ---- member definitions: qualified out-of-class definitions + in-class inline bodies
    member_defs = collections.defaultdict(list)   # (cls_short, member) -> [(file, line, info)]
    member_defs_q = collections.defaultdict(list)  # (cls_qual, member) -> same
    for p, sc in scans.items():
        for f in sc.funcs:
            parts = f['name'].lstrip(':').split('::')
            if len(parts) >= 2:
                member_defs[(parts[-2], parts[-1])].append((p, f['line'], f))
                q = parts[:-1] if f['name'].startswith('::') else f['ns'] + parts[:-1]
                member_defs_q[('::'.join(q), parts[-1])].append((p, f['line'], f))
            elif f['cls']:
                member_defs[(f['cls'], parts[-1])].append((p, f['line'], f))
                ci = f['cls_info']
                member_defs_q[('::'.join(ci['ns'] + ci['outer'] + [ci['name']]), parts[-1])].append((p, f['line'], f))

    # ---- code text of all PROD / tests for reference counts
    word_hits = collections.Counter(); word_hits_t = collections.Counter(); word_lines = collections.defaultdict(list)
    ident = re.compile(r'\b(Rt\w+|[A-Za-z_]\w*)\b')
    for plist, ctr in ((prod_files, word_hits), (test_files, word_hits_t)):
        for p in plist:
            code = load(p)[1]
            for ln, line in enumerate(code.split('\n'), 1):
                for w in set(ident.findall(line)):
                    ctr[w] += 1
                    if ctr is word_hits:
                        word_lines[w].append((p, ln))

    # ---- candidate C bodies in SCOPE
    in_scope = lambda p: any(p.startswith(d + '/') for d in SCOPE)
    rows = []
    free_defs = collections.defaultdict(list)
    dx11_slots = set()
    for p in prod_files:
        if p.startswith('dx11/'):
            for m in re.finditer(r'DX11_REGISTER_SLOT\s*\(\s*\w+\s*,\s*\d+\s*,\s*&?\s*(?:::)?(?:\w+::)*(\w+)', load(p)[1]):
                dx11_slots.add(m.group(1))
    internal_defs = collections.defaultdict(set)
    for p, sc in scans.items():
        for f in sc.funcs:
            if '::' not in f['name'] and not f['cls']:
                free_defs[f['name']].append((p, f))
                if f['internal'] and not p.endswith(('.h', '.hpp', '.inc')):
                    internal_defs[f['name']].add(p)

    for p, sc in scans.items():
        if not in_scope(p):
            continue
        raw, code, s = load(p)
        rawlines = raw.split('\n')
        for f in sc.funcs:
            name = f['name']
            if f['cls']:
                continue
            short = name.split('::')[-1]
            qual_parts = name.split('::')
            # a qualified name whose qualifier is a class = member definition, not a C body
            if len(qual_parts) > 1 and qual_parts[-2] in class_idx:
                continue
            ftyp, fnm = first_param(f['params'])
            rt = RT_NAME.match(short)
            is_voidp = bool(re.search(r'\bvoid\s*\*', ftyp)) and '**' not in ftyp.replace(' ', '')
            is_recv = bool(fnm and RECV_NAMES.match(fnm) and '*' in ftyp)
            if not (rt or f['externC'] or is_voidp or is_recv):
                continue
            if short == 'main' or short.startswith('WinMain'):
                continue
            # ---- evidence from the comment block before the definition and the first body lines
            lo = max(0, f['line'] - 14); hi = min(len(rawlines), f['def_line'] + 3)
            prev = [l for l in rawlines[lo:f['line'] - 1]]
            # stop at the previous function's closing brace
            for k in range(len(prev) - 1, -1, -1):
                if prev[k].startswith('}'):
                    prev = prev[k + 1:]; break
            # evidence candidates, nearest to the definition first. dx11/ addresses are
            # VideoDX11-x64.dll's and must not be looked up in Runtime's export table.
            ctx_lines = prev + rawlines[f['line'] - 1:hi]
            defpos = len(prev)
            cands = []   # (distance, 'addr'|'mangled', cls, member, kind, mangled, addr, on_header_line)
            hdr_line = lambda l: bool(re.match(r'\s*//\s*(\?\S+@@|0x18[0-9a-fA-F]{7}\b|.*\bretail\s+0x18)', l))
            for k, l in enumerate(ctx_lines):
                dist = abs(k - defpos) if k < defpos else (k - defpos) * 0.5
                if not p.startswith('dx11/'):
                    for a in ADDR_RE.findall(l):
                        a = '0x' + a.lower()
                        for r in by_addr.get(a, []):
                            cands.append((dist, 'addr', r['cls'], r['member'], r['kind'], r['mangled'], a, hdr_line(l) or k == defpos))
                        if a not in by_addr:
                            cands.append((dist, 'rawaddr', '', '', '', '', a, hdr_line(l) or k == defpos))
                for mg in MANGLED_RE.findall(l):
                    c, mm, acc = demangle(mg)
                    if c:
                        a = next((rr['addr'] for rr in by_cls_member.get((c.split('::')[-1], mm), []) if rr['mangled'] == mg), '')
                        cands.append((dist, 'mangled', c, mm, access_kind(acc), mg, a, hdr_line(l) or k == defpos))
            cands.sort(key=lambda t: t[0])
            name_cls = name_mem = ''
            if rt:
                ns0, name_cls, name_mem = rt.groups()
                name_ns = NS_MAP.get(ns0, ns0)
            else:
                pp = short.split('_')
                if len(pp) >= 2:
                    name_cls, name_mem = pp[-2], pp[-1]
            ev_cls = ev_mem = ev_kind = ''; conf = 'none'; ev = ''; addr = ''
            def agrees(c):
                a, b = c.split('::')[-1].lower().rstrip('<>'), (name_cls or '').lower()
                full = short.lower()
                return bool(b) and (a == b or a in b or b.endswith(a)) or (len(a) > 3 and a in full)
            agree = [t for t in cands if t[1] != 'rawaddr' and agrees(t[2])]
            # class AND member agree with the body name first: comments name neighbours and callees
            def mem_ok(t):
                m = t[3]
                return bool(name_mem) and (m.lower() == name_mem.lower() or
                                           re.sub(r'_\w*$', '', name_mem).lower() == m.lower() or
                                           (m.startswith('~') and bool(re.search(r'dtor', name_mem, re.I))) or
                                           (m == t[2].split('::')[-1] and bool(re.search(r'ctor', name_mem, re.I))))
            agree.sort(key=lambda t: (not mem_ok(t), t[0]))
            named = [t for t in cands if t[1] != 'rawaddr' and t[7]]
            # the export the body NAME points at (namespace of the Rt prefix preferred)
            name_hit = None; name_hits = []
            if name_cls:
                base_mem = re.sub(r'_[A-Za-z0-9]{1,6}$', '', name_mem)
                name_hits = by_cls_member.get((name_cls, name_mem)) or by_cls_member.get((name_cls, base_mem)) or []
                if rt:
                    pref = [h for h in name_hits if h['cls'].split('::')[0].lower() == name_ns.lower()]
                    name_hits = pref or name_hits
                name_hit = name_hits[0] if name_hits else None
            if agree and mem_ok(agree[0]):
                t = agree[0]
                ev_cls, ev_mem, ev_kind, addr = t[2], t[3], t[4], t[6]; conf = 'high'
                ev = ('export@addr ' if t[1] == 'addr' else 'mangled-in-comment ') + t[5] + ' (agrees with the body name)'
            elif name_hit:
                r = name_hit; ev_cls, ev_mem, ev_kind = r['cls'], r['member'], r['kind']; conf = 'medium'
                ev = 'name matches export ' + r['mangled'] + (' (+%d overloads)' % (len(name_hits) - 1) if len(name_hits) > 1 else '')
                addr = r['addr'] if len(name_hits) == 1 else ''
            elif agree:
                t = agree[0]
                ev_cls, ev_kind = t[2], ''; ev_mem = name_mem or short; conf = 'medium'
                ev = 'class agrees with ' + t[5] + ' in the comment; member not exported under the body name'
            elif named and named[0][0] <= 2:
                # an export named right at the definition (the comment line above it or the
                # definition itself) that the body's own name does not agree with
                t = named[0]
                ev_cls, ev_mem, ev_kind, addr = t[2], t[3], t[4], t[6]; conf = 'high' if not name_cls else 'medium'
                ev = ('export@addr ' if t[1] == 'addr' else 'mangled-in-comment ') + t[5] + (' (adjacent to the definition)')
            raw_addrs = [t for t in cands if t[1] == 'rawaddr']
            if not addr and raw_addrs and raw_addrs[0][0] <= 3:
                addr = raw_addrs[0][6]
            if not ev_cls and rt:
                if name_cls in exp_cls_short:
                    cl = sorted(exp_cls_short[name_cls])
                    ev_cls = next((c for c in cl if c.split('::')[0].lower() == name_ns.lower()), cl[0]); ev_mem = name_mem; conf = 'low'
                    ev = 'class exported, member name not (unexported member or renamed)'
                elif name_cls in class_idx:
                    ev_cls = name_ns + '::' + name_cls; ev_mem = name_mem; conf = 'low'
                    ev = 'class not among the exports'
            unexported = bool(not ev_cls and addr)
            # a short body whose work is ONE qualified call `X::Class::Member(` / `->Class::Member(`
            # names its class (the door shape); only used when nothing better is known.
            body0 = s[f['body_start']:f['body_end']]
            if (not ev_cls or conf in ('low', 'none')) and body0.count('\n') <= 6:
                qc = re.findall(r'((?:::)?(?:[A-Z]\w*::)+)(~?[A-Za-z_]\w*)\s*\(', body0)
                qc = [(q.strip(':').rstrip(':'), m) for q, m in qc if q.strip(':').split('::')[-1] in exp_cls_short or q.strip(':').split('::')[-1] in class_idx]
                qc = [(q, m) for q, m in qc if not q.startswith('Rt') and m not in ('ExternalPtr', 'InternalPtr')]
                # the allocator call inside an Array<T> destructor / node free does not name the class
                if (COLL_ARRAY.search(short) or COLL_TREE.search(short)) and not re.search(r'Mem(Free|Alloc)', short):
                    qc = [(q, m) for q, m in qc if not q.endswith('MemoryAllocator')]
                if len({x for x in qc}) == 1:
                    qq, mm = qc[0]
                    qshort = qq.split('::')[-1]
                    full = next((c for c in sorted(exp_cls_short.get(qshort, [])) if c.endswith(qq)), qq)
                    ev_cls, ev_mem, conf = full, mm, 'medium'
                    ev = f'body is one qualified call to {qq}::{mm}'
            if not ev_cls:
                ev_cls, ev_mem, conf, ev = infer_class(short, ftyp, class_idx, exp_cls_short, p.split('/')[2].lower() if p.startswith('runtime/src/') else ('video' if p.startswith('dx11') else ''))
            # member-name spellings of the C bodies -> C++ member names
            if ev_cls and ev_mem:
                cs = ev_cls.split('::')[-1]
                if re.fullmatch(r'(Ctor|DefaultCtor|CopyCtor|Ctor_\w+|Ctor\w*)', ev_mem):
                    ev_mem = cs
                elif re.fullmatch(r'(Dtor|Dtor_\w+|Destructor)', ev_mem):
                    ev_mem = '~' + cs
                elif ev_mem.startswith(cs) and len(ev_mem) > len(cs) and ev_mem[len(cs)].isupper():
                    ev_mem = ev_mem[len(cs):]
                elif re.fullmatch(r'OperatorDelete', ev_mem):
                    ev_mem = 'operatordelete'
                elif re.fullmatch(r'OperatorNew', ev_mem):
                    ev_mem = 'operatornew'
                elif re.fullmatch(r'(Operator)?(Assign|OperatorAssign)', ev_mem):
                    ev_mem = 'operator='
            if unexported:
                ev = (ev + '; ' if ev else '') + 'retail addr in comment is not an export (unexported fn)'
                if conf == 'none':
                    conf = 'low'
            cls_short = ev_cls.split('::')[-1] if ev_cls else ''
            notes = []
            if name_cls and ev_cls and 'adjacent to the definition' in ev and cls_short != name_cls:
                notes.append(f'NAME_MISLEADS(body says {name_cls}, retail says {cls_short})')
            # ---- class header
            cand = class_idx.get(cls_short, [])
            exact = [t for t in cand if t[0] == ev_cls]
            if exact:
                cand = exact
            elif ev_cls and '::' in ev_cls:
                # a different namespace's class of the same short name is NOT this class
                cand = [t for t in cand if t[0].split('::')[0] == ev_cls.split('::')[0]]
            cand = [t for t in cand if not t[0].startswith('Rt')]
            hdrs = [(q, hp) for (q, hp, ci) in cand]
            hdr_types = [hp for q, hp in hdrs if any(hp.startswith(d + '/') for d in TYPE_DIRS)]
            hdr_any = [hp for q, hp in hdrs]
            class_header = hdr_types[0] if hdr_types else (('(non-types) ' + hdr_any[0]) if hdr_any else '')
            # member declared?
            mem_decl = ''
            mem_key = ev_mem
            if mem_key and mem_key.startswith('~'):
                mem_key = mem_key
            if cls_short and (hdr_types or hdr_any):
                q, hp, ci = next(((q, hp, ci) for (q, hp, ci) in cand if hp in (hdr_types or hdr_any)))
                decls = class_members(hp, ci)
                mem_decl = 'yes' if mem_key in decls else 'no'
                if mem_decl == 'yes' and all(d['special'] for d in decls[mem_key]):
                    mem_decl = 'yes(=default/delete/0)'
            elif cls_short:
                mem_decl = 'no_header'
            # member defined?
            defs = member_defs_q.get((ev_cls, mem_key), []) if cls_short else []
            if not defs and cls_short and len({t[0] for t in class_idx.get(cls_short, []) if not t[0].startswith('Rt')}) <= 1:
                defs = member_defs.get((cls_short, mem_key), [])
            mem_def = 'yes' if defs else 'no'
            # delegation direction
            body = s[f['body_start']:f['body_end']]
            deleg = ''
            if defs:
                if re.search(r'(?:->|\.|::)\s*~?' + re.escape(mem_key.lstrip('~')) + r'\s*\(', body) or \
                        (mem_key.startswith('~') and '~' in body) or (mem_key == cls_short and re.search(r'\b' + re.escape(cls_short) + r'\s*[({]', body)):
                    deleg = 'C->member'
                else:
                    called = False
                    for (dp, dl, di) in defs:
                        mb = load(dp)[2][di['body_start']:di['body_end']]
                        if re.search(r'\b' + re.escape(short) + r'\b', mb):
                            called = True
                    deleg = 'member->C' if called else 'both_have_bodies'
            else:
                deleg = 'C_only'
            body_lines = body.count('\n') + 1
            # callers (code lines naming the C body, minus declarations/definitions of it)
            refs = 0; decl_lines = 0; tref = word_hits_t.get(short, 0)
            for (rp, rl) in word_lines.get(short, []):
                if f['internal'] and rp != p and not p.endswith(('.h', '.hpp', '.inc')):
                    continue    # internal linkage: only this translation unit can call it
                if rp != p and rp in internal_defs.get(short, set()):
                    continue    # that file has its own internal function of this name
                ln = load(rp)[1].split('\n')[rl - 1]
                qm = re.search(r'(?:->|\.|(\w+)\s*::)\s*' + re.escape(short) + r'\b', ln)
                if qm and (qm.group(1) is None or qm.group(1) in class_idx or qm.group(1) in exp_cls_short):
                    continue    # a member of the same name, not this free function
                idx = re.search(r'\b' + re.escape(short) + r'\s*\(', ln)
                pre = ln[:idx.start()].strip() if idx else None
                if pre and re.fullmatch(r'(?:extern\s+"C"\s+)?[\w:<>,\*&\s]*[\w>\*&]', pre) and \
                        not re.search(r'\b(return|else|case|delete|new|throw|do|co_return)\s*$', pre):
                    decl_lines += 1
                elif re.match(r'\s*' + re.escape(short) + r'\s*\(', ln) and rp == p and rl == f['line']:
                    decl_lines += 1
                else:
                    refs += 1
            kind_static = ev_kind or ('member?(inferred: receiver first param)' if (is_voidp or is_recv) else 'free?')
            role = role_of(p, short, deleg, body_lines, ev_cls, conf, unexported)
            if short in dx11_slots:
                role = 'dx11_seam_slot'   # installed into a Video::*Data vtable slot (carry-over 6, NEXT_SESSION 4.1)
            # casts and bridge markers inside the definition (header line .. closing brace)
            l0 = f['line'] - 1; l1 = s.count('\n', 0, f['body_end'])
            n_rc = sum(len(re.findall(r'reinterpret_cast\s*<', x)) for x in code.split('\n')[l0:l1 + 1])
            n_br = sum(1 for x in rawlines[l0:l1 + 1] if BRIDGE.search(x))
            notes.append(f'casts={n_rc};bridges={n_br}')
            notes.insert(0, f'kind=c_body;role={role};deleg={deleg};body_lines={body_lines};first_param={ftyp} {fnm}'.strip())
            notes.append('linkage=' + ('externC' if f['externC'] else ('internal' if f['internal'] else 'external')))
            if tref:
                notes.append(f'test_refs={tref}')
            if ev:
                notes.append('evidence=' + ev)
            if len(free_defs.get(short, [])) > 1:
                notes.append('DUP_NAME_DEFS=%d' % len(free_defs[short]))
            module = p.split('/')[0] + '/' + (p.split('/')[2] if p.startswith('runtime/') and p.count('/') >= 3 else (p.split('/')[1] if p.count('/') >= 2 else ''))
            rows.append([addr, short, module.rstrip('/'), f'{p}:{f["line"]}', ev_cls, class_header, mem_decl, mem_def,
                         kind_static, str(refs), conf, '; '.join(notes)])

    # ---- declared-but-undefined members of the type headers
    decl_rows = []
    for short, lst in class_idx.items():
        for (q, hp, ci) in lst:
            if not any(hp.startswith(d + '/') for d in TYPE_DIRS):
                continue
            for mem, ds in class_members(hp, ci).items():
                if any(d['inline'] or d['special'] for d in ds):
                    continue
                if member_defs_q.get((q, mem)):
                    continue
                if len({t[0] for t in lst}) <= 1 and member_defs.get((short, mem)):
                    continue
                # constructors / operators spelled differently
                exp = [e for e in by_cls_member.get((short, mem), []) if e['cls'] == q]
                addr = exp[0]['addr'] if len(exp) == 1 else ''
                refs = 0
                pat = re.compile(r'(?:->|\.|::)\s*' + re.escape(mem.lstrip('~')) + r'\s*\(') if not mem.startswith('operator') else None
                decl_rows.append([addr, f'{q}::{mem}' if not q.endswith('::' + mem) else q + '::' + mem, hp.split('/')[0] + '/' + hp.split('/')[2] if hp.count('/') >= 2 else hp,
                                  '', q, hp, 'yes', 'no', exp[0]['kind'] if exp else '', '', 'high' if exp else 'medium',
                                  'kind=decl_undefined; decl=' + ds[0]['text'].replace('\t', ' ').replace('\n', ' ')[:140] +
                                  ('; exported=' + exp[0]['mangled'] if exp else '; not exported')])

    hdr = ['addr', 'name', 'module', 'defining_file', 'class', 'class_header', 'member_declared', 'member_defined',
           'static_or_member', 'callers', 'confidence', 'note']
    with open(OUT, 'w', encoding='utf-8', newline='\n') as fo:
        fo.write('\t'.join(hdr) + '\n')
        for r in sorted(rows, key=lambda r: (r[3].split(':')[0], int(r[3].split(':')[1]))):
            fo.write('\t'.join(x.replace('\t', ' ') for x in r) + '\n')
        for r in sorted(decl_rows, key=lambda r: (r[5], r[1])):
            fo.write('\t'.join(x.replace('\t', ' ') for x in r) + '\n')
    print(f'# wrote {OUT}: {len(rows)} c_body rows, {len(decl_rows)} decl_undefined rows')
    summaries(rows, decl_rows, scans, class_idx, exp_cls_short)


# ----------------------------------------------------------------------------- summaries
def dir_key(p):
    parts = p.split('/')
    if parts[0] == 'runtime' and len(parts) > 3:
        return '/'.join(parts[:3])
    if parts[0] in ('dx11', 'engine') and len(parts) > 2:
        return '/'.join(parts[:2])
    return parts[0] if parts[0] in ('standalone',) else '/'.join(parts[:2])


BRIDGE = re.compile(r'B\.3\.(?:[0-9][a-z]?|x)\s+bridge')


def classify_bridge(line, nxt):
    t = line + ' ' + nxt
    if re.search(r'->\s*~', t):
        return 'dtor via cast'
    if re.search(r'Serializer|ReaderWriter|\bStream\b|Stream\*', t):
        return 'IO stream/serializer view'
    if re.search(r'InternalPtr|ExternalPtr|void\s*\*\*', t):
        return 'smart-ptr slot (void** <-> Ptr<T>)'
    if re.search(r'Object::Base\s*\*', t):
        return 'untyped object -> Object::Base (type test / virtual)'
    if re.search(r'\bRt[A-Z]\w*::\w+', t):
        return 'Rt* POD view <-> real class'
    if re.search(r'\(\s*\*\s*\)|Fn\b|fn\b', t):
        return 'function pointer'
    if re.search(r'reinterpret_cast<[^>]*>\s*\(\s*(self\w*|this_?\w*|obj\w*|me|p|ptr)\s*\)\s*(->|$)', t):
        return 'void* receiver -> class (door / C body)'
    if re.search(r'reinterpret_cast<[^>]*>\s*\(\s*&', t):
        return 'address-of POD -> class view'
    if re.search(r'void\s*\*', t):
        return 'void* field / factory result'
    if 'reinterpret_cast' in t:
        return 'other cast'
    return 'marker without cast on line'


def classify_rc(arg, target):
    if re.search(r'[+-]\s*(0x[0-9A-Fa-f]+|\d+)\b', arg):
        return 'raw offset read (p + off)'
    if re.search(r'\(\s*\*|\)\s*\(', target):
        return 'function pointer'
    if re.search(r'^\s*(?:const\s+)?(?:u?int8_t|unsigned char|char|std::byte|uintptr_t|intptr_t)\s*\*?\s*$', target) and \
            re.fullmatch(r'\s*[A-Za-z_]\w*\s*', arg):
        return 'byte view of a receiver (feeds offset reads)'
    if re.fullmatch(r'\s*(self\w*|this_?\w*|thisPtr|obj\w*|me|inst\w*|owner|elem\w*|dlg|p|ptr)\s*', arg):
        return 'void* receiver -> class'
    if re.fullmatch(r'\s*this\s*', arg):
        return 'this -> other type'
    if re.match(r'\s*&', arg):
        return 'address-of -> other type'
    return 'other (read to classify; candidate floor)'


def summaries(rows, decl_rows, scans, class_idx, exp_cls_short):
    print('\n## rows by directory: c_body total | Rt-named | extern C | deleg C->member | member->C | both | C_only | callers==0')
    agg = collections.defaultdict(lambda: collections.Counter())
    for r in rows:
        d = dir_key(r[3].split(':')[0]); n = r[11]
        agg[d]['total'] += 1
        agg[d]['rt'] += 1 if RT_NAME.match(r[1]) else 0
        agg[d]['externC'] += 1 if 'linkage=externC' in n else 0
        for k in ('C->member', 'member->C', 'both_have_bodies', 'C_only'):
            agg[d][k] += 1 if f'deleg={k};' in n else 0
        agg[d]['zero'] += 1 if r[9] == '0' and 'test_refs' not in n else 0
    tot = collections.Counter()
    for d in sorted(agg):
        a = agg[d]; tot.update(a)
        print(f"{d}\t{a['total']}\t{a['rt']}\t{a['externC']}\t{a['C->member']}\t{a['member->C']}\t{a['both_have_bodies']}\t{a['C_only']}\t{a['zero']}")
    print(f"TOTAL\t{tot['total']}\t{tot['rt']}\t{tot['externC']}\t{tot['C->member']}\t{tot['member->C']}\t{tot['both_have_bodies']}\t{tot['C_only']}\t{tot['zero']}")

    print('\n## roles per directory (role=... in the note column); z = of them with no code reference in PROD or tests')
    roles = ['door', 'c_body_behind_member', 'duplicate_body', 'c_body', 'collection_instance', 'accessor_helper', 'dx11_seam_slot', 'standalone_tool']
    print('dir\t' + '\t'.join(roles))
    ragg = collections.defaultdict(collections.Counter); rz = collections.defaultdict(collections.Counter)
    for r in rows:
        d = dir_key(r[3].split(':')[0]); ro = re.search(r'role=(\w+)', r[11]).group(1)
        ragg[d][ro] += 1
        if r[9] == '0' and 'test_refs' not in r[11]:
            rz[d][ro] += 1
    rt_ = collections.Counter(); rzt = collections.Counter()
    for d in sorted(ragg):
        rt_.update(ragg[d]); rzt.update(rz[d])
        print(d + '\t' + '\t'.join(f'{ragg[d][x]}(z{rz[d][x]})' for x in roles))
    print('TOTAL\t' + '\t'.join(f'{rt_[x]}(z{rzt[x]})' for x in roles))

    print('\n## reinterpret_casts / bridge markers INSIDE the C bodies, by role (all rows | rows with no reference)')
    cs = collections.Counter(); bs = collections.Counter(); czs = collections.Counter(); bzs = collections.Counter()
    for r in rows:
        ro = re.search(r'role=(\w+)', r[11]).group(1)
        m = re.search(r'casts=(\d+);bridges=(\d+)', r[11]); c, b = int(m.group(1)), int(m.group(2))
        cs[ro] += c; bs[ro] += b
        if r[9] == '0' and 'test_refs' not in r[11]:
            czs[ro] += c; bzs[ro] += b
    for ro in roles:
        print(f'{ro}\tcasts={cs[ro]}\tbridges={bs[ro]}\t| unreferenced: casts={czs[ro]}\tbridges={bzs[ro]}')
    print(f'TOTAL\tcasts={sum(cs.values())}\tbridges={sum(bs.values())}\t| unreferenced: casts={sum(czs.values())}\tbridges={sum(bzs.values())}')

    print('\n## confidence of the class column')
    print(dict(collections.Counter(r[10] for r in rows)))
    print('\n## class_header / member_declared / member_defined (c_body rows)')
    print('header in types:', sum(1 for r in rows if r[5] and not r[5].startswith('(non')),
          'header elsewhere:', sum(1 for r in rows if r[5].startswith('(non')), 'none:', sum(1 for r in rows if not r[5]))
    print('member_declared', dict(collections.Counter(r[6] for r in rows)))
    print('member_defined', dict(collections.Counter(r[7] for r in rows)))
    print('static_or_member', dict(collections.Counter(r[8].split('(')[0] for r in rows)))
    print('NAME_MISLEADS rows:', sum(1 for r in rows if 'NAME_MISLEADS' in r[11]))

    print('\n## top 30 classes by c_body rows (class | rows | C_only | header)')
    cc = collections.Counter(r[4] or '(unknown)' for r in rows)
    for c, k in cc.most_common(30):
        co = sum(1 for r in rows if (r[4] or '(unknown)') == c and 'deleg=C_only' in r[11])
        h = next((r[5] for r in rows if (r[4] or '(unknown)') == c), '')
        print(f'{c}\t{k}\t{co}\t{h}')

    print('\n## decl_undefined by header')
    dc = collections.Counter(r[5] for r in decl_rows)
    for h, k in dc.most_common():
        print(f'{h}\t{k}')

    # ---- reinterpret_cast causes and bridge markers per directory
    print('\n## reinterpret_cast per directory (code only) and cause')
    rc_re = re.compile(r'reinterpret_cast\s*<')
    cause = collections.defaultdict(collections.Counter); br = collections.defaultdict(collections.Counter)
    samples = collections.defaultdict(list)
    for p in sorted(scans):
        if not any(p.startswith(d + '/') for d in SCOPE):
            continue
        raw, code, s = load(p)
        rl = raw.split('\n'); cl = code.split('\n')
        d = dir_key(p)
        for k, line in enumerate(rl):
            if BRIDGE.search(line):
                br[d][classify_bridge(cl[k] if k < len(cl) else '', cl[k + 1] if k + 1 < len(cl) else '')] += 1
                br[d]['_markers'] += 1
        # casts in code
        for m in rc_re.finditer(code):
            i = m.end(); dd = 1; j = i
            while j < len(code) and dd:
                if code[j] == '<':
                    dd += 1
                elif code[j] == '>':
                    dd -= 1
                j += 1
            target = code[i:j - 1]
            k = j
            while k < len(code) and code[k].isspace():
                k += 1
            arg = ''
            if k < len(code) and code[k] == '(':
                dd = 0; e = k
                while e < len(code):
                    if code[e] == '(':
                        dd += 1
                    elif code[e] == ')':
                        dd -= 1
                        if dd == 0:
                            break
                    e += 1
                arg = code[k + 1:e]
            ln = code.count('\n', 0, m.start())
            rawline = rl[ln] if ln < len(rl) else ''
            c = classify_rc(arg, target)
            if BRIDGE.search(rawline):
                c = 'bridge-marked: ' + c
            cause[d][c] += 1; cause[d]['_total'] += 1
            if len(samples[c]) < 400:
                samples[c].append(f'{p}:{ln + 1}')
    cats = sorted({c for d in cause for c in cause[d] if c != '_total'})
    tot = collections.Counter()
    for d in sorted(cause):
        tot.update(cause[d])
        print(d + '\t' + str(cause[d]['_total']) + '\t' + '; '.join(f'{c}={cause[d][c]}' for c in cats if cause[d][c]))
    print('TOTAL\t' + str(tot['_total']) + '\t' + '; '.join(f'{c}={tot[c]}' for c in cats if tot[c]))

    print('\n## where the reinterpret_casts sit: inside MEMBER definitions | inside FREE functions | outside any function body')
    wagg = collections.defaultdict(collections.Counter); mcls = collections.Counter(); mfiles = collections.defaultdict(set)
    for p in sorted(scans):
        if not any(p.startswith(d + '/') for d in SCOPE):
            continue
        raw, code, s = load(p)
        cl = code.split('\n'); d = dir_key(p)
        per_line = [len(re.findall(r'reinterpret_cast\s*<', x)) for x in cl]
        owner = [None] * len(cl)
        for f in scans[p].funcs:
            kind = 'member' if (f['cls'] or ('::' in f['name'].lstrip(':') and f['name'].lstrip(':').split('::')[-2] in class_idx)) else 'free'
            n_in = 0
            for k in range(f['line'] - 1, min(len(cl), s.count('\n', 0, f['body_end']) + 1)):
                owner[k] = kind; n_in += per_line[k]
            if kind == 'member' and n_in:
                cname = f['cls'] or f['name'].lstrip(':').split('::')[-2]
                mcls[cname] += n_in; mfiles[cname].add(p)
        for k, n in enumerate(per_line):
            if n:
                wagg[d][owner[k] or 'outside'] += n
    tt = collections.Counter()
    for d in sorted(wagg):
        tt.update(wagg[d])
        print(f"{d}\tmember={wagg[d]['member']}\tfree={wagg[d]['free']}\toutside={wagg[d]['outside']}")
    print(f"TOTAL\tmember={tt['member']}\tfree={tt['free']}\toutside={tt['outside']}")
    print('\n## top 30 classes by reinterpret_casts inside their MEMBER definitions (scope dirs)')
    for c, n in mcls.most_common(30):
        print(f'{c}\t{n}\t' + ' '.join(sorted(mfiles[c])[:4]) + (' ...' if len(mfiles[c]) > 4 else ''))

    print('\n## B.3.x bridge markers per directory (raw lines, comments included) and what they bridge')
    btot = collections.Counter()
    for d in sorted(br):
        btot.update(br[d])
        print(d + '\t' + str(br[d]['_markers']) + '\t' + '; '.join(f'{c}={n}' for c, n in br[d].most_common() if c != '_markers'))
    print('TOTAL\t' + str(btot['_markers']) + '\t' + '; '.join(f'{c}={n}' for c, n in btot.most_common() if c != '_markers'))

    print('\n## Rt* POD mirrors: structs in namespace Rt* whose name is also a retail class')
    for short, lst in sorted(class_idx.items()):
        for (q, hp, ci) in lst:
            if q.startswith('Rt') and '::' in q and short in exp_cls_short and any(hp.startswith(x + '/') for x in SCOPE):
                real = [qq for (qq, hh, cc) in lst if not qq.startswith('Rt')]
                print(f'{q}\t{hp}:{ci["line"]}\tretail={",".join(sorted(exp_cls_short[short]))}\treal_class_in_tree={",".join(real) or "-"}')

    print('\n## local shadow types: a struct/class defined in a .cpp (or a runtime/src header) under a retail class name')
    shadow = collections.Counter(); where = collections.defaultdict(list)
    for short, lst in class_idx.items():
        if short not in exp_cls_short:
            continue
        for (q, hp, ci) in lst:
            if any(hp.startswith(x + '/') for x in SCOPE) and not any(hp.startswith(d + '/') for d in TYPE_DIRS):
                shadow[short] += 1; where[short].append(f'{hp}:{ci["line"]}({q})')
    for short, n in shadow.most_common():
        print(f'{short}\t{n}\t' +' '.join(where[short][:6]) + (' ...' if n > 6 else ''))
    print('TOTAL shadow definitions', sum(shadow.values()), 'of', len(shadow), 'retail class names')

    # sampling list for the hand check
    print('\n## sample rows for the hand check (every 25th c_body row)')
    for r in rows[::25]:
        print('\t'.join(r[:11]))


if __name__ == '__main__':
    main()
