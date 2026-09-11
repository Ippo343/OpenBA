#!/usr/bin/env python3
"""Re-derive the doors of the screenshot path and diff them against the board.

WHY THIS EXISTS
---------------
`runtime/src/io/file_stream_cutover_probe.cpp` keeps a hand-built array of the
doors on `?SaveImageToFile@ImageBuffer@Video@@`'s path.  A hand-built list has
no derivation to disagree with it, and this debt has now been found deeper than
stated four times: the Wave 19 plan said four exports, correction C2 said six, a
call-graph walk found ten, and Wave 20 lane DOORS found four MORE that are
`RT_EXPORT_ONCE` sites in `Video_ImageBuffer_SaveImageToFile` itself.

So the DIRECT half is derived here instead of read.  This tool scans the source
of the bodies on the path for door macros and set-diffs the names it finds
against the names in the probe's array.

WAVE 21 LANE SHOT -- THE TRANSITIVE HALF IS NOW DERIVED TOO (``--transitive``)
------------------------------------------------------------------------------
Wave 20 §8 item 6: *"the transitive half of the door set is still hand-built and
was four short."*  ``--transitive`` replaces that hand-built half with a walk:

  1. index every function DEFINITION in the compiled source dirs by brace
     matching over comment-stripped text;
  2. map every mangled symbol this tree registers to the function that
     implements it (`RegisterRuntimeExport("<name>", ...&Impl)` /
     `RegisterRuntimeAddr(0x..., ...&Impl)`);
  3. from the ROOT symbol, walk our OWN call graph -- following both door
     literals (a door name whose impl we own is a next node) and ordinary
     direct calls to indexed functions -- and collect every door literal seen.

That is a source-level closure over OUR ported bodies, which is the population
the cutover board is about: at cutover it is our body that runs, and the doors
that matter are the ones our bodies open.

WHAT IT CANNOT DO -- stated before any finding, not after
---------------------------------------------------------
* ``PATH_SOURCES`` (direct half) and ``ROOT_SYMBOL`` (transitive half) are FREE
  PARAMETERS -- the one thing WAVE19_CLOSED §7.1 says governs where a check
  points.  Neither can be removed (nothing in the tree states which files
  implement the screenshot path), so both are PRINTED BESIDE THE VERDICT, every
  run, before the findings.
* It matches string LITERALS.  A door spelled through a named constant is
  invisible to it (``reference_literal_grep_blind_to_variables.md``), and the
  count of non-literal door macros it had to skip is printed as its own line
  rather than silently dropped.
* DOORS IT STRUCTURALLY CANNOT SEE, counted as doors, not as calls
  (``reference_count_the_doors_not_the_calls.md``): the six resolve doors are
  `ResolveExport` / `ResolveExportRaw` / `ResolveRuntimeRaw` / `ResolveOrdinal`
  / `ResolveOrdinalRaw` / `ResolveAddr(Module::Runtime, ...)`.  ``DOOR_RE``
  below names which of them it reads.  An ORDINAL door has no mangled name at
  all and can never be set-diffed against a name-keyed board.  An
  address-keyed door (`ResolveAddr`/`RegisterRuntimeAddr`) is likewise not a
  name and is reported in its own bucket, never merged into the name set.
* The transitive walk follows our SOURCE.  A door opened by a body we have NOT
  ported is invisible to it -- there is no source to scan.  Its result is
  therefore a LOWER bound on the true door set and an UPPER bound on nothing.
* Function indexing is brace matching over comment-stripped text.  A method
  DEFINED INSIDE a class/struct body is not indexed (the tool only opens a
  function at a non-function nesting level), and the count of such skips is not
  measured.  Said here rather than discovered later.
* The index is keyed on the BARE identifier, so a name with several definitions
  is followed into all of them.  Every node entered through such a hop is
  TAINTED and its doors are flagged `[AMBIGUOUS HOP]`; the tainted count is
  printed beside the reached count.  The error direction is OVER-reach.
* IT HAS NO NOTION OF A GUARD.  A door inside `if (getenv(...))`, inside a
  diagnostic probe, or inside a `!RuntimeDllPresent()` arm is counted exactly
  like an unconditional one.  `RunFileStreamSlotsProbe` -- a diagnostic reached
  from `RtIo_FileSystem_CloseFile` after 2000 closes -- is a real instance in
  the current tree and contributes eight address doors.

CONTROLS
--------
direct, positive  the scan must find a non-zero number of door literals, and the
          probe array must parse to a non-zero number of names.
direct, negative  a symbol this tool synthesises and no source contains must NOT
          appear in either set -- if it does, the matcher is matching anything.
transitive, BOTH DIRECTIONS (``reference_absence_screen_must_be_handed_a_shape``)
          a screen whose output is "here is the set" answers an ABSENCE question
          the moment anyone reads a name as missing, and then "0 hits" is
          indistinguishable from a dead walker.  So the walk is re-run twice
          with a synthetic door literal INJECTED into the text of a named
          function: once into a function that is provably REACHABLE at depth >=
          2 (must be FOUND) and once into one that is provably UNREACHABLE from
          the root (must NOT be found).  Both must score, or the transitive
          verdict is withheld.

Usage:  python tools/screenshot_door_walk.py [--root <repo root>] [--transitive]
Exit:   0 = derived set is a subset of the board
        1 = the board is missing a door this tool derived (or a control caught a
            bad match)
        2 = a control failed, nothing reported
"""

import argparse
import os
import re
import sys

# THE FREE PARAMETER.  Printed beside the verdict on every run.
PATH_SOURCES = [
    "runtime/src/video/save_image_to_file.cpp",
]

PROBE = "runtime/src/io/file_stream_cutover_probe.cpp"

# THE SECOND FREE PARAMETER -- the transitive walk's root.
ROOT_SYMBOL = "?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z"

# The source roots the transitive walk indexes.  Same set the stamper census
# uses, so the two instruments cannot disagree about what "our tree" is.
SRC_DIRS = ["runtime", "engine", "code", "standalone", "dx11"]
SRC_EXT = (".cpp", ".c", ".h", ".hpp", ".inc")

# RT_EXPORT_ONCE(Type, "name") / RT_DATA_SLOT(Type, "name") / ResolveExport("name")
DOOR_RE = re.compile(
    r'(?:RT_EXPORT_ONCE|RT_DATA_SLOT)\s*\(\s*[^,]+,\s*"([^"]+)"', re.S)
# every door macro invocation, with the FIRST character of its name argument, so
# a name spelled through a constant instead of a literal can be counted
ANY_DOOR_RE = re.compile(
    r'(?:RT_EXPORT_ONCE|RT_DATA_SLOT)\s*\(\s*[^,]+,\s*(\S)', re.S)

# The transitive half reads a WIDER set of door spellings than the direct half,
# because a callee's body opens doors the root's body does not.  Every spelling
# here is a NAME door; ordinal and address doors are counted separately below.
TDOOR_RE = re.compile(
    r'(?:RT_EXPORT_ONCE|RT_DATA_SLOT|RT_ORIGINAL_ONCE)\s*\(\s*[^,]+,\s*"([^"]+)"'
    r'|RT_VTABLE_ONCE\s*\(\s*"([^"]+)"'
    r'|(?:ResolveExport|ResolveExportRaw)\s*\(\s*"([^"]+)"'
    r'|RtOriginal\s*<[^>]*>\s*\(\s*"([^"]+)"', re.S)
TANY_DOOR_RE = re.compile(
    r'(?:RT_EXPORT_ONCE|RT_DATA_SLOT|RT_ORIGINAL_ONCE)\s*\(\s*[^,]+,\s*(\S)'
    r'|RT_VTABLE_ONCE\s*\(\s*(\S)'
    r'|(?:ResolveExport|ResolveExportRaw)\s*\(\s*(\S)'
    r'|RtOriginal\s*<[^>]*>\s*\(\s*(\S)', re.S)
# doors that are NOT a mangled name and so can never be set-diffed against the
# board's name column.  Counted, named by bucket, never merged.
TADDR_DOOR_RE = re.compile(
    r'ResolveAddr\s*\(\s*Module::Runtime\s*,\s*(0x[0-9a-fA-F]+)'
    r'|ResolveRuntimeRaw\s*\(\s*(0x[0-9a-fA-F]+)')
TORD_DOOR_RE = re.compile(r'(?:ResolveOrdinal|ResolveOrdinalRaw)\s*\(\s*(\d+)')

# symbol -> implementing function
REG_NAME_RE = re.compile(
    r'RegisterRuntimeExport\s*\(\s*"([^"]+)"\s*,(.{0,200}?)\)\s*;', re.S)
REG_ADDR_RE = re.compile(
    r'RegisterRuntimeAddr\s*\(\s*(0x[0-9a-fA-F]+)\w*\s*,(.{0,200}?)\)\s*;', re.S)
IMPL_RE = re.compile(r'&\s*([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)')

# a row of the probe's kDoors array: { "name", 0xaddr, "note" } or { nullptr, ... }
ROW_RE = re.compile(r'\{\s*"([^"]+)"\s*,\s*(0x[0-9a-fA-F]+)?', re.S)

SYNTHETIC = "?ThisSymbolIsSynthetic@LaneDoors@Wave20@@QEAAXXZ"
# The transitive control's shape.  It is a door literal, spelled exactly like a
# real one, so that finding it proves the walker reads real ones.
CTL_DOOR = '?ThisDoorIsSyntheticWave21ShotControl@@QEAAXXZ'
CTL_INJECT = '\n    RT_EXPORT_ONCE(void*, "%s");\n' % CTL_DOOR

NOT_A_FUNCTION_HEAD = re.compile(
    r'\b(namespace|class|struct|union|enum|extern\s*"C"|do|else|try|switch)\b')
CONTROL_KW = re.compile(r'\b(if|for|while|switch|catch|return|sizeof)\s*\($')


def strip_comments(text):
    """Remove C and C++ comments.  Prose naming a symbol is not a door, and the
    prose most likely to name one is a comment explaining why a row was added."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1]); i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i); i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i); i = n if j < 0 else j + 2
        else:
            out.append(c); i += 1
    return "".join(out)


# ---------------------------------------------------------------------------
# THE TRANSITIVE HALF
# ---------------------------------------------------------------------------

def index_functions(src):
    """Return {name: (bodyStart, bodyEnd)} for the function definitions in one
    comment-stripped translation unit, by brace matching.

    A `{` opens a FUNCTION when the header since the previous statement
    boundary ends in `)` (optionally followed by const / noexcept / an
    initialiser list) and does not start a namespace/class/struct/enum, and we
    are not already inside a function body.  Everything else just nests."""
    funcs, depth, fn_depth, i, n = {}, 0, None, 0, len(src)
    bound = 0          # index just after the last ; { or }
    pending = None     # (name, bodyStart)
    while i < n:
        c = src[i]
        if c == '"' or c == "'":
            q = c; j = i + 1
            while j < n and src[j] != q:
                j += 2 if src[j] == '\\' else 1
            i = j + 1; continue
        if c == '{':
            if fn_depth is None:
                head = src[bound:i]
                name = _func_name(head)
                if name:
                    fn_depth = depth
                    pending = (name, i + 1)
            depth += 1; bound = i + 1; i += 1; continue
        if c == '}':
            depth -= 1
            if fn_depth is not None and depth == fn_depth:
                funcs.setdefault(pending[0], (pending[1], i))
                fn_depth, pending = None, None
            bound = i + 1; i += 1; continue
        if c == ';':
            bound = i + 1; i += 1; continue
        i += 1
    return funcs


def _func_name(head):
    """The declarator name of a function-definition header, or None."""
    h = head.strip()
    if not h or NOT_A_FUNCTION_HEAD.search(h):
        return None
    # strip a constructor initialiser list / trailing qualifiers
    h = re.sub(r'\)\s*(const|noexcept|override|final|\s)*(:.*)?$', ')', h, flags=re.S)
    if not h.endswith(')'):
        return None
    # find the '(' that matches the final ')'
    depth, j = 0, len(h) - 1
    while j >= 0:
        if h[j] == ')':
            depth += 1
        elif h[j] == '(':
            depth -= 1
            if depth == 0:
                break
        j -= 1
    if j < 0:
        return None
    decl = h[:j].rstrip()
    if CONTROL_KW.search(decl + '('):
        return None
    m = re.search(r'([A-Za-z_]\w*)(?:\s*<[^<>]*>)?$', decl)
    if not m:
        return None
    name = m.group(1)
    if name in ('if', 'for', 'while', 'switch', 'catch', 'return', 'operator'):
        return None
    return name


def load_tree(root, inject=None):
    """Index the compiled source dirs.

    `inject` is (functionName, text): the text is spliced into that function's
    body before indexing, which is how the transitive control is handed a shape
    without editing a tracked file."""
    funcs = {}          # name -> list of (file, body)
    sym2impl = {}       # mangled name -> impl function name
    nfiles = nbytes = 0
    for d in SRC_DIRS:
        for dp, _, fs in os.walk(os.path.join(root, d)):
            for f in fs:
                if not f.endswith(SRC_EXT):
                    continue
                p = os.path.join(dp, f)
                try:
                    raw = open(p, encoding="utf-8", errors="replace").read()
                except OSError:
                    continue
                nfiles += 1; nbytes += len(raw)
                src = strip_comments(raw)
                idx = index_functions(src)
                if inject and inject[0] in idx:
                    s, e = idx[inject[0]]
                    src = src[:s] + inject[1] + src[s:]
                    idx = index_functions(src)
                for name, (s, e) in idx.items():
                    funcs.setdefault(name, []).append((p, src[s:e]))
                for m in REG_NAME_RE.finditer(src):
                    im = IMPL_RE.search(m.group(2))
                    if im:
                        sym2impl.setdefault(m.group(1), im.group(1).split("::")[-1])
                for m in REG_ADDR_RE.finditer(src):
                    im = IMPL_RE.search(m.group(2))
                    if im:
                        sym2impl.setdefault(m.group(1).lower(),
                                            im.group(1).split("::")[-1])
    return funcs, sym2impl, nfiles, nbytes


def _first_group(m):
    for g in m.groups():
        if g:
            return g
    return None


def walk(funcs, sym2impl, root_symbol, max_nodes=20000):
    """Closure over our own bodies from `root_symbol`.

    Returns (doors, addrDoors, ordDoors, visitedFns, nonliteral, tainted).

    AMBIGUITY, AND WHY IT IS TRACKED RATHER THAN IGNORED.  The index is keyed on
    the BARE identifier, so a name defined in more than one translation unit
    (`Drive`, `Setup`, `Run` ...) cannot be resolved to one definition from the
    call text alone.  The walk follows all of them -- dropping them would turn
    an over-reach into an under-reach, which is the worse direction for a screen
    whose output is read as "the door set" -- but every node entered through an
    ambiguous hop is TAINTED, the taint propagates, and every door found only in
    a tainted node is reported separately.  A door in a tainted node may belong
    to a body that is not on this path at all."""
    start = sym2impl.get(root_symbol)
    if not start:
        return None
    seen, queue = set(), [(start, False)]
    tainted_fns = set()
    doors, addrs, ords_, nonlit = {}, {}, {}, 0
    while queue and len(seen) < max_nodes:
        fn, taint = queue.pop()
        if fn in seen:
            continue
        seen.add(fn)
        if taint:
            tainted_fns.add(fn)
        if fn not in funcs:
            continue
        ambig = len(funcs[fn]) > 1
        for _path, body in funcs[fn]:
            for m in TDOOR_RE.finditer(body):
                nm = _first_group(m)
                if nm:
                    doors.setdefault(nm, (fn, taint))
                    nxt = sym2impl.get(nm)
                    if nxt and nxt not in seen:
                        queue.append((nxt, taint))
            for m in TANY_DOOR_RE.finditer(body):
                if _first_group(m) != '"':
                    nonlit += 1
            for m in TADDR_DOOR_RE.finditer(body):
                a = _first_group(m).lower()
                addrs.setdefault(a, (fn, taint))
                nxt = sym2impl.get(a)
                if nxt and nxt not in seen:
                    queue.append((nxt, taint))
            for m in TORD_DOOR_RE.finditer(body):
                ords_.setdefault(_first_group(m), (fn, taint))
            # ordinary direct calls to functions we also own
            for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\(', body):
                cal = m.group(1)
                if cal != fn and cal in funcs and cal not in seen:
                    queue.append((cal, taint or ambig or len(funcs[cal]) > 1))
    return doors, addrs, ords_, seen, nonlit, tainted_fns


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    ap.add_argument("--transitive", action="store_true",
                    help="also derive the TRANSITIVE half of the door set")
    ap.add_argument("--root-symbol", default=ROOT_SYMBOL)
    ap.add_argument("--ctl-reachable", default="RtIo_FileSystem_CreatePath",
                    help="a function the walk MUST reach (positive control)")
    ap.add_argument("--ctl-unreachable", default="RtVideo_Device_EnumerateVideoModes",
                    help="a function the walk must NOT reach (negative control)")
    args = ap.parse_args()
    root = os.path.abspath(args.root)

    # SUBJECT FIRST, before any finding.
    print("SUBJECT ROOT : %s" % root)
    print("SUBJECT FILES (the free parameter -- this is where the check POINTS):")
    for f in PATH_SOURCES:
        print("               %s" % f)
    print("BOARD        : %s" % PROBE)
    if args.transitive:
        print("SCOPE        : DIRECT doors + TRANSITIVE closure over OUR OWN "
              "ported bodies from the root symbol below.")
        print("ROOT SYMBOL  : %s   (the transitive half's free parameter)"
              % args.root_symbol)
    else:
        print("SCOPE        : DIRECT doors only.  The transitive half of the "
              "board and its address-only rows are NOT derived here.  Pass "
              "--transitive to derive them.")

    derived, nonlit = {}, 0
    for rel in PATH_SOURCES:
        p = os.path.join(root, rel)
        if not os.path.isfile(p):
            print("CONTROL RED  : subject file does not exist: %s" % rel)
            return 2
        src = strip_comments(open(p, encoding="utf-8", errors="replace").read())
        for m in DOOR_RE.finditer(src):
            derived.setdefault(m.group(1), rel)
        nonlit += sum(1 for m in ANY_DOOR_RE.finditer(src)
                      if m.group(1) != '"')

    probe_path = os.path.join(root, PROBE)
    if not os.path.isfile(probe_path):
        print("CONTROL RED  : board file does not exist: %s" % PROBE)
        return 2
    # Comments stripped here too: the prose most likely to name a door is a
    # comment explaining a row, and counting prose as a row would make the
    # board look complete for exactly the symbols someone wrote ABOUT.
    probe_src = strip_comments(
        open(probe_path, encoding="utf-8", errors="replace").read())
    start = probe_src.find("const Door kDoors[]")
    end = probe_src.find("};", start) if start >= 0 else -1
    if start < 0 or end < 0:
        print("CONTROL RED  : could not find the kDoors array in the board file. "
              "It was renamed or restructured; this tool is aimed at nothing.")
        return 2
    board = set(m.group(1) for m in ROW_RE.finditer(probe_src[start:end]))
    # rows = every `{` in the slice MINUS the array's own opening brace.  The
    # count is printed beside the name count on purpose: they are different
    # numbers (four rows are address-only and have no name), and quoting the
    # name count as "the board" is exactly the denominator failure WAVE20 §5.10
    # is about.
    board_rows = probe_src[start:end].count("{") - 1

    # ---- controls ---------------------------------------------------------
    pos_ok = len(derived) > 0 and len(board) > 0
    neg_ok = SYNTHETIC not in derived and SYNTHETIC not in board
    print("CONTROL pos  : %d door literals derived from source, %d names parsed "
          "from the board -> %s" % (len(derived), len(board),
                                    "GREEN" if pos_ok else "RED"))
    print("CONTROL neg  : synthetic symbol absent from both sets -> %s"
          % ("GREEN" if neg_ok else "RED"))
    print("BLIND SPOT   : %d door macro(s) in the subject files spell their name "
          "with something other than a literal and are invisible to this tool"
          % nonlit)
    if not pos_ok or not neg_ok:
        print("*** MEASURED NOTHING: a control failed.  No verdict. ***")
        return 2

    missing = sorted(n for n in derived if n not in board)
    for n in missing:
        print("MISSING FROM BOARD  %-70s  (%s)" % (n, derived[n]))
    print("==== %d of %d derived direct doors are on the board, %d MISSING ===="
          % (len(derived) - len(missing), len(derived), len(missing)))

    if not args.transitive:
        return 1 if missing else 0

    # ---- the transitive half ---------------------------------------------
    print("")
    print("---- TRANSITIVE HALF (Wave 21 lane SHOT) ----")
    funcs, sym2impl, nfiles, nbytes = load_tree(root)
    print("POPULATION   : %d files / %d bytes across %s ; %d function "
          "definitions indexed ; %d mangled symbols mapped to an impl"
          % (nfiles, nbytes, "/".join(SRC_DIRS), len(funcs), len(sym2impl)))
    if args.root_symbol not in sym2impl:
        print("CONTROL RED  : the root symbol is not registered anywhere in this "
              "tree, so the walk starts nowhere. MEASURED NOTHING.")
        return 2
    print("ROOT IMPL    : %s" % sym2impl[args.root_symbol])

    res = walk(funcs, sym2impl, args.root_symbol)
    tdoors, taddrs, tords, tseen, tnonlit, ttaint = res

    # BOTH-DIRECTION CONTROL: hand the absence screen a shape it must find, and
    # one it must not.  (reference_absence_screen_must_be_handed_a_shape.md)
    ctl_pos = ctl_neg = None
    if args.ctl_reachable not in funcs:
        print("CONTROL RED  : the positive-control function %r is not in the "
              "index at all, so the control cannot be scored."
              % args.ctl_reachable)
        return 2
    if args.ctl_unreachable not in funcs:
        print("CONTROL RED  : the negative-control function %r is not in the "
              "index at all, so the control cannot be scored."
              % args.ctl_unreachable)
        return 2
    for tag, fn in (("pos", args.ctl_reachable), ("neg", args.ctl_unreachable)):
        f2, s2, _, _ = load_tree(root, inject=(fn, CTL_INJECT))
        r2 = walk(f2, s2, args.root_symbol)
        found = CTL_DOOR in r2[0]
        if tag == "pos":
            ctl_pos = found
        else:
            ctl_neg = not found
        print("CONTROL t-%s : synthetic door injected into %-34s -> %s  (%s)"
              % (tag, fn, "FOUND" if found else "not found",
                 "GREEN" if (found if tag == "pos" else not found) else "RED"))
    if not (ctl_pos and ctl_neg):
        print("*** TRANSITIVE MEASURED NOTHING: a control failed.  The set below "
              "is withheld -- a walker that cannot find a door handed to it, or "
              "that finds one it cannot reach, proves nothing by its output. ***")
        return 2

    print("REACHED      : %d of our own function bodies from the root, of which "
          "%d were entered through an AMBIGUOUS name hop (see walk())"
          % (len(tseen), len(ttaint)))
    print("BLIND SPOT   : %d transitive door macro(s) spell their name with "
          "something other than a literal" % tnonlit)
    print("NOT A NAME   : %d address-keyed door(s) and %d ordinal door(s) were "
          "found; neither is a mangled name, so neither is set-diffed against "
          "the board's name column." % (len(taddrs), len(tords)))
    # The board's own address column, so the address doors CAN be diffed --
    # against the right column.  Four board rows are address-only (name ==
    # nullptr) and would otherwise be invisible to every diff this tool prints.
    board_addrs = set(m.group(2).lower()
                      for m in ROW_RE.finditer(probe_src[start:end])
                      if m.group(2))
    board_addrs |= set(m.group(1).lower() for m in re.finditer(
        r'\{\s*nullptr\s*,\s*(0x[0-9a-fA-F]+)', probe_src[start:end]))
    for a in sorted(taddrs):
        if a not in board_addrs:
            fn, tt = taddrs[a]
            print("ADDR NOT ON BOARD  %-18s  (%s%s)"
                  % (a, fn, "  [AMBIGUOUS HOP]" if tt else ""))
    print("ADDRESS COLUMN: %d of %d derived address doors are on the board's "
          "address column (%d addresses there)"
          % (sum(1 for a in taddrs if a in board_addrs), len(taddrs),
             len(board_addrs)))

    tmissing = sorted(n for n in tdoors if n not in board)
    for n in tmissing:
        fn, tt = tdoors[n]
        print("NOT ON BOARD  %-72s  (%s%s)"
              % (n, fn, "  [AMBIGUOUS HOP -- may not be on this path]" if tt else ""))
    extra = sorted(n for n in board if n not in tdoors and n not in derived)
    for n in extra:
        # A board name whose IMPL the walk DID reach is not a missing door: it
        # is a symbol our source reaches by an ordinary C++ call rather than
        # through a door, so at cutover nothing has to resolve it.  Naming that
        # distinction is the point -- the board calls both "doors".
        impl = sym2impl.get(n)
        why = ("reached as a DIRECT CALL to %s, not through a door" % impl
               if impl in tseen else "the walk does not reach it at all")
        print("BOARD ONLY    %-72s  (%s)" % (n, why))
    clean = [n for n in tmissing if not tdoors[n][1]]
    print("==== transitive: %d name doors derived, %d of them on the board, "
          "%d NOT ON BOARD (%d of those on an UNAMBIGUOUS path); board has %d "
          "rows / %d names, %d of which the walk does not reach ===="
          % (len(tdoors), len(tdoors) - len(tmissing), len(tmissing), len(clean),
             board_rows, len(board), len(extra)))
    return 1 if (missing or tmissing) else 0


if __name__ == "__main__":
    sys.exit(main())
