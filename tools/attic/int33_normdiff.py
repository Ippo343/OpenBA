#!/usr/bin/env python3
"""Position-normalised body identity check for Runtime-x64.dll, Wave 26 lane INT33.

WHY THIS EXISTS
---------------
A RAW byte diff of two copies of ONE function reports ~11-23% different, because
every rel32 and every RIP displacement encodes a DISTANCE FROM THE INSTRUCTION
(reference_position_only_diffs_hide_one_body.md).  This decodes both bodies and,
at every operand naming an address, resolves it back to the ABSOLUTE address it
encodes before comparing.  Addresses INSIDE the body become body-relative (an
internal jump must land at the same offset); addresses OUTSIDE the body stay
absolute and must match exactly.

THE TRAP THIS TOOL IS BUILT TO NOT FALL INTO
--------------------------------------------
An ADDRESS-NORMALISED OPCODE HASH (Ghidra's, and Wave 23 used one) erases the
rel32 of a `CALL helper` -- which for the `~Array`-with-element-destroy family is
the ONLY thing that distinguishes one entry point from its siblings.  This tool
does the opposite: an out-of-body CALL target is compared ABSOLUTELY, so two J2
entries that call different helpers come back DIFFERENT, which is the answer that
keeps a stride-0x20 array from being walked at stride 0x10.

USAGE
    python tools/int33_normdiff.py 0x1800269a0 0x18002ac40 [nbytes]
    python tools/int33_normdiff.py --calls 0x1800269a0 [nbytes]
    python tools/int33_normdiff.py --selftest

CONTROLS (--selftest): every screen that can report an ABSENCE ("no differences")
is handed a POSITIVE case it must report as DIFFERENT, or "0 differences" is
indistinguishable from a dead decoder.
"""
import sys

import pefile
from capstone import CS_ARCH_X86, CS_MODE_64, Cs, CS_OP_IMM, CS_OP_MEM
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000

_pe = None


def _pefile():
    global _pe
    if _pe is None:
        _pe = pefile.PE(DLL, fast_load=True)
    return _pe


def read(va, n):
    rva = va - BASE
    for s in _pefile().sections:
        end = s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData)
        if s.VirtualAddress <= rva < end:
            return s.get_data(rva, n)
    raise SystemExit("address 0x%x is in no section" % va)


def _md():
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    return md


def extent(va, cap=4096):
    """Instruction-stream extent: decode until RET/JMP followed by int3 padding.

    NOT sized from .pdata -- a .pdata entry is a LOWER bound (MSVC chains
    fragments via UNW_FLAG_CHAININFO) and understating makes work look cheap.
    """
    data = read(va, cap)
    end = None
    for i in _md().disasm(data, va):
        if i.mnemonic in ("ret", "jmp"):
            nxt = i.address + i.size - va
            if nxt < len(data) and data[nxt] in (0xCC, 0x90):
                end = i.address + i.size
                break
            if i.mnemonic == "ret":
                end = i.address + i.size
                break
    if end is None:
        raise SystemExit("no terminator within %d bytes of 0x%x" % (cap, va))
    return end - va


def decode(va, n):
    """[(offset, mnemonic, normalised-operands, absolute-targets)]"""
    out = []
    for i in _md().disasm(read(va, n), va):
        if i.address - va >= n:
            break
        targets = []
        for op in i.operands:
            if op.type == CS_OP_IMM and (
                i.mnemonic.startswith("j") or i.mnemonic in ("call", "loop")
            ):
                targets.append(op.imm)
            elif op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                targets.append(i.address + i.size + op.mem.disp)
        norm = []
        for t in targets:
            if va <= t < va + n:
                norm.append("<body+0x%x>" % (t - va))
            else:
                norm.append("0x%x" % t)
        # operand text with every encoded distance stripped: keep the shape
        # (registers, immediates that are NOT addresses) and append the
        # resolved targets.
        txt = i.op_str
        if targets:
            txt = i.mnemonic + " [resolved:" + ",".join(norm) + "]"
        out.append((i.address - va, i.mnemonic, txt, tuple(norm)))
    return out


def calls(va, n=None):
    """Out-of-body CALL targets, in order -- the field a hash erases."""
    if n is None:
        n = extent(va)
    res = []
    for off, mn, txt, tg in decode(va, n):
        if mn == "call":
            for t in tg:
                if not t.startswith("<body"):
                    res.append((off, t))
    return res


def compare(a, b, n=None):
    if n is None:
        na, nb = extent(a), extent(b)
        if na != nb:
            return {"same_extent": False, "extent_a": na, "extent_b": nb,
                    "identical": False, "diffs": [("extent", na, nb)],
                    "raw_diff": None}
        n = na
    da, db = decode(a, n), decode(b, n)
    raw_a, raw_b = read(a, n), read(b, n)
    raw = sum(1 for x, y in zip(raw_a, raw_b) if x != y)
    diffs = []
    if len(da) != len(db):
        diffs.append(("insn_count", len(da), len(db)))
    for x, y in zip(da, db):
        if (x[0], x[1], x[2]) != (y[0], y[1], y[2]):
            diffs.append((x[0], "%s %s" % (x[1], x[2]), "%s %s" % (y[1], y[2])))
    return {"same_extent": True, "extent_a": n, "extent_b": n,
            "identical": not diffs, "diffs": diffs, "raw_diff": raw,
            "nbytes": n}


# ---------------------------------------------------------------------------
# CONTROLS
# ---------------------------------------------------------------------------
def selftest():
    """Each screen gets BOTH a case it must pass and a case it must fail.

    A screen that reports the ABSENCE of a difference has no natural positive
    case, so it is HANDED one: without the negative rows below, "identical" and
    "the decoder is dead" print the same thing.
    """
    rows = []

    def chk(name, cond, detail=""):
        rows.append((name, bool(cond), detail))

    # --- POSITIVE: two genuine copies of ONE body. Wave 23 proved by
    #     address-normalised hash that 0x18002f070 and 0x1800492d0 are the same
    #     255-byte Array<ExternalPtr<T>>::SetSize; unlike the J2 family they hold
    #     no out-of-body CALL that differs, so they are identical under THIS
    #     screen too.
    #
    #     NOTE FOR THE NEXT READER -- this control was first written against
    #     0x18002ac40 / 0x180039360 on the strength of Wave 23's "ONE body, hash
    #     089ee8", and it FAILED. The tool was right: those two call DIFFERENT
    #     helpers (0x18002afc0 vs 0x180039640) and merely happen to take the same
    #     DestroySpec. An address-normalised hash erases exactly that operand.
    #     The control was corrected; the claim it was written from was not
    #     re-checked by the two waves that repeated it.
    r = compare(0x18002F070, 0x1800492D0)
    chk("POS same body 0x18002f070 vs 0x1800492d0 -> IDENTICAL",
        r["identical"], "extent=%s raw_diff=%s" % (r.get("nbytes"), r["raw_diff"]))
    # and the raw diff must be NON-ZERO, or the tool is being flattered by two
    # byte-equal inputs and proves nothing about normalisation.
    chk("POS the raw byte diff is NON-ZERO (so normalisation did the work)",
        r["raw_diff"] and r["raw_diff"] > 0, "raw_diff=%s" % r["raw_diff"])

    # --- HANDED-IN NEGATIVE 1: the hard case, and the whole reason this tool is
    #     not a hash. All THREE of 0x18002ac40 / 0x180039360 / 0x180039280 share
    #     Wave 23's address-normalised hash 089ee8, and all three call a
    #     DIFFERENT helper. A screen that passed any of these pairs as
    #     "identical" would mean "in this neighbourhood", not "the same body".
    for a, b in ((0x180039360, 0x180039280), (0x18002AC40, 0x180039360),
                 (0x18002AC40, 0x180039280)):
        r2 = compare(a, b)
        chk("NEG hash-089ee8 sibling 0x%x vs 0x%x -> DIFFERENT" % (a, b),
            not r2["identical"], "diffs=%d" % len(r2["diffs"]))

    # --- HANDED-IN NEGATIVE 2: an unrelated body must not read as identical.
    r3 = compare(0x18002AC40, 0x1800042A0, n=64)
    chk("NEG unrelated body -> DIFFERENT", not r3["identical"],
        "diffs=%d" % len(r3["diffs"]))

    # --- EXTENT screen. This walker is an OVER-estimate, not an exact extent:
    #     it stops at the first RET/JMP followed by 0xCC/0x90 padding, so a body
    #     with an interior padded RET reads long (measured: 843 for a body Ghidra
    #     sizes at 818). It is used ONLY to (a) refuse to size from a .pdata
    #     fragment and (b) give compare() a common length; it is NOT quoted as a
    #     function size anywhere. The control asserts exactly that much.
    #     0x180078ad0's Ghidra body is 265 B and its .pdata ENTRY fragment is
    #     13 B -- the fragment whose chained flag reads False, which is the one
    #     that bites.
    e = extent(0x180078AD0, cap=8192)
    chk("EXTENT 0x180078ad0 is NOT the 13-byte .pdata entry fragment",
        e > 13, "extent=%d (over-estimate by design; Ghidra body 265)" % e)

    # --- CALLS screen: absence-shaped, so hand it a body with a known CALL.
    c = calls(0x1800269A0)
    chk("CALLS 0x1800269a0 names its element-destroy helper 0x180026b50",
        any(t == "0x180026b50" for _, t in c), repr(c))
    # NEGATIVE for the same screen: a body with no out-of-body call.
    chk("CALLS negative -- a leaf reports no out-of-body CALL",
        calls(0x180001000, n=8) == [], repr(calls(0x180001000, n=8)))

    ok = sum(1 for _, p, _ in rows if p)
    for name, passed, detail in rows:
        print("  %-4s %s   %s" % ("PASS" if passed else "FAIL", name, detail))
    print("\n%d/%d controls pass" % (ok, len(rows)))
    return 0 if ok == len(rows) else 1


def main(argv):
    if "--selftest" in argv:
        return selftest()
    if argv and argv[0] == "--map":
        for s in argv[1:]:
            va = int(s, 16)
            try:
                n = extent(va)
                print("0x%x  extent~%d" % (va, n))
                for off, t in calls(va, n):
                    print("        +0x%-4x CALL %s" % (off, t))
            except SystemExit as e:
                print("0x%x  <%s>" % (va, e))
        return 0
    if argv and argv[0] == "--calls":
        va = int(argv[1], 16)
        n = int(argv[2]) if len(argv) > 2 else None
        print("0x%x extent=%d bytes" % (va, n or extent(va)))
        for off, t in calls(va, n):
            print("  +0x%-4x CALL %s" % (off, t))
        return 0
    if len(argv) < 2:
        print(__doc__)
        return 2
    a, b = int(argv[0], 16), int(argv[1], 16)
    n = int(argv[2]) if len(argv) > 2 else None
    r = compare(a, b, n)
    print("0x%x vs 0x%x" % (a, b))
    print("  extent    %s / %s" % (r["extent_a"], r["extent_b"]))
    print("  raw byte diff   %s" % r["raw_diff"])
    print("  VERDICT   %s" % ("IDENTICAL" if r["identical"] else "DIFFERENT"))
    for d in r["diffs"][:40]:
        print("    %s" % (d,))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
