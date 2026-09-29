#!/usr/bin/env python3
"""C_offsets.py -- B.4 block 0, inventory C: every raw offset read/write in PROD code.

Re-run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b4_session/scripts/C_offsets.py
Writes docs/runtime_port/endgame_b/b4_session/data/C_offsets.tsv (one row per offset access)
and prints the per-class / per-module summaries the inventory quotes (stdout).

What it sees (comments stripped with b3r_session/codegrep.py's strip(); strings blanked):
  cast   reinterpret_cast<T*>(B + K) / static_cast<T*>(B + K) / (T*)(B + K), casts nested freely
  index  reinterpret_cast<T*>(B)[n] / ((T*)B)[n] / static_cast<T*>(B)[n]   (offset n*sizeof(T))
  addr   a byte pointer + K not under a pointer cast (a field ADDRESS: passed on, compared, cast later)
  byte   byteptr[K] (a uint8_t / char pointer indexed by a literal)
  helper Name<T>(B, K) where Name is a helper defined in PROD whose body adds its offset parameter
B is the receiver expression, K a literal (hex or decimal) or a named integer constant.
The receiver's class comes from, in order: `this` in a member; the declared type of the
variable / parameter; the variable's initializer (casts stripped, followed through aliases,
`X::_singleton`, `a->b` member types, a pointer loaded from an offset of a known class whose
header types that field); a self-like void* parameter of a body named Class_Member /
RtNs_Class_Member (conf=fname: body names mislead, known_port_issues 6).
"""
import os, re, sys, collections

ROOT = os.getcwd()


def strip(src):
    """b3r_session/codegrep.py's strip() (comments out, strings kept), but LENGTH-PRESERVING:
    comment characters become spaces so code offsets and raw offsets (for `// +0x..` field
    comments) line up."""
    out = []; i = 0; n = len(src); st = None
    while i < n:
        c = src[i]
        if st is None:
            if src.startswith('//', i):
                j = src.find('\n', i); j = n if j < 0 else j
                out.append(' ' * (j - i)); i = j; continue
            if src.startswith('/*', i):
                j = src.find('*/', i + 2); j = n if j < 0 else j + 2
                out.append(''.join('\n' if ch == '\n' else ' ' for ch in src[i:j])); i = j; continue
            if c == '"' or c == "'":
                if c == '"' and i > 0 and src[i - 1] == 'R':
                    m = re.match(r'"([^(\s]*)\(', src[i:])
                    if m:
                        end = src.find(')' + m.group(1) + '"', i)
                        end = n if end < 0 else end + len(m.group(1)) + 2
                        out.append(''.join('\n' if ch == '\n' else ' ' for ch in src[i:end])); i = end; continue
                if c == "'" and i > 0 and (src[i - 1].isalnum()):
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

PROD = ['code', 'runtime', 'standalone', 'dx11', 'engine']
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')
EXCL = ('standalone/build', 'tests/unit/build', '.claude/worktrees', '/build/', '/build-')
OUT = 'docs/runtime_port/endgame_b/b4_session/data/C_offsets.tsv'

PRIM = {
    'bool': 1, 'char': 1, 'signed char': 1, 'unsigned char': 1, 'uint8_t': 1, 'int8_t': 1,
    'std::uint8_t': 1, 'std::int8_t': 1, 'std::byte': 1, 'BYTE': 1, 'byte': 1, 'uint8': 1,
    'int16_t': 2, 'uint16_t': 2, 'short': 2, 'unsigned short': 2, 'wchar_t': 2, 'WORD': 2,
    'char16_t': 2, 'std::uint16_t': 2, 'std::int16_t': 2,
    'int': 4, 'unsigned': 4, 'unsigned int': 4, 'int32_t': 4, 'uint32_t': 4, 'float': 4,
    'long': 4, 'unsigned long': 4, 'DWORD': 4, 'UINT': 4, 'LONG': 4, 'BOOL': 4, 'HRESULT': 4,
    'std::uint32_t': 4, 'std::int32_t': 4, 'char32_t': 4,
    'int64_t': 8, 'uint64_t': 8, 'double': 8, 'uintptr_t': 8, 'intptr_t': 8, 'size_t': 8,
    'ptrdiff_t': 8, 'long long': 8, 'unsigned long long': 8, 'std::size_t': 8,
    'std::uintptr_t': 8, 'std::intptr_t': 8, 'std::ptrdiff_t': 8, 'std::uint64_t': 8,
    'std::int64_t': 8, 'UINT64': 8, 'ULONG_PTR': 8, 'LONG_PTR': 8, 'DWORD64': 8, 'SIZE_T': 8,
    'void': 1,
}
BYTEISH = {'char', 'signed char', 'unsigned char', 'uint8_t', 'int8_t', 'std::uint8_t',
           'std::int8_t', 'std::byte', 'BYTE', 'byte', 'void', 'uint8'}
INTISH = {k for k, v in PRIM.items() if v == 8 and k not in ('double',)}
KW_NOT_TYPE = {'return', 'delete', 'new', 'goto', 'case', 'else', 'throw', 'sizeof', 'typedef',
               'using', 'namespace', 'if', 'while', 'for', 'switch', 'do', 'co_return',
               'operator', 'template', 'typename', 'public', 'private', 'protected', 'friend',
               'decltype', 'alignof', 'static_assert', 'break', 'continue', 'default'}
SELFISH = re.compile(r'^(self\w*|this_?|thisp(tr)?|pThis|_this|me|obj|inst(ance)?|param_1|'
                     r'comp(onent)?[A-Z]?|s|p|o|th|this[A-Z]\w*|self_?[a-z]?)$')
VSLOT_HELPER = re.compile(r'slot|vtbl|vtable|Vt\b', re.I)
NUM = r'(?:0[xX][0-9A-Fa-f]+|\d+)(?:[uUlL]{0,3})'
IDENT = r'[A-Za-z_]\w*'


def norm_path(p):
    return p.replace('\\', '/')


def walk(roots):
    out = []
    for r in roots:
        for dp, dn, fs in os.walk(r):
            dn.sort()
            for f in sorted(fs):
                q = norm_path(os.path.join(dp, f))
                if any(e in q for e in EXCL) or not q.endswith(EXTS):
                    continue
                out.append(q)
    return out


def blank_strings(s):
    """Blank the inside of string / char literals (strip() kept them); keep length and newlines."""
    out = list(s); i = 0; n = len(s)
    while i < n:
        c = s[i]
        if c == '"' or (c == "'" and not (i > 0 and s[i - 1].isalnum())):
            q = c; j = i + 1
            while j < n and s[j] != q and s[j] != '\n':
                if s[j] == '\\':
                    out[j] = ' '
                    if j + 1 < n and s[j + 1] != '\n':
                        out[j + 1] = ' '
                    j += 2; continue
                out[j] = ' '; j += 1
            i = j + 1; continue
        i += 1
    return ''.join(out)


def module_of(path):
    parts = path.split('/')
    if parts[0] == 'code' and len(parts) > 3:
        return 'code/Game/' + parts[2]
    if parts[0] in ('runtime', 'dx11') and len(parts) > 3:
        return '/'.join(parts[:3])
    if parts[0] in ('runtime', 'dx11', 'engine') and len(parts) > 2:
        return '/'.join(parts[:2])
    return parts[0]


def match_close(s, i, o='(', c=')'):
    """s[i] == o; return index of the matching c (or -1)."""
    d = 0
    for j in range(i, len(s)):
        ch = s[j]
        if ch == o:
            d += 1
        elif ch == c:
            d -= 1
            if d == 0:
                return j
    return -1


def split_top(s, seps=','):
    out = []; d = 0; ang = 0; cur = []
    for ch in s:
        if ch in '([{':
            d += 1
        elif ch in ')]}':
            d -= 1
        elif ch == '<':
            ang += 1
        elif ch == '>':
            ang = max(0, ang - 1)
        if ch in seps and d == 0 and ang == 0:
            out.append(''.join(cur)); cur = []
        else:
            cur.append(ch)
    out.append(''.join(cur))
    return out


def base_type(t):
    """'const Foo::Bar<int>* const' -> ('Foo::Bar<int>', ptr_depth)."""
    t = t.replace('&', ' ').strip()
    depth = t.count('*')
    t = t.replace('*', ' ')
    t = re.sub(r'\b(const|volatile|struct|class|typename|static|inline|constexpr|mutable|'
               r'register|extern|thread_local|__restrict|restrict)\b', ' ', t)
    t = re.sub(r'\s+', ' ', t).strip()
    t = re.sub(r'\s*::\s*', '::', t)
    return t, depth


def short(cls):
    """Unqualified class name, template args dropped."""
    cls = re.sub(r'<.*>', '', cls)
    cls = cls.lstrip(':')
    return cls.split('::')[-1] if cls else cls


def parse_num(s):
    s = s.rstrip('uUlL')
    return int(s, 16) if s.lower().startswith('0x') else int(s)


# ----------------------------------------------------------------------------------------
# Header index
# ----------------------------------------------------------------------------------------
GUESS_PREFIX = re.compile(r'^_*(pad|unk|field|data|raw|gap|reserved|spare|unused|filler|'
                          r'm_unk|m_pad|m_field|u|f|dw|qw|b|w|q|d|off|at|x|z|slot|blob|bytes|'
                          r'storage|opaque|m_data|m_raw|rest|tail|pd|p|ptr|val|v|i)_?(0x)?'
                          r'[0-9A-Fa-f]+(_\w*)?$')
GUESS_WORD = re.compile(r'^_*(pad|unk|unknown|gap|reserved|spare|filler|raw|opaque|blob|rest|'
                        r'tail|storage|bytes)', re.I)


def is_guess(name, off):
    if GUESS_PREFIX.match(name) or GUESS_WORD.match(name):
        return True
    m = re.search(r'_?(?:0x)?([0-9A-Fa-f]{2,4})$', name)
    if m and off is not None:
        try:
            if int(m.group(1), 16) == off:
                return True
        except ValueError:
            pass
    return False


class Cls:
    __slots__ = ('name', 'qual', 'file', 'line', 'bases', 'fields', 'size', 'poly', 'opaque_nfields', 'ns')

    def __init__(self, name, qual, file, line):
        self.name = name; self.qual = qual; self.file = file; self.line = line
        self.bases = []; self.fields = []; self.size = None; self.poly = False
        self.opaque_nfields = 0; self.ns = ''


class Field:
    __slots__ = ('name', 'type', 'dims', 'off', 'size', 'asserted', 'ptr', 'note')

    def __init__(self, name, typ, dims, off, ptr):
        self.name = name; self.type = typ; self.dims = dims; self.off = off
        self.size = None; self.asserted = False; self.ptr = ptr; self.note = ''


ASSERT_OFF = re.compile(r'static_assert\s*\(\s*offsetof\s*\(\s*([\w:<>, ]+?)\s*,\s*([\w.\[\]]+)\s*\)'
                        r'\s*==\s*\(?\s*(' + NUM + r')')
ASSERT_SIZE = re.compile(r'static_assert\s*\(\s*sizeof\s*\(\s*([\w:<> ,*]+?)\s*\)\s*==\s*\(?\s*(' + NUM + r')')


def type_size(t, ptr, classes, sizes):
    if ptr:
        return 8
    if t in PRIM:
        return PRIM[t]
    s = short(t)
    if s in sizes:
        return sizes[s]
    return None


def parse_class_body(body_raw, body_code, cls, file, line0):
    """Parse member declarations at depth 0 of a class body. body_raw keeps comments per line."""
    raw_lines = body_raw.split('\n')
    # remove nested braces content (inline methods, nested types) but keep anonymous union/struct members
    stmts = []  # (text, line_index)
    d = 0; cur = []; cur_line = 0; i = 0; n = len(body_code); line = 0
    anon_stack = []
    while i < n:
        ch = body_code[i]
        if ch == '\n':
            line += 1
        if ch == '{':
            head = ''.join(cur).strip()
            hh = re.sub(r'^.*\b(public|private|protected)\s*:', '', head, flags=re.S).strip()
            if re.fullmatch(r'(union|struct)', hh):
                # anonymous aggregate: flatten its members
                anon_stack.append(hh); cur = []; i += 1
                stmts.append(('@anon_' + hh, line)); continue
            j = match_close(body_code, i, '{', '}')
            if j < 0:
                break
            line += body_code.count('\n', i, j)
            # function body or nested type: statement ends here
            if re.search(r'\b(class|struct|union|enum)\b', head) and not re.search(r'\)\s*(const)?\s*$', head):
                # nested named type followed by optional declarator `} name;`
                k = j + 1
                m = re.match(r'\s*(' + IDENT + r')\s*(\[[^\]]*\])*\s*;', body_code[k:])
                if m and re.search(r'\b(union|struct)\b', head):
                    nm = re.search(r'\b(?:union|struct|class)\s+(' + IDENT + ')', head)
                    typ = nm.group(1) if nm else ('union' if 'union' in head else 'struct')
                    stmts.append((typ + ' ' + body_code[k:k + m.end()].strip().rstrip(';'), line))
                    line += body_code.count('\n', k, k + m.end())
                    i = k + m.end(); cur = []; continue
                cur = []; i = j + 1
                if i < n and body_code[i:i + 1] == ';':
                    i += 1
                continue
            cur = []; i = j + 1
            continue
        if ch == '}' and anon_stack:
            anon_stack.pop(); stmts.append(('@endanon', line))
            k = body_code.find(';', i)
            cur = []; i = (k + 1) if k >= 0 else i + 1; continue
        if ch == ';':
            stmts.append((''.join(cur), cur_line)); cur = []; i += 1; continue
        if not cur or not ''.join(cur).strip():
            cur_line = line
        cur.append(ch); i += 1
    in_anon = 0
    for text, ln in stmts:
        if text.startswith('@anon_'):
            in_anon += 1; cls.fields.append(Field('@anon', text[6:], [], None, 0)); continue
        if text == '@endanon':
            in_anon = max(0, in_anon - 1); cls.fields.append(Field('@endanon', '', [], None, 0)); continue
        t = re.sub(r'^\s*(public|private|protected)\s*:', '', text.strip())
        t = re.sub(r'\b(public|private|protected)\s*:', '', t).strip()
        t = re.sub(r'^\s*#.*$', '', t, flags=re.M).strip()
        if not t:
            continue
        if re.match(r'^(static|using|typedef|friend|template|enum|virtual|explicit|operator|'
                    r'DECLARE_\w+|static_assert|~)\b', t) or t.startswith('~'):
            if 'virtual' in t.split('(')[0]:
                cls.poly = True
            continue
        if 'override' in t or re.search(r'\bvirtual\b', t):
            cls.poly = True
        if '(' in t:
            m = re.match(r'^(.*?)\(\s*\*\s*(' + IDENT + r')\s*\)\s*\(', t)
            if m:  # function pointer member
                raw = raw_lines[ln] if ln < len(raw_lines) else ''
                cls.fields.append(_mk_field(m.group(2), 'fnptr', [], raw, 1))
            elif re.match(r'^[\w:<>,\s\*&]+\s+(' + IDENT + r')\s*(\[[^\]]*\])*\s*[({]', t) and \
                    not re.search(r'\w\s*\(\s*\)|\w\s*\([^)]*\b\w+\s+\w+', t) and '=' not in t.split('(')[0]:
                pass  # method declaration
            continue
        # strip default member initialisers
        t = re.split(r'\s*=\s*(?![^<]*>)', t)[0]
        t = re.sub(r'\{[^}]*\}\s*$', '', t).strip()
        t = re.sub(r'\balignas\s*\([^)]*\)', '', t)
        t = re.sub(r'__attribute__\s*\(\(.*?\)\)', '', t).strip()
        parts = split_top(t)
        m = re.match(r'^(.*?)([\*&\s]+)?\b(' + IDENT + r')\s*((?:\[[^\]]*\]\s*)*)(:\s*\d+)?$', parts[0].strip())
        if not m:
            continue
        typ = m.group(1).strip()
        if not typ or typ in KW_NOT_TYPE:
            continue
        stars = (m.group(2) or '').count('*')
        decls = [(m.group(3), m.group(4), stars, m.group(5))]
        for extra in parts[1:]:
            m2 = re.match(r'^\s*(\**)\s*(' + IDENT + r')\s*((?:\[[^\]]*\]\s*)*)$', extra)
            if m2:
                decls.append((m2.group(2), m2.group(3), m2.group(1).count('*'), None))
        raw = raw_lines[ln] if ln < len(raw_lines) else ''
        for nm, dims, st, bits in decls:
            bt, bd = base_type(typ)
            dl = [d_ for d_ in re.findall(r'\[([^\]]*)\]', dims or '')]
            f = _mk_field(nm, bt, dl, raw if len(decls) == 1 else '', bd + st)
            if bits:
                f.size = -1  # bitfield: layout unknown after it
            cls.fields.append(f)


def _mk_field(nm, typ, dims, raw, ptr):
    off = None
    cm = re.search(r'//\s*[\[(]?\s*\+\s*(0x[0-9A-Fa-f]+)', raw) or \
        re.search(r'//\s*[\[(]?\s*(0x[0-9A-Fa-f]+)\b', raw)
    if cm:
        off = int(cm.group(1), 16)
    f = Field(nm, typ, dims, off, ptr)
    cmt = raw.split('//', 1)
    f.note = cmt[1].strip() if len(cmt) > 1 else ''
    return f


def eval_dim(d, consts):
    d = d.strip()
    if not d:
        return 0
    try:
        expr = re.sub(r'(0[xX][0-9A-Fa-f]+|\d+)[uUlL]*', r'\1', d)
        # identifiers only: the `x120` of a hex literal is not one
        expr = re.sub(r'(?<![0-9A-Za-z_])' + IDENT, lambda m: str(consts.get(m.group(0), 'X')), expr)
        if 'X' in expr or not re.fullmatch(r'[\dxXa-fA-F\s+\-*/()]+', expr):
            return None
        return int(eval(expr))
    except Exception:
        return None


def build_header_index(files, texts, consts):
    classes = collections.defaultdict(list)   # short name -> [Cls]
    sizes = {}
    asserts = collections.defaultdict(dict)   # short class -> {field: off}
    for f in files:
        code = texts[f][1]
        for m in ASSERT_OFF.finditer(code):
            c = short(m.group(1).strip()); fld = m.group(2).strip()
            asserts[c][fld] = parse_num(m.group(3))
        for m in ASSERT_SIZE.finditer(code):
            t = m.group(1).strip()
            if '*' in t:
                continue
            sizes.setdefault(short(t), parse_num(m.group(2)))
    for f in files:
        raw, code = texts[f][0], texts[f][1]
        ns_spans = []
        for nm_ in re.finditer(r'\bnamespace\s+(' + IDENT + r'(?:::' + IDENT + r')*)\s*\{', code):
            e_ = match_close(code, nm_.end() - 1, '{', '}')
            ns_spans.append((nm_.end() - 1, e_ if e_ > 0 else len(code), nm_.group(1)))
        for m in re.finditer(r'\b(class|struct|union)\s+((?:alignas\s*\([^)]*\)\s*|__attribute__\s*\(\(.*?\)\)\s*|'
                             r'__declspec\s*\([^)]*\)\s*|\w+_API\s+)*)(' + IDENT + r'(?:::' + IDENT + r')*)'
                             r'\s*(final\s*)?(:\s*[^;{()]*)?\{', code):
            name = m.group(3)
            if name in ('alignas', 'final'):
                continue
            pre = code[max(0, m.start() - 40):m.start()]
            if re.search(r'\benum\s*$', pre) or re.search(r'\(\s*$', pre):
                continue
            b0 = m.end() - 1
            b1 = match_close(code, b0, '{', '}')
            if b1 < 0:
                continue
            line0 = code.count('\n', 0, b0) + 1
            c = Cls(short(name), name, f, line0)
            inner_ns = [x for x in ns_spans if x[0] < b0 < x[1]]
            c.ns = inner_ns[-1][2].split('::')[-1] if inner_ns else (name.split('::')[-2] if '::' in name else '')
            if m.group(5):
                for b in split_top(m.group(5)[1:]):
                    b = re.sub(r'\b(public|private|protected|virtual)\b', '', b).strip()
                    if b:
                        c.bases.append(short(b))
            body_code = code[b0 + 1:b1]
            body_raw = raw[b0 + 1:b1] if len(raw) == len(code) else body_code
            try:
                parse_class_body(body_raw, body_code, c, f, line0)
            except RecursionError:
                pass
            if re.search(r'\bvirtual\b|\boverride\b|DECLARE_EXE_TYPE', body_code):
                c.poly = True
            c.size = sizes.get(c.name)
            classes[c.name].append(c)
    # a short name defined in two namespaces (Video::Configuration, System::Configuration) also gets
    # qualified keys, so a receiver spelled with its namespace finds its own header
    for nm in list(classes):
        nss = {c.ns for c in classes[nm]}
        if len(nss) > 1:
            for c in classes[nm]:
                if c.ns:
                    classes.setdefault(c.ns + '::' + nm, []).append(c)
    # layout: offsets from asserts, then comments, then sequential
    laid = set()
    for nm, lst in classes.items():
        for c in lst:
            if id(c) in laid:
                continue
            laid.add(id(c))
            am = asserts.get(c.name, {})
            base_end = 0
            if c.bases:
                bs = sizes.get(c.bases[0])
                base_end = bs if bs is not None else None
            elif c.poly:
                base_end = 8
            cur = base_end
            anon_union = 0; union_start = None; union_max = 0
            for fld in c.fields:
                if fld.name == '@anon':
                    if fld.type == 'union':
                        anon_union += 1; union_start = cur; union_max = 0
                    continue
                if fld.name == '@endanon':
                    if anon_union:
                        anon_union -= 1
                        cur = (union_start + union_max) if (union_start is not None and union_max) else None
                    continue
                if fld.name in am:
                    fld.off = am[fld.name]; fld.asserted = True
                esz = type_size(fld.type, fld.ptr, classes, sizes)
                n_el = 1
                for dd in fld.dims:
                    v = eval_dim(dd, consts)
                    n_el = n_el * v if (v is not None and n_el is not None) else None
                if fld.size == -1:
                    fsz = None
                else:
                    fsz = esz * n_el if (esz is not None and n_el is not None) else None
                fld.size = fsz
                if fld.off is None and cur is not None and esz is not None:
                    al = min(esz, 8) if fld.type in PRIM or fld.ptr else min(8, esz) if esz in (1, 2, 4, 8) else 8
                    if fld.type not in PRIM and not fld.ptr:
                        al = 8 if esz % 8 == 0 else 4 if esz % 4 == 0 else 1
                    fld.off = (cur + al - 1) // al * al
                if anon_union:
                    if fld.off is None:
                        fld.off = union_start
                    union_max = max(union_max, fsz or 0)
                    continue
                cur = (fld.off + fsz) if (fld.off is not None and fsz is not None) else None
    return classes, sizes, asserts


# ----------------------------------------------------------------------------------------
# Functions and variables
# ----------------------------------------------------------------------------------------
FN_TAIL = re.compile(r'\)\s*(?:const|override|final|noexcept(?:\s*\([^)]*\))?|volatile|&&?|'
                     r'__attribute__\s*\(\(.*?\)\)|\s)*(?:->\s*[^{;]+)?$')
CTOR_INIT = re.compile(r'\)\s*(?:noexcept\s*)?:\s*[\w:<>]+\s*[({]')


class Fn:
    __slots__ = ('name', 'cls', 'params', 'start', 'end', 'head')

    def __init__(self, name, cls, params, start, end, head):
        self.name = name; self.cls = cls; self.params = params; self.start = start
        self.end = end; self.head = head


def parse_params(plist):
    out = {}
    if not plist.strip() or plist.strip() == 'void':
        return out
    for i, p in enumerate(split_top(plist)):
        p = p.split('=')[0].strip()
        m = re.match(r'^(.*?)([\*&\s]+)(' + IDENT + r')\s*(\[[^\]]*\])?$', p)
        if not m:
            continue
        t = m.group(1) + m.group(2).replace(' ', '')
        bt, bd = base_type(t)
        out[m.group(3)] = (bt, bd + (1 if m.group(4) else 0), i)
    return out


def fn_name_from_head(head):
    h = re.sub(r'template\s*<[^{;]*?>\s*(?=[\w:~])', '', head, count=1)
    for m in re.finditer(r'(~?[A-Za-z_][\w]*(?:\s*::\s*~?[A-Za-z_]\w*)*|operator\s*[^\s(]+)\s*\(', h):
        nm = re.sub(r'\s+', '', m.group(1))
        if nm.split('::')[-1] in ('__attribute__', '__declspec', 'alignas', 'decltype', 'noexcept',
                                  'throw', '__asm__', 'asm', 'if', 'while', 'for', 'switch',
                                  'sizeof', 'return', 'static_assert', 'DECLARE_EXE_TYPE'):
            continue
        ob = h.find('(', m.end() - 1)
        cb = match_close(h, ob)
        return nm, h[ob + 1:cb] if cb > 0 else ''
    return None, ''


def segment_functions(code):
    fns = []
    stack = []  # (kind, name)
    last = 0
    i = 0; n = len(code)
    fn_depth = None
    while i < n:
        ch = code[i]
        if ch == '#' and (i == 0 or code[i - 1] == '\n' or code[max(0, code.rfind('\n', 0, i)):i].strip() == ''):
            j = code.find('\n', i)
            while j > 0 and code[j - 1] == '\\':
                j = code.find('\n', j + 1)
            j = n if j < 0 else j
            if fn_depth is None:
                last = j
            i = j; continue
        if ch == '{':
            if fn_depth is not None:
                stack.append(('block', None))
            else:
                head = code[last:i].strip()
                if re.search(r'^(inline\s+)?namespace\b', head):
                    stack.append(('ns', head.split()[-1] if len(head.split()) > 1 else ''))
                elif re.search(r'extern\s*"C(\+\+)?"\s*$', head) or re.search(r'extern\s+"\s*"\s*$', head):
                    stack.append(('ext', None))
                elif re.search(r'\b(class|struct|union)\s+(?:[\w\s()]*?\s)?(' + IDENT + r')\s*(final)?\s*(:[^;{()]*)?$', head) \
                        and not re.search(r'\benum\b', head) and '=' not in head:
                    mm = re.search(r'\b(class|struct|union)\s+(?:[\w\s()]*?\s)?(' + IDENT + r')\s*(final)?\s*(:[^;{()]*)?$', head)
                    stack.append(('class', mm.group(2)))
                elif '(' in head and (FN_TAIL.search(head) or CTOR_INIT.search(head)) and \
                        not re.match(r'^(if|for|while|switch|catch)\b', head) and not head.startswith('='):
                    nm, pl = fn_name_from_head(head)
                    cls = None
                    if nm and '::' in nm:
                        cls = nm.split('::')[-2]
                    else:
                        for k, v in reversed(stack):
                            if k == 'class':
                                cls = v; break
                    stack.append(('fn', None))
                    fn_depth = len(stack)
                    fns.append(Fn(nm or '?', cls, parse_params(pl), i, None, head))
                else:
                    stack.append(('block', None))
            last = i + 1
        elif ch == '}':
            if stack:
                k, _ = stack.pop()
                if fn_depth is not None and len(stack) < fn_depth:
                    fns[-1].end = i; fn_depth = None
            last = i + 1
        elif ch == ';':
            if fn_depth is None:
                last = i + 1
        i += 1
    for f in fns:
        if f.end is None:
            f.end = n
    return fns


DECL = re.compile(r'(?<![\w.>:])((?:(?:const|volatile|static|unsigned|signed|struct|class|constexpr|'
                  r'register)\s+)*(?:::)?[A-Za-z_][\w]*(?:\s*::\s*[A-Za-z_]\w*)*(?:\s*<[^;(){}=]*?>)?'
                  r'(?:\s+(?:const|long|int|short|char))*)\s*((?:\*\s*(?:const\s*)?)*|&&?)\s*'
                  r'\b([A-Za-z_]\w*)\s*(=|\{|;|,|\[|\))')


def collect_vars(body, off0):
    """name -> list of (abs_pos, base_type, ptr_depth, init_text)."""
    out = collections.defaultdict(list)
    for m in DECL.finditer(body):
        t = m.group(1).strip()
        first = t.split()[0] if t.split() else t
        if first in KW_NOT_TYPE or t in KW_NOT_TYPE:
            continue
        stars = m.group(2) or ''
        bt, bd = base_type(t + stars.replace('&', ''))
        init = ''
        if m.group(4) == '=':
            j = m.end(); d = 0; k = j
            while k < len(body):
                c = body[k]
                if c in '([{':
                    d += 1
                elif c in ')]}':
                    if d == 0:
                        break
                    d -= 1
                elif c in ';,' and d == 0:
                    break
                k += 1
            init = body[j:k].strip()
        elif m.group(4) in (')', ','):
            continue  # a parameter of a nested call/lambda, or a comma expression
        out[m.group(3)].append((off0 + m.start(3), bt, bd, init))
    return out


# ----------------------------------------------------------------------------------------
# Offset matches
# ----------------------------------------------------------------------------------------
CAST_OPEN = re.compile(r'\b(reinterpret_cast|static_cast|const_cast)\s*<')
CSTYLE = re.compile(r'\(\s*((?:const\s+|volatile\s+|struct\s+|class\s+)*(?:::)?[A-Za-z_][\w]*(?:\s*::\s*\w+)*'
                    r'(?:\s*<[^()<>]*>)?(?:\s+(?:const|long|int|char|short))*\s*(?:\*\s*(?:const\s*)?)+)\)\s*')


def cast_at(s, i):
    """If s[i:] starts a cast, return (target_type, expr_start, expr_end, end_index)."""
    m = CAST_OPEN.match(s, i)
    if m:
        a = m.end() - 1; d = 0; j = a
        while j < len(s):
            if s[j] == '<':
                d += 1
            elif s[j] == '>':
                d -= 1
                if d == 0:
                    break
            j += 1
        tgt = s[a + 1:j]
        k = j + 1
        while k < len(s) and s[k] in ' \t\n':
            k += 1
        if k < len(s) and s[k] == '(':
            e = match_close(s, k)
            if e > 0:
                return tgt.strip(), k + 1, e, e + 1
        return None
    m = CSTYLE.match(s, i)
    if m:
        tgt = m.group(1)
        k = m.end()
        if k < len(s) and s[k] == '(':
            e = match_close(s, k)
            if e > 0:
                return tgt.strip(), k + 1, e, e + 1
        mm = re.match(r'(?:::)?[A-Za-z_][\w]*(?:(?:::|\.|->)[A-Za-z_]\w*)*', s[k:])
        if mm and mm.group(0) not in KW_NOT_TYPE:
            return tgt.strip(), k, k + mm.end(), k + mm.end()
    return None


CSTYLE_INT = re.compile(r'\(\s*(?:const\s+)?(u?int(?:8|16|32|64|ptr)_t|std::u?int(?:8|16|32|64|ptr)_t|size_t|'
                        r'ptrdiff_t|char|unsigned char|unsigned|int|long long|unsigned long long|intptr_t|'
                        r'ULONG_PTR|DWORD64)\s*\)\s*')


def strip_casts(e):
    """Remove outer casts / parens: returns (inner_expr, outermost_cast_type or None).
    Integer C-style casts ((uintptr_t)p) are stripped too; they never become the cast type."""
    e = e.strip(); first = None
    for _ in range(12):
        if e.startswith('(') and match_close(e, 0) == len(e) - 1:
            e = e[1:-1].strip(); continue
        c = cast_at(e, 0)
        if c and c[3] == len(e):
            if first is None:
                first = c[0]
            e = e[c[1]:c[2]].strip(); continue
        m = CSTYLE_INT.match(e)
        if m:
            rest = e[m.end():]
            if rest.startswith('(') and match_close(rest, 0) == len(rest) - 1:
                e = rest[1:-1].strip(); continue
            if re.fullmatch(r'\*?\s*(?:::)?' + IDENT + r'(?:(?:::|\.|->)' + IDENT + r')*', rest.strip()):
                e = rest.strip(); continue
            c2 = cast_at(rest, 0)
            if c2 and c2[3] == len(rest):
                e = rest; continue
        break
    return e, first


def pick_base(terms):
    """The pointer term of a sum: the first term with no top-level multiplication, not a literal."""
    for t in terms:
        s = t.strip()
        if not s or re.fullmatch(NUM, s):
            continue
        if any(x.strip() == '' for x in split_top(s, '*')) and s.startswith('*'):
            return s
        if len(split_top(s, '*')) == 1:
            return s
    return terms[0].strip()


class Ctx:
    def __init__(self, path, code, fns, consts, gconsts, classes, sizes, globals_):
        self.path = path; self.code = code; self.fns = fns; self.consts = consts
        self.gconsts = gconsts; self.classes = classes; self.sizes = sizes; self.globals = globals_
        self.vars_cache = {}
        self.stem = os.path.splitext(os.path.basename(path))[0]

    def fn_at(self, pos):
        import bisect
        if not hasattr(self, '_starts'):
            self._starts = [f.start for f in self.fns]
        k = bisect.bisect_right(self._starts, pos) - 1
        return self.fns[k] if k >= 0 and self.fns[k].end >= pos else None

    def vars_of(self, fn):
        if id(fn) not in self.vars_cache:
            self.vars_cache[id(fn)] = collect_vars(self.code[fn.start:fn.end], fn.start)
        return self.vars_cache[id(fn)]

    def const_val(self, name):
        if name in self.consts:
            return self.consts[name]
        return self.gconsts.get(name)


def known_class(t):
    t = re.sub(r'<.*>', '', t).lstrip(':')
    parts = t.split('::')
    if len(parts) >= 2 and '::'.join(parts[-2:]) in CLASSES:
        return '::'.join(parts[-2:])
    s = parts[-1]
    return s if s in CLASSES and s not in PRIM else None


def class_for_fname(fn, ctx):
    """Class guessed from a C body's name: Class_Member / RtNs_Class_Member / file stem."""
    nm = fn.name.split('::')[-1]
    parts = nm.split('_')
    if parts and re.match(r'^Rt[A-Z]\w*$', parts[0]) and len(parts) > 2:
        parts = parts[1:]
    if len(parts) < 2:
        return None, None
    if parts[0] in ('Ctor', 'Dtor', 'Init', 'Construct', 'Destruct') and len(parts) >= 2:
        c = known_class(parts[1]) or CLASS_LOWER.get(parts[1].lower())
        if c:
            return c, 'fname'
    # the member's owner is the class part nearest the member name: CitizenComp_StorageComponent_QueryCount
    # is StorageComponent's QueryCount (known_port_issues 6), so scan right to left for an exact class
    for part in reversed(parts[1:-1]):
        c = known_class(part) if part in CLASSES else None
        if c and len(re.findall(r'[A-Z][a-z0-9]+', part)) >= 2:   # never a one-word class (State, Entry)
            return c, 'fname'
    for k in range(len(parts) - 1, 0, -1):
        cand = '_'.join(parts[:k])
        if cand in CLASSES:
            return cand, 'fname'
        if cand.lower() in CLASS_LOWER:
            return CLASS_LOWER[cand.lower()], 'fname'
    p0 = parts[0]
    for cand in (p0 + 'Component', re.sub(r'Comp$', 'Component', p0), re.sub(r'Desc$', 'Description', p0),
                 p0 + 'Description'):
        if cand != p0 and cand in CLASSES:
            return cand, 'fname-abbrev'
    if p0 in RETAIL:
        return p0, 'fname-nohdr'
    return None, 'fname?' + p0


def class_for_findname(name):
    """FindComponent_WorkPlace / InteractComponent_FindWorkplaceComponent / GetDescription...: the class the
    finder's NAME says it returns (weak evidence, like a body name)."""
    m = re.search(r'(?:Find|Get)(?:Component|Comp)?_?([A-Za-z]+?)(?:Component|Comp)?(?:_[0-9a-fA-F]+)?$', name)
    if not m:
        return None
    x = m.group(1).lower()
    for c in (x + 'component', x, x + 'description'):
        if c in CLASS_LOWER:
            return CLASS_LOWER[c]
    return None


def class_for_varname(root):
    """A variable named after a class (citizenComp, descComponent, gameWorld...): weakest evidence."""
    s = re.sub(r'(IBP|Ibp|Ptr|Ref|Raw|Obj|Bytes|Addr|Base|_+|[0-9]+|P|B|V|v|p)$', '', root)
    s = re.sub(r'^(p|m_|g_|s_|the|my|this|cur|out|in|src|dst|new|old)(?=[A-Z_])', '', s).lstrip('_')
    if len(s) < 4:
        return None
    low = s.lower()
    if low in GENERIC_VARS:
        return None
    cands = [low, low + 'component', re.sub(r'comp$', 'component', low)]
    if low.endswith('desc') and len(low) > 6:
        cands.append(low[:-4] + 'description')
    for c in cands:
        hit = CLASS_LOWER.get(c)
        # only a class whose name has two or more words (CitizenComponent, MenuScene), never Entry / State
        if hit and len(re.findall(r'[A-Z][a-z0-9]+', hit)) >= 2:
            return hit
    return None


WRAPPERS = {'ExternalPtr', 'InternalPtr', 'InternalBasePtr', 'ExternalBasePtr', 'Array', 'String', 'Vector',
            'Collection', 'Object', 'External', 'Internal', 'Resource', 'SmartPtr', 'List', 'Map', 'Set'}

GENERIC_VARS = {'entry', 'state', 'header', 'context', 'config', 'target', 'component', 'list', 'container',
                'device', 'renderer', 'scene', 'layer', 'transform', 'array', 'node', 'dialog', 'desc', 'data',
                'object', 'item', 'element', 'info', 'base', 'root', 'array', 'arraybase', 'arrayptr',
                'description', 'resource', 'entity', 'owner', 'parent', 'child', 'slot', 'table', 'buffer',
                'block', 'record', 'value', 'result', 'handle', 'instance', 'manager', 'descobj', 'descraw'}


def member_type(cls, member):
    for c in CLASSES.get(cls, []):
        seen = set(); todo = [c]
        while todo:
            x = todo.pop()
            if x.name in seen:
                continue
            seen.add(x.name)
            for f in x.fields:
                if f.name == member:
                    return f.type, f.ptr
            for b in x.bases:
                todo.extend(CLASSES.get(b, []))
    return None, None


def pointee_of(oc, k):
    """Class a pointer field (oc + k) points at: its declared type, else a known class named in its comment."""
    fld = lookup_field(oc, k)
    if not fld:
        return None, None
    if fld[2] is not None and fld[2][1] >= 1 and known_class(fld[2][0]):
        return known_class(fld[2][0]), 'field'
    note = FIELD_NOTES.get((fld[3], oc, fld[1]))
    if note:
        words = re.findall(r'<\s*(?:\w+::)*([A-Z][A-Za-z0-9]+)', note) + \
            re.findall(r'[A-Z][A-Za-z0-9]+(?:::[A-Z][A-Za-z0-9]+)*', note)
        for w in words:
            w = short(w)
            if w in WRAPPERS or len(w) < 5:
                continue
            if w in CLASSES and w != oc:
                return w, 'field-comment'
    return None, None


def wrapper_arg(expr, pos, fn, ctx):
    """If expr (casts and & stripped) names a variable declared ExternalPtr<X> / InternalPtr<X>, return X."""
    e, _ = strip_casts(expr)
    e = e.lstrip('&').strip()
    if not fn or not re.fullmatch(IDENT, e):
        return None
    txt = ctx.code[fn.start:pos]
    m = None
    for m in re.finditer(r'(?:Object::)?(?:Ex|In)ternal(?:Base)?Ptr\s*<\s*([\w:]+)\s*>\s*[\*&]?\s*' + re.escape(e) + r'\b', txt):
        pass
    if m:
        return known_class(m.group(1))
    return None


def last_assignment(root, start, pos, ctx):
    """The last plain assignment `root = EXPR;` in [start, pos)."""
    seg = ctx.code[start:pos]
    last = None
    for m in re.finditer(r'(?<![\w.>])' + re.escape(root) + r'\s*=(?!=)\s*', seg):
        j = m.end(); d = 0; k = j
        while k < len(seg):
            c = seg[k]
            if c in '([{':
                d += 1
            elif c in ')]}':
                if d == 0:
                    break
                d -= 1
            elif c == ';' and d == 0:
                break
            k += 1
        if k < len(seg) and seg[k] == ';':
            last = (start + m.start(), seg[j:k].strip())
    return last


def fn_return_expr(name, ctx):
    """For a call to a function defined in this file whose body is `return EXPR;`: (fn, EXPR)."""
    for f in ctx.fns:
        if f.name.split('::')[-1] == name:
            body = ctx.code[f.start + 1:f.end].strip()
            m = re.fullmatch(r'return\s+(.*?);', body, re.S)
            if m:
                return f, m.group(1)
    return None, None


def resolve_root(expr, pos, fn, ctx, depth=0):
    """-> (class or None, conf, origin)."""
    if depth > 8:
        return None, 'loop', expr[:40]
    e, cast_t = strip_casts(expr)
    if cast_t:
        bt, bd = base_type(cast_t)
        if bd >= 1 and known_class(bt):
            return known_class(bt), 'cast', cast_t
    e = e.strip()
    if e.startswith('::'):
        e = e[2:]
    q = split_top(e, '?')
    if len(q) == 2:
        arms = split_top(q[1], ':')
        arms = [a for a in arms if a.strip()]
        if arms:
            c, cf, org = resolve_root(arms[0], pos, fn, ctx, depth + 1)
            if c or len(arms) < 2:
                return c, cf, org
            return resolve_root(arms[-1], pos, fn, ctx, depth + 1)
    # an offset read as the root: *(T**)(X + K) -> pointee type from the header
    if e.startswith('*'):
        ie, it = strip_casts(e[1:].strip())
        terms = split_top(ie, '+')
        if len(terms) == 1 and re.fullmatch(r'(?:::)?' + IDENT + r'(?:(?:::|\.|->)' + IDENT + r')*', ie.strip()):
            terms = [ie, '0']
        if len(terms) >= 2 and re.fullmatch(NUM, terms[-1].strip()):
            k = parse_num(terms[-1].strip())
            wt = wrapper_arg(terms[0], pos, fn, ctx) if k == 0 else None
            if wt:
                return wt, 'wrapper', terms[0].strip()[:40]
            oc, conf, org = resolve_root(terms[0], pos, fn, ctx, depth + 1)
            if oc:
                pc, pconf = pointee_of(oc, k)
                if pc:
                    return pc, pconf, f'*({oc}+{k:#x})'
                return None, 'loaded', f'*({oc}+{k:#x})'
            return None, 'loaded', f'*(?+{k:#x})'
    m = re.fullmatch(r'(' + IDENT + r'(?:::' + IDENT + r')*)((?:\s*(?:->|\.)\s*' + IDENT + r')*)', e)
    if not m:
        cm = re.match(r'(' + IDENT + r'(?:::' + IDENT + r')*)\s*(<[^()]*>)?\s*\(', e)
        if cm and match_close(e, cm.end() - 1) == len(e) - 1:
            nm = cm.group(1)
            if nm.endswith(('::Get', '::Instance', '::GetInstance', '::Singleton')):
                c = nm.split('::')[-2]
                return c, 'singleton', nm
            short_nm = nm.split('::')[-1]
            args = split_top(e[cm.end():-1]) if e[cm.end():-1].strip() else []
            f2, rexpr = fn_return_expr(short_nm, ctx)
            if f2 is not None and depth < 6:
                re_, _ = strip_casts(rexpr)
                if re_ in f2.params and f2.params[re_][2] < len(args):
                    return resolve_root(args[f2.params[re_][2]], pos, fn, ctx, depth + 1)
                c, cf, org = resolve_root(rexpr, f2.end, f2, ctx, depth + 1)
                if c:
                    return c, 'return', nm + '()'
                return None, cf, org
            if short_nm in GLOBAL_PASS and GLOBAL_PASS[short_nm] < len(args):
                return resolve_root(args[GLOBAL_PASS[short_nm]], pos, fn, ctx, depth + 1)
            if short_nm in GLOBAL_RET0 and not args:
                c, cf, org = resolve_root(GLOBAL_RET0[short_nm], pos, None, ctx, depth + 1)
                if c:
                    return c, 'return', nm + '()'
            fc = class_for_findname(short_nm)
            if fc:
                return fc, 'findname', nm + '()'
            return None, 'call', short_nm + '()'
        return None, 'expr', e[:40]
    root = m.group(1); chain = re.findall(IDENT, m.group(2) or '')
    if root.endswith(('::_singleton', '::s_instance', '::_instance', '::s_singleton')):
        owner = '::'.join(root.split('::')[:-1])
        cls = known_class(owner) or owner.split('::')[-1]
        return walk_chain(cls, chain, 'singleton', root)
    if root == 'this':
        return walk_chain(fn.cls, chain, 'this', 'this') if fn and fn.cls else (None, 'this', 'this')
    if fn:
        vs = ctx.vars_of(fn).get(root)
        cand = [v for v in (vs or []) if v[0] < pos]
        if cand:
            dpos, bt, bd, init = cand[-1]
            if bd >= 1 and known_class(bt):
                return walk_chain(known_class(bt), chain, 'decl', root)
            la = last_assignment(root, dpos + len(root), pos, ctx)
            if la:
                init = la[1]; dpos = la[0]
            if init in ('0', 'nullptr', 'NULL', '{}', '0ULL', '0LL', ''):
                om = re.search(r'(' + IDENT + r')\s*(?:<[^()]*>)?\s*\([^;]*?&\s*' + re.escape(root) + r'\b',
                               ctx.code[dpos:pos])
                vn = class_for_varname(root)
                if vn:
                    return walk_chain(vn, chain, 'varname', root)
                if om:
                    fc = class_for_findname(om.group(1))
                    if fc:
                        return walk_chain(fc, chain, 'findname', om.group(1) + '(&)')
                    return None, 'outparam', om.group(1) + '(&)'
                return None, 'local', f'{root}:{bt}{"*" * bd}'
            c, cf, org = resolve_root(init, dpos, fn, ctx, depth + 1)
            if c:
                return walk_chain(c, chain, cf if cf.startswith(('fname', 'varname')) else 'alias', org)
            vn = class_for_varname(root)
            if vn:
                return walk_chain(vn, chain, 'varname', root)
            return None, cf, org
        if root in fn.params:
            bt, bd, idx = fn.params[root]
            if bd >= 1 and known_class(bt):
                return walk_chain(known_class(bt), chain, 'param', root)
            pfx = re.split(r'_', fn.name.split('::')[-1].replace('RtUi', ''))[0].lower()
            named_like = len(pfx) >= 3 and (root.lower().startswith(pfx[:4]) or
                                            re.sub(r'component$|comp$', '', pfx)[:4] == root.lower()[:4])
            if SELFISH.match(root) or (idx == 0 and named_like):
                if fn.cls and fn.cls in CLASSES:
                    return walk_chain(fn.cls, chain, 'member', root)
                c, cf = class_for_fname(fn, ctx)
                if c:
                    return walk_chain(c, chain, cf, root)
                if ctx.stem in CLASSES and SELFISH.match(root):
                    return walk_chain(ctx.stem, chain, 'file', root)
                vn = class_for_varname(root)
                if vn:
                    return walk_chain(vn, chain, 'varname', root)
                return None, cf or 'param', f'{root}:{bt}{"*" * bd}'
            vn = class_for_varname(root)
            if vn:
                return walk_chain(vn, chain, 'varname', root)
            return None, 'param', f'{root}:{bt}{"*" * bd}'
        if fn.cls and fn.cls in CLASSES:
            ft, fp = member_type(fn.cls, root)
            if ft is not None:
                if fp >= 1 and known_class(ft):
                    return walk_chain(known_class(ft), chain, 'member-field', root)
                return None, 'member', f'{fn.cls}.{root}'
    g = ctx.globals.get(root.split('::')[-1])
    if g:
        bt, bd = g
        if bd >= 1 and known_class(bt):
            return walk_chain(known_class(bt), chain, 'global', root)
        return None, 'global', root
    vn = class_for_varname(root)
    if vn:
        return walk_chain(vn, chain, 'varname', root)
    return None, 'unresolved', root


def walk_chain(cls, chain, conf, origin):
    for mbr in chain:
        if not cls:
            return None, conf, origin
        t, p = member_type(cls, mbr)
        if t is None or p < 1 or not known_class(t):
            return None, 'member', f'{cls}->{mbr}'
        cls = known_class(t)
    return cls, conf, origin


def def_rank(c):
    """Which definition of a class name is THE header: a .h under code/Game or runtime/types first, the
    engine/types POD forwarders and .cpp-local structs last, then the one with the most laid-out fields."""
    return (c.file.endswith(('.cpp', '.c')), 'engine/types' in c.file,
            -sum(1 for f in c.fields if f.off is not None), c.file)


def lookup_field(cls, off, depth=0):
    """-> (state, field_name, (type, ptr) or None, header path)."""
    lst = CLASSES.get(cls)
    if not lst:
        return None
    best = None
    for c in sorted(lst, key=def_rank):
        r = _lookup_in(c, off, depth)
        if r and r[0] not in ('undeclared',):
            return r
        best = best or r
    return best


def _lookup_in(c, off, depth):
    if depth > 6:
        return ('undeclared', '', None, c.file)
    if off == 0 and c.poly and not any(f.off == 0 for f in c.fields):
        base_hit = None
        for b in c.bases:
            if b in CLASSES:
                base_hit = lookup_field(b, 0, depth + 1)
        return base_hit or ('vptr', '__vptr', ('void', 1), c.file)
    for f in c.fields:
        if f.off is None or f.name.startswith('@'):
            continue
        if f.off == off:
            if f.dims and f.type in BYTEISH and is_guess(f.name, f.off):
                return ('guess', f.name, (f.type, f.ptr), c.file)
            if f.dims and not f.ptr and f.type not in PRIM and known_class(f.type):
                pass
            elif not f.dims and not f.ptr and f.type not in PRIM and known_class(f.type) and \
                    f.type not in ('uint8_t',):
                sub = lookup_field(short(f.type), 0, depth + 1)
                if sub and sub[0] in ('named', 'named_noassert', 'guess', 'inside', 'inpad'):
                    return (sub[0] if f.asserted and not is_guess(f.name, f.off) else
                            'guess' if is_guess(f.name, f.off) else 'named_noassert',
                            f.name, (f.type, f.ptr), c.file)
            st = 'guess' if is_guess(f.name, f.off) else ('named' if f.asserted else 'named_noassert')
            return (st, f.name, (f.type, f.ptr), c.file)
    for f in c.fields:
        if f.off is None or f.size is None or f.name.startswith('@'):
            continue
        if f.off < off < f.off + f.size:
            rel = off - f.off
            if not f.ptr and f.type not in PRIM and known_class(f.type):
                esz = type_size(f.type, 0, CLASSES, SIZES) or f.size
                idx, rr = divmod(rel, esz) if f.dims and esz else (0, rel)
                sub = lookup_field(short(f.type), rr, depth + 1)
                nm = f.name + (f'[{idx}]' if f.dims else '')
                if sub:
                    return (sub[0], nm + '.' + sub[1], sub[2], c.file)
                return ('inside', f'{nm}+{rr:#x}', None, c.file)
            if f.type in BYTEISH and f.dims and (GUESS_WORD.match(f.name) or is_guess(f.name, f.off)):
                return ('inpad', f'{f.name}+{rel:#x}', None, c.file)
            if not f.dims:
                # a read that straddles / sits inside a declared scalar: the header's width disagrees
                return ('inside', f'{f.name}+{rel:#x}', (f.type, f.ptr), c.file)
            if f.type in BYTEISH:
                # raw byte storage under a real name (CitizenComponent::meshInstance[0x120]): no typed field
                return ('inside', f'{f.name}+{rel:#x}', None, c.file)
            esz = type_size(f.type, f.ptr, CLASSES, SIZES)
            if f.dims and esz and rel % esz == 0:
                st = 'guess' if is_guess(f.name, f.off) else ('named' if f.asserted else 'named_noassert')
                return (st, f'{f.name}[{rel // esz}]', (f.type, f.ptr), c.file)
            if f.type in BYTEISH:
                return ('inside', f'{f.name}+{rel:#x}', None, c.file)
            return ('inside', f'{f.name}+{rel:#x}', None, c.file)
    # base classes
    for b in c.bases:
        bs = SIZES.get(b)
        if b in CLASSES and (bs is None or off < bs):
            r = lookup_field(b, off, depth + 1)
            if r and r[0] != 'undeclared':
                return r
    return ('undeclared', '', None, c.file)


def spelling_at(code, i):
    m = re.match(r'(reinterpret_cast|static_cast|const_cast)', code[i:i + 20])
    return m.group(1) if m else 'cstyle'


def width_of(t):
    bt, bd = base_type(t)
    if bd >= 2:
        return 'ptr', 8
    if bd == 1:
        s = PRIM.get(bt)
        if s is None:
            s = SIZES.get(short(bt))
        return bt, s
    return bt, PRIM.get(bt)


def scan_file(path, code, ctx, helpers, rows, stats):
    lines_at = [0]
    for i, ch in enumerate(code):
        if ch == '\n':
            lines_at.append(i + 1)
    import bisect

    def line_of(p):
        return bisect.bisect_right(lines_at, p)

    covered = []  # spans of cast expressions already consumed

    def add(pos, kind, base_expr, off, wtype, idxd, spelling=''):
        fn = ctx.fn_at(pos)
        cls, conf, origin = resolve_root(base_expr, pos, fn, ctx)
        hdr = ''; fname = ''; state = 'unknown_class'; htype = ''
        if cls:
            if cls in CLASSES:
                r = lookup_field(cls, off)
                if r:
                    state, fname, ht, hdr = r
                    if ht:
                        htype = ht[0] + '*' * ht[1]
                    csz = SIZES.get(cls.split('::')[-1])
                    if state == 'undeclared' and csz is not None and off >= csz:
                        state = 'beyond'   # past sizeof(class): the receiver is a derived class / another object
                else:
                    state = 'undeclared'
                    hdr = CLASSES[cls][0].file
            else:
                state = 'no_header'
        rows.append([path, line_of(pos), cls or '', f'{off:#x}', wtype, hdr, fname, state,
                     module_of(path), kind, conf or '', re.sub(r'\s+', ' ', origin or '')[:60],
                     ('idx' if idxd else ''), fn.name if fn else '', htype, spelling])

    # 1) casts
    for m in re.finditer(r'\b(reinterpret_cast|static_cast|const_cast)\s*<|\((?=\s*(?:const\s+|volatile\s+)?'
                         r'(?:::)?[A-Za-z_])', code):
        i = m.start()
        c = cast_at(code, i)
        if not c:
            continue
        tgt, a, b, end = c
        bt, bd = base_type(tgt)
        if bd < 1:
            continue
        ln_start = code.rfind('\n', 0, i) + 1
        if code[ln_start:i].lstrip().startswith('#'):
            continue
        inner = code[a:b]
        e, _ = strip_casts(inner)
        terms = split_top(e, '+-')
        # recover signs
        signs = []; d = 0; ang = 0
        for ch in e:
            if ch in '([{':
                d += 1
            elif ch in ')]}':
                d -= 1
            elif ch in '+-' and d == 0:
                signs.append(ch)
        post_idx = None
        tail = code[end:end + 60]
        if code[a - 1] != '(':
            # C-style cast of a bare operand: `(T*)x[n]` indexes x, not the cast; `((T*)x)[n]` indexes the cast
            k0 = i - 1
            while k0 >= 0 and code[k0] in ' \t':
                k0 -= 1
            wm = re.match(r'\s*\)', tail)
            tail = tail[wm.end():] if (k0 >= 0 and code[k0] == '(' and wm) else ''
        pm = re.match(r'\s*\[\s*(' + NUM + r'|' + IDENT + r')\s*\]', tail)
        if pm:
            v = pm.group(1)
            post_idx = parse_num(v) if re.fullmatch(NUM, v) else ctx.const_val(v)
        if len(terms) >= 2 and len(signs) == len(terms) - 1 and not any(t.strip() == '' for t in terms):
            k = 0; idxd = False; nconst = 0
            base = pick_base(terms)
            bi = [t.strip() for t in terms].index(base)
            if bi > 0 and signs[bi - 1] == '-':
                continue
            for j, t in enumerate(terms):
                if j == bi:
                    continue
                sg = signs[j - 1] if j > 0 else '+'
                t = t.strip()
                v = None
                if re.fullmatch(NUM, t):
                    v = parse_num(t)
                elif re.fullmatch(IDENT, t) and ctx.const_val(t) is not None:
                    v = ctx.const_val(t)
                elif re.search(r'\d', t) and not re.search(r'[A-Za-z_]\w*\s*\(', t):
                    v = eval_dim(t, {**ctx.gconsts, **ctx.consts})
                if v is None:
                    idxd = True; continue
                nconst += 1
                k += v if sg == '+' else -v
            if nconst == 0:
                stats['nonliteral_offset'] += 1
                continue
            # scale by the pointee of the base term
            bexpr, bcast = strip_casts(base)
            scale = 1
            btyp = None
            if bcast:
                btyp = base_type(bcast)
            elif re.fullmatch(IDENT, base):
                fn = ctx.fn_at(i)
                vv = ctx.vars_of(fn).get(base) if fn else None
                cand = [v for v in (vv or []) if v[0] < i]
                if cand:
                    btyp = (cand[-1][1], cand[-1][2])
                elif fn and base in fn.params:
                    btyp = fn.params[base][:2]
            if btyp and btyp[1] >= 1:
                if btyp[1] >= 2:
                    scale = 8
                elif btyp[0] in BYTEISH:
                    scale = 1
                else:
                    scale = PRIM.get(btyp[0]) or SIZES.get(short(btyp[0])) or 0
            if scale == 0:
                stats['typed_ptr_arith_unknown_scale'] += 1
                continue
            if btyp and btyp[1] == 1 and btyp[0] not in BYTEISH and btyp[0] not in PRIM:
                stats['class_ptr_arith'] += 1
                continue  # Foo* + n: element arithmetic, not a field offset
            k *= scale
            wt, wsz = width_of(tgt)
            kind = 'cast'
            if post_idx is not None:
                k += post_idx * (wsz or 0); kind = 'cast+index'
            if k < 0:
                stats['negative_offset'] += 1
            if btyp and btyp[1] == 1 and btyp[0] in PRIM and btyp[0] not in BYTEISH and scale > 1:
                kind += '/typedarith'
            if base.startswith(('"', "'")):
                continue
            covered.append((a, b))
            add(i, kind if not (bt == 'void' and bd == 1) else 'addr-cast', base, k,
                wt if not (bt == 'void' and bd == 1) else 'void*', idxd, spelling_at(code, i))
        elif post_idx is not None and len(terms) == 1:
            wt, wsz = width_of(tgt)
            if wsz is None:
                continue
            if bt in BYTEISH and bd == 1 and post_idx == 0:
                pass
            # the base's own pointee may already be the class (reinterpret_cast<Foo*>(p)[1]) - skip class casts
            if bd == 1 and known_class(bt):
                stats['class_array_index'] += 1
                continue
            add(i, 'index', inner, post_idx * wsz, wt, False, spelling_at(code, i))
        elif len(terms) == 1 and bt in BYTEISH and bd == 1 and not any(a0 <= i < b0 for a0, b0 in covered):
            # a byte view plus a literal NOT under an outer pointer cast: a field address passed on
            # (`Serialize(static_cast<uint8_t*>(self) + 0x30)`)
            am = re.match(r'\s*\+\s*(' + NUM + r')\b(?!\s*[\w.(\[*])', code[end:end + 40])
            if am:
                covered.append((i, end + am.end()))
                add(i, 'addr', inner, parse_num(am.group(1)), '-', False, spelling_at(code, i))
    # 2) byte pointer arithmetic not under a pointer cast (field addresses) and byteptr[K]
    for m in re.finditer(r'(?<![\w.>])(' + IDENT + r')\s*(\+|\[)\s*(' + NUM + r')\b(?!\s*[\w.(])', code):
        pos = m.start()
        if any(a <= pos < b for a, b in covered):
            continue
        name = m.group(1)
        fn = ctx.fn_at(pos)
        if not fn:
            continue
        vv = ctx.vars_of(fn).get(name)
        cand = [v for v in (vv or []) if v[0] < pos]
        if cand:
            bt, bd = cand[-1][1], cand[-1][2]
        elif name in fn.params:
            bt, bd = fn.params[name][:2]
        else:
            continue
        if not (bd == 1 and bt in BYTEISH and bt != 'void'):
            continue
        k = parse_num(m.group(3))
        if m.group(2) == '[':
            if k == 0:
                continue
            add(pos, 'byte', name, k, 'uint8_t' if 'unsigned' in bt or 'uint8' in bt or bt == 'BYTE' else bt, False,
                'byteindex')
        else:
            # skip `p + K` that is itself a term of a longer sum already under a cast (handled) or of a loop bound
            add(pos, 'addr', name, k, '-', False, 'bytearith')
    # 3) helpers
    for hname, (hkind, wdefault, wparam, hfile) in helpers.items():
        for m in re.finditer(r'(?<![\w.>:])' + re.escape(hname) + r'\s*(<\s*([^<>()]+?)\s*>)?\s*\(', code):
            pos = m.start()
            ob = m.end() - 1; cb = match_close(code, ob)
            if cb < 0:
                continue
            args = split_top(code[ob + 1:cb])
            if len(args) < 2:
                continue
            ln_start = code.rfind('\n', 0, pos) + 1
            if code[ln_start:pos].lstrip().startswith('#'):
                continue
            # skip the definition itself
            pre = code[max(0, pos - 80):pos]
            if re.search(r'(inline|static|template\s*<[^>]*>|[\w&\*>]\s)\s*$', pre) and \
                    re.search(r'\b(void|uint8_t|char|std::byte)\s*\*\s*\w+\s*$', args[0]):
                continue
            o = args[1].strip()
            if re.fullmatch(NUM, o):
                k = parse_num(o)
            elif re.fullmatch(IDENT, o) and ctx.const_val(o) is not None:
                k = ctx.const_val(o)
            else:
                ev = eval_dim(o, {**ctx.gconsts, **ctx.consts})
                if ev is None:
                    stats['helper_nonliteral'] += 1
                    continue
                k = ev
            if hkind == 'vslot':
                stats['helper_vslot_calls'] += 1
                HELPER_USES[(hfile, hname)] += 1
                continue
            wt = m.group(2).strip() if m.group(2) else wdefault
            if wparam is not None and len(args) > wparam and not m.group(2):
                wt = wdefault
            HELPER_USES[(hfile, hname)] += 1
            add(pos, 'helper:' + hname, args[0].strip(), k, wt or '?', False, 'helper')


HELPER_USES = collections.Counter()
HELPER_DEFS = {}


def find_helpers(files, texts):
    """name -> (kind, default width, value-param index); per defining file."""
    defs = collections.defaultdict(dict)  # file -> {name: (...)}
    pat = re.compile(r'\b(' + IDENT + r')\s*\(\s*(?:const\s+)?'
                     r'(void|uint8_t|char|unsigned char|std::byte|' + IDENT + r')\s*\*\s*(' + IDENT + r')\s*,\s*'
                     r'(?:const\s+)?(size_t|std::size_t|int|uint32_t|int32_t|ptrdiff_t|std::ptrdiff_t|uintptr_t|'
                     r'unsigned|unsigned int|int64_t|uint64_t|long long)\s+(' + IDENT + r')\s*([,)])')
    known = set()
    for _pass in range(3):
      for f in files:
        code = texts[f][1]
        for m0 in pat.finditer(code):
            ls = max(code.rfind(';', 0, m0.start()), code.rfind('}', 0, m0.start()), code.rfind('{', 0, m0.start()),
                     code.rfind('\n#', 0, m0.start()))
            pre = code[ls + 1:m0.start()]
            tm = re.match(r'\s*(template\s*<[^>]*>\s*)?((?:static\s+|inline\s+|constexpr\s+|__forceinline\s+)*)(.*)$',
                          pre, re.S)
            m = {1: tm.group(1), 3: tm.group(3), 4: m0.group(1), 6: m0.group(3), 8: m0.group(5)}
            if not m[3].strip() or re.search(r'[=(),]', m[3]):
                continue
            name = m[4]
            if name in KW_NOT_TYPE:
                continue
            ob = code.find('(', m0.start(1)); cb = match_close(code, ob)
            if cb < 0:
                continue
            bo = code.find('{', cb)
            if bo < 0 or code[cb + 1:bo].strip() not in ('', 'const', 'noexcept'):
                continue
            be = match_close(code, bo, '{', '}')
            body = code[bo:be]
            offn = m[8]; pn = m[6]
            if not re.search(r'\b' + re.escape(pn) + r'\s*\)*\s*(\+\s*(\(?\s*(size_t|ptrdiff_t|uintptr_t|int64_t)\s*\)\s*|'
                             r'static_cast<[^>]*>\s*\(\s*)?' + re.escape(offn) + r'\b|\[\s*' + re.escape(offn) + r'\s*\])',
                             body) and not (known and re.search(
                                 r'\b(' + '|'.join(map(re.escape, sorted(known))) + r')\s*(<[^>]*>)?\s*\(\s*'
                                 + re.escape(pn) + r'\s*,\s*' + re.escape(offn) + r'\s*\)', body)):
                continue
            if len(body) > 400:
                continue  # an accessor is a one-liner; longer bodies are real functions that take an offset
            known.add(name)
            ret = m[3].strip()
            params = split_top(code[ob + 1:cb])
            wparam = None
            if re.match(r'^(void)$', ret) and len(params) >= 3:
                wt = base_type(params[2].rsplit(None, 1)[0] if ' ' in params[2].strip() else params[2])[0]
                wparam = 2
                if base_type(params[2])[1] >= 1 or '*' in params[2]:
                    wt = 'ptr'
            else:
                rt, rd = base_type(ret.replace('&', ''))
                deref = re.search(r'return\s*\*', body) or '&' in ret
                wt = ('ptr' if rd >= 1 else rt) if (deref or rd == 0) else 'addr'
                if rt in ('T',) or m[1]:
                    wt = 'T'
            kind = 'vslot' if VSLOT_HELPER.search(name) or VSLOT_HELPER.search(body[:300]) and 'vtbl' in body else 'field'
            if re.search(r'\*\s*reinterpret_cast<\s*void\s*\*\*\s*>\s*\(\s*' + re.escape(pn) + r'\s*\)', body):
                kind = 'vslot'
            defs[f][name] = (kind, wt, wparam, f)
            HELPER_DEFS[(f, name)] = (kind, wt, code.count('\n', 0, m0.start(1)) + 1)
    return defs


def main():
    files = walk(PROD)
    texts = {}
    for f in files:
        raw = open(f, encoding='utf-8', errors='replace').read()
        code = blank_strings(strip(raw))
        # raw text with the same length for comment lookups
        assert len(raw) == len(code), f
        texts[f] = (raw, code)
    # named integer constants (constexpr / const / enum / #define), per file and global
    const_pat = re.compile(r'(?:constexpr|const)\s+(?:static\s+)?(?:inline\s+)?[\w:]+\s+(' + IDENT + r')\s*=\s*'
                           r'\(?\s*(' + NUM + r')\s*\)?\s*;')
    def_pat = re.compile(r'^\s*#\s*define\s+(' + IDENT + r')\s+\(?\s*(' + NUM + r')\s*\)?\s*$', re.M)
    enum_pat = re.compile(r'\b(' + IDENT + r')\s*=\s*(' + NUM + r')\s*[,}]')
    per_file = {}; gconsts = {}; clash = set()
    for f in files:
        code = texts[f][1]; d = {}
        for pat in (const_pat, def_pat):
            for m in pat.finditer(code):
                d[m.group(1)] = parse_num(m.group(2))
        for m in re.finditer(r'\benum\b[^{;]*\{([^}]*)\}', code):
            for mm in enum_pat.finditer(m.group(1) + '}'):
                d[mm.group(1)] = parse_num(mm.group(2))
        per_file[f] = d
        if f.endswith(('.h', '.hpp', '.inc')):
            for k, v in d.items():
                if k in gconsts and gconsts[k] != v:
                    clash.add(k)
                gconsts.setdefault(k, v)
    for k in clash:
        gconsts.pop(k, None)
    global CLASSES, SIZES, RETAIL, CLASS_LOWER
    hfiles = [f for f in files]
    CLASSES, SIZES, ASSERTS = build_header_index(hfiles, texts, gconsts)
    for nm_, lst_ in CLASSES.items():
        for c_ in lst_:
            for f_ in c_.fields:
                if f_.note:
                    FIELD_NOTES[(c_.file, nm_, f_.name)] = f_.note
    CLASS_LOWER = {k.lower(): k for k in CLASSES if len(k) >= 5}
    RETAIL = set(CLASSES)
    for md in ('decomp/application.md', 'decomp/runtime.md'):
        if os.path.exists(md):
            for m_ in re.finditer(r'^\|[^|\n]*\|\s*`([A-Z][A-Za-z0-9]+)`\s*\|', open(md, encoding='utf-8').read(), re.M):
                RETAIL.add(m_.group(1))
    for f in files:
        for m_ in re.finditer(r'DECLARE_EXE_TYPE\(\s*(' + IDENT + r')', texts[f][1]):
            RETAIL.add(m_.group(1))
    # globals: extern T* g; / T* g = ...; at file scope (approximate: any declaration line starting at col 0)
    globals_ = {}
    gpat = re.compile(r'^(?:extern\s+|static\s+|inline\s+)*((?:const\s+)?[A-Za-z_][\w:]*(?:<[^;>]*>)?)\s*(\*+)\s*'
                      r'(?:const\s+)?(' + IDENT + r')\s*(?:=[^;]*)?;', re.M)
    for f in files:
        for m in gpat.finditer(texts[f][1]):
            if m.group(1) in KW_NOT_TYPE:
                continue
            bt, bd = base_type(m.group(1) + m.group(2))
            globals_.setdefault(m.group(3), (bt, bd))
    helper_defs = find_helpers(files, texts)
    # pass-through byte views (`return static_cast<uint8_t*>(p);`) and zero-argument getters
    # (`return GameUpdateLoop::_singleton;`), by name across PROD
    for f in files:
        code = texts[f][1]
        for fn_ in segment_functions(code):
            body = code[fn_.start + 1:fn_.end].strip()
            m_ = re.fullmatch(r'return\s+(.*?);', body, re.S)
            if not m_:
                continue
            nm_ = fn_.name.split('::')[-1]
            inner_, ct_ = strip_casts(m_.group(1))
            if inner_ in fn_.params and ct_ is not None:
                GLOBAL_PASS.setdefault(nm_, fn_.params[inner_][2])
            elif not fn_.params and re.search(r'::_singleton\b|^g_', inner_):
                GLOBAL_RET0.setdefault(nm_, m_.group(1))
    # include graph (basename) so a helper in a header counts in its includers only
    inc = {f: set(re.findall(r'#\s*include\s*[<"]([^>"]+)[>"]', texts[f][0])) for f in files}
    rows = []; stats = collections.Counter()
    for f in files:
        code = texts[f][1]
        fns = segment_functions(code)
        helpers = dict(helper_defs.get(f, {}))
        for hf, hd in helper_defs.items():
            if hf != f and hf.endswith(('.h', '.hpp', '.inc')) and \
                    any(i.split('/')[-1] == hf.split('/')[-1] for i in inc[f]):
                helpers.update(hd)
        ctx = Ctx(f, code, fns, per_file[f], gconsts, CLASSES, SIZES, globals_)
        scan_file(f, code, ctx, helpers, rows, stats)
    # de-duplicate (a cast nested in a cast can be seen twice at the same position)
    seen = set(); out = []
    for r in rows:
        key = (r[0], r[1], r[2], r[3], r[4], r[9], r[11])
        if key in seen:
            continue
        seen.add(key); out.append(r)
    rows = out
    rows.sort(key=lambda r: (r[0], r[1]))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    hdr = ['file', 'line', 'class', 'offset', 'width_type', 'header', 'field_name', 'state', 'module',
           'kind', 'conf', 'origin', 'indexed', 'function', 'hdr_type', 'spelling']
    with open(OUT, 'w', encoding='utf-8', newline='\n') as fo:
        fo.write('\t'.join(hdr) + '\n')
        for r in rows:
            fo.write('\t'.join(str(x) for x in r) + '\n')
    report(rows, stats)


NONWIDTH = {'-', 'addr', 'void*', 'T', '?', 'void', ''}
SIGNED = {'int8_t', 'char', 'signed char', 'int16_t', 'short', 'int', 'int32_t', 'long', 'int64_t', 'long long',
          'intptr_t', 'ptrdiff_t', 'LONG', 'BOOL', 'std::int32_t', 'std::int64_t', 'std::int16_t', 'std::int8_t'}
FLOATS = {'float', 'double'}
STRONG = {'this', 'decl', 'param', 'singleton', 'alias', 'member', 'cast', 'member-field', 'field', 'global'}


def wclass(t):
    """(size, kind) of a read width: kind s=signed u=unsigned f=float p=pointer c=class; None if not a width."""
    if t in NONWIDTH:
        return None
    if t == 'ptr' or t.endswith('*'):
        return (8, 'p')
    if t in PRIM:
        return (PRIM[t], 'f' if t in FLOATS else 's' if t in SIGNED else 'u' if t != 'bool' else 'b')
    sz = SIZES.get(t.split('::')[-1])
    return (sz, 'c:' + t.split('::')[-1]) if sz else (None, 'c:' + t.split('::')[-1])


def report(rows, stats):
    C = collections.Counter
    print(f'# rows {len(rows)}')
    print('# by state', dict(C(r[7] for r in rows).most_common()))
    print('# by kind', dict(C(r[9].split(':')[0] for r in rows).most_common()))
    print('# by spelling', dict(C(r[15] for r in rows).most_common()))
    print('# by conf', dict(C(r[10] for r in rows).most_common()))
    print('# stats (sites NOT in the table)', dict(stats))

    def top(p):
        return p.split('/')[0] if not p.startswith(('code/', 'runtime/')) else '/'.join(p.split('/')[:2])
    print('\n## per top directory: rows, resolved, by state')
    for d, n in C(top(r[0]) for r in rows).most_common():
        sub = [r for r in rows if top(r[0]) == d]
        print(f'  {d}\t{n}\tresolved={sum(1 for r in sub if r[2])}\t', dict(C(r[7] for r in sub).most_common()),
              '\tspelling', dict(C(r[15] for r in sub).most_common()))
    print('\n## per module: rows, resolved%, unknown causes')
    for d, n in C(r[8] for r in rows).most_common():
        sub = [r for r in rows if r[8] == d]
        res = sum(1 for r in sub if r[2])
        print(f'  {d}\t{n}\t{100 * res // n}%')

    # per class
    per = collections.defaultdict(list)
    for r in rows:
        if r[2]:
            per[r[2]].append(r)
    print(f'\n## per class ({len(per)} classes; rows with a class: {sum(len(v) for v in per.values())})')
    print('  class\treads\tdistinct_off\tnamed\tnamed_noassert\tguess\tinside\tinpad\tundeclared\tbeyond\tvptr\t'
          'no_header\tmodules\tstrong%\theader')
    order = sorted(per, key=lambda c: -len(per[c]))
    for c in order:
        v = per[c]
        offs = {}
        for r in v:
            offs.setdefault(r[3], r[7])
        st = C(offs.values())
        mods = sorted({r[8] for r in v})
        strong = sum(1 for r in v if r[10] in STRONG)
        hdr = sorted(CLASSES[c], key=def_rank)[0].file if c in CLASSES else '(no header)'
        print(f'  {c}\t{len(v)}\t{len(offs)}\t{st["named"]}\t{st["named_noassert"]}\t{st["guess"]}\t{st["inside"]}\t'
              f'{st["inpad"]}\t{st["undeclared"]}\t{st["beyond"]}\t{st["vptr"]}\t{st["no_header"]}\t'
              f'{len(mods)}:{",".join(m.replace("code/Game/", "G/").replace("runtime/src/", "rt/") for m in mods)}\t'
              f'{100 * strong // len(v)}\t{hdr}')

    # cross-module classes
    print('\n## cross-module classes (read from 3+ modules), by module count then reads')
    cm = [(c, sorted({r[8] for r in per[c]})) for c in per]
    cm = [x for x in cm if len(x[1]) >= 3]
    for c, mods in sorted(cm, key=lambda x: (-len(x[1]), -len(per[x[0]]))):
        bym = C(r[8] for r in per[c])
        print(f'  {c}\t{len(per[c])} reads\t{len(mods)} modules\t' +
              ', '.join(f'{m.replace("code/Game/", "G/").replace("runtime/src/", "rt/")}={n}' for m, n in bym.most_common()))

    # width conflicts between sites
    print('\n## width conflicts: same class+offset read with different widths (addr / void* / template T excluded)')
    grp = collections.defaultdict(list)
    for r in rows:
        if r[2] and wclass(r[4]):
            grp[(r[2], r[3])].append(r)
    cats = C(); items = []
    for (c, o), v in grp.items():
        ws = {wclass(r[4]) for r in v}
        if len(ws) < 2:
            continue
        sizes = {w[0] for w in ws}
        kinds = {w[1] for w in ws}
        if len(sizes) > 1:
            cat = 'size'
        elif 'f' in kinds and kinds & {'s', 'u'}:
            cat = 'float-vs-int'
        elif 'p' in kinds and kinds & {'s', 'u'}:
            cat = 'ptr-vs-int'
        elif {'s', 'u'} <= kinds:
            cat = 'sign'
        else:
            cat = 'other'
        cats[cat] += 1
        items.append((cat, c, o, sorted({r[4] for r in v}), len(v), sorted({f'{r[0]}:{r[1]}' for r in v})[:3]))
    print('  by category (class+offset pairs):', dict(cats))
    for cat in ('size', 'float-vs-int', 'ptr-vs-int', 'sign', 'other'):
        lst = [x for x in items if x[0] == cat]
        print(f'  -- {cat}: {len(lst)}')
        for x in sorted(lst, key=lambda x: -x[4])[:25]:
            print(f'     {x[1]}+{x[2]}\t{"/".join(x[3])}\t{x[4]} sites\te.g. {x[5][0]}')

    # read width vs the header's declared type
    print('\n## read width vs the header field type at that exact offset (named/guess/named_noassert rows)')
    hm = C(); hitems = collections.defaultdict(C)
    for r in rows:
        if r[7] not in ('named', 'named_noassert', 'guess') or not r[14]:
            continue
        a = wclass(r[4]); b = wclass(r[14]) if not r[14].endswith('*') else (8, 'p')
        if not a or not b or a[0] is None or b[0] is None:
            continue
        if a[0] != b[0]:
            cat = 'size'
        elif {a[1], b[1]} == {'f', 's'} or {a[1], b[1]} == {'f', 'u'}:
            cat = 'float-vs-int'
        elif {a[1], b[1]} == {'s', 'u'}:
            cat = 'sign'
        else:
            continue
        hm[cat] += 1
        hitems[cat][(r[2], r[3], r[14], r[4])] += 1
    print('  rows by category:', dict(hm))
    for cat in ('size', 'float-vs-int'):
        print(f'  -- {cat} (class+off header_type read_type sites)')
        for (c, o, ht, rt), n in hitems[cat].most_common(20):
            print(f'     {c}+{o}\t{ht} -> {rt}\t{n}')

    # untyped pointer fields: *(Cls+K) whose pointee is unknown
    print('\n## untyped pointer fields: reads through *(Class+off) whose pointee class is unknown (type the field -> '
          'these resolve)')
    lo = C(r[11] for r in rows if r[10] == 'loaded' and not r[11].startswith('*(?'))
    print(f'  rows {sum(lo.values())} through {len(lo)} fields; of which *(?+off) (parent unknown) '
          f'{sum(1 for r in rows if r[10] == "loaded" and r[11].startswith("*(?"))}')
    for k, n in lo.most_common(30):
        print(f'     {k}\t{n}')

    print('\n## unknown_class rows by cause')
    for k, n in C(re.sub(r'\?.*', '?', r[10]) for r in rows if r[7] == 'unknown_class').most_common():
        print(f'     {k}\t{n}')
    print('  body-name prefixes with no class (fname?X), top 25:')
    for k, n in C(r[10] for r in rows if r[10].startswith('fname?')).most_common(25):
        print(f'     {k[6:]}\t{n}')
    print('  unresolved calls (call), top 15:')
    for k, n in C(r[11] for r in rows if r[10] == 'call').most_common(15):
        print(f'     {k}\t{n}')

    print('\n## reads through a singleton (conf singleton, or alias of X::_singleton), by class')
    sg = C(r[2] for r in rows if '_singleton' in r[11] or r[10] == 'singleton')
    for k, n in sg.most_common(15):
        print(f'     {k}\t{n}')

    print('\n## top 40 files by rows (rows, resolved, owner class = most-read class through this/self/param)')
    OWN = {'this', 'member', 'fname', 'fname-abbrev', 'param', 'decl'}
    fl = C(r[0] for r in rows)
    for f, n in fl.most_common(40):
        sub = [r for r in rows if r[0] == f]
        own = C(r[2] for r in sub if r[2] and (r[10] in OWN or r[11] in ('this', 'self', 'selfV', 's', 'p')))
        o = own.most_common(1)[0] if own else ('-', 0)
        print(f'     {f}\t{n}\t{sum(1 for r in sub if r[2])}\t{o[0]}({o[1]})')

    # file -> owner class, for the block proposal
    print('\n## owner classes: files whose most-read self class is X (files, rows in those files)')
    owners = collections.defaultdict(lambda: [0, 0, set()])
    for f in fl:
        sub = [r for r in rows if r[0] == f]
        own = C(r[2] for r in sub if r[2] and (r[10] in OWN or r[11] in ('this', 'self', 'selfV', 's', 'p')))
        o = own.most_common(1)[0][0] if own else '(none)'
        owners[o][0] += 1; owners[o][1] += len(sub); owners[o][2].add(module_of(f))
    for o, (nf, nr, mods) in sorted(owners.items(), key=lambda x: -x[1][1])[:60]:
        print(f'     {o}\t{nf} files\t{nr} rows\t{",".join(sorted(m.replace("code/Game/", "G/").replace("runtime/src/", "rt/") for m in mods))}')

    print('\n# helpers (definition file:line, kind, width, uses with a literal / named-constant offset)')
    for (f, n), (k, w, ln) in sorted(HELPER_DEFS.items()):
        print(f'  {n}\t{f}:{ln}\t{k}\t{w}\t{HELPER_USES.get((f, n), 0)}')


if __name__ == '__main__':
    CLASSES = {}; SIZES = {}; RETAIL = set(); CLASS_LOWER = {}; FIELD_NOTES = {}
    GLOBAL_PASS = {}; GLOBAL_RET0 = {}
    main()
