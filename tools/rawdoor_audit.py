#!/usr/bin/env python3
"""rawdoor_audit.py -- find raw-door resolves whose RESULT IS CALLED on a path a
normal (non-diffing) run executes.

WHY THIS EXISTS
---------------
`ResolveExportRaw` / `ResolveRuntimeRaw` / `ResolveOrdinalRaw` bypass the port
registry on purpose (standalone/resolve.h): the diff oracle, the detour
trampolines and the registration-time VerifyBody checks must reach the SHIPPED
code even when a port is registered.  At cutover -- `Runtime-x64.dll` absent --
every one of them returns NULL, silently:

    static void* ResolveExportRawImpl(const char* name) {
        if (!g_modules[1].handle || !name) return nullptr;   // <-- the cutover value
        ...
    }

So the *resolve* is always safe.  The **call** is what kills the process.  A raw
resolve is harmless at cutover if and only if the pointer is never called on a
normal run -- i.e. the call sits inside `if (rt_diff_on__)`, inside an
`RT_DIFF_*` macro argument, or in a function that returns early on
`if (!RtDiffEnabled())`.

Wave 9 lane V triaged the 104 measured `[RtRawResidue]` rows by hand and found
two sites where an ORACLE call had escaped its guard and was running on every
normal frame -- a cutover crash AND a live cross-DLL call
(reference_rtoriginal_is_getprocaddress.md).  This checker is that hand pass,
mechanised, so the third one does not need a person to notice it.

WHAT IT REPORTS
---------------
UNGUARDED-CALL  a raw resolve immediately called -- `RT_ORIGINAL_ONCE(F,"x")(a)`
                or `RtOriginal<F>("x")(a)` -- outside every diff guard.
                THIS IS THE FINDING.  Each one is a cutover crash unless the
                function it is in is itself dead.

DEFERRED-CALL   the resolve stores a pointer that is CALLED (or returned, or
                handed on) later in the same brace-matched function, outside
                every guard.  Added Wave 9 lane H, because "called" used to mean
                "an argument list follows the resolve ON THIS LINE" -- one
                spelling, and not the common one.  The common one is:

                    ReleaseFn release = RtOriginal<ReleaseFn>("?Release@…");
                    release(&tmp);            // the call, a statement later

                `value_types.cpp` held three live-path `String::Release`
                bypasses of which the checkers reported ONE; the other two were
                this shape and were found by hand-grepping the mangled name.

Bare resolves whose pointer is never used on a live path are still NOT reported:
that pattern is how nearly all of runtime/ reaches its unported neighbours, it is
legitimate, and reporting ~450 of them is how a real finding gets buried
(WAVE9_LANE_V.md sec5).  For the same reason DEFERRED-CALL is COUNTED in every
summary line but does not by itself fail the run -- 74 of the 74 in the tree are
data accessors and probes reaching UNPORTED neighbours, where a null at cutover
is a cutover row, not a port bypass.  The subset that IS a defect is "the name is
PORTED", and `port_bypass_audit.py` consumes `FileScan.sites` and gates on exactly
that.  `--strict-deferred` fails on any of them if a lane wants the harder gate.

FAILING CLOSED
--------------
Exit 2 when the checker CANNOT check -- an unreadable file, or a brace balance
that does not return to zero at end of file (which means the guard tracking
below is meaningless for that file).  The census answered when it could not
answer and that is how this project got here; this one refuses instead.

Exit 1 when an UNGUARDED-CALL is found that is not in the allowlist below.
Exit 0 when clean.

    tools/rawdoor_audit.py                  # audit the tree
    tools/rawdoor_audit.py --self-test      # fixtures + the two repaired sites
    tools/rawdoor_audit.py --all            # list every raw site, guarded or not
    tools/rawdoor_audit.py --deferred       # list the DEFERRED-CALL rows too
    tools/rawdoor_audit.py --strict-deferred  # ...and fail on them

--self-test carries an EFFECTIVENESS CONTROL and a FALSE-POSITIVE control rather
than only asserting the tree is clean.  It used to assert that the checker still
found the two defects lane V discovered; both have since been repaired, so that
assertion FAILED -- correctly.  Inverting it to "these two are clean" would have
left a test that passes just as happily when the matcher is broken, which is the
shape of every incident this wave has paid for.  So `tools/fixtures/` holds a
positive fixture (three shapes that MUST be reported, including the two the old
classifier could not see) and a negative one (the correct shapes, which must not
be), and the tree assertion is the third leg, not the whole test.
"""

import os
import re
import sys

ROOTS = ['runtime', 'code', 'engine', 'standalone', 'dx11']
EXTS = ('.cpp', '.h', '.hpp', '.c')

# A resolve-AND-call in one expression: the closing paren of the resolve is
# immediately followed by an argument list.
RESOLVE_CALL = re.compile(
    r'(?:RT_ORIGINAL_ONCE\s*\([^;]*?\)|RtOriginal\s*<[^;]*?>\s*\([^;]*?\))\s*\(')

# Any raw door at all (for --all).
RAW_ANY = re.compile(r'ResolveExportRaw|ResolveRuntimeRaw|ResolveOrdinalRaw|'
                     r'RT_ORIGINAL_ONCE|RtOriginal\s*<')

GUARD_OPEN = re.compile(r'\bif\s*\(\s*(?:!\s*)?rt_diff_on__')
DIFF_MACRO = re.compile(r'\bRT_DIFF_(?:SCALAR|FLOAT|BYTES|SHADOW)\s*\(')
EARLY_OUT = re.compile(r'\bif\s*\(\s*!\s*RtDiffEnabled\s*\(\s*\)\s*\)\s*return')

# Sites hand-verified by Wave 9 lane V as guarded by something this lexical
# scanner cannot see (a flag, a helper called only under a guard, a macro
# DEFINITION rather than a call).  Every entry carries the evidence; an entry
# without one is not allowed, because an allowlist is how a checker is talked
# out of a true finding.
#
# The two DEFECTS lane V found are deliberately NOT here: see KNOWN below.
ALLOWLIST = {
    'runtime/funcs/rt_port.h:99':
        'the RT_ORIGINAL_ONCE macro DEFINITION, not a call site',
    'runtime/src/io/serializer.cpp:223':
        'RT_SER_SCALAR macro definition; its expansion is the !g_portOn fallback, '
        'which IS a real dependency and is tracked as its own residue row',
    'runtime/src/io/serializer.cpp:239':
        'RT_SER_ARRAY macro definition; same as above',
    'runtime/src/system/value_types.cpp:93':
        'RtDiffRandomShadow(); all four callers (value_types.cpp:138,152,165,180) '
        'wrap it in if (rt_diff_on__)',
    'runtime/src/picking/scene.cpp:718':
        'DiffScratchScene(); its only caller (picking/scene.cpp:794) is inside '
        'if (rt_diff_on__)',
    'runtime/src/ui/dialog.cpp:600':
        'guarded by if (diffThisCall), a local set from rt_diff_on__',
    'runtime/src/ui/element.cpp:875':  'guarded by if (replay.copy); ReplayGuard '
        '(element.cpp:329) leaves copy null unless rt_diff_on__',
    'runtime/src/ui/element.cpp:887':  'ReplayGuard, as element.cpp:875',
    'runtime/src/ui/element.cpp:910':  'ReplayGuard, as element.cpp:875',
    'runtime/src/ui/element.cpp:983':  'ReplayGuard, as element.cpp:875',
    'runtime/src/ui/element.cpp:1011': 'ReplayGuard, as element.cpp:875',
    'runtime/src/ui/element.cpp:1069': 'ReplayGuard, as element.cpp:875',
    'runtime/src/ui/element.cpp:1128': 'ReplayGuard, as element.cpp:875',
}

# The two sites lane V found by hand.  Both have since been REPAIRED (the call
# moved inside `if (rt_diff_on__)`), so --self-test now asserts they are CLEAN.
#
# That inversion is why the fixtures below exist.  An assertion that the tree is
# clean passes just as happily when the matcher is broken -- it is the shape of
# a checker that answers when it cannot answer, which is the failure this whole
# wave has been paying for.  So the self-test carries an EFFECTIVENESS CONTROL
# (tools/fixtures/rawdoor_positive.cpp: rows that MUST still be found) and a
# FALSE-POSITIVE control (rawdoor_negative.cpp: correct shapes that must not be).
REPAIRED = [
    ('runtime/src/audio/audio.cpp', '?IsPaused@Scene@Audio@@QEBA_NH@Z'),
    ('runtime/src/ui/widgets_layout.cpp', '?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ'),
]

FIXTURE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures')
# (verdict, fixture basename, enclosing function) the positive fixture must yield.
FIXTURE_POSITIVE = [
    ('UNGUARDED-CALL', '?Release@String@System@@AEAAXXZ'),    # fixture_immediate_call
    ('DEFERRED-CALL', '?Release@String@System@@AEAAXXZ'),     # fixture_accessor_shape
    ('DEFERRED-CALL', '?ToInteger@String@System@@SAHPEBD@Z'), # fixture_static_accessor
]


def strip_comments_and_strings(line):
    """Blank out // comments so a commented-out call is not a finding, but KEEP
    string literals: the mangled name inside one is what identifies the site."""
    i = line.find('//')
    return line[:i] if i >= 0 else line


def blank_literals(code):
    """Replace the CONTENTS of string and char literals with spaces, so a brace
    or a quote inside one cannot move the depth counter.  Applied only to the
    copy used for brace counting -- the name extraction needs the real text.
    A brace inside a literal is exactly how the first version of this checker
    declared 11 files unscannable."""
    out = []
    i, n = 0, len(code)
    while i < n:
        c = code[i]
        if c in ('"', "'"):
            quote = c
            out.append(c)
            i += 1
            while i < n:
                if code[i] == '\\':
                    out.append('  ')
                    i += 2
                    continue
                if code[i] == quote:
                    out.append(quote)
                    i += 1
                    break
                out.append(' ')
                i += 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)


# ---------------------------------------------------------------------------
# Following a resolved pointer ACROSS statements
# ---------------------------------------------------------------------------
# The line-at-a-time `called` bit above answers "is the resolve IMMEDIATELY
# applied to an argument list?".  That is one spelling of a call and it is not
# the common one.  The common one is the accessor:
#
#     ReleaseFn release = RtOriginal<ReleaseFn>("?Release@String@System@@AEAAXXZ");
#     release(&tmp);                    // <-- the call, a statement later
#
# Wave 9 measured this hole rather than reasoning about it: `value_types.cpp`
# held THREE live-path `String::Release` bypasses of which the checkers reported
# ONE -- the immediate-call spelling.  The other two were this shape, and were
# found by hand-grepping the mangled name.  Two more (`widgets_input.cpp`'s
# `StringToInteger()` / `RealNumberEditElementSetValue()` accessors) were the
# same shape and were reported by no checker at all.
#
# So: when a raw resolve is ASSIGNED to something, follow that name inside the
# brace-matched enclosing function and record every later use of it.  Pairing is
# by assignment target and enclosing function, never by a line window -- a line
# window is what let a twelve-line comment delete a site from
# `resolve_fallback_audit.py` (166 -> 162, reading as a clean pass).

# Any `name(args) {` that is not a control statement.  NOT anchored to the start
# of a line: `extern "C" void RtAudio_Scene_SerializeTraceProbe(void*) {` is a
# function definition too, and a start-anchored pattern missed it -- which made
# two save-trace probes classify as BYPASS in port_bypass_audit's first run,
# exactly the "your first number will be too big" failure.  A lambda cannot
# match: `](...)` has no name character before the paren.
FN_DEF = re.compile(r"\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*"
                    r"(?:const\s*)?(?:noexcept\s*)?\{")
NOT_A_FN = {"if", "for", "while", "switch", "catch", "return", "sizeof", "do"}


def enclosing_function(text, pos):
    """(name, body, body_start) of the brace-matched function containing `pos`.

    Brace matching rather than a line window; the INNERMOST containing
    definition wins.  `port_bypass_audit.py` imports this rather than keeping a
    second copy -- two checkers that disagree about where a function begins is
    how this project got its 166 -> 162 incident.
    """
    best = None
    for m in FN_DEF.finditer(text, 0, pos):
        if m.group(1) in NOT_A_FN:
            continue
        open_brace = text.rindex("{", m.start(), m.end())
        depth, i, n = 0, open_brace, len(text)
        while i < n:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        if open_brace < pos < i:
            if best is None or open_brace > best[0]:
                best = (open_brace, m.group(1), text[open_brace:i])
    return (best[1], best[2], best[0]) if best else ("", "", -1)


# The last real `=` before the raw token in the same statement.  `==`, `!=`,
# `<=`, `>=` and the compound assignments are excluded so a comparison against
# a cached pointer is not read as a store into one.
ASSIGN = re.compile(r"(?<![=!<>+\-*/%&|^])=(?!=)")
IDENT_BEFORE = re.compile(r"([A-Za-z_]\w*)\s*$")
STMT_BREAK = ";{}"


def _assign_target(ctext, raw_pos):
    """The identifier a raw resolve at `raw_pos` is stored into, or ''."""
    start = max(ctext.rfind(c, 0, raw_pos) for c in STMT_BREAK) + 1
    head = ctext[start:raw_pos]
    eqs = list(ASSIGN.finditer(head))
    if not eqs:
        return ""
    m = IDENT_BEFORE.search(head[:eqs[-1].start()])
    return m.group(1) if m else ""


def _uses(body, target, skip_lo, skip_hi):
    """[(offset, kind)] of every use of `target` in `body` outside its own
    statement.  `call` = applied to an argument list; `escape` = returned or
    handed to something else, so the call happens somewhere this scan cannot
    see -- which is exactly the accessor shape, and a row a human must read."""
    t = re.escape(target)
    pats = ((re.compile(r"\b%s\s*\(" % t), "call"),
            (re.compile(r"\breturn\s+%s\s*[;)]" % t), "escape-return"),
            (re.compile(r"[(,]\s*%s\s*[,)]" % t), "escape-arg"))
    out = []
    for pat, kind in pats:
        for m in pat.finditer(body):
            if skip_lo <= m.start() < skip_hi:
                continue
            out.append((m.start(), kind))
    return sorted(out)


class FileScan:
    def __init__(self, path):
        self.path = path
        self.findings = []      # (line, names, text) -- immediate unguarded call
        self.deferred = []      # dicts: resolve here, call/escape over there
        self.all_sites = []     # legacy 5-tuples
        self.sites = []         # the same sites as dicts, with the deferred rows
        self.line_guarded = {}  # line -> was any diff guard open on it
        self.error = None


def scan(path):
    fs = FileScan(path)
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            lines = f.read().split('\n')
    except OSError as e:
        fs.error = 'unreadable: %s' % e
        return fs

    depth = 0
    guard_depths = []      # brace depths at which a diff guard was opened
    diff_only_fn_depth = None   # depth of a function that early-outs on !RtDiffEnabled()
    pending_macro_parens = 0    # open parens of an RT_DIFF_* argument list
    in_block_comment = False
    # The comment-stripped text, one entry per SOURCE line so offsets in the
    # joined text map back to real line numbers.  A line inside a block comment
    # stays empty rather than being dropped: dropping it would shift every line
    # number after it, and this project has already had a checker's own prose
    # move a finding (166 -> 162).
    code_lines = [''] * len(lines)

    for n, raw in enumerate(lines, 1):
        line = raw
        # crude /* */ handling: enough to not mis-read a commented block as code
        if in_block_comment:
            if '*/' in line:
                line = line.split('*/', 1)[1]
                in_block_comment = False
            else:
                continue
        # `//` FIRST, then `/*`: a line-comment containing a `/*` (this tree has
        # several, in prose about comment syntax) otherwise opens a block
        # comment that swallows the rest of the file and destroys the brace
        # count.  That is what made the first run declare 11 files unscannable.
        line = strip_comments_and_strings(line)
        if '/*' in line:
            before, _, after = line.partition('/*')
            if '*/' in after:
                line = before + after.split('*/', 1)[1]
            else:
                line = before
                in_block_comment = True

        code = line

        if EARLY_OUT.search(code) and diff_only_fn_depth is None:
            diff_only_fn_depth = depth

        opened_guard = bool(GUARD_OPEN.search(code))

        # An RT_DIFF_* macro's arguments routinely span several lines, and the
        # raw resolve is usually on a CONTINUATION line -- a per-line test for
        # the macro name reports those as unguarded (3 such false positives on
        # the first run).  Track the macro's argument list across lines by
        # paren balance instead.
        if DIFF_MACRO.search(code):
            pending_macro_parens += code.count('(') - code.count(')')
            in_macro = pending_macro_parens > 0
        elif pending_macro_parens > 0:
            in_macro = True
            pending_macro_parens += code.count('(') - code.count(')')
            if pending_macro_parens <= 0:
                pending_macro_parens = 0
        else:
            in_macro = False

        code_lines[n - 1] = code
        guarded_here = (bool(guard_depths) or
                        diff_only_fn_depth is not None or
                        in_macro or
                        bool(DIFF_MACRO.search(code)))
        # `opened_guard` is added ONLY to the per-line map the deferred pass
        # reads, never to the raw-site verdict above: a braceless
        # `if (rt_diff_on__) release(&t);` guards its own line, and the deferred
        # pass has to be able to see that.  The immediate-call verdict keeps its
        # pre-existing formula so this change cannot move that count.
        fs.line_guarded[n] = guarded_here or opened_guard

        if RAW_ANY.search(code):
            guarded = guarded_here
            called = bool(RESOLVE_CALL.search(code))
            names = re.findall(r'"([^"]{4,})"', code)
            fs.all_sites.append((n, guarded, called, names, code.strip()[:150]))
            if called and not guarded:
                fs.findings.append((n, names, code.strip()[:150]))

        for ch in blank_literals(code):
            if ch == '{':
                depth += 1
                if opened_guard:
                    guard_depths.append(depth)
                    opened_guard = False
            elif ch == '}':
                while guard_depths and guard_depths[-1] > depth - 1:
                    guard_depths.pop()
                depth -= 1
                if diff_only_fn_depth is not None and depth <= diff_only_fn_depth:
                    diff_only_fn_depth = None
        # a one-line `if (rt_diff_on__) foo();` guards only its own line
        if opened_guard:
            pass

    if depth != 0:
        fs.error = ('brace balance ended at %d, not 0 -- guard tracking for this '
                    'file is meaningless' % depth)
        return fs

    _deferred_pass(fs, '\n'.join(code_lines))
    return fs


def _line_of(ctext, pos):
    return ctext.count('\n', 0, pos) + 1


def _deferred_pass(fs, ctext):
    """Second pass: follow each raw resolve's assignment target.

    Runs on the comment-stripped text with line numbering preserved, so a
    twelve-line comment between the resolve and the call cannot change the
    answer.  Populates fs.deferred and fs.sites; fs.findings (the immediate
    unguarded call) is untouched, so this pass cannot move that count either.
    """
    by_line = {n: (g, c, nm, t) for (n, g, c, nm, t) in fs.all_sites}
    starts = {}
    for m in RAW_ANY.finditer(ctext):
        line = _line_of(ctext, m.start())
        if line in by_line and line not in starts:
            starts[line] = m.start()

    raw_starts = sorted(m.start() for m in RAW_ANY.finditer(ctext))

    for (n, guarded, called, names, text) in fs.all_sites:
        row = {'line': n, 'guarded': guarded, 'called': called,
               'names': list(names), 'text': text, 'target': '', 'uses': []}
        pos = starts.get(n)
        if pos is not None:
            # NAMES ARE STATEMENT-SCOPED, NOT LINE-SCOPED.  The normal
            # formatting in this tree puts the mangled name on the line AFTER
            # the door:
            #     RtOriginal<Box*(*)(const Box*, Box*, const Vector3*)>(
            #         "?Extend@Box@Volume@@QEBA?AV12@AEBVVector3@Math@@@Z")(...)
            # A per-line `names` list is empty for such a site, and
            # port_bypass_audit's macro sweep then `continue`s past it without
            # ever assigning a verdict -- 79 sites silently absent from a
            # checker that printed a clean pass.  The --by-name grep found them.
            #
            # The span stops at the NEXT raw door in the same statement, so
            # `VerifyDtorThunk(&s, "??1A", ResolveExportRaw("??_7B"),
            #  ResolveExportRaw("??1C"))` does not attribute B's and C's names
            # to each other.
            stmt_end = ctext.find(';', pos)
            stmt_end = len(ctext) if stmt_end < 0 else stmt_end
            for s in raw_starts:
                if s > pos:
                    stmt_end = min(stmt_end, s)
                    break
            for nm in re.findall(r'"([^"]{4,})"', ctext[pos:stmt_end]):
                if nm not in row['names']:
                    row['names'].append(nm)
        if pos is not None and not called:
            target = _assign_target(ctext, pos)
            if target:
                fn, body, body_start = enclosing_function(ctext, pos)
                if body_start >= 0:
                    # Skip the resolve's OWN statement so `fn = RtOriginal<Fn>(x)`
                    # is not read as a use of `fn`.
                    lo = max(ctext.rfind(ch, 0, pos) for ch in STMT_BREAK) + 1
                    hi = ctext.find(';', pos)
                    hi = len(ctext) if hi < 0 else hi + 1
                    row['target'] = target
                    row['fn'] = fn
                    for off, kind in _uses(body, target,
                                           lo - body_start, hi - body_start):
                        uline = _line_of(ctext, body_start + off)
                        row['uses'].append(
                            {'line': uline, 'kind': kind,
                             'guarded': fs.line_guarded.get(uline, False)})
        live = [u for u in row['uses'] if not u['guarded']]
        row['deferred_live'] = live
        fs.sites.append(row)
        if live and not guarded:
            fs.deferred.append(row)


def walk(root_dir):
    for r in ROOTS:
        base = os.path.join(root_dir, r)
        if not os.path.isdir(base):
            continue
        for dp, _dn, fn in os.walk(base):
            for f in sorted(fn):
                if f.endswith(EXTS):
                    yield os.path.join(dp, f)


def audit(paths, root_dir):
    """(errors, findings, deferred, total_sites) over `paths`."""
    errors, findings, deferred, total = [], [], [], 0
    for path in paths:
        fs = scan(path)
        rel = os.path.relpath(path, root_dir).replace(os.sep, '/')
        if fs.error:
            errors.append((rel, fs.error))
            continue
        total += len(fs.all_sites)
        for n, names, text in fs.findings:
            if '%s:%d' % (rel, n) in ALLOWLIST:
                continue
            findings.append((rel, n, names, text))
        for row in fs.deferred:
            if '%s:%d' % (rel, row['line']) in ALLOWLIST:
                continue
            deferred.append((rel, row))
    return errors, findings, deferred, total


def run_self_test(root_dir):
    """Three assertions, none of which can pass on a broken matcher alone.

    1. EFFECTIVENESS CONTROL -- the positive fixture still yields all three
       shapes, including the two the pre-hardening classifier could not see.
    2. FALSE-POSITIVE CONTROL -- the negative fixture yields nothing.
    3. REGRESSION -- the two lane V defects are still repaired in the tree.
    """
    ok = True

    pos = os.path.join(FIXTURE_DIR, 'rawdoor_positive.cpp')
    neg = os.path.join(FIXTURE_DIR, 'rawdoor_negative.cpp')
    for f in (pos, neg):
        if not os.path.isfile(f):
            print('SELF-TEST CANNOT RUN -- missing fixture %s' % f, file=sys.stderr)
            return 2

    errors, findings, deferred, total = audit([pos], FIXTURE_DIR)
    if errors:
        print('SELF-TEST CANNOT RUN -- positive fixture unscannable: %s'
              % errors, file=sys.stderr)
        return 2
    got = {('UNGUARDED-CALL', nm) for _r, _n, names, _t in findings for nm in names}
    got |= {('DEFERRED-CALL', nm) for _r, row in deferred for nm in row['names']}
    print('positive fixture: %d sites, %d UNGUARDED-CALL, %d DEFERRED-CALL'
          % (total, len(findings), len(deferred)))
    for want in FIXTURE_POSITIVE:
        if want not in got:
            print('SELF-TEST FAILED -- the checker no longer finds %s %s in '
                  'the positive fixture. The matcher is broken; a clean tree '
                  'proves nothing until this passes.' % want, file=sys.stderr)
            ok = False

    errors, findings, deferred, total = audit([neg], FIXTURE_DIR)
    if errors:
        print('SELF-TEST CANNOT RUN -- negative fixture unscannable: %s'
              % errors, file=sys.stderr)
        return 2
    print('negative fixture: %d sites, %d UNGUARDED-CALL, %d DEFERRED-CALL'
          % (total, len(findings), len(deferred)))
    for rel, n, names, _t in findings:
        print('SELF-TEST FAILED -- false positive UNGUARDED-CALL %s:%d %s'
              % (rel, n, names), file=sys.stderr)
        ok = False
    for rel, row in deferred:
        print('SELF-TEST FAILED -- false positive DEFERRED-CALL %s:%d %s'
              % (rel, row['line'], row['names']), file=sys.stderr)
        ok = False

    errors, findings, deferred, total = audit(list(walk(root_dir)), root_dir)
    if errors:
        print('SELF-TEST CANNOT RUN -- %d unscannable file(s)' % len(errors),
              file=sys.stderr)
        return 2
    seen = {(rel, nm) for rel, _n, names, _t in findings for nm in names}
    seen |= {(rel, nm) for rel, row in deferred for nm in row['names']}
    for key in REPAIRED:
        if key in seen:
            print('SELF-TEST FAILED -- a repaired lane V site is unguarded '
                  'again: %s  %s' % key, file=sys.stderr)
            ok = False
    print('tree: %d sites, %d UNGUARDED-CALL, %d DEFERRED-CALL'
          % (total, len(findings), len(deferred)))

    if not ok:
        return 2
    print('SELF-TEST OK -- positive fixture 3/3 found, negative fixture clean, '
          'both repaired lane V sites still clean.')
    return 0


def main(argv):
    show_all = '--all' in argv
    self_test = '--self-test' in argv
    show_deferred = '--deferred' in argv
    strict = '--strict-deferred' in argv
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    if self_test:
        return run_self_test(root_dir)

    paths = list(walk(root_dir))
    if show_all:
        for path in paths:
            fs = scan(path)
            rel = os.path.relpath(path, root_dir).replace(os.sep, '/')
            if fs.error:
                continue
            for n, guarded, called, names, text in fs.all_sites:
                print('%-6s %-8s %s:%d  %s' %
                      ('CALL' if called else 'resolve',
                       'guarded' if guarded else 'LIVE', rel, n, text))

    errors, findings, deferred, total_sites = audit(paths, root_dir)

    if errors:
        print('CANNOT CHECK -- %d file(s):' % len(errors), file=sys.stderr)
        for rel, err in errors:
            print('  %s: %s' % (rel, err), file=sys.stderr)
        print('Refusing to report a verdict on a partial scan.', file=sys.stderr)
        return 2

    # THE DENOMINATOR IS PART OF THE VERDICT.  Three separate incidents this
    # wave presented as a clean pass with a silently smaller population, so the
    # site count and BOTH finding counts are printed unconditionally -- there is
    # no invocation of this tool that reports zero without also reporting what
    # zero was out of.
    print('rawdoor_audit: %d raw-door sites scanned, %d UNGUARDED-CALL, '
          '%d DEFERRED-CALL' % (total_sites, len(findings), len(deferred)))
    for rel, n, names, text in findings:
        print('UNGUARDED-CALL %s:%d' % (rel, n))
        for nm in names:
            print('    symbol: %s' % nm)
        print('    %s' % text)

    # DEFERRED-CALL: the resolve stores a pointer that is called (or escapes)
    # later in the same function, outside every guard.  Lane V deliberately
    # refused to gate on these because ~450 of them are how all of runtime/
    # reaches its UNPORTED neighbours, and burying a real finding is the failure
    # this checker exists to avoid.  That reasoning holds for the cutover-null
    # question and does NOT hold for the port-bypass question, where the name
    # being PORTED is the discriminator -- so port_bypass_audit.py consumes
    # fs.sites and gates on exactly that subset.  Here they are listed on
    # request and counted always.
    if show_deferred or strict:
        for rel, row in deferred:
            u = row['deferred_live'][0]
            print('DEFERRED-CALL %s:%d -> %s at :%d  (via `%s` in %s)'
                  % (rel, row['line'], u['kind'], u['line'], row['target'],
                     row.get('fn') or '?'))
            for nm in row['names']:
                print('    symbol: %s' % nm)

    if findings:
        return 1
    return 1 if (strict and deferred) else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
