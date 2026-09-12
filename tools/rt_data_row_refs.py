#!/usr/bin/env python3
r"""In-DLL READ / WRITE / LEA census for an ARBITRARY set of exported DATA rows.

WAVE 25 LANE DATA.  This is ``tools/video_data_refs.py``'s decoder aimed at a
different, CALLER-SUPPLIED population: the 46 DATA gap rows the census prints.
It renders NO verdicts.  It answers exactly one question per row:

    of this row's references inside Runtime-x64.dll's .text, how many are
    READS, how many are WRITES, how many take its ADDRESS, and how many sit in
    a body this tree has NOT ported?

because promotion safety for a DATA row is governed by the UNPORTED READERS
(Wave 23 lane GFXDATA), not by whether some writer happens to be ported.

WHAT IT EXAMINES (rule: state your subject)
    ONE section, ``.text`` of the RETAIL ``Runtime-x64.dll``.  ``.rdata`` and
    ``.data`` are NOT scanned, so a row referenced only from another table or
    from a relocation entry reads 0 here and that 0 means "no CODE reference",
    never "no reference".  The profile DLL is not consulted (its offsets are
    known to disagree).

TWO DECODER TRAPS THIS TOOL IS BUILT TO AVOID -- both shipped as confident
under-counts in Wave 23:
  1. ``objdump`` emits AT&T syntax for this binary, so an Intel-syntax WRITE
     regex matches nothing and every site falls through to READ (wrong by 20x).
     This tool never parses disassembly TEXT: it classifies from capstone's
     decoded operand list.
  2. A reader classifier keyed on ``REX.W`` misses the non-REX
     ``MOV EAX,[rip]; RET`` form entirely and reports "0 readers".  Operand
     decoding is prefix-agnostic, so that form is classified like any other.

METHOD (identical to video_data_refs.py's, for comparability)
    route SCAN      candidate generation, decoder-independent and complete by
                    construction: for every byte offset p in .text and every
                    trailing-immediate size t in {0,1,2,4},
                    base+p+4+t+disp32(p) is a candidate target.  No
                    RIP-relative form can escape it; it over-generates, which
                    is the safe direction.  Built ONCE as an index, because
                    this tool has ~50 subjects rather than 18.
    route BACKWARD  confirmation: decode one instruction at every start in
                    [p-15, p-1] and keep the decode whose own RIP target is the
                    subject.
    route SWEEP     independent linear disassembly, used as the boundary map
                    that resolves BACKWARD's ambiguities.

THE UNCLASSIFIED BUCKET IS PRINTED, ALWAYS.  Wave 23 lane OBJ's totals would
have read "0 readers" for two rows and it was caught only by reading the
unclassified bucket.  A candidate no route can decode is emitted as
UNKNOWN-FORM and counted in its own column; it is never folded into READ.

CONTROLS
  control 1 (positive, and it is HANDED A KNOWN WRITE)
      the five known ``?_singleton@SaveLoad@Save@@`` references at their five
      known VAs, TWO of which are WRITEs.  video_data_refs.py checked only that
      the five ADDRESSES were found; this tool also checks the five KINDS,
      because the kind is the thing the verdicts in this lane are made of and a
      control that never exercises the WRITE arm cannot detect trap 1 above.
  control 2 (negative, quirk)  the same scan restricted to trailing-imm=0.  It
      MUST lose the imm32 reference, or the imm handling is decoration.
  control 3 (negative, WRITE arm)  re-classify control 1's two known WRITE
      sites with the destination-operand test DISABLED (the exact shape trap 1
      produces).  It MUST report them as READ -- that is what shows the WRITE
      arm is load-bearing rather than incidentally agreeing.
  control 4 (blind spot)  an arbitrary .rdata address with no code reference.
      It must report 0; a 0 elsewhere is only meaningful because this is 0 and
      controls 1-3 are green.

Effectiveness counters are printed for every control: how many comparisons each
one actually made, so a green control cannot hide an untested arm.

USAGE
    python tools/rt_data_row_refs.py --census <census.csv> [--symbols f.txt]
Needs capstone: run with the SYSTEM python (the project venv lacks capstone).
"""
import argparse
import bisect
import collections
import csv
import os
import struct
import sys

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000

CONTROL1_SLOT = 0x1801FC6F0  # ?_singleton@SaveLoad@Save@@0PEAV12@EA
CONTROL1 = {
    0x1800C3D9B: "WRITE", 0x1800C3CA6: "WRITE",
    0x1800DC3E9: "READ", 0x1800DCC1D: "READ", 0x180030D40: "READ",
}
CONTROL4_ADDR = 0x180165000  # middle of a .rdata string table


def load(path):
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    secs = []
    off = pe + 24 + optsz
    for i in range(nsec):
        s = data[off + i * 40: off + (i + 1) * 40]
        name = s[:8].rstrip(b"\0").decode()
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", s, 8)
        secs.append((name, vaddr, vsize, raddr, rsize))
    return data, secs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--census", required=True,
                    help="runtime_census.py --csv output, for port state + the row list")
    ap.add_argument("--symbols",
                    help="file with one mangled symbol per line; default = every "
                         "unported DATA row with callers in --census")
    ap.add_argument("--detail", action="store_true",
                    help="print every reference, not just the per-row totals")
    ap.add_argument("--width", type=lambda s: int(s, 0), default=1,
                    help="scan each subject across [base, base+width) instead of the "
                         "base byte alone.  THE DEFAULT OF 1 IS A BLIND SPOT FOR ANY "
                         "ROW WIDER THAN ITS FIRST FIELD -- see the WIDTH note below. "
                         "Use 0x38 for a BaseTypeInfo node.")
    args = ap.parse_args()

    try:
        import capstone
    except ImportError:
        print("capstone missing -- run with the SYSTEM python, not the venv")
        return 3
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from pe_exports import exports

    data, secs = load(DLL)
    tname, tva, tvs, tra, trs = [s for s in secs if s[0] == ".text"][0]
    tbase = BASE + tva
    blob = data[tra: tra + trs]

    ex = exports(DLL)
    byaddr = {}
    byname = {}
    for rva, n in ex:
        byaddr.setdefault(BASE + rva, []).append(n)
        byname[n] = BASE + rva
    eaddrs = sorted(byaddr)

    def owner(va):
        i = bisect.bisect_right(eaddrs, va) - 1
        return byaddr[eaddrs[i]][0] if i >= 0 else "?"

    ported, kinds_of, callers_of = {}, {}, {}
    with open(args.census, newline="") as f:
        for r in csv.DictReader(f):
            ported[r["key"]] = r["ported"] == "1"
            kinds_of[r["key"]] = r["symbol_kind"]
            callers_of[r["key"]] = r["callers"]

    if args.symbols:
        subjects = [l.strip() for l in open(args.symbols) if l.strip()]
    else:
        subjects = sorted(k for k, v in kinds_of.items()
                          if v == "DATA" and not ported.get(k) and callers_of.get(k))

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True

    # ---- route SWEEP: boundary map ----------------------------------------
    sweep_starts = set()
    off, n = 0, len(blob)
    while off < n:
        try:
            ins = next(md.disasm(blob[off:off + 16], tbase + off))
        except StopIteration:
            off += 1
            continue
        sweep_starts.add(off)
        off += ins.size

    # ---- route SCAN: ONE index for all subjects ---------------------------
    # (video_data_refs.py re-walked .text per subject; with ~50 subjects that is
    # ~400M python operations.  The index is the same enumeration, hoisted.)
    index = collections.defaultdict(list)
    index0 = collections.defaultdict(list)   # trailing-imm = 0 only, for control 2
    for p in range(0, n - 4):
        d = struct.unpack_from("<i", blob, p)[0]
        base_t = tbase + p + 4 + d
        index0[base_t].append(p)
        for t in (0, 1, 2, 4):
            index[base_t + t].append(p)

    ambiguous = [0]

    def classify(ins, capstone_mod, write_arm=True):
        mn = ins.mnemonic
        dst = ins.operands[0] if ins.operands else None
        dst_is_slot = (dst is not None
                       and dst.type == capstone_mod.x86.X86_OP_MEM
                       and dst.mem.base == capstone_mod.x86.X86_REG_RIP)
        if mn == "lea":
            return "LEA"
        if mn in ("cmp", "test"):
            return "READ"
        # A ONE-OPERAND instruction whose single operand is the slot is not
        # automatically a store.  `idiv/div/mul/imul m64` READ memory and write
        # only RAX/RDX; classifying them by "the destination operand is the
        # slot" called all twelve of ?_systemTicksPerSecond@Timer@System@@'s
        # readers RMW and made the row look like a read-modify-write hot spot
        # instead of what it is -- twelve READERS and one writer.
        if mn in ("idiv", "div", "mul", "imul", "push", "jmp", "call"):
            return "READ"
        if write_arm and dst_is_slot:
            return "WRITE" if mn in ("mov", "movq", "movsd", "movss") else "RMW"
        return "READ"

    def scan(target, imm0=False, write_arm=True):
        cands = sorted(set((index0 if imm0 else index).get(target, ())))
        out = []
        for p in cands:
            hits = []
            for s in range(max(0, p - 15), p):
                try:
                    ins = next(md.disasm(blob[s:s + 16], tbase + s))
                except StopIteration:
                    continue
                if s + ins.size < p + 4:
                    continue
                ok = False
                for op in ins.operands:
                    if (op.type == capstone.x86.X86_OP_MEM
                            and op.mem.base == capstone.x86.X86_REG_RIP
                            and tbase + s + ins.size + op.mem.disp == target):
                        ok = True
                if not ok:
                    continue
                hits.append((s, tbase + s, classify(ins, capstone, write_arm),
                             "%s %s" % (ins.mnemonic, ins.op_str)))
            if len(hits) > 1:
                on = [h for h in hits if h[0] in sweep_starts]
                if len(on) == 1:
                    hits = on
                else:
                    ambiguous[0] += 1
                    hits = [hits[-1]] if not on else [on[-1]]
            out.append(hits[0][1:] if hits
                       else (tbase + p, "UNKNOWN-FORM", "(no decode)"))
        return out

    print("SUBJECT EXAMINED: .text of %s" % DLL)
    print("  %s .. %s (%d bytes).  .rdata/.data NOT scanned." %
          (hex(tbase), hex(tbase + trs), trs))
    print("  subjects: %d rows, scanned across [base, base+0x%x)" %
          (len(subjects), args.width))
    if args.width == 1:
        print("  WIDTH NOTE -- THIS IS A REAL BLIND SPOT, FOUND BY HAND-CHECK, NOT A")
        print("  THEORETICAL ONE.  At width 1 only the row's FIRST BYTE is scanned, so a")
        print("  write to any LATER FIELD of a multi-field row is invisible and the row")
        print("  reads `W=0`.  Measured: all seven `?_typeInfo@` rows read W=0 at width 1")
        print("  and each has TWO in-DLL writes -- `parentName` (+0x10) and `next` (+0x30),")
        print("  both in the CRT static initialiser -- which appear only at --width 0x38.")
        print("  Width 1 is COMPLETE for a scalar row (a pointer, bool, double or qword)")
        print("  and INCOMPLETE for every aggregate.  Pass --width for aggregates.")
    print()

    # ------------------------- controls ------------------------------------
    print("=" * 78)
    print("CONTROLS")
    print("=" * 78)
    c1 = scan(CONTROL1_SLOT)
    got = {v: k for v, k, d in c1}
    addr_hits = len(set(CONTROL1) & set(got))
    kind_cmp = [(v, CONTROL1[v], got.get(v)) for v in CONTROL1 if v in got]
    kind_ok = [t for t in kind_cmp if t[1] == t[2]]
    print("control 1 (positive, SaveLoad ground truth, INCLUDES 2 KNOWN WRITES)")
    print("   addresses: %d/%d found            %s" %
          (addr_hits, len(CONTROL1), "GREEN" if addr_hits == len(CONTROL1) else "RED"))
    print("   kinds:     %d/%d agree             %s   [effectiveness: %d kind "
          "comparisons, %d of them on the WRITE arm]" %
          (len(kind_ok), len(kind_cmp), "GREEN" if len(kind_ok) == len(kind_cmp) else "RED",
           len(kind_cmp), sum(1 for t in kind_cmp if t[1] == "WRITE")))
    for v, want, gotk in kind_cmp:
        if want != gotk:
            print("      MISMATCH %s want=%s got=%s" % (hex(v), want, gotk))

    c2 = {v for v, k, d in scan(CONTROL1_SLOT, imm0=True)}
    lost = set(CONTROL1) - c2
    print("control 2 (negative, imm=0 only): lost %d of %d  %s   [effectiveness: "
          "%d addresses compared]" %
          (len(lost), len(CONTROL1),
           "GREEN (imm handling is load-bearing)" if lost else "RED (decoration)",
           len(CONTROL1)))

    c3 = scan(CONTROL1_SLOT, write_arm=False)
    c3k = {v: k for v, k, d in c3}
    writes = [v for v in CONTROL1 if CONTROL1[v] == "WRITE"]
    demoted = [v for v in writes if c3k.get(v) == "READ"]
    print("control 3 (negative, WRITE arm disabled = the exact shape trap 1 "
          "produces): %d/%d known WRITEs fell through to READ  %s   "
          "[effectiveness: %d WRITE sites compared]" %
          (len(demoted), len(writes),
            "GREEN (the WRITE arm is load-bearing)" if len(demoted) == len(writes)
            else "RED (the WRITE arm is not what produced the verdict)", len(writes)))

    c4 = scan(CONTROL4_ADDR)
    print("control 4 (blind spot, an arbitrary .rdata address): %d refs  %s" %
          (len(c4), "GREEN" if not c4 else "RED (parser is finding phantoms)"))
    print()

    # ------------------------- subjects ------------------------------------
    print("=" * 78)
    print("PER-ROW REFERENCE CENSUS")
    print("=" * 78)
    hdr = ("%-58s %-11s %5s %5s %5s %5s %5s %6s" %
           ("symbol", "VA", "tot", "R", "W", "RMW", "LEA", "UNKN"))
    print(hdr)
    print("-" * len(hdr))
    results = {}
    for sym in subjects:
        va = byname.get(sym)
        if va is None:
            print("%-58s  NOT AN EXPORT -- skipped" % sym[:58])
            continue
        refs = []
        for d in range(args.width):
            for rva, k, dis in scan(va + d):
                refs.append((rva, k, dis if d == 0 else "%s   [+0x%x]" % (dis, d)))
        refs.sort()
        c = collections.Counter(k for _, k, _ in refs)
        unported_readers = 0
        for rva, k, dis in refs:
            if k in ("READ", "RMW"):
                o = owner(rva)
                if not ported.get(o, False):
                    unported_readers += 1
        results[sym] = dict(va=va, total=len(refs), R=c["READ"], W=c["WRITE"],
                            RMW=c["RMW"], LEA=c["LEA"], UNK=c["UNKNOWN-FORM"],
                            unported_readers=unported_readers, refs=refs)
        print("%-58s %-11s %5d %5d %5d %5d %5d %6d" %
              (sym[:58], hex(va), len(refs), c["READ"], c["WRITE"], c["RMW"],
               c["LEA"], c["UNKNOWN-FORM"]))
        if args.detail:
            for rva, k, dis in refs:
                o = owner(rva)
                print("      %s %-12s %-52s %-10s %s" %
                      (hex(rva), k, o[:52],
                       "PORTED" if ported.get(o) else "UNPORTED", dis))
    print()
    tot_unk = sum(r["UNK"] for r in results.values())
    print("UNCLASSIFIED BUCKET (printed always, per rule): %d UNKNOWN-FORM "
          "candidates across %d rows.  A row whose READ count looks low must be "
          "checked against ITS OWN UNKN column before that low count is believed."
          % (tot_unk, len(results)))
    print("ambiguous backward decodes resolved against route SWEEP: %d" % ambiguous[0])
    print()
    print("UNPORTED-READER column (the governing quantity, Wave 23 GFXDATA):")
    print("  owner = nearest preceding export, which for an unexported internal "
          "OVER-attributes to that export.  That bias is toward calling a reader "
          "PORTED when it is not, i.e. toward FALSE SAFE -- read it accordingly.")
    for sym, r in sorted(results.items(),
                         key=lambda kv: -kv[1]["unported_readers"]):
        print("  %-58s unported readers (R+RMW): %d of %d" %
              (sym[:58], r["unported_readers"], r["R"] + r["RMW"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
