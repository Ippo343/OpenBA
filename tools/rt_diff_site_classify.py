#!/usr/bin/env python3
"""RT_DIFF_SHADOW site classifier, tier 2: does the DLL's answer reach EQUAL?

SUBJECT: every RT_DIFF_SHADOW occurrence in code (comments and string-literal
contents stripped) in *.cpp under runtime/ code/ standalone/ engine/ dx11/,
runtime/funcs/rt_diff.* excluded.

TIER 1 (sound in the negative direction)
  ATTENDANCE  EQUAL is a literal true/false -- compares nothing.
  SELF-CHECK  EQUAL is an expression but the enclosing function has NO door to
              the shipped DLL body, so the verdict cannot consult retail.
  ORACLE-CAND EQUAL is an expression and a DLL door exists in the function.

TIER 2 (applied to ORACLE-CAND only): bounded textual dataflow from the
identifiers in EQUAL back through their assignments inside the function, depth
<= 6.  If the closure reaches a DLL door the site is ORACLE-LINKED; otherwise
ORACLE-UNLINKED -- a door is present in the body but its answer does not appear
to reach the verdict.  UNLINKED is a HAND-CHECK QUEUE, not a verdict.

CONTROLS: six synthetic sites, two per tier-2 outcome, plus the three tier-1
controls.  Effectiveness counters report how many real sites each test moved.
"""
import os, re, sys, json, bisect, collections

# Derive the repo root from THIS FILE, never from the current directory: the
# runtime-port scanners that walked os.walk("runtime") relative to the CWD
# silently measured nothing when run from elsewhere, and printed a confident 0.
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOTS = [os.path.join(_REPO, d) for d in ("runtime", "code", "standalone", "engine", "dx11")]
EXCLUDE = ("rt_diff.cpp", "rt_diff.h")
DOOR_RE = re.compile(r'\bRT_ORIGINAL_ONCE\b|\bRtOriginal\s*<|\bg_orig_\w+|\bRT_ORIGINAL\b')
CONST_EQ = re.compile(r'^(true|false|1|0)$')
IDENT = re.compile(r'\b([A-Za-z_]\w*)\b')
KEYWORDS = set('''if else for while do return true false nullptr sizeof static const auto int
unsigned char short long float double bool void struct class using typedef reinterpret_cast
static_cast const_cast NULL std memcmp strcmp snprintf size_t uint32_t int32_t uint64_t'''.split())


def strip(src):
    out = []; i = 0; n = len(src); st = 0
    while i < n:
        c = src[i]; d = src[i:i + 2]
        if st == 0:
            if d == '//': st = 1; i += 2; out.append('  '); continue
            if d == '/*': st = 2; i += 2; out.append('  '); continue
            if c == '"': st = 3; out.append('"'); i += 1; continue
            if c == "'": st = 4; out.append("'"); i += 1; continue
            out.append(c); i += 1; continue
        if st == 1:
            out.append('\n' if c == '\n' else ' ')
            if c == '\n': st = 0
            i += 1; continue
        if st == 2:
            if d == '*/': st = 0; i += 2; out.append('  '); continue
            out.append('\n' if c == '\n' else ' '); i += 1; continue
        if c == '\\': out.append('  '); i += 2; continue
        q = '"' if st == 3 else "'"
        if c == q: st = 0; out.append(q); i += 1; continue
        out.append('\n' if c == '\n' else ' '); i += 1
    return ''.join(out)


def first_arg(code, mstart):
    i = code.find('(', mstart)
    if i < 0: return None
    depth = 0; j = i
    while j < len(code):
        c = code[j]
        if c in '([{': depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0: return code[i + 1:j].strip()
        elif c == ',' and depth == 1: return code[i + 1:j].strip()
        j += 1
    return None


def is_func_header(pre):
    pre = pre.rstrip()
    if not pre: return False
    if re.search(r'\b(namespace|extern\s*"C")\s*$', pre): return False
    if re.search(r'\b(class|struct|enum|union)\b[^;{}()]*$', pre): return False
    if pre.endswith('='): return False
    return bool(re.search(r'\)\s*(const)?\s*(noexcept)?\s*(->[^;{}]*)?$', pre))


def leaf_funcs(code, lo, hi, depth=0):
    out = []; inner = []; d = 0; st = None
    for m in re.finditer(r'[{}]', code[lo:hi]):
        if m.group() == '{':
            if d == 0: st = m.start()
            d += 1
        else:
            d -= 1
            if d == 0 and st is not None: inner.append((lo + st, lo + m.end())); st = None
            if d < 0: d = 0
    for (s, e) in inner:
        if is_func_header(code[max(0, s - 400):s]): out.append((s, e))
        elif depth < 5: out.extend(leaf_funcs(code, s + 1, e - 1, depth + 1))
    return out


def idents(expr):
    return set(m.group(1) for m in IDENT.finditer(expr)) - KEYWORDS


def statements(body):
    """Split a function body into ;-terminated statements, brace-aware so a
    braced initialiser stays attached to its statement."""
    # The body still carries its own outer braces; drop them or every `;`
    # sits at depth >= 1 and the whole function reads as ONE statement.
    body = body.strip()
    if body.startswith('{') and body.endswith('}'):
        body = body[1:-1]
    # A `{` that follows `)`, `{`, `}`, `;` or `else` opens a BLOCK and is a
    # statement boundary; one that follows `=` or `,` opens an INITIALISER and
    # must stay inside its statement.  Without this an `if (...) { ... }` is a
    # single statement, and any identifier mentioned anywhere in that block
    # links to any door anywhere in it -- which produced two false ORACLEs that
    # a hand-check caught.
    out = []; start = 0; d = 0
    for i, ch in enumerate(body):
        if ch in '([':
            d += 1
        elif ch in ')]':
            d = max(0, d - 1)
        elif ch == '{':
            pre = body[:i].rstrip()
            block = pre.endswith((')', '{', '}', ';', ':')) or pre.endswith('else')
            if block and d == 0:
                out.append(body[start:i]); start = i + 1
            else:
                d += 1
        elif ch == '}':
            pre_open = d == 0
            if pre_open:
                out.append(body[start:i]); start = i + 1
            else:
                d = max(0, d - 1)
        elif ch == ';' and d == 0:
            out.append(body[start:i + 1]); start = i + 1
    out.append(body[start:])
    return [s for s in out if s.strip()]


def linked(body, eq, mode):
    """Bounded dataflow from EQUAL's identifiers back to a DLL door.

    mode 'assign' -- only statements that DEFINE the identifier:
                     `nm = ...`, `T nm[] = ...`, `nm(...)`.
                     Blind to out-parameters (`orig(&theirs)`).
    mode 'occur'  -- any statement mentioning `nm` that also holds a door.
                     Catches out-parameters; OVER-approximates, because an
                     identifier merely passed to the original as an INPUT also
                     matches.  The two modes BRACKET the true answer.
    """
    stmts = statements(body)
    if DOOR_RE.search(eq):
        return 0, 0                      # the door call IS the EQUAL expression
    want = idents(eq); seen = set(); steps = 0
    for hop in range(1, 7):
        new = set()
        for nm in list(want - seen):
            seen.add(nm)
            w = re.compile(r'\b%s\b' % re.escape(nm))
            defn = re.compile(r'\b%s\b\s*(?:\[[^\]]*\])?\s*(?:=[^=]|\()' % re.escape(nm))
            for st in stmts:
                if not w.search(st): continue
                if mode == 'assign' and not defn.search(st): continue
                steps += 1
                if DOOR_RE.search(st): return hop, steps
                new |= idents(st)
        want |= new
        if not (want - seen): break
    return -1, steps


CONTROL = '''
void CtlAttendance() { RT_DIFF_BEGIN("CTL::attendance");
  using Fn=int(*)(); (void)RT_ORIGINAL_ONCE(Fn,"?x@@YAHXZ"); RT_DIFF_SHADOW(true,"a","b"); }
void CtlSelfcheck() { RT_DIFF_BEGIN("CTL::selfcheck"); int a=1,b=2; RT_DIFF_SHADOW(a==b,"a","b"); }
void CtlLinkeddirect() { RT_DIFF_BEGIN("CTL::linkeddirect"); using Fn=int(*)(); int o=1;
  RT_DIFF_SHADOW(o == RT_ORIGINAL_ONCE(Fn,"?x@@YAHXZ")(), "a","b"); }
void CtlLinkedhop() { RT_DIFF_BEGIN("CTL::linkedhop"); using Fn=int(*)();
  int t = RT_ORIGINAL_ONCE(Fn,"?x@@YAHXZ")(); bool ok = (1 == t); RT_DIFF_SHADOW(ok,"a","b"); }
void CtlLinkedtwohop() { RT_DIFF_BEGIN("CTL::linkedtwohop"); using Fn=int(*)();
  int t = RT_ORIGINAL_ONCE(Fn,"?x@@YAHXZ")(); int u = t + 1; bool ok = (1 == u);
  RT_DIFF_SHADOW(ok,"a","b"); }
void CtlOutparam() { RT_DIFF_BEGIN("CTL::outparam"); using Fn=void(*)(int*);
  int theirs=0; RT_ORIGINAL_ONCE(Fn,"?x@@YAXPEAH@Z")(&theirs); int o=1;
  RT_DIFF_SHADOW(o == theirs,"a","b"); }
void CtlUnlinked() { RT_DIFF_BEGIN("CTL::unlinked"); using Fn=int(*)();
  int t = RT_ORIGINAL_ONCE(Fn,"?x@@YAHXZ")(); (void)t; int a=1,b=1; bool ok=(a==b);
  RT_DIFF_SHADOW(ok,"a","b"); }
'''


def scan(p, src, rows, unscoped):
    code = strip(src)
    assert len(code) == len(src), p
    if 'RT_DIFF_SHADOW' not in code: return
    funcs = leaf_funcs(code, 0, len(code))
    starts = [0] + [i + 1 for i, ch in enumerate(code) if ch == '\n']
    for m in re.finditer(r'\bRT_DIFF_SHADOW\b', code):
        o = m.start(); ln = bisect.bisect_right(starts, o)
        owner = None
        for (s, e) in funcs:
            if s <= o < e and (owner is None or (e - s) < (owner[1] - owner[0])): owner = (s, e)
        if owner is None: unscoped.append((p, ln)); continue
        body = code[owner[0]:owner[1]]
        eq = first_arg(code, o) or ''
        nm = re.search(r'RT_DIFF_BEGIN\s*\(\s*"([^"]*)"', src[owner[0]:owner[1]])
        door = bool(DOOR_RE.search(body))
        steps = 0
        hop = None
        if CONST_EQ.match(eq): kind = 'ATTENDANCE'
        elif not door: kind = 'SELF-CHECK'
        else:
            ha, sa = linked(body, eq, 'assign')
            ho, so = linked(body, eq, 'occur')
            steps = sa + so
            hop = ha if ha >= 0 else ho
            kind = ('ORACLE-LINKED' if ha >= 0 else
                    'ORACLE-LINKED-OUTPARAM' if ho >= 0 else 'ORACLE-UNLINKED')
        rows.append(dict(file=p, line=ln, name=nm.group(1) if nm else '?', eq=eq,
                         kind=kind, steps=steps, hop=hop))


def main():
    rows, unscoped = [], []
    for root in ROOTS:
        for dp, _, fns in os.walk(root):
            for fn in fns:
                if fn.endswith('.cpp') and fn not in EXCLUDE:
                    p = os.path.relpath(os.path.join(dp, fn), _REPO).replace('\\', '/')
                    full = os.path.join(dp, fn)
                    scan(p, open(full, encoding='utf-8', errors='replace').read(), rows, unscoped)
    ctl, cun = [], []
    scan('<CONTROL>', CONTROL, ctl, cun)

    print("SUBJECT: *.cpp under %s (repo root %s); rt_diff.* excluded;"
          % (' '.join(os.path.basename(r) for r in ROOTS), _REPO))
    print("         comments and string-literal")
    print("         contents stripped.  %d RT_DIFF_SHADOW occurrences IN CODE, %d unscoped."
          % (len(rows) + len(unscoped), len(unscoped)))
    for u in unscoped: print("  UNSCOPED %s:%d" % u)
    c = collections.Counter(r['kind'] for r in rows)
    print()
    for k in ('ORACLE-LINKED', 'ORACLE-LINKED-OUTPARAM', 'ORACLE-UNLINKED', 'SELF-CHECK', 'ATTENDANCE'):
        print("  %-16s %4d" % (k, c[k]))
    print("  %-16s %4d" % ('TOTAL', len(rows)))

    print("\nCONTROLS (6 synthetic sites; each must land in its named population):")
    exp = {'CTL::attendance': 'ATTENDANCE', 'CTL::selfcheck': 'SELF-CHECK',
           'CTL::linkeddirect': 'ORACLE-LINKED', 'CTL::linkedhop': 'ORACLE-LINKED',
           'CTL::linkedtwohop': 'ORACLE-LINKED', 'CTL::unlinked': 'ORACLE-UNLINKED',
           'CTL::outparam': 'ORACLE-LINKED-OUTPARAM'}
    ok = 0
    for r in ctl:
        good = exp.get(r['name']) == r['kind']
        ok += good
        print("  %-20s -> %-16s %s" % (r['name'], r['kind'], 'PASS' if good else 'FAIL (want %s)' % exp.get(r['name'])))
    print("  %d/%d controls pass; %d control sites unscoped" % (ok, len(exp), len(cun)))

    print("\nEFFECTIVENESS -- how many REAL sites each test moved:")
    print("  const-EQUAL test fired                 : %d" % c['ATTENDANCE'])
    print("  no-door test fired                     : %d" % c['SELF-CHECK'])
    print("  dataflow ran on                        : %d sites, %d reached a door, %d did not"
          % (c['ORACLE-LINKED'] + c['ORACLE-LINKED-OUTPARAM'] + c['ORACLE-UNLINKED'],
             c['ORACLE-LINKED'] + c['ORACLE-LINKED-OUTPARAM'], c['ORACLE-UNLINKED']))
    tot = sum(r['steps'] for r in rows)
    print("  dataflow examined                      : %d assignment statements in total" % tot)
    print("  (a dataflow that examined 0 statements would be a dead parser)")
    hops = collections.Counter(r['hop'] for r in rows if r['hop'] is not None)
    print("")
    print("LINK STRENGTH -- hops from EQUAL back to the DLL door (0 = the door call IS")
    print("the EQUAL expression; higher = weaker evidence; a 6-hop closure inside a")
    print("function that holds a door anywhere is close to vacuous):")
    for h in sorted(hops): print("  hop %-3s %4d" % (h, hops[h]))

    json.dump(rows, open(sys.argv[1], 'w'), indent=1)
    print("\n--- ORACLE-UNLINKED (hand-check queue: a door in the body, but the")
    print("    dataflow does not see its answer reaching EQUAL) ---")
    for r in rows:
        if r['kind'] == 'ORACLE-UNLINKED':
            print("  %s:%d  %s  [eq: %s]" % (r['file'], r['line'], r['name'], r['eq'][:70]))


main()
