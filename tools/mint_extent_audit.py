#!/usr/bin/env python3
r"""Audit every minted vtable row for the OVER-SIZING defect (Wave 19; extended
in Wave 20 Phase 0d by lane EXTENT to RE-DERIVE each row from its WRITER).

WHAT THE DEFECT IS

`tools/rt_mint_gen.py` sizes a vtable by `adjacent-export`: "the last slot ends
exactly where the next EXPORTED symbol starts". That is an UPPER bound, read as
exact. Unexported vtables (every concrete IO stream ctor heap-allocates an
8-byte IO::Encoding and stamps one) live in .rdata interleaved with the exported
class tables, so the row before one SWALLOWS it. Two rows did, for the whole
life of the generator: ??_7ReaderWriter@ (36 -> 26, swallowing TWO tables) and
??_7TextFileStream@ (20 -> 10).

THE ASYMMETRY THAT SETS THE ERROR DIRECTION

An OVER-sized row is a visible surplus (extra trap slots nothing dispatches).
An UNDER-sized row loses slots with NO SYMPTOM. So this tool never edits an
extent, and never converts "I found no interior stamp" into "the count is
exact". Those are different sentences and it prints them differently.

WHY A SCREEN IS NEEDED AT ALL -- no existing instrument can see this.
BANISHED_RT_MINT_SHADOW compares each minted slot against the DLL AT THE SLOT'S
OWN ADDRESS, so an over-sized row's surplus slots are read from the DLL at their
real addresses and compare EQUAL. "598 slot comparisons, 0 MISMATCH" was true
and silent. A screen can be right on every case it examines and still be blind
to whether it examined the right SET.

THIS DLL HAS NO RTTI, so the usual exact enumeration is unavailable.
In an MSVC /GR build every vtable is preceded by a pointer to its ??_R4
Complete Object Locator, which enumerates ALL vtable starts -- exported or not,
with no reference to any writer. Checked on this DLL and it is NOT so: the qword
before ??_7ReaderWriter@ (0x1801553a8) is 0x1800a8950, a .text address -- the
PREVIOUS table's last slot. Five row starts checked, five for five .text. That
is why the writer is the only oracle available here, and why this tool is a
writer-side instrument.

THE SIGNAL

A constructor NAMES the address it stamps. The canonical leaf ctor for a class
with no members is four instructions:

    LEA  RAX,[rip+disp32]     ; 48 8d 05 xx xx xx xx   the vtable
    MOV  [RCX],RAX            ; 48 89 01               vptr = &vtable
    MOV  RAX,RCX              ; 48 8b c1               return this
    RET                       ; c3

TWO CHANNELS, and they are not equally strong -- the tool keeps them apart:

  DISASM  objdump -d over the whole image, parsed for `lea 0x..(%rip),%reg`
          and `movabs $0x..,%reg`. Instruction-boundary correct, so a hit is a
          real instruction, not bytes inside one. THIS IS THE CHANNEL THE
          VERDICTS ARE COMPUTED FROM.
  BYTES   the original Wave 19 linear byte scan, plus absolute qwords in
          .rdata/.data. Strictly WIDER (it can see through a region objdump
          mis-syncs) and strictly WEAKER (a hit may be bytes inside an
          instruction). Reported as WEAK-ONLY candidates, never as a verdict.

Per row, using the DISASM channel, in descending strength:

  * OVER-SIZED-BY-N          an attested table start lies STRICTLY INSIDE the
                             row's extent. The true extent is at most the
                             distance to it.
  * CORROBORATED-EXACT       the row's own start is writer-attested AND the
                             NEXT address any instruction names is exactly the
                             row's end. The count no longer rests on
                             adjacent-export: two methods that CAN disagree
                             (they did, on ??_7ReaderWriter@) agree here.
                             Where the row's end is also the next exported
                             symbol this says the thing actually at issue --
                             NO UNEXPORTED TABLE STARTS IN BETWEEN.
  * CONFIRMED-NO-INTERIOR-STAMP
                             start attested, nothing named inside, but the
                             next named address is PAST the row's end -- so the
                             writer channel only bounds the row from above and
                             the printed count still rests on its original
                             proof, which the line names.
  * COULD-NOT-DERIVE         the row's own start is named by no instruction in
                             the image. The scan did not reach this row's
                             writer, so it MEASURED NOTHING about that row and
                             says so rather than folding it into a clean total.

WHAT THIS SCREEN CANNOT DO, stated before its output is read

* It NEVER edits an extent. A bound that DELETES slots must be hand-checked into
  rt_mint_gen.py's VTABLE_EXTENT with its ctor site.
* CONFIRMED-NO-INTERIOR-STAMP IS NOT "EXACT". A row it clears may still be
  over-sized by a table NOTHING names in either channel -- a computed or
  indirect stamp, or a table only ever reached by copying an existing vptr.
  Silence here is not proof. The verdict is strictly stronger than
  `adjacent-export` and strictly weaker than a hand-read constructor.
* Its writer attribution is `.pdata`-based. 6045 of the DLL's ~15290 functions
  have a `.pdata` entry; the frameless leaf ctors that stamp vtables often do
  NOT, and for those the tool prints `(no .pdata)` plus the nearest preceding
  export as a HINT explicitly labelled as one. It does not silently name a
  function it cannot bound.

POSITIVE CONTROL -- and it reads NON-ZERO

`--control` re-runs the row scan with the two rows Wave 19 corrected restored to
their pre-correction (wrong, adjacent-export) sizes: ??_7ReaderWriter@ 26->36 and
??_7TextFileStream@ 10->20. The screen must report BOTH as OVER-SIZED. A run
reporting fewer than 2 of 2 is a screen not known to catch the defect it exists
for, and the tool says so and exits non-zero.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_extent_audit.py
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_extent_audit.py --control
Exit 0 clean, 2 controls red / row regex empty, 3 candidates found.
"""
import argparse
import bisect
import os
import re
import shutil
import subprocess
import sys

import pefile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
DEFAULT_INC = os.path.join(REPO, "standalone", "rt_data_mint_tables.inc")

ROW = re.compile(r'^\s*\{ (?:"([^"]*)"|nullptr), (0x[0-9a-f]+)ull, (\d+), kVSlots'
                 r'\d+, kVTable\d+, kVTraps\d+, (".*?[^\\]") \},\s*$', re.M | re.S)

INSN = re.compile(r'^\s*([0-9a-f]{6,}):\t([0-9a-f ]+)\t(.*)$')
LEA = re.compile(r'^(?:lea|movabs)\s')
TARGET = re.compile(r'#\s*0x([0-9a-f]+)\s*$')
MOVABS = re.compile(r'^movabs\s+\$0x([0-9a-f]+),')

# Three KNOWN real stamped tables. They are the positive control on the DECODER:
# the scan must find all three before any clean verdict below is worth anything.
# They are inside no row once VTABLE_EXTENT is applied, so they are checked
# against the TARGET SET, not against the findings.
CONTROLS = {
    0x180155480: "the all-purecall table, leaf ctor 0x180023e10",
    0x1801554a8: "the IO::Encoding EXTRA_VTABLES_BY_ADDR mints, leaf ctor 0x180023e00",
    0x1801559d0: "the TextFileStream Encoding, stamped by ??0TextFileStream 0x1800ba431",
}

# The positive control on the ROW SCAN: the two rows Wave 19 corrected, restored
# to the size the adjacent-export proof gave them. --control must catch both.
REGRESSION_ROWS = {
    0x1801553b0: (36, 26, "??_7ReaderWriter@IO@@6B@"),
    0x180155980: (20, 10, "??_7TextFileStream@IO@@6B@"),
}


def disasm_channel(dll, objdump):
    """objdump -d over the image -> {target_va: [(site_va, next_insn_text)]}.

    Instruction-boundary correct. Returns (targets, n_lea, n_movabs, n_insn).
    """
    out = subprocess.run([objdump, "-d", dll], capture_output=True, text=True,
                         errors="replace")
    if out.returncode != 0:
        raise SystemExit("objdump failed rc=%d: %s" % (out.returncode,
                                                       out.stderr[:400]))
    tgt, n_lea, n_mov, n_insn = {}, 0, 0, 0
    # A stamp is `LEA reg,[rip+vtable]` ... `MOV %reg,(%base)`, and MSVC
    # INTERLEAVES up to a couple of unrelated instructions between the two (seen
    # at 0x180054fd6: lea; mov %rcx,%rbx; mov %rax,(%rcx)). So look at the next
    # THREE instructions and report the first STORE of the LEA'd register --
    # reporting only the literal next instruction made real ctors read as
    # `mov %rcx,%rbx` and look like non-stamps.
    pend = []                        # [(target, site, reg, [following insns])]
    for line in out.stdout.splitlines():
        m = INSN.match(line)
        if not m:
            continue
        n_insn += 1
        site, text = int(m.group(1), 16), m.group(3).strip()
        keep = []
        for tv, sv, reg, foll in pend:
            foll.append(text)
            if len(foll) >= 3:
                store = next((f for f in foll
                              if f.startswith("mov    %" + reg + ",(")), None)
                tgt.setdefault(tv, []).append((sv, store or foll[0]))
            else:
                keep.append((tv, sv, reg, foll))
        pend = keep
        if not LEA.match(text):
            continue
        reg = text.rsplit(",%", 1)[-1].split()[0] if ",%" in text else "?"
        mm = MOVABS.match(text)
        if mm:
            n_mov += 1
            pend.append((int(mm.group(1), 16), site, reg, []))
            continue
        if "(%rip)" in text:
            mt = TARGET.search(text)
            if mt:
                n_lea += 1
                pend.append((int(mt.group(1), 16), site, reg, []))
    for tv, sv, reg, foll in pend:
        tgt.setdefault(tv, []).append((sv, (foll or ["(end of section)"])[0]))
    return tgt, n_lea, n_mov, n_insn


def byte_channel(pe, ib, isize, text, sections):
    """The Wave 19 linear byte scan, widened. Strictly wider, strictly weaker."""
    blob = pe.get_data(text[0], text[1] - text[0])
    tgt = {}
    i, L = 0, len(blob)
    while i < L - 10:
        b0 = blob[i]
        if b0 in (0x48, 0x49, 0x4C, 0x4D):
            if blob[i + 1] == 0x8D and (blob[i + 2] & 0xC7) == 0x05:
                disp = int.from_bytes(blob[i + 3:i + 7], "little", signed=True)
                site = text[0] + i
                t = site + 7 + disp
                if 0 <= t < isize:
                    tgt.setdefault(ib + t, []).append(ib + site)
                i += 7
                continue
            if 0xB8 <= blob[i + 1] <= 0xBF:
                imm = int.from_bytes(blob[i + 2:i + 10], "little")
                if ib <= imm < ib + isize:
                    tgt.setdefault(imm, []).append(ib + text[0] + i)
                i += 10
                continue
        i += 1
    rdata = next((lo, hi) for n, lo, hi in sections if n == ".rdata")
    for name, lo, hi in sections:
        if name not in (".rdata", ".data"):
            continue
        try:
            d = pe.get_data(lo, hi - lo)
        except Exception:
            continue
        for off in range(0, len(d) - 8, 8):
            q = int.from_bytes(d[off:off + 8], "little")
            if ib <= q < ib + isize and rdata[0] <= q - ib < rdata[1]:
                tgt.setdefault(q, []).append(ib + lo + off)
    return tgt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", default=DEFAULT_DLL)
    ap.add_argument("--inc", default=DEFAULT_INC)
    ap.add_argument("--objdump", default=shutil.which("objdump") or "objdump")
    ap.add_argument("--control", action="store_true",
                    help="POSITIVE CONTROL: restore the two Wave 19 rows to "
                         "their wrong pre-correction sizes and require the row "
                         "scan to catch BOTH")
    a = ap.parse_args()

    pe = pefile.PE(a.dll)
    ib = pe.OPTIONAL_HEADER.ImageBase
    isize = pe.OPTIONAL_HEADER.SizeOfImage
    sections = [(s.Name.decode().rstrip("\0"), s.VirtualAddress,
                 s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData))
                for s in pe.sections]
    text = next((lo, hi) for n, lo, hi in sections if n == ".text")

    print("SUBJECT   inc : %s" % a.inc)
    print("          dll : %s" % a.dll)
    print("TREE ROOT     : %s" % REPO)
    print("PARAMETERS    : --dll=%s" % a.dll)
    print("                --inc=%s" % a.inc)
    print("                --objdump=%s   --control=%s"
          % (a.objdump, "ON" if a.control else "off"))

    # ---- writer attribution: .pdata bounds, exports for names --------------
    pe.parse_data_directories()
    exps = sorted((s.address, (s.name or b"").decode(errors="replace"))
                  for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.address)
    exp_rvas = [e[0] for e in exps]
    exp_at = dict(exps)
    pdata = sorted((f.struct.BeginAddress, f.struct.EndAddress)
                   for f in getattr(pe, "DIRECTORY_ENTRY_EXCEPTION", []))
    pd_begin = [p[0] for p in pdata]

    def owner(va):
        rva = va - ib
        j = bisect.bisect_right(pd_begin, rva) - 1
        if j >= 0 and pdata[j][0] <= rva < pdata[j][1]:
            b = pdata[j][0]
            nm = exp_at.get(b)
            return ("%s+%#x" % (nm, rva - b)) if nm else "FUN_%08x+%#x" % (
                ib + b, rva - b)
        k = bisect.bisect_right(exp_rvas, rva) - 1
        hint = exps[k][1] if k >= 0 else "?"
        return "(no .pdata; leaf) HINT nearest export %s" % hint

    dis, n_lea, n_mov, n_insn = disasm_channel(a.dll, a.objdump)
    byt = byte_channel(pe, ib, isize, text, sections)

    print("\nSTAMP-TARGET DECODE (denominators)")
    print("  DISASM  objdump instructions parsed .......... %7d" % n_insn)
    print("          of which lea reg,[rip+d32] ........... %7d" % n_lea)
    print("          of which movabs reg,imm64 ............ %7d" % n_mov)
    print("          -> distinct addresses NAMED .......... %7d" % len(dis))
    print("  BYTES   linear scan + data qwords, distinct .. %7d" % len(byt))
    print("          named by BYTES but not by DISASM ..... %7d  (weak-only)"
          % len(set(byt) - set(dis)))
    print("  .pdata function ranges for attribution ....... %7d" % len(pdata))

    red = 0
    for va, why in CONTROLS.items():
        ok = va in dis
        print("CONTROL %#x %-9s %s" % (va, "FOUND" if ok else "*MISSING*", why))
        red += 0 if ok else 1
    print("DECODER CONTROL: %d of %d found" % (len(CONTROLS) - red, len(CONTROLS)))
    if red:
        print("\n%d of %d controls RED -- the scan is not seeing known stamps, so "
              "every clean line below would be an artefact. Refusing to report."
              % (red, len(CONTROLS)))
        return 2

    src = open(a.inc, encoding="utf-8").read()
    rows = [(m.group(1) or "(by address)", int(m.group(2), 16), int(m.group(3)),
             m.group(4)) for m in ROW.finditer(src)]
    if not rows:
        print("\nROW REGEX MATCHED NOTHING -- refusing to report a clean scan of "
              "an empty set.")
        return 2
    adjvas = {r[1] for r in rows if r[3].startswith('"adjacent-export:')}
    print("\nROW SET: %d vtable rows in %s" % (len(rows), os.path.basename(a.inc)))
    print("  carrying an UNCORRECTED adjacent-export proof (the shape at risk) : %d"
          % len(adjvas))
    print("  already corrected to a ctor-stamped-neighbour proof ..............: %d"
          % sum(1 for r in rows if r[3].startswith('"ctor-stamped-neighbour')))
    print("  sized by tail-not-a-pointer .....................................: %d"
          % sum(1 for r in rows if r[3].startswith('"tail-not-a-pointer')))

    if a.control:
        rows = [(n, va, REGRESSION_ROWS[va][0], p)
                if va in REGRESSION_ROWS and c == REGRESSION_ROWS[va][1]
                else (n, va, c, p) for n, va, c, p in rows]
        print("\nCONTROL MODE: %d of %d regression rows restored to their wrong "
              "pre-correction sizes"
              % (sum(1 for _, va, c, _ in rows
                     if va in REGRESSION_ROWS and c == REGRESSION_ROWS[va][0]),
                 len(REGRESSION_ROWS)))

    keys = sorted(dis)
    bkeys = sorted(byt)
    over, conf, corrob, undrv, weak = [], [], [], [], []
    print("\n%-42s %5s %5s %3s  VERDICT / writer" % ("ROW", "SLOT", "DERIV", "adj"))
    print("-" * 108)
    for name, va, n, proof in sorted(rows, key=lambda r: r[1]):
        hi = va + 8 * n
        inside = [t for t in keys[bisect.bisect_right(keys, va):] if t < hi]
        binside = [t for t in bkeys[bisect.bisect_right(bkeys, va):]
                   if t < hi and t not in dis]
        writers = dis.get(va, [])
        tag = "adj" if va in adjvas else "  ."
        if inside:
            t = min(inside)
            newn = (t - va) // 8
            over.append((name, va, n, newn, t))
            print("%-42s %5d %5d %3s  OVER-SIZED-BY-%d  interior stamped start %#x"
                  % (name[:42], n, newn, tag, n - newn, t))
            for site, nxt in dis[t][:4]:
                print("      LEA at %#x  next insn `%s`%s   [%s]"
                      % (site, nxt,
                         "  <- the ctor stamp shape" if nxt.startswith("mov    %rax,(")
                         else "", owner(site)))
        elif writers:
            site, nxt = writers[0]
            # Does the WRITER channel pin the row's END as well as its start?
            # The next address any instruction names, after this row's start, is
            # an upper bound on the extent that owes NOTHING to adjacent-export.
            j = bisect.bisect_right(keys, va)
            nxt_named = keys[j] if j < len(keys) else None
            ub = ((nxt_named - va) // 8) if nxt_named else None
            if ub == n:
                corrob.append((name, va, n))
                verdict = ("CORROBORATED-EXACT  next named table start is %#x = "
                           "exactly this row's end" % nxt_named)
            else:
                conf.append((name, va, n))
                kind = proof.strip('"').split(":")[0]
                verdict = ("CONFIRMED-NO-INTERIOR-STAMP  (writer channel bounds it "
                           "at <=%s slots only; the %d still rests on %s)"
                           % (ub if ub is not None else "inf", n, kind))
            print("%-42s %5d %5d %3s  %s"
                  % (name[:42], n, n, tag, verdict))
            print("      %d writer site(s), first %#x `%s` [%s]"
                  % (len(writers), site, nxt, owner(site)))
            if binside:
                weak.append((name, va, binside))
                print("      WEAK-ONLY: BYTES names %#x inside this row and DISASM "
                      "does not -- settle by hand before trusting %d slots"
                      % (min(binside), n))
        else:
            undrv.append((name, va, n, tag))
            print("%-42s %5d %5s %3s  COULD-NOT-DERIVE  no instruction in the image "
                  "names %#x -- MEASURED NOTHING about this row"
                  % (name[:42], n, "?", tag, va))
            if byt.get(va):
                print("      (BYTES names it at %#x, DISASM does not: weak evidence "
                      "only)" % byt[va][0])

    print("-" * 108)
    N = len(rows)
    print("TOTALS over %d rows: %d OVER-SIZED, %d CORROBORATED-EXACT, "
          "%d CONFIRMED-NO-INTERIOR-STAMP, %d COULD-NOT-DERIVE, "
          "%d carrying a WEAK-ONLY interior hit"
          % (N, len(over), len(corrob), len(conf), len(undrv), len(weak)))
    print("  of the %d UNCORRECTED adjacent-export rows: %d OVER-SIZED, "
          "%d corroborated-exact, %d confirmed-no-interior-stamp, "
          "%d could-not-derive"
          % (len(adjvas),
             sum(1 for o in over if o[1] in adjvas),
             sum(1 for c in corrob if c[1] in adjvas),
             sum(1 for c in conf if c[1] in adjvas),
             sum(1 for u in undrv if u[1] in adjvas)))
    print("  COMPLETENESS of the writer channel, measured on the only table-start "
          "set we independently know: %d of %d row starts are named by a real "
          "instruction. The 'invisible stamp' failure mode has %d instance(s) "
          "among them." % (N - len(undrv), N, len(undrv)))
    print("  CONFIRMED-NO-INTERIOR-STAMP IS NOT 'EXACT': a table no instruction in "
          "the image names is invisible to this screen.")

    if a.control:
        caught = sum(1 for o in over if o[1] in REGRESSION_ROWS)
        print("\nPOSITIVE CONTROL: %d of %d known-wrong rows CAUGHT"
              % (caught, len(REGRESSION_ROWS)))
        for va, (wrong, right, nm) in REGRESSION_ROWS.items():
            hit = [o for o in over if o[1] == va]
            print("   %-34s %d -> true %d, screen derived %s"
                  % (nm, wrong, right, hit[0][3] if hit else "*NOT CAUGHT*"))
        if caught != len(REGRESSION_ROWS):
            print("CONTROL RED -- this screen is not known to catch the defect it "
                  "exists for. Every clean line above is unsupported.")
            return 2
        return 0

    return 3 if over else 0


if __name__ == "__main__":
    sys.exit(main())
