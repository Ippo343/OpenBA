#!/usr/bin/env python3
"""Wave 9 lane W — every PORT-AWARE resolve of a symbol that has NEITHER a port
NOR a mint entry, found from SOURCE rather than from a run.

WHY THIS EXISTS. Lane V established the finding this lane inherited:

    ResolveExport misses the port registry for an unported symbol and falls
    through to ResolveExportRawImpl (standalone/resolve_standalone.cpp), which
    returns null with no module handle. So RT_ORIGINAL_ONCE -> RT_EXPORT_ONCE on
    a symbol with no port and no mint entry changes only WHICH RESIDUE LIST IT
    PRINTS IN. The sufficient condition is a PORT or a MINT ENTRY.

Lane V then enumerated 14 such symbols. That enumeration is correct and it is
INCOMPLETE, for a reason that is structural rather than an oversight: its input
is the `[RtRawResidue]` union, i.e. the requests NINE HARNESS CELLS ACTUALLY
MADE. A symbol on a live path that no cell exercised is not in the union and so
cannot be in the verdict table. `?UpdateSave@SaveLoad@Save@@QEAAXXZ` and
`?UpdateDelete@SaveLoad@Save@@QEAAXXZ` are the demonstration: they sit in the
SAME switch statement as the three `?UpdateList` / `?UpdateLoad` /
`?UpdateLoadFromFile` arms lane V DID list (runtime/src/save/saveload.cpp), on
the same door, with the same disposition — and they appear in ZERO rows of
docs/runtime_port/wave9_rawresidue_verdicts.csv, because no cell in the matrix
saved a game or deleted a save.

So residue answers "what did this run ask for", and this script answers "what
would ANY run ask for", which is the question a cutover needs. The two are
complementary and neither subsumes the other -- the same pairing
reference_count_the_doors_not_the_calls.md records for the census.

WHAT IT DOES AND DOES NOT SEE. Deliberately conservative, and it says so rather
than claiming coverage:

  * It finds a mangled-name LITERAL inside RT_EXPORT_ONCE(...) / ResolveExport(...)
    / RT_ORIGINAL_ONCE(...) / RtOriginal<...>(...). A name reached through a
    wrapper, a named constant, or a table walked in a loop is INVISIBLE to it --
    the same three shapes WAVE9_LANE_C2.md records for the census. It reports
    that blind spot in its own output rather than leaving the caller to assume
    a clean sweep.
  * It does NOT decide reachability. A site inside `if (rt_diff_on__)` dies with
    the oracle and is harmless at cutover; the guard column says which, using
    the same brace-matched enclosing-function scan rawdoor_audit uses, and rows
    are split into GUARDED and LIVE. LIVE here means "not inside a diff guard",
    not "a run executed it".

Exit code is always 0: this is an inventory, not a gate. Making it a gate would
require the reachability judgement it deliberately does not make.
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN_DIRS = ["runtime", "standalone", "engine", "code", "dx11"]

# A mangled MSVC name: starts with ? and has no spaces.
NAME = r'"(\?[^"\s]{4,})"'
DOOR = re.compile(
    # WAVE 9 LANE SYS2, 2026-09-08: RT_DATA_SLOT was MISSING from this list, and
    # the omission was not cosmetic -- it silently SHRANK the denominator. This
    # lane converted one `ResolveExport("??_7Thread@System@@6B@")` to
    # `RT_DATA_SLOT(void*, "??_7Thread@System@@6B@")` and the scan's total went
    # 789 -> 788 with no symbol having been satisfied by anything. A change that
    # makes a door SAFER must not be able to make it INVISIBLE to the instrument
    # that counts doors; that is the shape of
    # reference_documenting_a_check_can_delete_it.md, one tool over. Same macro
    # family as RT_EXPORT_ONCE (both are RtResolveCached with a literal name),
    # so it belongs here and always did.
    r'(RT_EXPORT_ONCE|RT_DATA_SLOT|RT_ORIGINAL_ONCE|ResolveExportRaw|ResolveExport|RtOriginal)'
    r'\s*(?:<[^;{}]*?>)?\s*\([^;{}]*?' + NAME,
    re.S)
REG = re.compile(r'RegisterRuntimeExport\s*\(\s*"([^"]+)"')
MINT = re.compile(r'"(\?[^"\s]+)"')


def strip_comments(text):
    out, i, n = [], 0, len(text)
    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif text[i] in '"\'':
            q = text[i]
            out.append(text[i]); i += 1
            while i < n and text[i] != q:
                if text[i] == '\\':
                    out.append(text[i]); i += 1
                if i < n:
                    out.append(text[i]); i += 1
            if i < n:
                out.append(text[i]); i += 1
        else:
            out.append(text[i]); i += 1
    return "".join(out)


def sources():
    for d in SCAN_DIRS:
        for root, _dirs, files in os.walk(os.path.join(REPO, d)):
            for f in files:
                if f.endswith((".cpp", ".h", ".inc")):
                    p = os.path.join(root, f)
                    yield os.path.relpath(p, REPO).replace(os.sep, "/"), p


def guard_of(text, pos):
    """True when `pos` sits inside an `if (rt_diff_on__)` block. Brace-matched
    from the guard forward, never a line window -- a fixed window is what
    reference_documenting_a_check_can_delete_it.md is about."""
    for m in re.finditer(r'if\s*\(\s*rt_diff_on__\s*\)', text):
        start = text.find("{", m.end())
        if start < 0 or start > m.end() + 4:
            # single-statement guard: covers to the next ;
            end = text.find(";", m.end())
            if end > 0 and m.end() <= pos <= end:
                return True
            continue
        depth, i = 0, start
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        if start <= pos <= i:
            return True
    return False


def main():
    ported, minted, requests = set(), set(), {}
    wrappers = 0

    for rel, path in sources():
        raw = open(path, encoding="utf-8", errors="replace").read()
        text = strip_comments(raw)
        for m in REG.finditer(text):
            ported.add(m.group(1))
        if rel == "standalone/rt_data_mint_tables.inc":
            for m in MINT.finditer(text):
                minted.add(m.group(1))
        for m in DOOR.finditer(text):
            name = m.group(2)
            line = text.count("\n", 0, m.start()) + 1
            requests.setdefault(name, []).append((rel, line, guard_of(text, m.start())))
        # count the shapes we cannot see, so the blind spot has a number
        wrappers += len(re.findall(r'\b(RtVtable|RtDataExport)\s*\(\s*[A-Za-z_]', text))

    unsat = {n: s for n, s in requests.items() if n not in ported and n not in minted}
    live = {n: [x for x in s if not x[2]] for n, s in unsat.items()}
    live = {n: s for n, s in live.items() if s}

    print("unsatisfied_export_scan: %d distinct names resolved through a door; "
          "%d have neither a port nor a mint entry" % (len(requests), len(unsat)))
    print("  of those, %d have at least one site OUTSIDE a diff guard:\n" % len(live))
    for n in sorted(live):
        sites = live[n]
        print("  %-88s %s%s" % (n[:88], "%s:%d" % (sites[0][0], sites[0][1]),
                                "" if len(sites) == 1 else "  (+%d more)" % (len(sites) - 1)))
    print("\n  BLIND SPOT, measured not assumed: %d resolves go through a wrapper "
          "taking a non-literal argument and are invisible to this scan." % wrappers)
    print("  Reachability is NOT established here -- 'outside a diff guard' is a "
          "structural property, not 'a run executed it'.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
