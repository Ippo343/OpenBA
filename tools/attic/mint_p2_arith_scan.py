#!/usr/bin/env python3
r"""PREMISE P2: is any vtable stamped by an address COMPUTED from a LEA'd base?

SUBJECT, before any finding
---------------------------
Lane EXTENT's premise P2, self-reported as NOT CHECKED and carried by the
integrator as the wave's residue:

    lea  rax,[rip+A]        # A is a table (or a table's base)
    add  rax,0x50           # ...arithmetic on the LEA'd register...
    mov  [rcx],rax          # ...and the STAMPED address is A+0x50, not A

Both the writer channel in `mint_extent_audit.py` and the grouping in
`mint_stamper_census.py` attribute such a stamp to A. If the shape exists, an
INTERIOR table could hide inside a row and neither channel could see it.

This tool asks ONLY that question, over the whole image: for every
rip-relative `lea` (and `movabs`) into a register, does an ARITHMETIC
instruction on that same register appear before the register is stored or
killed?  It reports every hit with its containing function, and it reports the
DENOMINATOR whether or not there are hits, so a zero is a measured zero.

WHAT COUNTS AS ARITHMETIC:  add / sub / or / and / xor / shl / shr / inc / dec
/ imul / neg  with the LEA'd register as DESTINATION, and a second
`lea reg,[reg+disp]` on it (the compiler's other way of writing `add`).

WINDOW.  The register is followed until it is STORED (a hit is only reported if
arithmetic came first), REDEFINED (`mov`/`lea` writing it from something else),
or `WINDOW` instructions pass.  Default 12 -- wider than the 3-instruction
window the writer channel uses, deliberately, because this scan is looking for
the case that window was too narrow to see.

CONTROLS, both of which must fire or the tool refuses to report
---------------------------------------------------------------
  POSITIVE  a SYNTHETIC hit is injected into the stream (`--control`) and must
            be reported, so a run printing "0 hits" is known to be a scan that
            CAN print one.  Without it, zero hits and a broken parser are the
            same output.
  DENOMINATOR the count of rip-relative LEAs tracked must be non-zero and is
            printed beside the verdict.

Exit 0 report produced, 2 control red, 3 hits found.

USAGE
  tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_p2_arith_scan.py [--dis FILE]
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

INSN = re.compile(r'^\s*([0-9a-f]{6,}):\t([0-9a-f ]+)\t(.*)$')
LEA_RIP = re.compile(r'^lea\s+.*\(%rip\).*#\s*0x([0-9a-f]+)\s*$')
MOVABS = re.compile(r'^movabs\s+\$0x([0-9a-f]+),%(\w+)$')
ARITH = re.compile(r'^(add|sub|or|and|xor|shl|shr|sar|inc|dec|imul|neg)\w*\s+')

# The synthetic positive control: three instruction texts spliced into the
# stream at a VA no real code occupies. It MUST come back as a hit.
CONTROL = [(0xDEAD00, "lea    0x1000(%rip),%rax        # 0x180155000"),
           (0xDEAD07, "add    $0x50,%rax"),
           (0xDEAD0b, "mov    %rax,(%rcx)")]


def dest_reg(text):
    """Destination register of a two-operand AT&T instruction, or None.

    THE COMMENT MUST BE STRIPPED FIRST. objdump renders a rip-relative LEA as
    `lea 0xb34a6(%rip),%rax        # 0x180154960`, so an anchored match on the
    RAW text finds no destination and every such LEA silently drops out of the
    scan. That is exactly what happened on this tool's first run: it reported
    `tracked 146` (the movabs forms only) and its own positive control read
    *NOT REPORTED*, which is the control doing the job it exists for.
    """
    m = re.search(r',%(\w+)$', text.split('#')[0].strip())
    return m.group(1) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", default=DEFAULT_DLL)
    ap.add_argument("--dis", default="", help="pre-made objdump -d output")
    ap.add_argument("--objdump", default=shutil.which("objdump") or "objdump")
    ap.add_argument("--window", type=int, default=12)
    ap.add_argument("--control", default="on", choices=["on", "off"])
    a = ap.parse_args()

    print("SUBJECT  premise P2 -- a vtable stamped by an address COMPUTED from a")
    print("         rip-relative LEA (lea A ; add imm ; store), which both the")
    print("         extent audit and the stamper census would attribute to A.")
    print("  dll  : %s" % a.dll)
    print("  tree : %s" % REPO)
    print("PARAMETERS  --window=%d  --control=%s  --dis=%s"
          % (a.window, a.control, a.dis or "(run objdump)"))

    if a.dis:
        text = open(a.dis, encoding="utf-8", errors="replace").read()
    else:
        out = subprocess.run([a.objdump, "-d", a.dll], capture_output=True,
                             text=True, errors="replace")
        if out.returncode != 0:
            print("objdump failed rc=%d" % out.returncode)
            return 2
        text = out.stdout

    pe = pefile.PE(a.dll)
    ib = pe.OPTIONAL_HEADER.ImageBase
    pe.parse_data_directories()
    exp_at = {}
    for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if s.address:
            exp_at.setdefault(ib + s.address, (s.name or b"").decode(errors="replace"))
    # ---- A .pdata RANGE IS A FRAGMENT, NOT A FUNCTION (Wave 21) ----------
    # MSVC chains function fragments through UNW_FLAG_CHAININFO, so a .pdata
    # entry bounds ONE FRAGMENT.  Measured in this DLL by Wave 21 lane CLASS:
    # 932 of 6045 entries (15.4%) carry that flag, covering 116,964 bytes.
    #
    # TWO CONSEQUENCES, and the second is why this note is here:
    #   * END - BEGIN IS A LOWER BOUND ON A FUNCTION'S SIZE, not its size.  Lane
    #     GFXBODY measured AnimationManager::Update at 50 bytes from one record;
    #     it is 430, and 3 of its 13 rows were understated, the worst by 8.6x.
    #   * ATTRIBUTION by raw range names the FRAGMENT's start, which for a
    #     chained fragment is NOT the owning function's entry point.
    #
    # A SIZE-UNDERSTATING blind spot is the opposite polarity from an inflating
    # one: it does not manufacture work, it makes work look CHEAP -- and cheap is
    # what gets picked up without scrutiny.  This tool does NOT follow the chain.
    # It is DECLARED here rather than fixed: correcting attribution would move
    # numbers other lanes are quoting, and lane GFXBODY's corrected method has
    # only 4 of its 13 extents Ghidra-confirmed.  Wave 22 input.
    pdata = sorted((ib + f.struct.BeginAddress, ib + f.struct.EndAddress)
                   for f in getattr(pe, "DIRECTORY_ENTRY_EXCEPTION", []))
    pd_begin = [p[0] for p in pdata]

    def owner(va):
        j = bisect.bisect_right(pd_begin, va) - 1
        if j >= 0 and pdata[j][0] <= va < pdata[j][1]:
            b = pdata[j][0]
            return exp_at.get(b) or ("FUN_%08x" % b)
        return "(no .pdata; leaf)"

    stream = []
    for line in text.splitlines():
        m = INSN.match(line)
        if m:
            stream.append((int(m.group(1), 16), m.group(3).strip()))
    if a.control == "on":
        stream.extend(CONTROL)

    n_insn = len(stream)
    tracked = 0
    hits = []
    live = []   # [(target, lea_va, reg, age, [arith texts])]
    for va, t in stream:
        # NOTE: `t` keeps its objdump comment, because LEA_RIP reads the TARGET
        # out of it. Only dest_reg() strips it -- stripping here made LEA_RIP
        # match nothing, the mirror image of the first bug and caught by the
        # same control.
        keep = []
        for tgt, lva, reg, age, ar in live:
            if age >= a.window:
                continue
            # a STORE of the register ends the window
            if t.startswith("mov    %" + reg + ",") and "(" in t.split(",", 1)[1]:
                if ar:
                    hits.append((lva, tgt, reg, list(ar), va, t))
                continue
            # arithmetic ON the register
            if ARITH.match(t) and dest_reg(t) == reg:
                ar.append((va, t))
                keep.append((tgt, lva, reg, age + 1, ar))
                continue
            # a second lea onto the same register from itself = add
            if t.startswith("lea") and dest_reg(t) == reg and ("%" + reg + ")") in t:
                ar.append((va, t))
                keep.append((tgt, lva, reg, age + 1, ar))
                continue
            # REDEFINED from something else -> stop tracking
            if dest_reg(t) == reg and (t.startswith("mov") or t.startswith("lea")
                                       or t.startswith("movabs")):
                continue
            keep.append((tgt, lva, reg, age + 1, ar))
        live = keep

        m = LEA_RIP.match(t)
        if m:
            r = dest_reg(t)
            if r:
                tracked += 1
                live.append((int(m.group(1), 16), va, r, 0, []))
            continue
        m = MOVABS.match(t)
        if m:
            tracked += 1
            live.append((int(m.group(1), 16), va, m.group(2), 0, []))

    print("\nDENOMINATORS")
    print("  instructions parsed ................ %d" % n_insn)
    print("  rip-relative LEA / movabs TRACKED .. %d" % tracked)
    print("  window (instructions) .............. %d" % a.window)

    ctl = [h for h in hits if h[0] == 0xDEAD00]
    if a.control == "on":
        print("  POSITIVE CONTROL synthetic lea;add;store .. %s"
              % ("REPORTED" if ctl else "*NOT REPORTED*"))
        if not ctl:
            print("REFUSING TO REPORT: the scan did not find a hit it was HANDED, "
                  "so a zero below would say nothing.")
            return 2
    real = [h for h in hits if h[0] != 0xDEAD00]

    print("\nHITS (arithmetic on a LEA'd register before it is stored): %d" % len(real))
    for lva, tgt, reg, ar, sva, st in real[:60]:
        print("  %#x lea -> %#x (%%%s)  in %s" % (lva, tgt, reg, owner(lva)))
        for ava, at in ar:
            print("      %#x  %s" % (ava, at))
        print("      %#x  %s   <- the store" % (sva, st))
    if len(real) > 60:
        print("  ... %d more" % (len(real) - 60))
    if not real:
        print("  NONE. Premise P2 has ZERO instances in this image, over the")
        print("  denominators above, with the positive control REPORTED.")
        print("  This does NOT cover a computed address whose arithmetic happens")
        print("  in a DIFFERENT register, or more than %d instructions from the"
              % a.window)
        print("  LEA, or across a basic-block boundary the linear scan reorders.")
    return 3 if real else 0


if __name__ == "__main__":
    sys.exit(main())
