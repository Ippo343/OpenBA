#!/usr/bin/env python3
r"""Audit every minted vtable row for the OVER-SIZING defect (Wave 19).

WHAT THE DEFECT IS

`tools/rt_mint_gen.py` sizes a vtable by `adjacent-export`: "the last slot ends
exactly where the next EXPORTED symbol starts". That is an UPPER bound, read as
exact. Unexported vtables (every concrete IO stream ctor heap-allocates an
8-byte IO::Encoding and stamps one) live in .rdata interleaved with the exported
class tables, so the row before one SWALLOWS it. Two rows did, for the whole
life of the generator: ??_7ReaderWriter@ (36 -> 26, swallowing TWO tables) and
??_7TextFileStream@ (20 -> 10).

WHY A SCREEN IS NEEDED AT ALL -- no existing instrument can see this.
BANISHED_RT_MINT_SHADOW compares each minted slot against the DLL AT THE SLOT'S
OWN ADDRESS, so an over-sized row's surplus slots are read from the DLL at their
real addresses and compare EQUAL. "598 slot comparisons, 0 MISMATCH" was true
and silent. A screen can be right on every case it examines and still be blind
to whether it examined the right SET.

THE SIGNAL

A constructor NAMES the address it stamps. The canonical leaf ctor for a class
with no members is four instructions:

    LEA  RAX,[rip+disp32]     ; 48 8d 05 xx xx xx xx   the vtable
    MOV  [RCX],RAX            ; 48 89 01               vptr = &vtable
    MOV  RAX,RCX              ; 48 8b c1               return this
    RET                       ; c3

So: decode every RIP-relative LEA in .text, and report any whose target lands
STRICTLY INSIDE a minted row's extent. Such a target is another table's start
and the row is over-sized.

WHAT THIS SCREEN CANNOT DO, stated before its output is read

* It is a LINEAR BYTE SCAN, not a disassembler. It cannot distinguish an
  instruction from a byte sequence inside one, so a hit is a CANDIDATE. Each is
  printed with its alignment evidence (preceded by ret/int3?) and the three
  bytes after the LEA, so a human can settle it -- which is exactly how both
  real hits were settled. On this DLL it has so far scored 2 of 2 real; lane
  MINT wrongly dismissed one of those two by hand, so the scan has a better
  record here than the hand-check that overruled it.
* It NEVER edits an extent. A bound that DELETES slots must be hand-checked into
  rt_mint_gen.py's VTABLE_EXTENT with its ctor site, because an over-sized row is
  a visible surplus while an under-sized row loses slots with NO symptom.
* A row it clears may still be over-sized by a table NOTHING stamps with a
  plain LEA (a computed or indirect stamp). Silence here is not proof.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_extent_audit.py
Exit 0 clean, 3 candidates found, 2 controls red.
"""
import argparse
import bisect
import os
import re
import sys

import pefile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
DEFAULT_INC = os.path.join(REPO, "standalone", "rt_data_mint_tables.inc")

ROW = re.compile(r'^\s*\{ (?:"([^"]*)"|nullptr), (0x[0-9a-f]+)ull, (\d+), kVSlots', re.M)

# Both KNOWN real stamped tables. They are the positive control: this scan must
# find both before any clean verdict below is worth anything. They are inside no
# row once VTABLE_EXTENT is applied, so they are checked against the TARGET SET,
# not against the findings.
CONTROLS = {
    0x180155480: "the all-purecall table, leaf ctor 0x180023e10",
    0x1801554a8: "the IO::Encoding EXTRA_VTABLES_BY_ADDR mints, leaf ctor 0x180023e00",
    0x1801559d0: "the TextFileStream Encoding, stamped by ??0TextFileStream 0x1800ba431",
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", default=DEFAULT_DLL)
    ap.add_argument("--inc", default=DEFAULT_INC)
    a = ap.parse_args()

    pe = pefile.PE(a.dll)
    ib = pe.OPTIONAL_HEADER.ImageBase
    isize = pe.OPTIONAL_HEADER.SizeOfImage
    text = rdata = None
    for s in pe.sections:
        n = s.Name.decode().rstrip("\0")
        span = (s.VirtualAddress, s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData))
        if n == ".text":
            text = span
        elif n == ".rdata":
            rdata = span
    blob = pe.get_data(text[0], text[1] - text[0])

    # ---- every RIP-relative LEA target, with its evidence -------------------
    tgt = {}                       # target rva -> list of (site va, aligned, tail)
    n_lea = 0
    i, L = 0, len(blob)
    while i < L - 7:
        if (blob[i] in (0x48, 0x4C) and blob[i + 1] == 0x8D
                and (blob[i + 2] & 0xC7) == 0x05):
            disp = int.from_bytes(blob[i + 3:i + 7], "little", signed=True)
            site = text[0] + i
            t = site + 7 + disp
            n_lea += 1
            if 0 <= t < isize:
                # is this LEA at a clean function boundary? (ret/int3 before)
                prev = blob[i - 1] if i else 0
                aligned = prev in (0xC3, 0xCC)
                tail = blob[i + 7:i + 10].hex()
                tgt.setdefault(t, []).append((ib + site, aligned, tail))
            i += 7
            continue
        i += 1

    print("SUBJECT: %s" % a.inc)
    print("         %s" % a.dll)
    print("RIP-relative LEAs decoded in .text: %d -> %d distinct in-image targets, "
          "%d in .rdata"
          % (n_lea, len(tgt), sum(1 for t in tgt if rdata[0] <= t < rdata[1])))

    # ---- positive controls, printed BEFORE any verdict ---------------------
    red = 0
    for va, why in CONTROLS.items():
        ok = (va - ib) in tgt
        print("CONTROL %#x %-8s  %s" % (va, "FOUND" if ok else "*MISSING*", why))
        if not ok:
            red += 1
    if red:
        print("\n%d of %d controls RED -- the scan is not seeing known stamps, so "
              "every clean line below would be an artefact. Refusing to report."
              % (red, len(CONTROLS)))
        return 2

    rows = [(m.group(1) or "(by address)", int(m.group(2), 16), int(m.group(3)))
            for m in ROW.finditer(open(a.inc, encoding="utf-8").read())]
    adj = sum(1 for _ in re.finditer(r'"adjacent-export:', open(a.inc, encoding="utf-8").read()))
    print("\nvtable rows examined: %d  (of which %d still carry an adjacent-export "
          "proof, the shape at risk)" % (len(rows), adj))

    keys = sorted(tgt)
    hits = 0
    for name, va, n in rows:
        lo, hi = va - ib, va - ib + 8 * n
        j = bisect.bisect_right(keys, lo)
        for t in keys[j:]:
            if t >= hi:
                break
            hits += 1
            print("\nCANDIDATE OVER-SIZE  %s" % name)
            print("   row %#x, %d slots, extent %#x..%#x" % (va, n, va, ib + hi))
            print("   stamped address inside it: %#x  -> true extent at most %d slots"
                  % (ib + t, (t - lo) // 8))
            for site, aligned, tail in tgt[t][:4]:
                print("      LEA at %#x  %s  next bytes %s%s"
                      % (site,
                         "ret/int3-aligned (function START)" if aligned
                         else "NOT aligned -- could be bytes inside another instruction",
                         tail,
                         "  <- 48 89 01 = MOV [RCX],RAX, the ctor stamp shape"
                         if tail == "488901" else ""))
    if not hits:
        print("\nNo minted row contains a stamped address. NOT a proof that no row "
              "is over-sized: a table stamped other than by a plain RIP-relative "
              "LEA is invisible here (see the docstring).")
    else:
        print("\n%d candidate(s). Each is a CANDIDATE, not a verdict: settle it from "
              "the bytes, then record it in rt_mint_gen.py's VTABLE_EXTENT with the "
              "constructor site. This tool never edits an extent." % hits)
    return 3 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
