#!/usr/bin/env python3
r"""Bound a Runtime-x64.dll vtable HARD, from raw image bytes. (Wave 25 lane VTGFX)

SUBJECT
-------
The shipped bytes of ``C:\GOG Games\Banished\Runtime-x64.dll`` between an
exported ``??_7`` table's start and the first qword that CANNOT be a vtable
slot.  Nothing else: no Ghidra, no ``.pdata``, no generated ``.inc``.

WHY THIS EXISTS
---------------
``standalone/rt_data_mint_tables.inc`` records a proof string per row, and for
many rows that string is ``adjacent-export:<next symbol>``.  That is an UPPER
bound read by four waves as if it were exact
(``reference_adjacent_export_is_an_upper_bound.md``): an unexported table
beginning inside the run is swallowed and nothing notices.  Wave 23 lane GFXVT
hard-bounded the eight Graphics rows this way and found two that rest on the
soft bound alone; **nobody had ever done it for the four Achievement rows.**

A vtable slot is always a pointer into ``.text``.  So the run ends at the first
qword that is not one.  If the run does not stop before the next exported
symbol, the bound is SOFT and this tool SAYS SO rather than printing a number
that looks exact.

CONTROLS
--------
A screen looking for a terminator has no natural positive case, so it is handed
one (``reference_absence_screen_must_be_handed_a_shape.md``).  All four are
printed on every run, above the subject rows, and a FAIL is visible without
reading the source:

  POS    two tables whose HARD terminator Wave 23 GFXVT §3 read independently
         (MaterialInstance = 9, SunLight = 11) must reproduce those extents.
  NEG    a start inside the ``"ImageBuffer\0..."`` string run must report 0.
  SHIFT  a start one slot into a known 9-slot table must report 8 -- this is
         the control Wave 23 GFXVT flagged as the instrument's real LIMIT: the
         tool measures an extent FROM A GIVEN START and cannot validate a
         start.  It is kept, and the limit is restated, so the number is not
         read as more than it is.

USAGE
    tools/vtgfx25_bound.py 0x180154740 0x180154700 ...
"""
import sys

import pefile

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"

pe = pefile.PE(DLL)
IB = pe.OPTIONAL_HEADER.ImageBase
pe.parse_data_directories()

# .text bounds come from the section table, never a hardcoded constant.
TEXT = None
for s in pe.sections:
    if s.Name.rstrip(b"\x00") == b".text":
        TEXT = (IB + s.VirtualAddress, IB + s.VirtualAddress + s.Misc_VirtualSize)
if not TEXT:
    sys.exit(".text section not found -- refusing to guess")

exp_by_va = {}
for sym in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if sym.address:
        exp_by_va.setdefault(IB + sym.address, (sym.name or b"").decode(errors="replace"))
export_vas = sorted(exp_by_va)


def qword(va):
    return int.from_bytes(pe.get_data(va - IB, 8), "little")


def walk(start, limit=64):
    """(extent, bound-kind, terminating qword or None, next export VA or None)."""
    nxt = None
    for v in export_vas:
        if v > start:
            nxt = v
            break
    n = 0
    while n < limit:
        va = start + 8 * n
        if nxt is not None and va >= nxt and n > 0:
            return n, "SOFT(adjacent-export)", None, nxt
        q = qword(va)
        if not (TEXT[0] <= q < TEXT[1]):
            return n, "HARD(tail-not-a-pointer)", q, nxt
        n += 1
    return n, "LIMIT-HIT", None, nxt


def ascii_of(q):
    return "".join(chr(c) if 32 <= c < 127 else "." for c in q.to_bytes(8, "little"))


# ---------------------------------------------------------------------------
# SECOND CHANNEL -- the swallowed-table detector.
#
# The bytes alone cannot harden a SOFT row: the pointer run does not stop, so
# the only thing ending it is the next export.  A DIFFERENT question can be
# asked of the same bytes, and it is the one that matters:
#
#     does a SECOND vtable BEGIN inside this run?
#
# Every MSVC vtable in this DLL begins with the scalar-deleting destructor,
# which is UNEXPORTED (it prints as FUN_/unnamed in every dump in this project)
# while every other slot of these Object::External-family tables is an exported
# method of the class or of a base.  So an unexported table swallowed by a SOFT
# bound shows up as an UNNAMED slot at an interior index.  That is an absence
# claim, so it is handed a positive shape: the row's own neighbour, which is a
# real second table starting at a known index.
# ---------------------------------------------------------------------------
#
# ONE FALSE-POSITIVE CAUSE, FOUND BY THE TOOL DISAGREEING WITH ME AND KEPT HERE
# SO IT IS NOT REDISCOVERED: `_purecall` (0x18007ec28) is NOT in this DLL's
# export directory, so a pure-virtual slot also reads as "unnamed".  The first
# draft therefore reported ??_7Goal@Achievement@@6B@ SUSPECT at [6,7,8] and
# ??_7ParticleInstance@Graphics@@6B@ SUSPECT at [1] -- which are EXACTLY their
# purecall slots as counted independently by tools/vtable_dump.py (3 and 1).
# Two channels naming the same indices is what identified it as an instrument
# defect rather than a swallowed table.  The VA is the same constant
# tools/vtable_dump.py and tools/rt_mint_gen.py carry.
PURECALL_VA = 0x18007EC28


def interior_unnamed(start, extent):
    """Indices > 0 within [start, start+extent) that are neither an exported
    function nor `_purecall` -- i.e. candidate starts of a swallowed table."""
    out = []
    for i in range(1, extent):
        q = qword(start + 8 * i)
        if q != PURECALL_VA and q not in exp_by_va:
            out.append(i)
    return out


def main():
    print(f"SUBJECT {DLL}")
    print(f"  .text 0x{TEXT[0]:x}..0x{TEXT[1]:x}   exports named by VA: {len(exp_by_va)}")
    print()
    print("CONTROLS  (a terminator screen has no natural positive case; it is handed one)")
    failures = 0
    for label, va, expect, kindpfx in [
        ("POS   MaterialInstance, GFXVT read HARD 9 ", 0x1801545E0, 9, "HARD"),
        ("POS   SunLight,         GFXVT read HARD 11", 0x180158E10, 11, "HARD"),
        ('NEG   start inside the "ImageBuf" string  ', 0x180154EA8, 0, "HARD"),
        ("SHIFT Geometry + 1 slot, expect 8         ", 0x180154E68, 8, "HARD"),
    ]:
        n, b, _t, _x = walk(va)
        ok = n == expect and b.startswith(kindpfx)
        failures += 0 if ok else 1
        print(f"  {'OK  ' if ok else 'FAIL'} {label}  0x{va:x}  extent={n:<3} bound={b}")
    print(f"  CONTROLS: {4 - failures} of 4 green")
    print("  LIMIT (Wave 23 GFXVT): SHIFT returns a plausible 8 with no complaint --")
    print("        this measures an extent FROM A GIVEN START and cannot validate a start.")
    print()
    if not sys.argv[1:]:
        print("no subject rows given")
        return
    print("SUBJECT ROWS")
    for va in [int(a, 16) for a in sys.argv[1:]]:
        n, b, t, nxt = walk(va)
        nm = exp_by_va.get(va, "(NOT AN EXPORT -- start is unverified)")
        print(f"  0x{va:x}  extent={n:<3} {b}")
        print(f"      {nm}")
        if t is not None:
            print(f'      terminator qword = 0x{t:016x} = "{ascii_of(t)}"')
        if nxt:
            print(f"      next export      = 0x{nxt:x}  {exp_by_va.get(nxt, '?')}")
        if b.startswith("SOFT"):
            # CONTROL FIRST: run the detector over this row PLUS the next table,
            # where a second table provably begins at index `extent`.  If that
            # does not fire, a clean result on the row alone means nothing.
            nn, _b2, _t2, _x2 = walk(nxt)
            ctrl = interior_unnamed(start=va, extent=n + nn)
            fired = n in ctrl
            print(f"      CONTROL swallowed-table detector over {n}+{nn} slots: "
                  f"interior-unnamed at {ctrl} -> "
                  f"{'FIRES at the known boundary' if fired else 'DEAD PARSER'}")
            if fired:
                bad = interior_unnamed(start=va, extent=n)
                print(f"      SUBJECT {n} slots: interior-unnamed at {bad or 'none'} -> "
                      f"{'no swallowed table detected' if not bad else 'SUSPECT'}")
                print("      (still a SOFT bound; this is corroboration from a second "
                      "channel, not a byte terminator)")


if __name__ == "__main__":
    main()
