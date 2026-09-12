#!/usr/bin/env python3
"""Is an address-keyed Runtime row REALLY unexported?  Wave 26 lane INT33.

WHY RE-DERIVE THIS
------------------
`AEAA` (and `0…A`, and `AEBA`) mean PRIVATE, not unexported, and this DLL
EXPORTS ITS PRIVATE MEMBERS.  Three silent address doors survived review because
the only thing saying "unexported" was a source COMMENT
(reference_aeaa_is_private_not_unexported.md).  So before any row here is
recorded as "no name exists", the export directory is asked.

THE SCREEN REPORTS AN ABSENCE ("0 of these have a name"), which is
indistinguishable from a DEAD LOOKUP.  It is therefore HANDED a positive
population it must name: every in-population address that IS an export must come
back with its name, and a shifted address must not
(reference_absence_screen_must_be_handed_a_shape.md).

    python tools/int33_export_screen.py            # screen + controls
    python tools/int33_export_screen.py 0x180010f00 ...
"""
import sys

import pefile

RETAIL = r"C:\GOG Games\Banished\Runtime-x64.dll"
PROFILE = r"C:\GOG Games\Banished\ToolKit\bin\x64\Runtime-x64-profile.dll"
BASE = 0x180000000

# The 33 address-keyed `internal` gap rows, from
#   tools/runtime_census.py --namespace internal --unported
ROWS = [
    0x180004590, 0x180005220, 0x180009A20, 0x180010F00, 0x1800269A0,
    0x180028760, 0x18002C6A0, 0x18002F170, 0x1800332D0, 0x180034010,
    0x1800493D0, 0x18004BDE0, 0x180052640, 0x180053F00, 0x180075B20,
    0x180075D20, 0x180078950, 0x180078AD0, 0x18007F050, 0x18007F8A8,
    0x18007FBFC, 0x18014BC20, 0x18014BE30, 0x1801554A8, 0x1801557E8,
    0x180155860, 0x1801559F8, 0x180157C60, 0x180161780, 0x1801FBA28,
    0x1801FC6D0, 0x1801FE7C0, 0x1801FE7E0,
]


def export_map(path):
    """{absolute VA -> name} straight out of the export directory."""
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(
        [pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
    base = pe.OPTIONAL_HEADER.ImageBase
    out = {}
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.name:
            out[base + e.address] = e.name.decode()
    return out, base, pe


def section_of(pe, va, base):
    rva = va - base
    for s in pe.sections:
        end = s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData)
        if s.VirtualAddress <= rva < end:
            return s.Name.rstrip(b"\x00").decode()
    return "<none>"


def main(argv):
    addrs = [int(a, 16) for a in argv] or ROWS
    retail, rbase, rpe = export_map(RETAIL)
    print("retail export directory: %d named exports" % len(retail))

    print("\nSUBJECT: %d address-keyed rows" % len(addrs))
    named = []
    for a in addrs:
        nm = retail.get(a)
        print("  0x%x  %-7s  %s" % (a, section_of(rpe, a, rbase), nm or "-- no export name --"))
        if nm:
            named.append((a, nm))
    print("\n  rows WITH an export name: %d of %d" % (len(named), len(addrs)))

    # ---------------- CONTROLS ----------------
    print("\nCONTROLS")
    rows = []

    # POSITIVE, handed in, IN POPULATION: 400 real export VAs must come back
    # named through the SAME dict lookup the rows above take.
    sample = sorted(retail)[:400]
    hits = sum(1 for v in sample if retail.get(v))
    rows.append(("POS 400 known export VAs are named by this lookup",
                 hits == 400, "%d/400" % hits))

    # POSITIVE: the worked specimen of the AEAA trap -- a PRIVATE static datum
    # that IS exported. If the screen could not see this, "unexported" would be
    # unearned for every row above.
    ti = 0x1801FADD0   # ?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A
    rows.append(("POS a PRIVATE ('0...A') datum IS found as an export",
                 retail.get(ti) is not None, str(retail.get(ti))))

    # NEGATIVE: shifted addresses must not resolve, or every lookup is a hit.
    ghosts = sum(1 for v in sample if retail.get(v + 3))
    rows.append(("NEG 400 export VAs shifted +3 resolve to nothing",
                 ghosts == 0, "%d unexpected hits" % ghosts))

    # DENOMINATOR FLOOR: a stub/empty directory must not pass as "checked".
    rows.append(("FLOOR the directory holds >= 4000 named exports",
                 len(retail) >= 4000, str(len(retail))))

    ok = 0
    for name, passed, detail in rows:
        ok += bool(passed)
        print("  %-4s %s   %s" % ("PASS" if passed else "FAIL", name, detail))
    print("\n  %d/%d controls pass" % (ok, len(rows)))
    return 0 if ok == len(rows) and not named else (1 if ok != len(rows) else 2)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
