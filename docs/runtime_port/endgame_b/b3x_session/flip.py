#!/usr/bin/env python3
"""flip.py -- B3X W1, THE FLIP (B3X_RULES X4): `virtual T* DeletingDtor(unsigned)` -> `virtual ~T()`.

Run from the repo root:
    python docs/runtime_port/endgame_b/b3x_session/flip.py --check    dry run: what it would do + what it cannot
    python docs/runtime_port/endgame_b/b3x_session/flip.py            apply
    python docs/runtime_port/endgame_b/b3x_session/flip.py --survey   per-class context of the dtor / slot 0

Driven by classes.py (imported: its census runs on import). Scope: PRODUCT classes (code, runtime, standalone,
engine, dx11) whose hierarchy root is one of FLIP_ROOTS, shape STD or VEC. Everything else is reported:
the PURE roots, the five Game loops (OTHER), the INHERITED three (need nothing), the exempt `Video::*Data`.

What it does (idempotent: a flipped class has no DeletingDtor, so classes.py no longer lists it):
  DECL   the in-class slot-0 statement (declaration, or inline definition with its vector arm) becomes the
         destructor at that position: `~T() override…` (subclass) / `virtual ~T()…` (root). The old
         non-virtual `~T()` statement is removed from where it stood and its text (`= default`, `;`, or the
         inline body byte-for-byte) is what the new statement carries. The slot comment on the line stays.
         The old destructor's own comment lines (the contiguous `//` lines directly above it, and a trailing
         `//` on its last line) move with it. An access label left with nothing under it is removed.
  DEF    the out-of-class `T::DeletingDtor(unsigned) { … }` definition is deleted, with the contiguous
         comment block directly above it when that block only describes the deleted body.
  CALLS  `DeletingDtor(0)` on `this` inside a member of a flipping class  -> `this->~T();` (unqualified;
         T = the class the member belongs to);  `p->X::DeletingDtor(0)` -> `p->X::~X()`;
         `p->DeletingDtor(1)` whose receiver is a flipping class -> `delete p` (receiver typed by the cast it
         is spelled with, a known accessor, or its declaration in the function); everything else is LISTED.
"""
import os, re, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
_argv = sys.argv; sys.argv = [sys.argv[0]]
import classes as C          # noqa: E402  (runs the census)
sys.argv = _argv

FLIP_ROOTS = {'Object::Base', 'Graphics::Geometry', 'IO::Stream', 'IO::ReaderWriter', 'Engine::UpdateLoop'}
PRODUCT_DIRS = ('code/', 'runtime/', 'standalone/', 'engine/', 'dx11/')

REPORT = collections.defaultdict(list)   # section -> lines


def rep(sec, msg):
    REPORT[sec].append(msg)


# ------------------------------------------------------------------------------------------------------------
# Position-preserving mask: comments -> spaces (newlines kept), literal contents -> spaces (quotes kept).
# ------------------------------------------------------------------------------------------------------------
def mask(src):
    out = list(src); i = 0; n = len(src)
    while i < n:
        c = src[i]
        if src.startswith('//', i):
            j = src.find('\n', i); j = n if j < 0 else j
            for k in range(i, j): out[k] = ' '
            i = j; continue
        if src.startswith('/*', i):
            j = src.find('*/', i + 2); j = n if j < 0 else j + 2
            for k in range(i, j):
                if src[k] != '\n': out[k] = ' '
            i = j; continue
        if c == '"' and i > 0 and src[i - 1] == 'R':
            m = re.match(r'"([^(\s]*)\(', src[i:])
            if m:
                end = src.find(')' + m.group(1) + '"', i)
                end = n if end < 0 else end + len(m.group(1)) + 2
                for k in range(i + 1, end - 1):
                    if src[k] != '\n': out[k] = ' '
                i = end; continue
        if c == '"' or (c == "'" and not (i > 0 and src[i - 1].isalnum())):
            q = c; j = i + 1
            while j < n and src[j] != q and src[j] != '\n':
                if src[j] == '\\': out[j] = ' '; j += 1
                if j < n and src[j] != '\n': out[j] = ' '
                j += 1
            i = j + 1; continue
        i += 1
    return ''.join(out)


class Src:
    def __init__(self, path):
        self.path = path
        self.text = open(path, encoding='utf-8', newline='').read()
        self.m = mask(self.text)
        self.ls = [0]
        for k, ch in enumerate(self.text):
            if ch == '\n': self.ls.append(k + 1)
        self.edits = []          # (start, end, replacement, tag)

    def line_start(self, ln): return self.ls[ln - 1]
    def line_end(self, ln): return self.ls[ln] - 1 if ln < len(self.ls) else len(self.text)   # index of '\n'
    def line_of(self, idx):
        lo, hi = 0, len(self.ls) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if self.ls[mid] <= idx: lo = mid
            else: hi = mid - 1
        return lo + 1

    def match(self, i, o, c):
        """i: index of opening char o in the mask; return index of its matching c."""
        d = 0; n = len(self.m)
        while i < n:
            ch = self.m[i]
            if ch == o: d += 1
            elif ch == c:
                d -= 1
                if d == 0: return i
            i += 1
        return None

    def add(self, s, e, rep_, tag):
        self.edits.append((s, e, rep_, tag))

    def apply(self):
        ed = sorted(self.edits, key=lambda x: (x[0], x[1]))
        for a, b in zip(ed, ed[1:]):
            if b[0] < a[1]:
                raise SystemExit(f'OVERLAP in {self.path}: {a[3]} [{a[0]},{a[1]}) vs {b[3]} [{b[0]},{b[1]})')
        t = self.text
        for (s, e, r, tag) in sorted(ed, key=lambda x: -x[0]):
            t = t[:s] + r + t[e:]
        return t


SRCS = {}


def src(p):
    if p not in SRCS: SRCS[p] = Src(p)
    return SRCS[p]


ACCESS_LABEL = re.compile(r'\b(?:public|protected|private)\s*:(?!:)')
WS = ' ' + chr(9) + chr(13) + chr(10)
TYPE_TOK = re.compile(r'(?:\s|\w|::|\*|&|<|>|,)')


def back_over_type(s, i):
    """From index i (start of a name token in the mask), walk back over a return type / `virtual` / a class
    qualifier. Stops at ; { } ) and friends; an access label inside the walked text is skipped."""
    j = i
    while j > 0 and (s.m[j - 1] in WS + ':*&<>,' or s.m[j - 1].isalnum() or s.m[j - 1] == '_'):
        j -= 1
    seg = s.m[j:i]
    mm = None
    for mm in ACCESS_LABEL.finditer(seg): pass
    if mm: j += mm.end()
    # never walk back into a preprocessor line (`#include <new>` ends in `>`)
    k = s.line_of(j)
    while k < s.line_of(i):
        if s.m[s.line_start(k):s.line_end(k)].lstrip().startswith('#'):
            j = max(j, s.line_start(k + 1))
        k += 1
    while j < i and s.m[j] in WS: j += 1
    return j


def skip_ws(s, i):
    while i < len(s.m) and s.m[i] in ' \t\r\n': i += 1
    return i


def statement_after_params(s, k):
    """k: index just after `)` of a declarator. Return (kind, end_index_exclusive): kind decl/pure/inline."""
    j = skip_ws(s, k)
    while True:
        mm = re.match(r'(override|final|const|noexcept)\b', s.m[j:j + 12])
        if not mm: break
        j = skip_ws(s, j + mm.end())
    if s.m[j] == ';': return 'decl', j + 1
    mm = re.match(r'=\s*0\s*;', s.m[j:j + 20])
    if mm: return 'pure', j + mm.end()
    mm = re.match(r'=\s*default\s*;', s.m[j:j + 30])
    if mm: return 'default', j + mm.end()
    if s.m[j] == '{':
        e = s.match(j, '{', '}')
        return 'inline', e + 1
    return None, j


# ------------------------------------------------------------------------------------------------------------
# Class body location in the ORIGINAL file (classes.py positions are in its stripped/expanded copy).
# ------------------------------------------------------------------------------------------------------------
def class_body(c):
    s = src(c.file)
    F = C.FILES[c.file]
    ln = c.sc.line
    ls = F.rfind('\n', 0, c.sc.start) + 1
    k = F.count('{', ls, c.sc.start)
    a = s.line_start(ln); b = s.line_end(ln)
    idx = -1; pos = a
    for _ in range(k + 1):
        idx = s.m.find('{', pos, b)
        if idx < 0: return None
        pos = idx + 1
    e = s.match(idx, '{', '}')
    if e is None or s.line_of(e) != C.line_of(c.file, c.sc.end):
        return None
    return idx, e


def depth0_positions(s, a, b):
    """Yield indices in (a, b) that are at the class's own level."""
    d = 0; i = a + 1
    while i < b:
        ch = s.m[i]
        if ch in '({[': d += 1
        elif ch in ')}]': d -= 1
        elif d == 0: yield i
        i += 1


def find_at_level(s, a, b, pat):
    lvl = set()
    d = 0
    out = []
    for m in pat.finditer(s.m, a + 1, b):
        # depth at m.start()
        out.append(m)
    res = []
    for m in out:
        d = 0
        seg = s.m[a + 1:m.start()]
        # count braces/parens (cheap: classes are small)
        d = seg.count('{') - seg.count('}') + seg.count('(') - seg.count(')')
        if d == 0: res.append(m)
    return res


def comment_block_above(s, line):
    """Contiguous pure-comment lines directly above `line` (1-based). Returns first line of the block or None."""
    k = line - 1; first = None
    while k >= 1:
        t = s.text[s.line_start(k):s.line_end(k)]
        if t.strip().startswith('//') and s.m[s.line_start(k):s.line_end(k)].strip() == '':
            first = k; k -= 1; continue
        break
    return first


def whole_lines(s, st, en):
    """If [st, en) is alone on its lines (only ws before; only ws or a // comment after), return
    (line_a, line_b, trailing_comment) else None."""
    la = s.line_of(st); lb = s.line_of(max(st, en - 1))
    before = s.text[s.line_start(la):st]
    after_m = s.m[en:s.line_end(lb)]
    if before.strip() or after_m.strip(): return None
    trailing = s.text[en:s.line_end(lb)].strip()
    return la, lb, trailing


# ------------------------------------------------------------------------------------------------------------
# DECL + DEF
# ------------------------------------------------------------------------------------------------------------
DD_TOKEN = re.compile(r'\bDeletingDtor\s*\(\s*(?:unsigned|uint32_t|int|uint8_t)\b[^)]*\)')


def flip_class(c, survey=False):
    tag = f'{c.qname} ({c.file}:{c.line})'
    s = src(c.file)
    cb = class_body(c)
    if cb is None:
        rep('HAND', f'{tag}: class body not located in the original file'); return False
    a, b = cb
    is_root = c.root is c
    # ---- slot-0 statement ----
    hits = [m for m in find_at_level(s, a, b, DD_TOKEN)]
    if len(hits) != 1:
        rep('HAND', f'{tag}: {len(hits)} class-level DeletingDtor statements'); return False
    dm = hits[0]
    name_i = s.m.rfind('DeletingDtor', dm.start(), dm.end())
    d_st = back_over_type(s, name_i)
    head = s.m[d_st:name_i]
    if not re.fullmatch(r'\s*(?:virtual\s+)?(?:(?:::)?[\w:]+(?:<[^;{}]*>)?\s*[*&]?\s*)', head):
        rep('HAND', f'{tag}: unexpected slot-0 head {head!r}'); return False
    kind, d_en = statement_after_params(s, dm.end())
    if kind not in ('decl', 'inline'):
        rep('HAND', f'{tag}: slot-0 statement kind {kind}'); return False
    if (kind == 'inline') != (c.dd_def[2] == 'inline'):
        rep('HAND', f'{tag}: slot-0 inline/ooc mismatch with classes.py'); return False
    if c.dd_access != 'public':
        rep('HAND', f'{tag}: slot 0 is {c.dd_access} -- the destructor must be public'); return False

    # ---- the old destructor ----
    dtor_pat = re.compile(r'~\s*' + re.escape(c.short) + r'\s*\(\s*(?:void)?\s*\)')
    dh = [m for m in find_at_level(s, a, b, dtor_pat) if not re.search(r'(->|\.|::)\s*$', s.m[max(0, m.start() - 4):m.start()])]
    if len(dh) != 1:
        rep('HAND', f'{tag}: {len(dh)} class-level ~{c.short}() statements'); return False
    tm = dh[0]
    t_st = back_over_type(s, tm.start())
    thead = s.m[t_st:tm.start()]
    if not re.fullmatch(r'\s*(?:virtual\s+|inline\s+)*', thead):
        rep('HAND', f'{tag}: unexpected dtor head {thead!r}'); return False
    tkind, t_en = statement_after_params(s, tm.end())
    if tkind not in ('decl', 'default', 'inline'):
        rep('HAND', f'{tag}: dtor statement kind {tkind}'); return False
    if re.search(r'\bvirtual\b', thead):
        rep('HAND', f'{tag}: dtor already virtual'); return False
    # the new statement: the old text from `~T(...)` on, with `override` after the declarator (or `virtual`)
    q = re.match(r'\s*noexcept\b', s.m[tm.end():t_en])
    ins = tm.end() + (q.end() if q else 0)
    if is_root:
        new = 'virtual ' + s.text[tm.start():t_en]
    else:
        new = s.text[tm.start():ins] + ' override' + s.text[ins:t_en]
    # ---- comments that belong to the old destructor ----
    wl = whole_lines(s, t_st, t_en)
    if wl is None:
        rep('HAND', f'{tag}: the old ~{c.short}() shares its line with other code'); return False
    la, lb, trailing = wl
    cfirst = comment_block_above(s, la)
    moved_lead = ''
    if cfirst is not None:
        moved_lead = s.text[s.line_start(cfirst):s.line_start(la)]
    # is the comment block directly above the dtor right under an access label? then it belongs to the dtor.
    # (kept simple: the contiguous // block is the dtor's own comment)
    # removal span: whole lines la..lb plus the lead comment block
    r_st = s.line_start(cfirst if cfirst is not None else la)
    r_en = s.line_start(lb + 1) if lb + 1 <= len(s.ls) else len(s.text)
    # ---- slot-0 replacement text ----
    indent = s.text[s.line_start(s.line_of(d_st)):d_st]
    if moved_lead:
        # re-indent the moved comment lines to the slot-0 statement's indentation
        lead_lines = [indent + x.strip() for x in moved_lead.rstrip('\n').split('\n')]
        lead_lines[0] = lead_lines[0][len(indent):]      # the first line sits after the existing indentation
        eol = '\r\n' if '\r\n' in moved_lead else '\n'
        lead = eol.join(lead_lines) + eol + indent
    else:
        lead = ''
    new_stmt = new
    old_len = d_en - d_st
    # trailing comment of the old dtor: append after the slot comment line
    d_line_end = s.line_end(s.line_of(d_en - 1))
    slot_trailing = s.text[d_en:d_line_end].strip()
    if (slot_trailing and '\n' not in new_stmt and s.line_of(d_st) == s.line_of(d_en - 1)
            and len(new_stmt) < old_len):
        new_stmt = new_stmt + ' ' * (old_len - len(new_stmt))   # keep the slot comment's column
    extra_edits = []
    if trailing:
        if slot_trailing:
            # keep both: the dtor's comment goes on its own line above the statement
            eol = '\r\n' if s.text[d_line_end - 1:d_line_end] == '\r' else '\n'
            lead = lead + trailing + eol + indent
        else:
            new_stmt = new_stmt.rstrip() + '  ' + trailing
    # Replace starting at the statement (the indentation before d_st stays)
    if survey:
        print(f'=== {tag}  root={is_root} slot0={kind} dtor={tkind}')
        print('  SLOT0:', ' '.join(s.text[d_st:d_en].split())[:140], '|', slot_trailing[:80])
        print('  DTOR :', ' '.join(s.text[t_st:t_en].split())[:140], '|', trailing[:80])
        if moved_lead: print('  LEAD :', ' / '.join(x.strip() for x in moved_lead.strip().split('\n'))[:300])
        print('  NEW  :', (lead + new_stmt).replace('\n', '\n         '))
    s.add(d_st, d_en, lead + new_stmt, f'decl {c.qname}')
    # An old dtor ABOVE or BELOW the slot-0 statement: its removal must not overlap the replacement
    if r_st < d_en and r_en > d_st:
        rep('HAND', f'{tag}: old dtor overlaps slot 0'); s.edits.pop(); return False
    s.add(r_st, r_en, '', f'rmdtor {c.qname}')
    # An access label left empty: `protected:` followed only by blank lines, then `};` or another label
    lab_ln = la - 1 if cfirst is None else cfirst - 1
    if lab_ln >= 1:
        lab = s.text[s.line_start(lab_ln):s.line_end(lab_ln)]
        if re.fullmatch(r'\s*(public|protected|private)\s*:\s*', lab):
            nxt = s.m[r_en:b + 1]
            nm = re.match(r'\s*(?:\}|(?:public|protected|private)\s*:(?!:))', nxt)
            if nm:
                # drop the label line (and one blank line before it, if any)
                st = s.line_start(lab_ln)
                prev = lab_ln - 1
                if prev >= 1 and s.text[s.line_start(prev):s.line_end(prev)].strip() == '':
                    st = s.line_start(prev)
                s.add(st, s.line_start(lab_ln + 1), '', f'rmlabel {c.qname}')
    for m in ('non-virtual', 'NON-VIRTUAL', 'nonvirtual', 'Non-virtual', 'protected', 'Protected', 'decision 2', 'Decision 2'):
        if m in moved_lead or m in trailing:
            rep('COMMENT', f'{tag}: moved dtor comment mentions "{m}" -- review'); break
    # ---- out-of-class DeletingDtor definition ----
    if c.dd_def[2] == 'ooc':
        if not delete_ooc(c): return False
    return True


BANNER_KEEP = re.compile(r'={4,}|-{4,}|#region')
BANNER_DD = re.compile(r'\[0\]|slot 0|DeletingDtor|deleting|MemFree|\?\?_[GE]|~\w+', re.I)


def one_line(block):
    return ' / '.join(x.strip() for x in block.strip().splitlines())


def delete_ooc(c):
    f, ln, _ = c.dd_def
    s = src(f)
    tag = f'{c.qname} def {f}:{ln}'
    a = s.line_start(ln); b = s.line_end(ln)
    mm = re.compile(r'\bDeletingDtor\s*\(').search(s.m, a, b)
    if not mm:
        rep('HAND', f'{tag}: DeletingDtor not on the recorded line'); return False
    st = back_over_type(s, mm.start())
    pe = s.match(s.m.index('(', mm.start()), '(', ')')
    kind, en = statement_after_params(s, pe + 1)
    if kind != 'inline':
        rep('HAND', f'{tag}: out-of-class definition not followed by a body'); return False
    wl = whole_lines(s, st, en)
    if wl is None:
        rep('HAND', f'{tag}: definition shares its line'); return False
    la, lb, trailing = wl
    first = comment_block_above(s, la)
    r_st = s.line_start(la)
    if first is not None:
        block = s.text[s.line_start(first):s.line_start(la)]
        if not BANNER_DD.search(block):
            rep('BANNER-KEPT', f'{tag}: comment block above kept: ' + one_line(block)[:200])
        else:
            r_st = s.line_start(first)
            if block.count(chr(10)) > 6:
                rep('BANNER-DELETED', f'{tag}: {block.count(chr(10))}-line banner deleted: ' + one_line(block)[:240])
    r_en = s.line_start(lb + 1) if lb + 1 <= len(s.ls) else len(s.text)
    # collapse a double blank line
    prev_blank = s.line_of(r_st) > 1 and s.text[s.line_start(s.line_of(r_st) - 1):s.line_end(s.line_of(r_st) - 1)].strip() == ''
    nxt_ln = s.line_of(r_en) if r_en < len(s.text) else None
    if prev_blank and nxt_ln and s.text[s.line_start(nxt_ln):s.line_end(nxt_ln)].strip() == '':
        r_en = s.line_start(nxt_ln + 1) if nxt_ln + 1 <= len(s.ls) else len(s.text)
    s.add(r_st, r_en, '', f'rmdef {c.qname}')
    return True


# ------------------------------------------------------------------------------------------------------------
# CALLS
# ------------------------------------------------------------------------------------------------------------
def flipping(c):
    return c is not None and c.in_scope and c.root.qname in FLIP_ROOTS


def cls_by_name(name, path=(), ctx='runtime/x.cpp'):
    r, how = C.resolve(name, tuple(path), ctx)
    return r


NS_BY_FILE = collections.defaultdict(list)
for _sc in C.ALL_SCOPES:
    if _sc.kind in ('ns', 'class'): NS_BY_FILE[_sc.file].append(_sc)


def scope_path_at(path, line):
    """The namespace/class path enclosing `line` (innermost)."""
    best = None
    for sc in NS_BY_FILE[path]:
        l2 = C.line_of(path, sc.end) if sc.end is not None else 10 ** 9
        if sc.line <= line <= l2 and (best is None or sc.start >= best.start): best = sc
    return best.path if best is not None else ()


def cls_at(name, path, line):
    """Resolve a type name as seen from `line` of `path` (namespace, then the member's class)."""
    r = cls_by_name(name, scope_path_at(path, line), path)
    if r is None:
        cl = enclosing_member(path, line)
        if cl is not None: r = cls_by_name(name, cl.sc.path, path)
    return r


# the enclosing function of an index in the ORIGINAL file, from classes.py's scopes (line-based)
SCOPES_BY_FILE = collections.defaultdict(list)
for _sc in C.ALL_SCOPES:
    if _sc.kind == 'other': SCOPES_BY_FILE[_sc.file].append(_sc)
CLASS_OF_SCOPE = {id(c.sc): c for c in C.CLASSES}


def enclosing_member(path, line):
    """The class the function containing `line` belongs to (in-class definition: the class; out-of-class:
    its qualifier), from classes.py's scopes."""
    best = None
    for sc in SCOPES_BY_FILE[path]:
        l1 = sc.line; l2 = C.line_of(path, sc.end) if sc.end is not None else 10 ** 9
        if l1 <= line <= l2:
            if best is None or sc.start >= best.start: best = sc
    if best is None: return None
    sc = best
    while sc.parent is not None and sc.parent.kind == 'other': sc = sc.parent
    if sc.parent is not None and sc.parent.kind == 'class':
        return CLASS_OF_SCOPE.get(id(sc.parent))
    m = re.search(r'((?:::)?[A-Za-z_]\w*(?:\s*::\s*[A-Za-z_]\w*)*)\s*::\s*~?\w+\s*\([^()]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?$', sc.head)
    if not m: return None
    cl, how = C.resolve_qualifier(m.group(1), sc)
    return cl


SELF0 = re.compile(r'(?<![\w>.:])(?:this\s*->\s*)?DeletingDtor\s*\(\s*0u?\s*\)')
QUAL0 = re.compile(r'(->|\.)\s*((?:(?:::)?\w+::)*)(\w+)::DeletingDtor\s*\(\s*0u?\s*\)')
DEL1 = re.compile(r'->\s*DeletingDtor\s*\(\s*1u?\s*\)')
RECV0 = re.compile(r'(->|\.)\s*DeletingDtor\s*\(\s*0u?\s*\)')


def product_files():
    for p in C.FILES:
        if p.startswith(PRODUCT_DIRS): yield p


# receiver typing for `delete`
ACCESSOR_TYPES = {'AsRw': 'IO::ReaderWriter', 'Rw': 'IO::ReaderWriter'}


def expr_start(s, i):
    """i = index of the `->`; walk back over a postfix-expression (casts, calls, member chains)."""
    j = i
    while j > 0 and s.m[j - 1] in ' ' + chr(9): j -= 1
    while True:
        if j > 0 and s.m[j - 1] == ')':
            o = j - 1; d = 0
            while o >= 0:
                if s.m[o] == ')': d += 1
                elif s.m[o] == '(':
                    d -= 1
                    if d == 0: break
                o -= 1
            j = o; k2 = j
            if k2 > 0 and s.m[k2 - 1] == '>':
                d = 0; o = k2 - 1
                while o >= 0:
                    if s.m[o] == '>': d += 1
                    elif s.m[o] == '<':
                        d -= 1
                        if d == 0: break
                    o -= 1
                k2 = o
            k3 = k2
            while k3 > 0 and (s.m[k3 - 1].isalnum() or s.m[k3 - 1] in '_:'): k3 -= 1
            if k3 < k2: j = k3
        else:
            k3 = j
            while k3 > 0 and (s.m[k3 - 1].isalnum() or s.m[k3 - 1] in '_:'): k3 -= 1
            if k3 == j: return j
            j = k3
        if j >= 2 and s.m[j - 2:j] == '->': j -= 2; continue
        if j >= 1 and s.m[j - 1] == '.' : j -= 1; continue
        return j


def receiver_type(s, expr, at):
    e = expr.strip()
    m = re.match(r'^(?:static_cast|reinterpret_cast)\s*<\s*(?:const\s+)?((?:::)?[\w:]+)\s*\*\s*>\s*\(', e)
    if m: return m.group(1), 'cast'
    m = re.match(r'^(\w+)\s*\(', e)
    if m and m.group(1) in ACCESSOR_TYPES: return ACCESSOR_TYPES[m.group(1)], 'accessor ' + m.group(1)
    m = re.fullmatch(r'(\w+)', e)
    if m:
        v = m.group(1)
        # the nearest declaration above `at` in the same file: `Type* v` / `Type *v`
        seg = s.m[max(0, at - 20000):at]
        dm = None
        for dm in re.finditer(r'((?:::)?[A-Za-z_][\w:]*(?:<[^;(){}]*>)?)\s*\*\s*(?:const\s+)?' + re.escape(v) + r'\b\s*(?:=|;|,|\))', seg):
            pass
        if dm: return dm.group(1), 'decl'
        return None, 'variable ' + v
    m = re.fullmatch(r'(?:this\s*->\s*)?(_\w+|m_\w+)', e)
    if m:
        return None, 'member ' + m.group(1)
    return None, 'expr'


def do_calls(apply_mode):
    counts = collections.Counter()
    for p in sorted(product_files()):
        s = src(p)
        if 'DeletingDtor' not in s.text: continue
        # (1) self-calls
        for m in SELF0.finditer(s.m):
            ln = s.line_of(m.start())
            cl = enclosing_member(p, ln)
            if cl is None:
                rep('CALL-HAND', f'{p}:{ln}: DeletingDtor(0) self-call, enclosing class unknown'); continue
            if not flipping(cl):
                rep('CALL-KEPT', f'{p}:{ln}: DeletingDtor(0) self-call in {cl.qname} (not flipping)'); continue
            s.add(m.start(), m.end(), f'this->~{cl.short}()', f'self0 {cl.qname}')
            counts['DeletingDtor(0) on this -> this->~T()'] += 1
        # (2) qualified (0)
        for m in QUAL0.finditer(s.m):
            ln = s.line_of(m.start())
            q = m.group(2) + m.group(3)
            cl = cls_at(q, p, ln)
            if not flipping(cl):
                rep('CALL-KEPT', f'{p}:{ln}: {q}::DeletingDtor(0) (not flipping)'); continue
            s.add(m.start(), m.end(), f'{m.group(1)}{q}::~{m.group(3)}()', f'qual0 {q}')
            counts['X::DeletingDtor(0) -> X::~X()'] += 1
        # (3) unqualified (0) on another object -> listed (hand)
        for m in RECV0.finditer(s.m):
            if QUAL0.match(s.m, m.start()): continue
            pre = s.m[max(0, m.start() - 6):m.start()]
            if re.search(r'this\s*$', pre): continue
            ln = s.line_of(m.start())
            rep('CALL-HAND', f'{p}:{ln}: unqualified DeletingDtor(0) on another object: {s.text[s.line_start(ln):s.line_end(ln)].strip()}')
        # (4) (1) -> delete
        for m in DEL1.finditer(s.m):
            ln = s.line_of(m.start())
            st = expr_start(s, m.start())
            expr = s.text[st:m.start()]
            # the statement: [ (void) ] expr->DeletingDtor(1) ;
            k = st
            pre_m = s.m[s.line_start(s.line_of(st)):st]
            void_cast = re.search(r'\(\s*void\s*\)\s*$', pre_m)
            after = s.m[m.end():m.end() + 3]
            if not re.match(r'\s*;', after):
                rep('CALL-HAND', f'{p}:{ln}: DeletingDtor(1) not a statement: {s.text[s.line_start(ln):s.line_end(ln)].strip()}'); continue
            ty, how = receiver_type(s, expr, st)
            cl = cls_at(ty, p, ln) if ty else None
            if cl is None:
                rep('CALL-HAND', f'{p}:{ln}: DeletingDtor(1) receiver not typed ({how}): {s.text[s.line_start(ln):s.line_end(ln)].strip()}'); continue
            if not flipping(cl):
                rep('CALL-KEPT', f'{p}:{ln}: DeletingDtor(1) on {cl.qname} (exempt/not flipping)'); continue
            vst = st - (len(void_cast.group(0)) if void_cast else 0)
            s.add(vst, m.end(), 'delete ' + expr.strip(), f'del1 {cl.qname}')
            counts[f'DeletingDtor(1) -> delete ({how.split()[0]})'] += 1
            rep('CALL-DELETE', f'{p}:{ln}: delete {expr.strip()}  [{cl.qname}; typed by {how}]')
    return counts


# ------------------------------------------------------------------------------------------------------------
def main():
    check = '--check' in sys.argv
    survey = '--survey' in sys.argv
    todo = collections.Counter(); done = []
    for c in sorted(C.SC, key=lambda c: (c.file, c.line)):
        if c.area != 'product': continue
        if c.root.qname not in FLIP_ROOTS:
            rep('EXEMPT', f'{c.qname} ({c.file}:{c.line}) root {c.root.qname} -- stays on DeletingDtor'); continue
        if c.shape in ('STD', 'VEC'):
            if flip_class(c, survey): done.append(c); todo[c.root.qname] += 1
            notes = []
            if re.search(r'\bfinal\b', c.sc.head): notes.append('final')
            if c.sc.parent is not None and c.sc.parent.kind == 'class': notes.append('nested in ' + '::'.join(c.sc.parent.path))
            q = c.sc.parent
            while q is not None:
                if q.kind == 'ns' and q.name == '(anon)': notes.append('anonymous namespace'); break
                q = q.parent
            if c.file.endswith('.cpp'): notes.append('class defined in a .cpp')
            if notes: rep('NOTE', f'{c.qname} ({c.file}:{c.line}): ' + ', '.join(notes))
        elif c.shape == 'INHERITED':
            rep('NOTHING', f'{c.qname} ({c.file}:{c.line}) INHERITED -- nothing to do')
        else:
            rep('HAND', f'{c.qname} ({c.file}:{c.line}) shape {c.shape} -- by hand (dd {c.dd_decl}, def {c.dd_def})')
    counts = do_calls(not check)
    if survey: return
    print('# flip.py ' + ('--check (nothing written)' if check else '(applied)'))
    print(f'classes flipped by the script: {len(done)}  ' + ', '.join(f'{k} {v}' for k, v in todo.most_common()))
    for k, v in counts.most_common(): print(f'call sites: {k}: {v}')
    for sec in ('HAND', 'CALL-HAND', 'COMMENT', 'BANNER-DELETED', 'BANNER-KEPT', 'CALL-KEPT', 'NOTHING', 'EXEMPT',
                'NOTE', 'CALL-DELETE'):
        L = REPORT.get(sec, [])
        print(f'\n## {sec} ({len(L)})')
        for x in L: print('  ' + x)
    if not check:
        n = 0
        for p, s in SRCS.items():
            if not s.edits: continue
            t = s.apply()
            if t != s.text:
                open(p, 'w', encoding='utf-8', newline='').write(t); n += 1
        print(f'\nfiles written: {n}')


if __name__ == '__main__':
    main()
