#!/usr/bin/env python3
"""Lane SERIAL: re-check the IO codec family's mint rows against BOTH doors.

WHY THIS EXISTS
---------------
The Wave 9 mint-wall sweep matches LITERAL registrations only, so its wall figure
is an UPPER BOUND on the walls, never a lower one: a registration spelled through
a variable or a named constant is invisible to it.  A row it CLEARS is genuinely
clear; a row it FLAGS may still be registered.  This re-checks the 75 rows of
??_7BinaryReader@IO@@6B@ / ??_7BinaryWriter@IO@@6B@ / ??_7Visitor@IO@@6B@ by both
the NAME door and the ADDRESS door before anything is ported, because this
project has three times had a lane port something the tree already had and a
duplicate registration is last-binding-wins -- it silently makes one body dead
code.

TWO BLIND SPOTS, ANNOUNCED RATHER THAN DISCOVERED LATER
------------------------------------------------------
1. It cannot see a mangled name assembled at runtime from pieces.  Nothing under
   runtime/ does that today.
2. It matches the two registration spellings the tree actually uses.  A NEW
   wrapper macro would be invisible to it -- which is not hypothetical: lane
   SERIAL's own first draft wrapped its 73 registrations in an RT_SERIAL_REG
   macro, this checker reported 73 WALL, and that FAILED PREDICTION is what
   caught it.  The port was changed to literal calls, not the checker, because
   tools/runtime_census.py has the same blind spot and the point of the lane was
   visibility to the sweep.

THE COMMENT STRIPPER AND ITS CONTROL
------------------------------------
Prose is stripped before matching, because a checker with no comment stripper
reports PROSE as code and the prose most likely to name an address is a comment
saying that code was DELETED (reference_uncommented_checker_manufactures_work.md).
Per that lesson the stripper ships with a control it must NOT move: `rows
examined` is computed from the mint table and is independent of stripping, so if
stripping ever changes it, the stripper is eating code.
"""
import os
import re
import sys

MINT = os.path.join("standalone", "rt_data_mint_tables.inc")

ROW_RE = re.compile(r'\{\s*"([^"]+)"\s*,\s*(0x[0-9a-fA-F]+)ull\s*,\s*RT_MINT_NAMED')
BLOCK_COMMENT = re.compile(r'/\*.*?\*/', re.S)
LINE_COMMENT = re.compile(r'//[^\n]*')

CLASSES = ("BinaryReader@IO@@", "BinaryWriter@IO@@", "Visitor@IO@@")


def strip_comments(text):
    """Remove // and /* */ comments. Deliberately naive about comment markers
    inside string literals -- a mangled MSVC name cannot contain `//` or `/*`,
    so for this checker's alphabet the naive version and a correct lexer agree.
    """
    return LINE_COMMENT.sub("", BLOCK_COMMENT.sub("", text))


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."

    rows = []
    with open(os.path.join(root, MINT), encoding="utf-8", errors="replace") as f:
        for line in f:
            m = ROW_RE.search(line)
            if m and any(c in m.group(1) for c in CLASSES):
                rows.append((m.group(1), int(m.group(2), 16)))

    sources = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames
                       if d not in (".git", ".claude", "build", "build-perf",
                                    "build-debug", "obj")]
        for fn in filenames:
            if not fn.endswith((".cpp", ".h", ".inc", ".c", ".hpp")):
                continue
            p = os.path.join(dirpath, fn)
            if p.replace("\\", "/").endswith("standalone/rt_data_mint_tables.inc"):
                continue
            try:
                sources[p] = strip_comments(
                    open(p, encoding="utf-8", errors="replace").read())
            except OSError:
                pass

    cleared, walls = [], []
    for name, addr in rows:
        name_door = f'RegisterRuntimeExport("{name}"'
        addr_door = re.compile(r'RegisterRuntimeAddr\s*\(\s*0x0*%x\b' % addr,
                               re.IGNORECASE)
        hits = []
        for p, txt in sources.items():
            if name_door in txt or addr_door.search(txt):
                hits.append(p)
        (cleared if hits else walls).append((name, addr, sorted(set(hits))))

    print(f"rows examined: {len(rows)}   CLEARED: {len(cleared)}   WALL: {len(walls)}")
    print("-- control: `rows examined` must be 75 and must NOT move when the")
    print("   comment stripper changes; a run that clears 0 rows is a broken")
    print("   checker, not a clean sweep --")

    if cleared:
        print("\n== CLEARED (registered through at least one door) ==")
        by_file = {}
        for n, a, h in cleared:
            by_file.setdefault(", ".join(h), []).append((a, n))
        for f, items in sorted(by_file.items()):
            print(f"  {len(items):3d} rows <- {f}")
    if walls:
        print("\n== WALL (no registration found by either door) ==")
        for n, a, _ in walls:
            print(f"  {a:#011x}  {n}")
    return 0 if not walls else 1


if __name__ == "__main__":
    sys.exit(main())
