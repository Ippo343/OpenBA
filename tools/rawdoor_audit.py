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

It deliberately does NOT report a bare resolve (`static F f = RtOriginal<F>(m);`)
whose call site is elsewhere: that pattern is how nearly all of runtime/ reaches
its unported neighbours, it is legitimate, and reporting ~450 of them is how a
real finding gets buried (WAVE9_LANE_V.md sec5).  Those rows are triaged by the
residue matrix, not by a grep.

FAILING CLOSED
--------------
Exit 2 when the checker CANNOT check -- an unreadable file, or a brace balance
that does not return to zero at end of file (which means the guard tracking
below is meaningless for that file).  The census answered when it could not
answer and that is how this project got here; this one refuses instead.

Exit 1 when an UNGUARDED-CALL is found that is not in the allowlist below.
Exit 0 when clean.

    tools/rawdoor_audit.py                 # audit the tree
    tools/rawdoor_audit.py --self-test     # prove it still finds the 2 known sites
    tools/rawdoor_audit.py --all           # list every raw site, guarded or not
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

# The two sites lane V found by hand.  --self-test asserts the checker still
# reports both; if a repair lands, update this list in the same commit.
KNOWN = [
    ('runtime/src/audio/audio.cpp', '?IsPaused@Scene@Audio@@QEBA_NH@Z'),
    ('runtime/src/ui/widgets_layout.cpp', '?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ'),
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


class FileScan:
    def __init__(self, path):
        self.path = path
        self.findings = []
        self.all_sites = []
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

        if RAW_ANY.search(code):
            guarded = (bool(guard_depths) or
                       diff_only_fn_depth is not None or
                       in_macro or
                       bool(DIFF_MACRO.search(code)))
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


def walk(root_dir):
    for r in ROOTS:
        base = os.path.join(root_dir, r)
        if not os.path.isdir(base):
            continue
        for dp, _dn, fn in os.walk(base):
            for f in sorted(fn):
                if f.endswith(EXTS):
                    yield os.path.join(dp, f)


def main(argv):
    show_all = '--all' in argv
    self_test = '--self-test' in argv
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    errors, findings, total_sites = [], [], 0
    for path in walk(root_dir):
        fs = scan(path)
        rel = os.path.relpath(path, root_dir).replace(os.sep, '/')
        if fs.error:
            errors.append((rel, fs.error))
            continue
        total_sites += len(fs.all_sites)
        if show_all:
            for n, guarded, called, names, text in fs.all_sites:
                print('%-6s %-8s %s:%d  %s' %
                      ('CALL' if called else 'resolve',
                       'guarded' if guarded else 'LIVE', rel, n, text))
        for n, names, text in fs.findings:
            if '%s:%d' % (rel, n) in ALLOWLIST:
                continue
            findings.append((rel, n, names, text))

    if errors:
        print('CANNOT CHECK -- %d file(s):' % len(errors), file=sys.stderr)
        for rel, err in errors:
            print('  %s: %s' % (rel, err), file=sys.stderr)
        print('Refusing to report a verdict on a partial scan.', file=sys.stderr)
        return 2

    print('rawdoor_audit: %d raw-door sites scanned, %d UNGUARDED-CALL' %
          (total_sites, len(findings)))
    for rel, n, names, text in findings:
        print('UNGUARDED-CALL %s:%d' % (rel, n))
        for nm in names:
            print('    symbol: %s' % nm)
        print('    %s' % text)

    if self_test:
        seen = {(rel, nm) for rel, _n, names, _t in findings for nm in names}
        missing = [k for k in KNOWN if k not in seen]
        if missing:
            print('SELF-TEST FAILED -- the checker no longer finds:', file=sys.stderr)
            for rel, nm in missing:
                print('  %s  %s' % (rel, nm), file=sys.stderr)
            print('Either the site was repaired (update KNOWN in the same commit) '
                  'or the matcher broke. A checker that reports nothing is a '
                  'broken checker until proved otherwise.', file=sys.stderr)
            return 2
        print('SELF-TEST OK -- both lane V sites still reported.')

    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
