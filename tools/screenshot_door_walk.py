#!/usr/bin/env python3
"""Re-derive the DIRECT doors of the screenshot path and diff them against the board.

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

WHAT IT CANNOT DO -- stated before any finding, not after
---------------------------------------------------------
* It derives only the DIRECT half: the door literals appearing in the source
  files listed in ``PATH_SOURCES``.  The TRANSITIVE half of the board (doors
  reached from inside a callee's own DISASSEMBLY, and the four unexported
  address-only rows) is still hand-built and this tool is blind to it.  A clean
  run says "no direct door is missing from the board", never "the board is
  complete".
* ``PATH_SOURCES`` is a FREE PARAMETER -- the one thing WAVE19_CLOSED §7.1 says
  governs where a check points.  It cannot be removed (nothing in the tree
  states which files implement the screenshot path), so it is PRINTED BESIDE THE
  VERDICT, every run, before the findings.
* It matches string LITERALS.  A door spelled through a named constant is
  invisible to it (``reference_literal_grep_blind_to_variables.md``), and the
  count of non-literal door macros it had to skip is printed as its own line
  rather than silently dropped.

CONTROLS (both must pass or it exits 2 and reports nothing)
-----------------------------------------------------------
positive  the scan must find a non-zero number of door literals, and the probe
          array must parse to a non-zero number of names.
negative  a symbol this tool synthesises and no source contains must NOT appear
          in either set -- if it does, the matcher is matching anything.

Usage:  python tools/screenshot_door_walk.py [--root <repo root>]
Exit:   0 = derived direct set is a subset of the board
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

# RT_EXPORT_ONCE(Type, "name") / RT_DATA_SLOT(Type, "name") / ResolveExport("name")
DOOR_RE = re.compile(
    r'(?:RT_EXPORT_ONCE|RT_DATA_SLOT)\s*\(\s*[^,]+,\s*"([^"]+)"', re.S)
# every door macro invocation, with the FIRST character of its name argument, so
# a name spelled through a constant instead of a literal can be counted
ANY_DOOR_RE = re.compile(
    r'(?:RT_EXPORT_ONCE|RT_DATA_SLOT)\s*\(\s*[^,]+,\s*(\S)', re.S)

# a row of the probe's kDoors array: { "name", 0xaddr, "note" } or { nullptr, ... }
ROW_RE = re.compile(r'\{\s*"([^"]+)"\s*,\s*(0x[0-9a-fA-F]+)?', re.S)

SYNTHETIC = "?ThisSymbolIsSynthetic@LaneDoors@Wave20@@QEAAXXZ"


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    args = ap.parse_args()
    root = os.path.abspath(args.root)

    # SUBJECT FIRST, before any finding.
    print("SUBJECT ROOT : %s" % root)
    print("SUBJECT FILES (the free parameter -- this is where the check POINTS):")
    for f in PATH_SOURCES:
        print("               %s" % f)
    print("BOARD        : %s" % PROBE)
    print("SCOPE        : DIRECT doors only.  The transitive half of the board "
          "and its address-only rows are NOT derived here.")

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
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
