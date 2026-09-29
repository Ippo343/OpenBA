#!/usr/bin/env python3
"""D_big_objects.py -- B.4 block 0, inventory D (the big objects).

Re-run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b4_session/scripts/D_big_objects.py

Reads CODE lines only (comments stripped by b3r_session/codegrep.py's strip()) of
PROD (code/ runtime/ standalone/ dx11/ engine/) and finds every raw offset read of
four objects:

  GameUpdateLoop   GameUpdateLoop::_singleton, file-local helpers returning it
                   (GW(), GameWorld(), World(), ...) and local aliases of either
  CitizenComponent the receiver of functions named CitizenComponent_* /
                   CitizenComp_* / CitizenComponent::* (its first parameter, or
                   `this`), and local aliases of it, in code/Game/Component/citizen*
  GameSettings     g_gameSettings (int32_t*: `[i]` and un-cast `+ n` scale by 4)
  GameContainer    g_gameContainer, its getters and aliases

For each read it records the offset, the width/type of the cast that reads it and
the file/module. Rows of `D_retail_layouts.tsv` (the hand-maintained retail side)
get the tree-side columns merged in. GameUpdateLoop reads one level deeper
(`*(T*)(*(X**)(gw + 0xF8) + 0x10)` and aliases of a field) are emitted as
`GameUpdateLoop.<field>->` rows: those are reads of the POINTEE classes.

Output: docs/runtime_port/endgame_b/b4_session/data/D_big_objects.tsv
        (and a per-read listing in the scratch dir if B4_D_SCRATCH is set in argv
        as --reads=PATH).
Deterministic: files are walked in sorted order; no build needed.
"""
import collections
import os
import re
import sys

ROOT = os.getcwd()
sys.path.insert(0, os.path.join(ROOT, 'docs/runtime_port/endgame_b/b3r_session'))
from codegrep import strip  # noqa: E402

PROD = ['code', 'runtime', 'standalone', 'dx11', 'engine']
EXCL = ('standalone/build', 'tests/unit/build', '.claude/worktrees', '/build/', '/build-')
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.c')
DATA = 'docs/runtime_port/endgame_b/b4_session/data'
RETAIL = os.path.join(DATA, 'D_retail_layouts.tsv')
OUT = os.path.join(DATA, 'D_big_objects.tsv')

HEADERS = {
    'GameUpdateLoop': ('code/Game/Game/GameUpdateLoop.h', 'GameUpdateLoop'),
    'CitizenComponent': ('code/Game/Component/CitizenComponent.h', 'CitizenComponent'),
    'GameSettings': (None, None),
    'GameContainer': (None, None),
}
LIMIT = {'GameUpdateLoop': 0x160, 'CitizenComponent': 0x290,
         'GameSettings': 0x34, 'GameContainer': 0xE0}

NUM = r'(0x[0-9A-Fa-f]+|\d+)'
BYTE_T = {'uint8_t', 'int8_t', 'char', 'unsigned char', 'signed char', 'uintptr_t',
          'intptr_t', 'std::uintptr_t', 'size_t', 'uint64_t', 'int64_t', 'unsigned long long',
          'long long', 'std::byte', 'byte', 'BYTE'}
INT_T = {'uintptr_t', 'intptr_t', 'std::uintptr_t', 'uint64_t', 'int64_t', 'size_t',
         'unsigned long long', 'long long'}
WIDTH = {
    'bool': ('bool', 1), 'char': ('i8', 1), 'int8_t': ('i8', 1), 'signed char': ('i8', 1),
    'uint8_t': ('u8', 1), 'unsigned char': ('u8', 1), 'BYTE': ('u8', 1),
    'int16_t': ('i16', 2), 'short': ('i16', 2), 'uint16_t': ('u16', 2), 'unsigned short': ('u16', 2),
    'int': ('i32', 4), 'int32_t': ('i32', 4), 'long': ('i32', 4), 'uint32_t': ('u32', 4),
    'unsigned': ('u32', 4), 'unsigned int': ('u32', 4), 'DWORD': ('u32', 4), 'float': ('f32', 4),
    'int64_t': ('i64', 8), 'long long': ('i64', 8), 'uint64_t': ('u64', 8),
    'unsigned long long': ('u64', 8), 'uintptr_t': ('u64', 8), 'intptr_t': ('i64', 8),
    'double': ('f64', 8), 'size_t': ('u64', 8),
}


def norm_type(t):
    t = re.sub(r'\bconst\b|\bvolatile\b', '', t)
    t = re.sub(r'\s+', ' ', t).strip()
    t = re.sub(r'\s*\*', '*', t)
    return t


def width_of(ptr_type):
    """ptr_type is the cast's type, e.g. 'int**' -> the field is 'ptr'."""
    t = norm_type(ptr_type)
    if not t.endswith('*'):
        return (t or '?', 0)
    inner = t[:-1]
    if inner.endswith('*'):
        return ('ptr', 8)
    if inner in WIDTH:
        return WIDTH[inner]
    if inner == 'void':
        return ('void?', 0)
    return ('obj:' + inner, 0)


def module_of(path):
    p = path.split('/')
    if p[0] == 'code' and len(p) > 2:
        return p[2]
    return p[0]


def files():
    for r in PROD:
        for dp, dn, fs in os.walk(r):
            dn.sort()
            for f in sorted(fs):
                q = os.path.join(dp, f).replace('\\', '/')
                if any(e in q for e in EXCL) or not q.endswith(EXTS):
                    continue
                yield q


# --------------------------------------------------------------------------- casts
# A receiver cast that only turns the pointer into bytes / an integer
RECV_CAST = re.compile(
    r'(?:reinterpret_cast|static_cast)\s*<\s*(?:const\s+)?([\w: ]+?)\s*(\**)\s*>\s*\(\s*\(?\s*$'
    r'|\(\s*(?:const\s+)?([\w: ]+?)\s*(\**)\s*\)\s*\(?\s*$')
# the reading cast: a pointer type cast, possibly followed by open parens
READ_CAST = re.compile(
    r'(?:reinterpret_cast|static_cast)\s*<\s*((?:const\s+)?[\w:<> ]+?\s*\*+)\s*>\s*\(\s*\(?\s*\(?\s*$'
    r'|\(\s*((?:const\s+)?[\w:<>]+(?:\s+[\w:<>]+)*\s*\*+)\s*\)\s*\(?\s*\(?\s*$')


BYTE_PTR = {'uint8_t', 'int8_t', 'char', 'unsigned char', 'signed char', 'std::byte', 'byte', 'BYTE'}


def left_context(line, start, nclose=1, prev=''):
    """(receiver_is_bytes, read_type) for the receiver token starting at `start`.

    nclose = the number of ')' between the token and its '+': a function-style
    receiver cast `reinterpret_cast<uint8_t*>(tok) + off` needs one; without it the
    cast applies to the SUM and is the reading cast. read_type is None when the
    expression is not dereferenced (an address computation) and 'SELF' when it is
    dereferenced without any cast (typed receiver)."""
    left = line[:start]
    recv_bytes = None
    recv_type = None
    m = RECV_CAST.search(left)
    if m:
        base = norm_type((m.group(1) or m.group(3) or '').strip())
        stars = m.group(2) if m.group(1) is not None else m.group(4)
        opens = m.group(0).count('(') - (1 if m.group(3) is not None else 0)
        applies_to_token = opens <= nclose
        if applies_to_token and ((stars == '*' and base in BYTE_PTR) or (stars == '' and base in INT_T)):
            recv_bytes = True
            recv_type = base + stars if stars == '*' else None
            left = left[:m.start()]
    left2 = left.rstrip()
    if prev and re.fullmatch(r'[\s(]*', left2 or ''):
        left2 = prev.rstrip() + ' ' + left2
    elif prev and re.search(r'[=,(]\s*\(*\s*$', left2) is None and left2.strip() == '':
        left2 = prev.rstrip()
    m2 = READ_CAST.search(left2)
    if not m2 and recv_type and re.search(r'\*\s*\(*\s*$', left2):
        # `*(reinterpret_cast<const char*>(g) + 0x17)`: the receiver cast IS the read
        return recv_bytes, recv_type
    if not m2:
        if re.search(r'\*\s*\(+\s*$', left2) and m is None:
            return recv_bytes, 'SELF'
        return recv_bytes, None
    rtype = m2.group(1) or m2.group(2)
    before = left2[:m2.start()].rstrip()
    if not re.search(r'\*\s*\(*$', before):
        rtype = 'addr:' + rtype
    return recv_bytes, rtype


# --------------------------------------------------------------------------- walker
class Walker:
    def __init__(self):
        self.reads = []      # (obj, off, width_type, width, path, line, text, via)
        self.named = collections.Counter()   # (obj, member) -> lines
        self.deep = []       # (obj, field_off, inner_off, width_type, width, path, line)

    def add(self, obj, off, rtype, path, ln, text, via, recv_is_int32=False, scale_hint=1):
        if rtype is not None and rtype.startswith('SELF?'):
            wt, w = ('untyped-deref', 0)
        elif rtype is None:
            wt, w = ('addr', 0)
        elif rtype.startswith('addr:'):
            wt, w = width_of(rtype[5:])
            wt, w = ('addr:' + wt, 0)
        else:
            wt, w = width_of(rtype)
        self.reads.append((obj, off, wt, w, path, ln, text.strip()[:160], via))


FUNC_HDR = re.compile(r'([A-Za-z_][\w:]*)\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*(?:const\s*)?(?:noexcept\s*)?\{?\s*$')
CIT_FUNC = re.compile(r'^(?:CitizenComponent_|CitizenComp_|CitizenComponent::)')
# The first parameter of these bodies is the receiver; other names (storageComp, mgr,
# arrV, ownerEntity ...) are a different class.
CITIZEN_PARAM_NAMES = {'self', 'selfV', 'param_1', 'citizen'}
# Read by hand: the body name says Citizen, the receiver is another class
# (known_port_issues 6: body names mislead).
NOT_CITIZEN_RECEIVER = {
    'CitizenComp_FUN_14012c3f0',        # StatusIconComponent SetFlag: int64_t* param_1 = the icon
    'CitizenComp_Entity_impl_ef40',     # param_1 is an entity / finder context
    'CitizenComp_helper_a4f0',          # retail 0x14012a4f0 = ResidenceComponent_IsHouseholdFull
}
# pointer arithmetic on a typed receiver scales by the pointee size
SCALE = {'int64_t*': 8, 'uint64_t*': 8, 'uintptr_t*': 8, 'void**': 8, 'int32_t*': 4, 'int*': 4,
         'uint32_t*': 4, 'float*': 4, 'int16_t*': 2, 'uint16_t*': 2}


CAST_RX = re.compile(r'(?:reinterpret_cast|static_cast|const_cast)\s*<[^<>]*(?:<[^<>]*>[^<>]*)*>'
                     r'|\(\s*(?:const\s+)?[\w:]+(?:\s+[\w:]+)*\s*\**\s*\)(?=\s*[\w(])')


def is_pure_copy(rhs, rx):
    """True when rhs is only the token under casts / parens, or `c ? token : 0`."""
    branches = [rhs] if '?' not in rhs else re.split(r'(?<!:):(?!:)', rhs.split('?', 1)[1])
    ok = False
    for b in branches:
        t = CAST_RX.sub('', b)
        t = re.sub(r'[()\s]', '', t)
        if re.fullmatch(rx.replace(r'(?<![\w.>])', '').replace(r'\s*', ''), t):
            ok = True
        elif t not in ('0', 'nullptr', 'NULL', '0u'):
            return False
    return ok


def scan_file(path, src, W):
    lines = [blank_strings(x) for x in strip(src).split('\n')]
    is_cit_file = re.search(r'code/Game/Component/(citizen_|citizencomponent)', path) is not None

    # ---- file-local helpers returning a root
    helpers = {}   # name -> obj
    roots = [('GameUpdateLoop', r'GameUpdateLoop::_singleton'),
             ('GameSettings', r'\bg_gameSettings\b'),
             ('GameContainer', r'\bg_gameContainer\b')]
    for i, line in enumerate(lines):
        for obj, rx in roots:
            if re.search(r'\breturn\b', line) and re.search(rx, line):
                m = re.search(r'return\s+([^;]*);', line)
                if not m:
                    continue
                rhs = m.group(1)
                if re.search(r'->|==|!=|\[|\+\s*' + NUM, rhs.split('?')[-1]) or re.search(r'!=\s*nullptr\s*;', line):
                    continue
                name = None
                hm = re.search(r'([A-Za-z_]\w*)\s*\(\s*\)\s*(?:const\s*)?\{[^{}]*\breturn\b', line)
                if hm:
                    name = hm.group(1)
                else:
                    for j in range(i - 1, max(-1, i - 8), -1):
                        hm = re.search(r'([A-Za-z_]\w*)\s*\(\s*(?:void)?\s*\)\s*(?:const\s*)?\{?\s*$', lines[j])
                        if hm:
                            name = hm.group(1)
                            break
                if name and name not in ('if', 'while', 'for', 'switch', 'return'):
                    helpers[name] = obj

    depth = 0
    aliases = []   # dicts: name, obj, depth, int32, field(off or None)
    cur_func = None
    cur_func_depth = None

    def tokens():
        out = [('GameUpdateLoop', r'GameUpdateLoop::_singleton', False, None, 'root', 'GameUpdateLoop*'),
               ('GameSettings', r'(?<![\w.>])g_gameSettings\b', True, None, 'root', 'int32_t*'),
               ('GameContainer', r'(?<![\w.>])g_gameContainer\b', False, None, 'root', 'void*')]
        for n, o in helpers.items():
            out.append((o, r'(?<![\w.>])' + re.escape(n) + r'\s*\(\s*\)', o == 'GameSettings', None, 'helper', ''))
        for a in aliases:
            out.append((a['obj'], r'(?<![\w.>])' + re.escape(a['name']) + r'\b(?!\s*\()', a['int32'], a['field'],
                        'alias', a.get('decl', '')))
        return out

    for i, line in enumerate(lines):
        ln = i + 1
        prev = lines[i - 1] if i else ''
        # ---- function headers (CitizenComponent receivers)
        if is_cit_file and depth == 0:
            # a definition (not a declaration) of a CitizenComponent_* / CitizenComp_* /
            # CitizenComponent:: function; the signature may span lines
            hm = re.search(r'\b((?:CitizenComponent_|CitizenComp_)\w+|CitizenComponent::\w+)\s*\(([^)]*)', line)
            if hm and not re.search(r'\)\s*;\s*$', line) and not re.match(r'\s*(if|while|for|switch|return)\b', line) \
                    and re.match(r'\s*(?:extern\s+"[^"]*"\s+|static\s+|inline\s+)*[\w:<>]+[\s*&]+[\w:]*\b', line) \
                    and hm.group(1) not in NOT_CITIZEN_RECEIVER:
                cur_func = hm.group(1)
                recv, rdecl = None, ''
                if cur_func.startswith('CitizenComponent::'):
                    recv, rdecl = 'this', 'CitizenComponent*'
                else:
                    first = hm.group(2).split(',')[0].strip()
                    pm = re.search(r'^(.*?)([A-Za-z_]\w*)\s*$', first)
                    if pm and pm.group(2) in CITIZEN_PARAM_NAMES:
                        recv, rdecl = pm.group(2), norm_type(pm.group(1))
                if recv:
                    aliases.append({'name': recv, 'obj': 'CitizenComponent', 'depth': 1,
                                    'int32': False, 'field': None, 'decl': rdecl, 'pending': True})

        toks = tokens()
        # ---- named reads of GameUpdateLoop
        for m in re.finditer(r'GameUpdateLoop::_singleton\s*->\s*(\w+)', line):
            W.named[('GameUpdateLoop', m.group(1))] += 1
        # ---- alias declarations
        am = re.search(r'(?:^|[;{(,]\s*|^\s*)(?:(?:const\s+)?([\w:]+(?:\s*\*+)?|auto\s*\*?)\s+)?\b([A-Za-z_]\w*)\s*=\s*([^;=][^;]*);', line)
        if am:
            name, rhs, decl_t = am.group(2), am.group(3), (am.group(1) or '')
            src_obj = None
            src_int32 = False
            cond_free = rhs.split('?')[-1] if '?' in rhs else rhs
            for obj, rx, i32, field, via, decl in toks:
                if field is not None:
                    continue
                if re.search(rx, cond_free) and is_pure_copy(rhs, rx):
                    src_obj, src_int32 = obj, i32
                    break
            if src_obj:
                i32 = 'int32_t*' in norm_type(decl_t) or 'int*' == norm_type(decl_t) or \
                    (src_int32 and not decl_t and False)
                if decl_t == '' and src_int32:
                    i32 = any(a['name'] == name and a['int32'] for a in aliases)
                aliases[:] = [a for a in aliases if a['name'] != name]
                aliases.append({'name': name, 'obj': src_obj, 'depth': depth, 'int32': i32, 'field': None,
                                'decl': norm_type(decl_t)})
            else:
                # field alias: NAME = *<cast>(<gul token> + OFF);
                fm = None
                for obj, rx, i32, field, via, decl in toks:
                    if obj != 'GameUpdateLoop' or field is not None:
                        continue
                    fm = re.match(r'\s*\*\s*(?:reinterpret_cast\s*<[^>]*>|\([^()]*\))\s*\(\s*(?:reinterpret_cast\s*<[^>]*>\s*\(|\([^()]*\))?\s*(?:' + rx + r')\s*\)*\s*\+\s*' + NUM + r'\s*\)\s*$', rhs)
                    if fm:
                        off = int(fm.group(fm.lastindex), 0)
                        dt = norm_type(decl_t)
                        aliases[:] = [a for a in aliases if a['name'] != name]
                        aliases.append({'name': name, 'obj': 'GameUpdateLoop', 'depth': depth,
                                        'int32': dt in ('int*', 'int32_t*'), 'field': off,
                                        'decl': dt})
                        break
                if not fm and decl_t:
                    # a re-declaration shadows an alias of the same name
                    aliases[:] = [a for a in aliases if not (a['name'] == name and a['depth'] < depth)]
        # ---- reads
        seen = set()
        for obj, rx, i32, field, via, decl in toks:
            for m in re.finditer('(' + rx + r')(\s*\)*\s*)\+\s*' + NUM + r'\b', line):
                start = m.start(1)
                if (start, obj) in seen:
                    continue
                seen.add((start, obj))
                off = int(m.group(m.lastindex), 0)
                recv_bytes, rtype = left_context(line, start, m.group(2).count(')'), prev)
                scale = 1 if recv_bytes else (4 if i32 else SCALE.get(decl, 1))
                if rtype == 'SELF':
                    rtype = 'int32_t*' if i32 else (decl if decl.endswith('*') else 'SELF?' + decl)
                off *= scale
                if field is not None:
                    W.deep.append((obj, field, off, rtype, path, ln, line.strip()[:160]))
                    continue
                W.add(obj, off, rtype, path, ln, line, via)
                # one level deeper: (... + OFF) ) + OFF2
                rest = line[m.end():]
                dm = re.match(r'\s*\)\s*\)*\s*\+\s*' + NUM + r'\b', rest)
                if obj == 'GameUpdateLoop' and dm:
                    off2 = int(dm.group(1), 0)
                    # the outer cast: search the text left of the inner deref
                    left = line[:start]
                    cm = list(re.finditer(r'(?:reinterpret_cast\s*<\s*([^<>]+?\*)\s*>|\(\s*([\w:]+\s*\*+)\s*\))\s*\(\s*\*', left))
                    otype = (cm[-1].group(1) or cm[-1].group(2)) if cm else None
                    W.deep.append((obj, off, off2, otype, path, ln, line.strip()[:160]))
            # index form: TOKEN[ n ] or (cast)TOKEN)[n]
            for m in re.finditer('(' + rx + r')\s*\)?\s*\[\s*' + NUM + r'\s*\]', line):
                start = m.start(1)
                off = int(m.group(m.lastindex), 0)
                recv_bytes, _ = left_context(line, start, 1, prev)
                amp = re.search(r'&\s*\(?\s*$', line[:start])
                if amp and field is None:
                    _, rt_amp = left_context(line, amp.start(), 1, prev)
                    if rt_amp and rt_amp != 'SELF':
                        W.add(obj, off * (4 if i32 else 1), rt_amp.replace('addr:', ''), path, ln, line, via + '&[]')
                        continue
                if field is not None:
                    elem = 4 if i32 else 8
                    W.deep.append((obj, field, off * elem, 'int*' if i32 else None, path, ln, line.strip()[:160]))
                    continue
                if recv_bytes or re.search(r'(?:uint8_t|char)\s*\*\s*>\s*\(\s*$|\(\s*(?:const\s+)?(?:uint8_t|char)\s*\*\s*\)\s*$', line[:start]):
                    W.add(obj, off, 'uint8_t*', path, ln, line, via + '[]')
                elif i32:
                    W.add(obj, off * 4, 'int32_t*', path, ln, line, via + '[]')
                elif decl and decl[:-1] in BYTE_PTR and decl.endswith('*'):
                    W.add(obj, off, decl, path, ln, line, via + '[]')
                elif decl in SCALE:
                    W.add(obj, off * SCALE[decl], decl, path, ln, line, via + '[]')
            # the At(self, off) helper of citizencomponent_serialize.cpp
            if obj == 'CitizenComponent':
                for m in re.finditer(r'\bAt\s*\(\s*(' + rx + r')\s*,\s*' + NUM + r'\s*\)', line):
                    off = int(m.group(m.lastindex), 0)
                    _, rtype = left_context(line, m.start(), 1, prev)
                    W.add(obj, off, rtype, path, ln, line, 'At()')
        # ---- brace depth / scope end
        depth += line.count('{') - line.count('}')
        if depth < 0:
            depth = 0
        if depth >= 1:
            for a in aliases:
                a.pop('pending', None)
        aliases[:] = [a for a in aliases if a['depth'] <= depth or a.get('pending')]
        if cur_func is not None and depth <= (cur_func_depth or 0) and '}' in line:
            cur_func = None


def blank_strings(line):
    """Replace the CONTENT of string / char literals with spaces (codegrep keeps them)."""
    return re.sub(r'"(?:\\.|[^"\\])*"', lambda m: '"' + ' ' * (len(m.group(0)) - 2) + '"', line)


def header_fields(obj):
    path, cls = HEADERS[obj]
    decl, asserted = {}, set()
    if not path or not os.path.exists(path):
        return decl, asserted
    src = open(path, encoding='utf-8', errors='replace').read()
    for m in re.finditer(r'^\s*([\w:<>]+(?:\s*\*+)?)\s+(\w+)(\[[^\]]*\])?\s*;\s*//\s*\+0x([0-9A-Fa-f]+)', src, re.M):
        decl[int(m.group(4), 16)] = (m.group(1).replace(' ', ''), m.group(2) + (m.group(3) or ''))
    names = {v[1].split('[')[0]: k for k, v in decl.items()}
    for m in re.finditer(r'offsetof\(\s*' + cls + r'\s*,\s*(\w+)\s*\)\s*==\s*(0x[0-9A-Fa-f]+)', src):
        asserted.add(int(m.group(2), 16))
    return decl, asserted


def load_retail():
    rows = []
    with open(RETAIL, encoding='utf-8') as f:
        hdr = f.readline().rstrip('\n').split('\t')
        for line in f:
            if not line.strip():
                continue
            rows.append(dict(zip(hdr, line.rstrip('\n').split('\t'))))
    for r in rows:
        r['_off'] = int(r['offset'], 16)
        r['_size'] = int(r['size'], 16)
    return rows


def containing(rows, obj, off):
    best = None
    for r in rows:
        if r['object'] == obj and r['_off'] <= off < r['_off'] + r['_size']:
            if best is None or r['_size'] < best['_size']:
                best = r
    return best


def main():
    argv = sys.argv[1:]
    reads_out = None
    for a in argv:
        if a.startswith('--reads='):
            reads_out = a[8:]
    W = Walker()
    for p in files():
        try:
            src = open(p, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        if not re.search(r'GameUpdateLoop::_singleton|g_gameSettings|g_gameContainer|CitizenComp', src):
            continue
        scan_file(p, src, W)

    retail = load_retail()
    agg = collections.defaultdict(lambda: {'lines': set(), 'widths': collections.Counter(),
                                           'mods': collections.Counter(), 'files': set()})
    stray = collections.defaultdict(lambda: {'lines': set(), 'widths': collections.Counter(),
                                             'mods': collections.Counter(), 'files': set()})
    for obj, off, wt, w, path, ln, text, via in W.reads:
        if off >= LIMIT[obj]:
            key = (obj, off)
            d = stray[key]
        else:
            r = containing(retail, obj, off)
            key = (obj, r['_off'] if r else off)
            d = agg[key]
        d['lines'].add((path, ln))
        d['widths'][f"{wt}@+0x{off:X}" if key[1] != off else wt] += 1
        d['mods'][module_of(path)] += 1
        d['files'].add(path)

    out = []
    cols = ['object', 'offset', 'size', 'type', 'proposed_name', 'evidence', 'readers_in_tree',
            'confidence', 'tree_lines', 'tree_widths', 'header_decl', 'header_asserted',
            'width_check', 'modules']
    for obj in ['GameUpdateLoop', 'CitizenComponent', 'GameSettings', 'GameContainer']:
        decl, asserted = header_fields(obj)
        for r in [x for x in retail if x['object'] == obj]:
            d = agg.get((obj, r['_off']))
            n = len(d['lines']) if d else 0
            widths = ', '.join(f'{k}:{v}' for k, v in sorted(d['widths'].items())) if d else ''
            mods = ', '.join(f'{k}:{v}' for k, v in d['mods'].most_common()) if d else ''
            hd = decl.get(r['_off'])
            hdecl = f"{hd[0]} {hd[1]}" if hd else ('-' if HEADERS[obj][0] else 'no header')
            check = width_check(r, d)
            out.append([obj, r['offset'], r['size'], r['type'], r['proposed_name'], r['evidence'],
                        f"{n} lines / {len(d['files']) if d else 0} files", r['confidence'], str(n),
                        widths, hdecl, 'yes' if r['_off'] in asserted else 'no', check, mods])
        for (o, off), d in sorted(stray.items()):
            if o != obj:
                continue
            out.append([obj, f'0x{off:X}', '?', 'BEYOND sizeof', '', 'read past the retail size: a different receiver', f"{len(d['lines'])} lines / {len(d['files'])} files", 'n/a', str(len(d['lines'])), ', '.join(f'{k}:{v}' for k, v in sorted(d['widths'].items())), '-', 'no', 'CHECK', ', '.join(f'{k}:{v}' for k, v in d['mods'].most_common())])

    # pointee rows (GameUpdateLoop field -> inner offset)
    dd = collections.defaultdict(lambda: {'lines': set(), 'widths': collections.Counter(), 'mods': collections.Counter()})
    gul = {x['_off']: x for x in retail if x['object'] == 'GameUpdateLoop'}
    for obj, f, off2, rt, path, ln, text in W.deep:
        key = (f, off2)
        dd[key]['lines'].add((path, ln))
        dd[key]['widths'][width_of(rt)[0] if rt else '?'] += 1
        dd[key]['mods'][module_of(path)] += 1
    for (f, off2), d in sorted(dd.items()):
        g = gul.get(f)
        fname = g['proposed_name'] if g else f'+0x{f:X}'
        out.append([f'GameUpdateLoop.{fname}->', f'0x{off2:X}', '', (g['type'] if g else ''), '', 'tree-only (pointee field)',
                    f"{len(d['lines'])} lines", '', str(len(d['lines'])),
                    ', '.join(f'{k}:{v}' for k, v in sorted(d['widths'].items())), '', '', '',
                    ', '.join(f'{k}:{v}' for k, v in d['mods'].most_common())])
    # named reads
    for (obj, member), n in sorted(W.named.items()):
        out.append([obj, 'named', '', '', member, 'already read by name (GameUpdateLoop::_singleton->member)',
                    f'{n} lines', '', str(n), '', '', '', '', ''])

    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\t'.join(cols) + '\n')
        for row in out:
            f.write('\t'.join(c.replace('\t', ' ') for c in row) + '\n')

    if reads_out:
        with open(reads_out, 'w', encoding='utf-8', newline='\n') as f:
            for obj, off, wt, w, path, ln, text, via in sorted(W.reads, key=lambda x: (x[0], x[4], x[5], x[1])):
                f.write(f'{obj}\t0x{off:X}\t{wt}\t{path}:{ln}\t{via}\t{text}\n')
            for obj, fo, off2, rt, path, ln, text in sorted(W.deep, key=lambda x: (x[4], x[5])):
                f.write(f'{obj}.+0x{fo:X}->\t0x{off2:X}\t{rt}\t{path}:{ln}\tdeep\t{text}\n')

    tot = collections.Counter()
    lines = collections.defaultdict(set)
    for obj, off, wt, w, path, ln, text, via in W.reads:
        lines[obj].add((path, ln))
    for obj, f, off2, rt, path, ln, text in W.deep:
        lines[obj + '->'].add((path, ln))
    for k in sorted(lines):
        print(f'{k}: {len(lines[k])} code lines with a raw offset read')
    print(f'wrote {OUT} ({len(out)} rows)')


SIZE_OF = {'i8': 1, 'u8': 1, 'bool': 1, 'i16': 2, 'u16': 2, 'i32': 4, 'u32': 4, 'f32': 4,
           'i64': 8, 'u64': 8, 'f64': 8, 'ptr': 8}


def width_check(r, d):
    """Compare the tree's read widths at the field's own offset with the retail type."""
    if not d:
        return ''
    t = r['type'].lower()
    if 'padding' in t:
        return 'READS PADDING'
    want = None
    if t.startswith('int32') or t.startswith('uint32'):
        want = {'i32', 'u32'}
    elif t.startswith('float'):
        want = {'f32'}
    elif t.startswith('bool'):
        want = {'bool', 'u8', 'i8'}
    elif t.startswith('int64') or 'time' in t:
        want = {'i64', 'u64'}
    elif any(k in t for k in ('ptr', '*', 'pointer', 'string')):
        want = {'ptr', 'u64', 'i64', 'void?'}
    if not want:
        return ''
    bad = []
    for k in d['widths']:
        k2 = k[5:] if k.startswith('addr:') else k
        if '@' in k2 or k2 in want or k2 in ('addr', 'untyped-deref', 'void?') or k2.startswith('obj:'):
            continue
        bad.append(k)
    return ('DIFFERS: ' + ','.join(sorted(bad))) if bad else 'ok'


if __name__ == '__main__':
    main()
