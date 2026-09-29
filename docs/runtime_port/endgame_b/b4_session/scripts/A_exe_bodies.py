#!/usr/bin/env python3
"""A_exe_bodies.py -- B.4 block 0, inventory A: every C-body declaration in code/Game.

Run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b4_session/scripts/A_exe_bodies.py
It rewrites docs/runtime_port/endgame_b/b4_session/data/A_exe_bodies.tsv.

Scope (the C-body headers of code/Game): the nine code/Game/<Module>/exe_bodies.h, every
code/Game/**/*_funcs.h, code/Game/Work/marketwork_bst_*.h and the helper headers in EXTRA_HEADERS.
A declaration is a top-level (namespace scope) function declaration or inline definition.

Tree evidence is recomputed on every run (comments stripped with b4_session/codegrep.py).
Retail evidence (Ghidra, Application-x64.exe) cannot be recomputed without Ghidra: it is kept in the
table's trailing columns retail_name / retail_vtable / retail_this and re-read from the existing
table on a re-run (keyed by address). `--ghidra-script` prints the read-only Ghidra script that
produced them; `--addrs FILE` writes the address list it takes; `--retail FILE` merges its output.
"""
import os, re, sys, glob, csv
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..', '..'))
sys.path.insert(0, os.path.dirname(HERE))
from codegrep import strip  # noqa: E402

OUT = os.path.join(ROOT, 'docs/runtime_port/endgame_b/b4_session/data/A_exe_bodies.tsv')
PROD = ('code', 'runtime', 'standalone', 'dx11', 'engine')
EXCL = ('standalone/build', 'tests/unit/build', '.claude/worktrees', '/build/', '/build-')
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')
EXTRA_HEADERS = [
    'code/Game/Dialog/ui_dialogs.h', 'code/Game/Dialog/ui_panels.h',
    'code/Game/Dialog/dialog_exe_helpers.h', 'code/Game/Dialog/citizen_icon_helpers.h',
    'code/Game/Goal/goal_exe_helpers.h', 'code/Game/Goal/statvaluedispatch.h',
    'code/Game/Game/game.h', 'code/Game/Component/controller_state_table.h',
]
COLS = ['addr', 'name', 'module', 'defining_file', 'class', 'class_header', 'member_declared',
        'member_defined', 'static_or_member', 'callers', 'confidence', 'note',
        'decl', 'first_param', 'caller_files', 'retail_name', 'retail_vtable', 'retail_this']
ADDR = re.compile(r'(?:0x|FUN_)(14[0-9a-fA-F]{7})\b')   # function addresses only (not DAT_)
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'catch', 'sizeof', 'alignof', 'decltype',
            'static_assert', 'defined', 'alignas', 'noexcept', 'throw', 'new', 'delete'}
SCALARS = {'void', 'int', 'char', 'bool', 'float', 'double', 'short', 'long', 'unsigned', 'signed',
           'int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
           'uintptr_t', 'intptr_t', 'size_t', 'uint', 'const', 'volatile', 'struct', 'class'}


def rel(p):
    return os.path.relpath(p, ROOT).replace('\\', '/')


def walk(dirs):
    for d in dirs:
        for dp, dn, fs in os.walk(os.path.join(ROOT, d)):
            q = dp.replace('\\', '/') + '/'
            if any(e in q for e in EXCL):
                continue
            for f in fs:
                if f.endswith(EXTS):
                    yield os.path.join(dp, f)


_cache = {}


def code_of(path):
    if path not in _cache:
        src = open(path, encoding='utf-8', errors='replace').read()
        code = strip(src)
        # blank preprocessor lines (keep line count)
        code = '\n'.join('' if l.lstrip().startswith('#') else l for l in code.split('\n'))
        _cache[path] = (src, code)
    return _cache[path]


def scope_headers():
    hs = sorted(glob.glob(os.path.join(ROOT, 'code/Game/*/exe_bodies.h')))
    hs += sorted(glob.glob(os.path.join(ROOT, 'code/Game/**/*_funcs.h'), recursive=True))
    hs += sorted(glob.glob(os.path.join(ROOT, 'code/Game/Work/marketwork_bst_*.h')))
    hs += [os.path.join(ROOT, h) for h in EXTRA_HEADERS if os.path.exists(os.path.join(ROOT, h))]
    return [h for h in dict.fromkeys(os.path.normpath(h) for h in hs)]


def match_paren(s, i):
    """s[i] == '(' ; return index after the matching ')'."""
    d = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == '(':
            d += 1
        elif c == ')':
            d -= 1
            if d == 0:
                return i + 1
        i += 1
    return n


def match_brace(s, i):
    d = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == '{':
            d += 1
        elif c == '}':
            d -= 1
            if d == 0:
                return i + 1
        i += 1
    return n


FN_HEAD = re.compile(r'([A-Za-z_~][\w]*(?:\s*::\s*~?[A-Za-z_]\w*)*)\s*\(')


def parse_fn(stmt):
    """stmt: text of a top-level statement (no trailing ; or {). Return (qualname, params, ret) or None."""
    t = ' '.join(stmt.split())
    if not t or '(' not in t:
        return None
    if re.match(r'(typedef|using|static_assert|template\s*<\s*>|friend|return)\b', t):
        return None
    if re.match(r'(class|struct|union|enum)\b[^(]*$', t):
        return None
    m = None
    for mm in FN_HEAD.finditer(t):
        name = mm.group(1).replace(' ', '')
        if name.split('::')[-1] in KEYWORDS or name in ('extern', '__attribute__', 'alignas'):
            continue
        if name in ('__declspec',):
            continue
        m = mm
        break
    if not m:
        return None
    pre = t[:m.start()]
    if '=' in pre or pre.rstrip().endswith('(*') or pre.strip() == '':
        return None
    close = match_paren(t, m.end() - 1)
    params = t[m.end():close - 1].strip()
    rest = t[close:].strip()
    if rest.startswith('('):   # function pointer variable
        return None
    if '=' in rest and not re.search(r'=\s*(default|delete|0)\b', rest):
        return None
    ret = re.sub(r'extern\s+"C"\s*', '', pre).strip()
    return m.group(1).replace(' ', ''), params, ret


def parse_header(path):
    src, code = code_of(path)
    raw = src.split('\n')
    decls = []
    i, n = 0, len(code)
    start = 0
    frames = []           # only transparent frames (namespace / extern "C") are ever open here

    def line_of(pos):
        return code.count('\n', 0, pos) + 1
    while i < n:
        c = code[i]
        if c == ';':
            stmt = code[start:i]
            r = parse_fn(stmt)
            if r:
                decls.append((r, start, i, False))
            start = i + 1
        elif c == '{':
            stmt = code[start:i]
            t = ' '.join(stmt.split())
            if re.match(r'(namespace\b|extern\s+"C"\s*$)', t) or t == '':
                frames.append(i)
                start = i + 1
            else:
                end = match_brace(code, i)
                r = parse_fn(stmt)
                if r and not re.match(r'(class|struct|union|enum)\b', t):
                    decls.append((r, start, i, True))
                # skip the body; a following ';' (class) closes it
                i = end
                j = i
                while j < n and code[j] in ' \t\r\n':
                    j += 1
                if j < n and code[j] == ';':
                    i = j
                start = i + 1 if (i < n and code[i] == ';') else i
                i += 1 if (i < n and code[i] == ';') else 0
                continue
        elif c == '}':
            if frames:
                frames.pop()
            start = i + 1
        i += 1
    out = []
    for (qn, params, ret), s, e, inline in decls:
        # position of the name
        k = code.find(qn.split('::')[-1] + '', s)
        ln = line_of(k if k >= 0 else s)
        eln = line_of(e)
        addrs = []
        trail = raw[eln - 1]
        if '//' in trail:
            addrs = ADDR.findall(trail[trail.index('//'):])
        src_where = 'trailing'
        if not addrs:
            # nearest contiguous comment block above the first line of the statement
            first = line_of(s + len(code[s:e]) - len(code[s:e].lstrip()))
            j = first - 2
            block = []
            while j >= 0:
                l = raw[j].strip()
                if l.startswith('//') or l.startswith('*') or l.startswith('/*'):
                    if re.match(r'//\s*[=\-]{6,}', l):
                        break
                    block.insert(0, l)
                    j -= 1
                    continue
                break
            for l in block:
                addrs += ADDR.findall(l)
            src_where = 'comment-above'
        if not addrs:
            # a comment inside the declaration's own lines
            for l in raw[ln - 1:eln]:
                if '//' in l:
                    addrs += ADDR.findall(l[l.index('//'):])
            src_where = 'comment-in'
        addrs = list(dict.fromkeys('0x' + a.lower() for a in addrs))
        noaddr = 'no retail address' in ' '.join(raw[max(0, ln - 3):eln])
        out.append(dict(qual=qn, name=qn.split('::')[-1], params=params, ret=ret, line=ln,
                        inline=inline, addrs=addrs, addr_src=src_where if addrs else '', noaddr=noaddr))
    return out


def load_p0():
    m = {}
    for f in ('P0a_declared.tsv', 'P0b_declared.tsv', 'P0c_declared.tsv'):
        p = os.path.join(ROOT, 'docs/runtime_port/endgame_b/b3r_session/data', f)
        if not os.path.exists(p):
            continue
        for row in csv.reader(open(p, encoding='utf-8'), delimiter='\t'):
            if len(row) >= 2 and row[0].startswith('0x'):
                m.setdefault(row[1], []).append(row[0].lower())
    return m


DEF_RE = re.compile(r'(?<![\w:.>])((?:[A-Za-z_]\w*\s*::\s*)*~?[A-Za-z_]\w*)\s*\(')


def index_definitions(files):
    """qualified name -> [(file, line)] for every function definition (a body follows the params)."""
    defs = defaultdict(list)
    for p in files:
        src, code = code_of(p)
        for m in DEF_RE.finditer(code):
            qn = re.sub(r'\s+', '', m.group(1))
            last = qn.split('::')[-1]
            if last in KEYWORDS:
                continue
            close = match_paren(code, m.end() - 1)
            j = close
            tail = code[j:j + 400]
            mm = re.match(r'\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?'
                          r'(?:__attribute__\s*\(\([^)]*\)\)\s*)?(\{|:(?!:))', tail)
            if not mm:
                continue
            if mm.group(1) == ':' and '::' not in qn:
                continue   # ?: or label, not a ctor init list
            # reject calls inside expressions: previous non-space char must end a type/decl
            k = m.start() - 1
            while k >= 0 and code[k] in ' \t\r\n':
                k -= 1
            prev = code[k] if k >= 0 else '\n'
            if prev in '=(,+-/%!?|^[' or (prev == '&' and code[k - 1:k + 1] == '&&'):
                continue
            if prev in ';{}' and '::' not in qn:
                # `foo(a) {` after ; is a call only in macro soup; a definition needs a return type
                continue
            defs[qn].append((rel(p), code.count('\n', 0, m.start()) + 1))
    return defs


def index_classes(files):
    """class name -> [header] for every class/struct definition with a body."""
    cls = defaultdict(list)
    rx = re.compile(r'\b(class|struct)\s+(?:alignas\s*\(\s*\d+\s*\)\s*)?(?:__declspec\([^)]*\)\s*)?'
                    r'([A-Za-z_]\w*)\s*(?:final\s*)?(?::[^;{()]*)?\{')
    for p in files:
        if not p.endswith(('.h', '.hpp')):
            continue
        src, code = code_of(p)
        for m in rx.finditer(code):
            cls[m.group(2)].append((rel(p), code.count('\n', 0, m.start()) + 1, m.start()))
    return cls


def index_member_addrs(classes):
    """retail address -> (class, member, header:line) for every member declaration inside a class
    body whose line carries the address in a comment (`void RefreshEntityFlags(); // 0x1400c3470`).
    The innermost class wins."""
    spans = []
    for cname, hs in classes.items():
        for hpath, hline, hpos in hs:
            src, code = code_of(os.path.join(ROOT, hpath))
            b = code.find('{', hpos)
            e = match_brace(code, b)
            spans.append((code.count('\n', 0, b) + 1, code.count('\n', 0, e) + 1, cname, hpath))
    by_file = defaultdict(list)
    for s in spans:
        by_file[s[3]].append(s)
    out = {}
    for hpath, ss in by_file.items():
        src, code = code_of(os.path.join(ROOT, hpath))
        raw, cl = src.split('\n'), code.split('\n')
        for ln in range(len(cl)):
            if ln >= len(raw) or '//' not in raw[ln]:
                continue
            addrs = ADDR.findall(raw[ln][raw[ln].index('//'):])
            m = re.search(r'(~?[A-Za-z_]\w*|operator\S+)\s*\(', cl[ln])
            if not addrs or not m or m.group(1) in KEYWORDS:
                continue
            inner = [s for s in ss if s[0] <= ln + 1 <= s[1]]
            if not inner:
                continue
            s = min(inner, key=lambda s: s[1] - s[0])
            for a in addrs:
                out.setdefault('0x' + a.lower(), (s[2], m.group(1), f'{hpath}:{ln + 1}'))
    return out


def class_members(path, pos):
    """names declared in the class body opening at code index `pos` (method names only)."""
    src, code = code_of(os.path.join(ROOT, path))
    b = code.find('{', pos)
    e = match_brace(code, b)
    body = code[b + 1:e - 1]
    # drop nested bodies (inline methods) but keep their heads
    flat, d = [], 0
    for ch in body:
        if ch == '{':
            d += 1
            if d == 1:
                flat.append(';')
            continue
        if ch == '}':
            d -= 1
            continue
        if d == 0:
            flat.append(ch)
    names = set()
    for m in re.finditer(r'(~?[A-Za-z_]\w*)\s*\(', ''.join(flat)):
        if m.group(1) not in KEYWORDS:
            names.add(m.group(1))
    return names, code[b:e]


def first_param_type(params):
    if not params or params == 'void':
        return ''
    d = 0
    for i, ch in enumerate(params):
        if ch in '<(':
            d += 1
        elif ch in '>)':
            d -= 1
        elif ch == ',' and d == 0:
            params = params[:i]
            break
    p = params.strip()
    p = re.sub(r'\s*=.*$', '', p)
    m = re.match(r'(.*?[\*&\s])\s*([A-Za-z_]\w*)?$', p)
    typ = (m.group(1) if m else p).strip()
    return typ


def type_class(typ):
    t = re.sub(r'\b(const|volatile|struct|class)\b', '', typ).replace('*', ' ').replace('&', ' ').split()
    if not t:
        return ''
    base = t[-1].split('::')[-1]
    if base in SCALARS or not ('*' in typ or '&' in typ):
        return ''
    return base


# Runtime value / handle types: a function whose first parameter is one of these is not thereby
# a member of it (Ghidra auto-types param_1 as Internal* / InternalBasePtr* all over the exe).
GENERIC = {'Internal', 'InternalBasePtr', 'External', 'ExternalPtr', 'Serializer', 'Stream', 'Thread',
           'String', 'Time', 'Vector3', 'Vector2', 'Vector4', 'Matrix', 'Matrix4', 'Object', 'Random',
           'Type', 'TypeInfo', 'BaseTypeInfo', 'Factory', 'undefined', 'undefined8', 'longlong', 'char',
           'Array', 'Encoding', 'Mutex', 'Event', 'Base', 'Geometry', 'Element'}
# the template families: (class key in the class index, header it must live in, body-name verb -> member)
TEMPLATE_HOMES = {
    'Collection::Array': ('Array', 'runtime/types/collection/Collection.h',
                          [(r'Resize|SetCount|Stride|ResizeFill|SetSize', 'SetSize'), (r'Clone|Copy|Assign', 'Assign')]),
    'Collection::Set': ('Set', 'runtime/types/collection/Collection.h',
                        [(r'Insert|Upsert', 'Insert'), (r'Find|Lookup', 'Find'), (r'Erase|Remove', 'Remove')]),
    'Object::ExternalPtr': ('ExternalPtr', 'runtime/types/object/External.h',
                            [(r'Array_Serialize|SerializeArray', 'SerializeArray'), (r'Serialize', 'Serialize')]),
}
# verbs of an Array<T> operation (a bare "Array" in a name is not one: CitizenUI_BuildIconArray_Impl)
ARRAYISH = re.compile(r'(Resize|DynArray|Erase|Push|Upsert|Clone|SetCount|SetSize|Stride|Reverse|Append|VectorClone)', re.I)


VTCOMMENT = re.compile(r'(?:vtable|Vtable|VTABLE|vftable|table)\b[^\n0-9]{0,24}0x(14[0-9a-fA-F]{7})')


def vtable_map(files, classes, lower_classes):
    """retail table address -> class, from the tree's own `table 0x...` / `Vtable: 0x...` comments in the
    file that defines the class (header) or whose stem names it (.cpp)."""
    vt = {}
    for p in files:
        if not p.startswith(os.path.join(ROOT, 'code')):
            continue
        src = code_of(p)[0]
        stem = os.path.splitext(os.path.basename(p))[0].lower()
        owner = lower_classes.get(stem, [''])[0]
        if not owner:
            continue
        for m in VTCOMMENT.finditer(src[:4000]):     # the file banner only
            vt.setdefault('0x' + m.group(1).lower(), owner)
    return vt


def table_class(lab, vtmap):
    """a table label `name@addr` -> class: `vtable_X` (Ghidra's typed label), else the tree's `table 0x...`
    comment, else a `g_X_Vtable` label. The g_ labels are older and wrong twice: g_PauseDialog_Vtable
    0x14015f550 holds MainMenuDialog_* slots (tree: MainMenuDialog.h table 0x14015f550) and
    g_ReloadDialog_Vtable 0x14015c480 holds MessageDialog_* slots."""
    name, _, addr = lab.partition('@')
    m = re.match(r'vtable_([A-Za-z_]\w*)$', name)
    if m:
        return m.group(1)
    tree = vtmap.get('0x' + addr.lower().rjust(9, '0'), '')
    if tree:
        return tree
    m = re.match(r'g_([A-Za-z_]\w*?)_Vtable$', name)
    return m.group(1) if m else ''


def vtable_class(rv, vtmap, prefer=''):
    """(class, slot, label, all classes) for the tables holding the function; a slot shared by several
    tables (inherited, or identical bodies folded) goes to `prefer` when it is one of them."""
    hits = []
    for lab in (rv or '').split(','):
        m = re.match(r'([^\[,]*@[0-9a-fA-F]+)\[(\d+)\]$', lab)
        if m:
            c = table_class(m.group(1), vtmap)
            if c:
                hits.append((c, int(m.group(2)), lab))
    if not hits:
        return None
    pick = next((h for h in hits if h[0] == prefer), hits[0])
    return pick + (sorted({h[0] for h in hits}),)


def attribute(name, qual, fpc, rn, rv, rt, deffile, classes, lower_classes, vtmap, tmem):
    """(class, confidence, static_or_member, evidence[]) -- strongest evidence first."""
    ev = []
    offs = re.search(r'rcx=obj\(([^)]*)\)', rt or '')
    offs = set(x for x in offs.group(1).split(',') if x) if offs else None
    nparams = re.search(r'nparams=(\d+)', rt or '')
    nparams = int(nparams.group(1)) if nparams else -1
    retail_word = re.split(r'_', rn or '')[0] if rn and not rn.startswith(('FUN_', 'thunk_')) else ''
    pre = name.split('_')[0]
    vt = vtable_class(rv, vtmap, pre)
    if vt:
        c, slot, lab, allc = vt
        ev.append(f'retail {lab}')
        if len(allc) > 1:
            ev.append('slot shared by the tables of ' + ','.join(allc) + ' (a base-class member or a folded body)')
        if pre in classes and pre != c:
            ev.append(f'NAME MISLEADS: body name says {pre}')
        return c, 'high', 'member(virtual)', ev
    st_ = re.search(r'stores-vtable=(\S+)', rt or '')
    if st_ and table_class(st_.group(1), vtmap):
        c = table_class(st_.group(1), vtmap)
        ev.append(f'retail stores {st_.group(1)} into [this] (ctor/dtor of {c})')
        if pre in classes and pre != c:
            ev.append(f'NAME MISLEADS: body name says {pre}')
        return c, 'high', 'member(ctor/dtor)', ev
    # retail shapes: the receiver's field offsets identify a template family
    ce_fields = {'0x0', '0x8', '0x10', '0x18', '0x1c', '0x1f', '0x20', '0x28', '0x30', '0x38', '0x3c', '0x40'}
    if offs is not None and {'0x30', '0x38'} <= offs and offs <= {'0x30', '0x38', '0x28', '0x0'} \
            and nparams in (2, 3, -1) and re.search(r'FindComp|GetComp|Component|JobHost|Entity_', name + ' ' + (rn or '')):
        ev.append('retail this-use +0x30/+0x38 = ComponentEntity::_components/_componentCount (FindComponent<T>)')
        return 'ComponentEntity', 'high', 'member-template', ev
    if tmem:
        c, mem, where = tmem
        ev.append(f'tree declares {c}::{mem} with this address ({where})')
        if offs is not None and c == 'ComponentEntity' and not offs <= ce_fields:
            ev.append('CONFLICT: retail this-use reads offsets beyond ComponentEntity (0x48)')
        if pre in classes and pre != c:
            ev.append(f'NAME MISLEADS: body name says {pre}')
        som = 'member' if offs is not None or re.search(r'rcx=fwd', rt or '') else 'member?'
        return c, 'medium', som, ev
    if offs is not None and {'0x30', '0x38'} <= offs and offs <= ce_fields and '0x40' in offs | {'0x10'}:
        ev.append('retail this-use offsets are all ComponentEntity fields (+0x30 components, +0x38 count)')
        return 'ComponentEntity', 'medium', 'member', ev
    if offs is not None and offs <= {'0x0', '0x8', '0xc', '0x4'} and ('0x8' in offs or '0xc' in offs) \
            and ARRAYISH.search(name + ' ' + (rn or '')):
        ev.append('retail this-use +0/+8/+0xc = Collection::Array<T> data/size/capacity')
        return 'Collection::Array', 'high', 'member-template', ev
    if re.search(r'p1type=Serializer\b|fwd->\S*IO::Serializer::', rt or '') and not re.search(r'ExternalPtr|ExtPtr|External_', name):
        ev.append('retail receiver is an IO::Serializer (a Persist helper / template instantiation), '
                  'not the class the body name says')
        return '', 'medium', 'free-or-template (persist helper)', ev
    if re.search(r'ExternalPtr|ExtPtr|Ext_Serialize|External_', name):
        ev.append('name + shape: Object::ExternalPtr<T> persist instantiation')
        return 'Object::ExternalPtr', 'medium', 'member-template', ev
    if re.search(r'Bst|BST|Avl|AVL|TreeNode|RbTree|MapFind|MapInsert', name):
        ev.append('name: Collection AVL/BST internals (template)')
        return 'Collection::Set', 'medium' if offs is not None else 'low', 'member-template', ev
    rcx_obj = offs is not None
    rcx_scalar = bool(re.search(r'rcx=scalar', rt or ''))
    rcx_unused = bool(re.search(r'rcx=unused', rt or ''))
    cand, conf = '', 'low'
    if '::' in (rn or ''):
        cand, conf = rn.split('::')[-2], 'medium'
        ev.append(f'ghidra namespace {rn}')
    if not cand and fpc and fpc in classes and fpc not in GENERIC:
        cand, conf = fpc, 'medium'
        ev.append(f'param0 typed {fpc}*')
    if not cand and '::' in qual:
        cand, conf = qual.split('::')[-2], 'medium'
        ev.append('qualified declaration')
    m = re.search(r'p1type=([A-Za-z_]\w*)', rt or '')
    if not cand and m and m.group(1) in classes and m.group(1) not in GENERIC:
        cand, conf = m.group(1), 'medium'
        ev.append(f'ghidra param_1 typed {m.group(1)}*')
    if not cand and retail_word and retail_word in classes and rcx_obj:
        cand, conf = retail_word, 'medium' if retail_word == pre else 'low'
        ev.append(f'ghidra name {rn}' + ('' if retail_word == pre else f' (body name says {pre})'))
    if not cand and rcx_obj:
        # every retail caller Ghidra names is `X_...` for one tree class X: X passes its receiver on
        cm = re.search(r'rcallers=(\d+)\{([^}]*)\}', rt or '')
        if cm:
            fam = {c.split('_')[0] for c in cm.group(2).split('|') if not c.startswith(('FUN_', 'thunk_'))}
            fam = {f for f in fam if f in classes and f not in GENERIC}
            if len(fam) == 1 and int(cm.group(1)) <= 4:
                cand, conf = fam.pop(), 'low'
                ev.append(f'retail callers are all {cand} functions (Ghidra names)')
    if not cand and pre in classes:
        cand, conf = pre, 'low'
        ev.append('body name prefix')
    if not cand and deffile != '-':
        stem = os.path.splitext(os.path.basename(deffile.split(':')[0]))[0].lower()
        if stem in lower_classes:
            cand, conf = lower_classes[stem][0], 'low'
            ev.append('defining file stem')
    if cand and retail_word and retail_word in classes and retail_word != cand:
        ev.append(f'CONFLICT: ghidra name says {retail_word}')
    if rcx_scalar and not rcx_obj:
        som = 'static?' if cand else 'free'
        ev.append('retail RCX is a scalar')
    elif rcx_unused:
        som = 'static?' if cand else 'free'
        ev.append('retail RCX unused')
    elif rcx_obj or re.search(r'rcx=fwd', rt or ''):
        som = 'member' if cand else 'member-of-unknown'
    elif cand:
        som = 'member?'
    else:
        som = 'unknown'
    return cand, conf, som, ev


def main():
    args = sys.argv[1:]
    if '--ghidra-script' in args:
        print(GHIDRA_SCRIPT)
        return
    prod_files = list(walk(PROD))
    test_files = list(walk(('tests',)))
    headers = scope_headers()
    p0 = load_p0()
    rows = []
    for h in headers:
        mod = rel(h).split('/')[2]
        for d in parse_header(h):
            d['header'] = rel(h)
            d['module'] = mod
            if not d['addrs'] and d['name'] in p0:
                d['addrs'] = p0[d['name']]
                d['addr_src'] = 'P0 table'
            rows.append(d)
    defs = index_definitions(prod_files)
    classes = index_classes(prod_files)
    lower_classes = defaultdict(list)
    for c in classes:
        lower_classes[c.lower()].append(c)
    vtmap = vtable_map(prod_files, classes, lower_classes)
    memaddr = index_member_addrs(classes)

    # retail cache: prior table (keyed by address, and by name for rows whose address came from
    # Ghidra), or a fresh Ghidra dump (--retail)
    retail, retail_by_name = {}, {}
    if os.path.exists(OUT):
        for r in csv.DictReader(open(OUT, encoding='utf-8'), delimiter='\t'):
            if r.get('retail_name') or r.get('retail_this'):
                v = (r['retail_name'], r['retail_vtable'], r['retail_this'])
                retail[r['addr']] = v
                if 'addr from ghidra' in r['note']:
                    retail_by_name[r['name']] = (r['addr'],) + v
    if '--retail' in args:
        for line in open(args[args.index('--retail') + 1], encoding='utf-8', errors='replace'):
            f = line.rstrip('\r\n').split('\t')
            if len(f) < 4:
                continue
            v = (f[1], f[2], ' '.join(f[3:]))
            if f[0].startswith('name:'):
                m = re.match(r'name:(\S+?)(?:=([0-9a-fA-F]+) (.*))?$', f[0])
                if m and m.group(2):
                    a_ = '0x' + m.group(2).lower().lstrip('0').rjust(9, '0')
                    retail_by_name[m.group(1)] = (a_,) + v
                    retail[a_] = v
            elif f[0].startswith('0x'):
                retail[f[0].lower()] = v

    # references: code lines naming the body, per file
    names = {r['name'] for r in rows}
    word = re.compile(r'\b(' + '|'.join(sorted(map(re.escape, names), key=len, reverse=True)) + r')\b')
    refs = defaultdict(list)
    for p in prod_files + test_files:
        src, code = code_of(p)
        rp = rel(p)
        for ln, l in enumerate(code.split('\n'), 1):
            for m in set(word.findall(l)):
                refs[m].append((rp, ln))
    decl_lines = defaultdict(set)
    for r in rows:
        decl_lines[r['name']].add((r['header'], r['line']))

    header_addrs = {a for r in rows for a in r['addrs']}
    table = []
    for r in rows:
        name, qual = r['name'], r['qual']
        dl = defs.get(qual, []) or [x for q, v in defs.items() if q.split('::')[-1] == name and '::' not in q for x in v]
        if r['inline']:
            dl = [(r['header'], r['line'])] + [x for x in dl if x != (r['header'], r['line'])]
        deffile = ';'.join(sorted({f'{f}:{l}' for f, l in dl})) or '-'
        if not r['addrs'] and dl and not r['noaddr']:
            # the address in the comment block right above the definition
            fsrc = code_of(os.path.join(ROOT, dl[0][0]))[0].split('\n')
            j = dl[0][1] - 2
            got = []
            while j >= 0 and j > dl[0][1] - 25:
                l = fsrc[j].strip()
                if not (l.startswith('//') or l.startswith('*') or l.startswith('/*') or l == ''
                        or re.match(r'(extern|static|inline|[A-Za-z_][\w:<>\*\s]*$)', l)):
                    break
                if re.match(r'.*[;}]\s*$', l):
                    break
                got += ADDR.findall(l)
                j -= 1
            if got:
                r['addrs'] = ['0x' + got[0].lower()]   # the nearest one
                r['addr_src'] = 'comment above definition'
                if r['addrs'][0] in header_addrs:
                    # either a second body of that function (EntityList_FlagAggregation) or the comment
                    # names a callee (TradeUI_RefreshOrderQuantityDisplay): read by hand
                    r['addr_src'] += ' (ALSO another row\'s header address: duplicate body or a callee named)'
        if not r['addrs'] and name in retail_by_name:
            r['addrs'] = [retail_by_name[name][0]]
            r['addr_src'] = 'ghidra (Class_SlotN vtable read or exact Ghidra name)'
        defset = {(f, l) for f, l in dl}
        # a reference is a code line that is not a declaration or the definition head
        callers = [(f, l) for f, l in refs.get(name, [])
                   if (f, l) not in decl_lines[name] and (f, l) not in defset]
        prod_c = [x for x in callers if not x[0].startswith('tests/')]
        test_c = [x for x in callers if x[0].startswith('tests/')]
        fpt = first_param_type(r['params'])
        fpc = type_class(fpt)
        addr = r['addrs'][0] if r['addrs'] else '-'
        rn, rv, rt = retail.get(addr, ('', '', ''))

        # a header address that is data, not code, is not the body's address
        bad = ''
        if rt.startswith('not code'):
            bad = addr          # kept in the addr column (so a re-run finds its cached verdict)
        tmem = memaddr.get(addr)
        cand, conf, som, ev = attribute(name, qual, fpc, rn, rv, rt, deffile, classes, lower_classes, vtmap, tmem)
        # class header
        ch = '-'
        declared = '-'
        defined = '-'
        if cand in TEMPLATE_HOMES:
            key, home, verbs = TEMPLATE_HOMES[cand]
            hs = [x for x in classes.get(key, []) if x[0] == home]
            # the template class is the span declaring the most of the family's members
            hs.sort(key=lambda x: -len({mm for _, mm in verbs} & class_members(x[0], x[2])[0]))
            if hs:
                ch = f'{hs[0][0]}:{hs[0][1]}'
                mnames, _ = class_members(hs[0][0], hs[0][2])
                want = next((mm for rx, mm in verbs if re.search(rx, name)), '')
                hit = want if want in mnames else ''
                declared = hit or 'no'
                defined = 'yes:template (in the header)' if hit else 'no'
        elif cand == 'ComponentEntity' and som == 'member-template':
            hs = classes.get('ComponentEntity', [])
            ch = f'{hs[0][0]}:{hs[0][1]}' if hs else 'none'
            declared, defined = 'no (no FindComponent<T> member in the tree)', 'no'
        elif cand and cand in classes:
            hdrs = classes[cand]
            gh = [x for x in hdrs if x[0].startswith(('code/Game', 'runtime/types'))] or hdrs
            hpath, hline, hpos = gh[0]
            ch = f'{hpath}:{hline}'
            mnames, body = class_members(hpath, hpos)
            mem = name
            if name.startswith(cand + '_'):
                mem = name[len(cand) + 1:]
            elif '::' in qual:
                mem = name
            hit = ''
            if tmem and tmem[0] == cand:
                hit = tmem[1]
                hpath, hline = tmem[2].rsplit(':', 1)
                ch = tmem[2]
            elif mem in mnames:
                hit = mem
            else:
                # the address appears somewhere in the class's header file (a comment, not a declaration)
                hsrc, _ = code_of(os.path.join(ROOT, hpath))
                if addr != '-' and re.search(addr[2:], hsrc, re.I):
                    hit = 'addr-in-header'
            declared = hit or 'no'
            if hit and hit != 'addr-in-header':
                dq = [q for q in defs if q.endswith(cand + '::' + hit)]
                defined = 'yes:' + defs[dq[0]][0][0] if dq else 'no'
            elif hit:
                defined = '?'
            else:
                defined = 'no'
        elif cand:
            ch = 'none'
        note = []
        if len(r['addrs']) > 1:
            note.append('addrs ' + ','.join(r['addrs']))
        if r['addr_src'] and r['addr_src'] != 'trailing':
            note.append('addr from ' + r['addr_src'])
        if r['inline']:
            note.append('inline in header')
        if r.get('discarded'):
            note.append(f"the comment above the definition names {r['discarded']}, which another row owns: not this body's address")
        if addr == '-' and r['noaddr']:
            note.append('header says: no retail address')
        if test_c:
            note.append(f'tests {len(test_c)}')
        note += ev
        if bad:
            note.append(f'the address {bad} is DATA, not code: no retail function checked')
        # the body's own name says one member, the tree/retail says another (a vtable slot misnamed)
        if cand and declared not in ('-', 'no', 'addr-in-header') and not declared.startswith('no'):
            said = name[len(cand) + 1:] if name.startswith(cand + '_') else ''
            if said and said != declared and declared != cand and not re.match(r'Slot\d+$', said) \
                    and re.match(r'[A-Z][A-Za-z]+\d*$', said):
                note.append(f'MEMBER NAME MISLEADS: body says {said}, the member at this address is {declared}')
        if defined.startswith('yes'):
            note.append('member already DEFINED: this C body is a second body of it (rewire callers, delete)')
        table.append({
            'addr': addr, 'name': qual, 'module': r['module'], 'defining_file': deffile,
            'class': cand or '-', 'class_header': ch, 'member_declared': declared,
            'member_defined': defined, 'static_or_member': som, 'callers': str(len(prod_c)),
            'confidence': conf if cand else 'low', 'note': '; '.join(note),
            'decl': f"{r['header']}:{r['line']}", 'first_param': fpt or '-',
            'caller_files': str(len({f for f, _ in prod_c})),
            'retail_name': rn, 'retail_vtable': rv, 'retail_this': rt,
        })
    # several C bodies declared for one retail address: duplicates (or one function split in pieces)
    by_addr = defaultdict(list)
    for t in table:
        if t['addr'] != '-':
            by_addr[t['addr']].append(t['name'])
    for t in table:
        others = [n for n in by_addr.get(t['addr'], []) if n != t['name']]
        if others:
            t['note'] = '; '.join(x for x in (t['note'], 'SAME ADDRESS as ' + ','.join(others)) if x)
    if '--addrs' in args:
        # the Ghidra pass's input; an address-less row goes by name (Class_SlotN -> vtable_Class)
        with open(args[args.index('--addrs') + 1], 'w') as o:
            for a in sorted({t['addr'] for t in table if t['addr'] != '-'}):
                o.write(a + '\n')
            for t in table:
                if t['addr'] == '-':
                    o.write('name:' + t['name'] + '\n')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8', newline='') as o:
        o.write('\t'.join(COLS) + '\n')
        for t in table:
            o.write('\t'.join(str(t[c]).replace('\t', ' ') for c in COLS) + '\n')
    print(f'{len(table)} declarations from {len(headers)} headers -> {rel(OUT)}')


# The read-only Ghidra pass that produced retail_name / retail_vtable / retail_this. Run it through the
# Ghidra MCP `run_script_inline` with args `<addrs file from --addrs> <out tsv>`, then `--retail <out tsv>`.
# It reads Application-x64.exe through the ProgramManager: no switch_program, no transaction.
GHIDRA_SCRIPT = r'''import ghidra.app.script.GhidraScript;
import ghidra.app.services.ProgramManager;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.mem.*;
import java.io.*;
import java.util.*;

// READ-ONLY: reads Application-x64.exe through the ProgramManager (no switch_program, no
// transaction, nothing renamed or saved). args: <in addrs file> <out tsv>
public class RetailA extends GhidraScript {
  Program p; Listing L; SymbolTable st; ReferenceManager rm; Memory mem; AddressSpace sp;

  Address A(long v) { return sp.getAddress(v); }

  Map<Long, List<Long>> qmap = new HashMap<>();

  void buildQmap() throws Exception {
    for (MemoryBlock b : mem.getBlocks()) {
      if (b.isExecute() || !b.isInitialized()) continue;
      String bn = b.getName();
      if (bn.startsWith(".pdata") || bn.startsWith(".xdata") || bn.startsWith(".reloc")) continue;
      long s0 = (b.getStart().getOffset() + 7) & ~7L, e = b.getEnd().getOffset();
      byte[] buf = new byte[(int) (e - s0 + 1)];
      b.getBytes(A(s0), buf);
      for (int i = 0; i + 8 <= buf.length; i += 8) {
        long v = 0; for (int k = 7; k >= 0; k--) v = (v << 8) | (buf[i + k] & 0xff);
        if (v >= 0x140001000L && v < 0x140200000L) qmap.computeIfAbsent(v, x -> new ArrayList<>()).add(s0 + i);
      }
    }
  }

  boolean isCodePtr(long x) throws Exception {
    long v = mem.getLong(A(x));
    MemoryBlock b = mem.getBlock(A(v));
    return v >= 0x140001000L && v < 0x140200000L && b != null && b.isExecute();
  }

  String tableName(long start) {
    Symbol s = st.getPrimarySymbol(A(start));
    return (s == null ? "" : s.getName(true)) + "@" + Long.toHexString(start);
  }

  String vtables(Address entry) throws Exception {
    StringBuilder sb = new StringBuilder();
    List<Long> hits = qmap.get(entry.getOffset());
    if (hits == null) return "";
    for (long from : hits) {
      MemoryBlock fb = mem.getBlock(A(from));
      if (fb == null || !fb.getName().startsWith(".rdata")) {
        if (sb.length() > 0) sb.append(",");
        sb.append("dataptr@" + Long.toHexString(from) + "(" + (fb == null ? "?" : fb.getName()) + ")");
        continue;
      }
      long x = from;
      while (x - 8 >= fb.getStart().getOffset() && isCodePtr(x - 8) && from - x < 0x800) {
        Symbol us = st.getPrimarySymbol(A(x));
        if (us != null && us.getSource() != SourceType.DEFAULT) break;   // a named table starts here
        x -= 8;
      }
      Symbol s0 = st.getPrimarySymbol(A(x));
      String lab = s0 == null ? "" : s0.getName(true);
      if (lab.startsWith("UNWIND") || lab.contains("IPToState") || lab.contains("UnwindMap")
          || lab.contains("RUNTIME_FUNCTION") || lab.startsWith("TryBlock") || lab.startsWith("HandlerType")) continue;
      if (sb.length() > 0) sb.append(",");
      sb.append(tableName(x) + "[" + ((from - x) / 8) + "]");
    }
    return sb.toString();
  }

  boolean isRcxFamily(Register r) {
    if (r == null) return false;
    String n = r.getName();
    return n.equals("RCX") || n.equals("ECX") || n.equals("CX") || n.equals("CL") || n.equals("CH");
  }

  String thisUse(Function f) {
    Set<String> alias = new HashSet<>(); alias.add("RCX");
    TreeSet<Long> offs = new TreeSet<>();
    boolean scalar = false, obj = false; String fwd = null; boolean rcxWritten = false;
    Map<String,String> leaVt = new HashMap<>(); String vtStore = null;
    InstructionIterator ii = L.getInstructions(f.getBody(), true);
    int n = 0;
    while (ii.hasNext() && n < 120 && !alias.isEmpty()) {
      Instruction in = ii.next(); n++;
      String mn = in.getMnemonicString();
      if (mn.equals("LEA") && in.getNumOperands() == 2) {
        Object[] d0 = in.getOpObjects(0);
        for (Reference r : in.getReferencesFrom()) {
          MemoryBlock tb = mem.getBlock(r.getToAddress());
          if (tb != null && tb.getName().startsWith(".rdata") && d0.length == 1 && d0[0] instanceof Register) {
            leaVt.put(((Register) d0[0]).getBaseRegister().getName(), tableName(r.getToAddress().getOffset()));
          }
        }
      }
      if (mn.equals("MOV") && in.getNumOperands() == 2 && (in.getOperandType(0) & OperandType.DYNAMIC) != 0) {
        Object[] d0 = in.getOpObjects(0), s1 = in.getOpObjects(1);
        boolean base = false, nz = false;
        for (Object o : d0) { if (o instanceof Register && alias.contains(((Register) o).getBaseRegister().getName())) base = true;
          if (o instanceof Scalar && ((Scalar) o).getValue() != 0) nz = true; }
        if (base && !nz && s1.length == 1 && s1[0] instanceof Register) {
          String lab = leaVt.get(((Register) s1[0]).getBaseRegister().getName());
          if (lab != null) vtStore = lab;
        }
      }
      // memory operands based on an alias
      for (int i = 0; i < in.getNumOperands(); i++) {
        int t = in.getOperandType(i);
        Object[] os = in.getOpObjects(i);
        if ((t & OperandType.DYNAMIC) != 0 || (t & OperandType.ADDRESS) != 0 && os.length > 1) {
          boolean hit = false; long off = 0;
          for (Object o : os) {
            if (o instanceof Register && alias.contains(((Register) o).getBaseRegister().getName())
                && ((Register) o).getBitLength() == 64) hit = true;
            if (o instanceof Scalar) off = ((Scalar) o).getSignedValue();
          }
          if (hit && !mn.equals("LEA")) { obj = true; if (offs.size() < 12) offs.add(off); }
          if (hit && mn.equals("LEA")) { obj = true; }
        }
      }
      // scalar use of ECX/CL while RCX still holds arg 1
      if (alias.contains("RCX") && !rcxWritten) {
        for (Object o : in.getInputObjects()) {
          if (o instanceof Register) {
            Register r = (Register) o;
            if (isRcxFamily(r) && r.getBitLength() < 64 && !mn.startsWith("PUSH")) scalar = true;
          }
        }
      }
      if (mn.equals("CALL")) {
        if (alias.contains("RCX") && fwd == null) {
          Reference[] rs = in.getReferencesFrom();
          String callee = "indirect";
          for (Reference r : rs) if (r.getReferenceType().isCall()) {
            Function g = p.getFunctionManager().getFunctionAt(r.getToAddress());
            callee = g == null ? r.getToAddress().toString() : g.getName(true);
          }
          fwd = callee;
        }
        for (String v : new String[]{"RCX","RDX","R8","R9","R10","R11","RAX"}) alias.remove(v);
        continue;
      }
      if (mn.equals("JMP") && alias.contains("RCX") && fwd == null) {
        for (Reference r : in.getReferencesFrom()) {
          Function g = p.getFunctionManager().getFunctionAt(r.getToAddress());
          if (g != null) fwd = "tail:" + g.getName(true);
        }
      }
      // MOV r64, alias
      if (mn.equals("MOV") && in.getNumOperands() == 2) {
        Object[] d0 = in.getOpObjects(0), s1 = in.getOpObjects(1);
        if (d0.length == 1 && d0[0] instanceof Register && s1.length == 1 && s1[0] instanceof Register
            && (in.getOperandType(0) & OperandType.DYNAMIC) == 0 && (in.getOperandType(1) & OperandType.DYNAMIC) == 0) {
          Register dr = (Register) d0[0], sr = (Register) s1[0];
          if (sr.getBitLength() == 64 && alias.contains(sr.getName())) { alias.add(dr.getName()); continue; }
        }
      }
      for (Object o : in.getResultObjects()) {
        if (o instanceof Register) {
          String bn = ((Register) o).getBaseRegister().getName();
          if (alias.remove(bn) && bn.equals("RCX")) rcxWritten = true;
        }
      }
    }
    StringBuilder sb = new StringBuilder();
    if (obj) { sb.append("rcx=obj("); boolean first = true;
      for (long o : offs) { if (!first) sb.append(","); sb.append(String.format("0x%x", o)); first = false; }
      sb.append(")"); }
    else if (scalar) sb.append("rcx=scalar");
    else if (fwd != null) sb.append("rcx=fwd");
    else sb.append("rcx=unused");
    if (scalar && obj) sb.append(" +scalar");
    if (fwd != null) sb.append(" fwd->" + fwd);
    if (vtStore != null) sb.append(" stores-vtable=" + vtStore);
    Parameter[] ps = f.getParameters();
    if (ps.length > 0) {
      String tn = ps[0].getDataType().getName().replace(" ", "");
      if (tn.endsWith("*")) sb.append(" p1type=" + tn.substring(0, tn.length() - 1));
      else sb.append(" p1=" + tn);
    }
    sb.append(" nparams=" + ps.length);
    // callers
    Set<String> cs = new TreeSet<>(); int ncall = 0;
    ReferenceIterator it = rm.getReferencesTo(f.getEntryPoint());
    while (it.hasNext()) { Reference r = it.next(); if (!r.getReferenceType().isCall() && !r.getReferenceType().isJump()) continue;
      ncall++; Function g = p.getFunctionManager().getFunctionContaining(r.getFromAddress());
      if (g != null && cs.size() < 4) cs.add(g.getName(true)); }
    sb.append(" rcallers=" + ncall + (cs.isEmpty() ? "" : "{" + String.join("|", cs) + "}"));
    return sb.toString();
  }

  @Override public void run() throws Exception {
    ProgramManager pm = state.getTool().getService(ProgramManager.class);
    for (Program q : pm.getAllOpenPrograms()) if (q.getName().equals("Application-x64.exe")) p = q;
    L = p.getListing(); st = p.getSymbolTable(); rm = p.getReferenceManager(); mem = p.getMemory();
    sp = p.getAddressFactory().getDefaultAddressSpace();
    buildQmap();
    String[] a = getScriptArgs();
    BufferedReader br = new BufferedReader(new FileReader(a[0]));
    PrintWriter pw = new PrintWriter(new FileWriter(a[1]));
    String line; int nrow = 0;
    while ((line = br.readLine()) != null) {
      line = line.trim(); if (line.isEmpty()) continue;
      String key = line; Address ad = null; String how = "";
      if (line.startsWith("name:")) {
        String nm = line.substring(5);
        java.util.regex.Matcher m = java.util.regex.Pattern.compile("^(\\w+?)_Slot(\\d+)$").matcher(nm);
        if (m.find()) {
          SymbolIterator si = st.getSymbols("vtable_" + m.group(1));
          if (si.hasNext()) {
            Address va = si.next().getAddress().add(8L * Integer.parseInt(m.group(2)));
            ad = A(mem.getLong(va)); how = "via vtable_" + m.group(1) + "[" + m.group(2) + "]";
          }
        }
        if (ad == null) {
          SymbolIterator si = st.getSymbols(nm);
          while (si.hasNext()) { Symbol s = si.next(); if (s.getSymbolType() == SymbolType.FUNCTION) { ad = s.getAddress(); how = "via ghidra name"; break; } }
        }
        if (ad == null) { pw.println(key + "\t\t\tnot found by name"); continue; }
      } else {
        ad = A(Long.parseUnsignedLong(line.substring(2), 16));
      }
      MemoryBlock b = mem.getBlock(ad);
      if (b == null || !b.isExecute()) { pw.println(key + "\t\t\tnot code (" + (b == null ? "unmapped" : b.getName()) + ")"); continue; }
      Function f = p.getFunctionManager().getFunctionAt(ad);
      String note = "";
      if (f == null) { f = p.getFunctionManager().getFunctionContaining(ad); note = f == null ? " nofunc" : " inside:" + f.getEntryPoint(); }
      if (f == null) { pw.println(key + "\t\t\tno function"); continue; }
      String vt = vtables(f.getEntryPoint());
      String tu = thisUse(f);
      pw.println(key + (how.isEmpty() ? "" : "=" + f.getEntryPoint() + " " + how) + "\t" + f.getName(true) + "\t" + vt + "\t" + tu + note);
      nrow++;
    }
    pw.close(); br.close();
    println("rows " + nrow);
  }
}
'''

if __name__ == '__main__':
    main()
