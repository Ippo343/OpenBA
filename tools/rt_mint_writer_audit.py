#!/usr/bin/env python3
r"""Who WRITES each minted storage symbol after cutover? (Wave 9 lane C2)

THE BUG CLASS THIS EXISTS TO ENUMERATE

The mint gives every storage symbol zeroed bytes it owns, and registers them so
our callers get ours instead of the DLL's. That is only safe if SOMETHING WRITES
THE ROW after cutover. There are exactly three ways it can happen:

  (a) a CRT initialiser we reproduce   -- MintConstructedStatics()
  (b) a PORTED function that writes the slot the way retail's does
  (c) nothing                          -- the row is zero forever

Category (c) is a null dereference on the first boot after cutover, and it
CANNOT be caught by any run we can do beforehand: the mint only registers under
BANISHED_RT_MINT=1, so today every writer still writes the DLL's copy and the
row looks alive. Two instances were found by hand from different directions --
?_singleton@Modification@IO@@ (lane M) and ?_singleton@MemoryAllocator@System@@
(lane E3, where the write is `if (slot) *slot = self`, so a null resolve skips
it in silence). This tool replaces the sampling with an enumeration.

HOW IT FINDS WRITERS

It scans the DLL's .text for rip-relative STORES whose target is a minted row,
and attributes each to the exported function that contains it (nearest preceding
export). Forms decoded, all with mod=00 rm=101 (rip+disp32):

    48 89 /r        MOV [rip+d32], r64          (7 bytes)
    89 /r           MOV [rip+d32], r32          (6)
    66 89 /r        MOV [rip+d32], r16          (7)
    88 /r           MOV [rip+d32], r8           (6)
    48 C7 05        MOV qword [rip+d32], imm32  (11)
    C7 05           MOV dword [rip+d32], imm32  (10)
    C6 05           MOV byte  [rip+d32], imm8   (7)
    48 83 /1 /5     ADD/OR/... [rip+d32], imm8  (8)     (read-modify-write)
    F3 0F 11 /r     MOVSS [rip+d32], xmm        (8)
    F2 0F 11 /r     MOVSD [rip+d32], xmm        (8)

A form this list misses would show as a missing writer, i.e. a FALSE (c) -- the
dangerous direction -- so the tool carries a NEGATIVE CONTROL: nine symbols whose
writers were read independently out of Ghidra's own xref database, checked on
every run. If the scanner cannot reproduce Ghidra's answer for those nine, it
says so and exits non-zero rather than reporting a clean enumeration.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_mint_writer_audit.py
"""
import bisect
import csv
import os
import re
import subprocess
import sys
import tempfile

import pefile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
TABLES = os.path.join(REPO, "standalone", "rt_data_mint_tables.inc")
CENSUS_TOOL = os.path.join(REPO, "tools", "runtime_census.py")

# Ground truth read out of Ghidra's xref database (get_bulk_xrefs), used as this
# scanner's negative control. address -> set of instruction addresses Ghidra
# calls WRITE or READ_WRITE. If the byte scan cannot find these, it is blind to
# a store form and its "no writer" verdicts cannot be trusted.
GHIDRA_WRITES = {
    0x1801fc6d0: {0x1800bfae6, 0x1800c0616, 0x1800db63f},
    0x1801fe788: {0x1800f37d5, 0x1800f380a, 0x1800f370b},
    0x1801fe798: {0x1800f5935, 0x1800f596a, 0x1800f586b},
    0x1801fe780: {0x1800f308b, 0x1800f30c2, 0x1800f2fcb},
    0x1801fc6e0: {0x18000634f},          # plus 105 CRT initialisers
    0x1801fe77c: {0x1800201f0},          # READ_WRITE
    0x1801fc70b: {0x180017040},
    0x1801fe728: {0x1800db9ca, 0x1800ca7e0},
    0x1801fc720: set(),                  # Ghidra finds NO write at all
}


def store_targets(code, base):
    """Yield (instruction_va, target_va) for every rip-relative store."""
    n = len(code)
    i = 0
    while i < n - 11:
        b0 = code[i]
        hit = None
        if b0 == 0x48 and code[i + 1] == 0x89 and (code[i + 2] & 0xC7) == 0x05:
            hit = (7, 3)
        elif b0 == 0x48 and code[i + 1] == 0xC7 and code[i + 2] == 0x05:
            hit = (11, 3)
        elif b0 == 0x89 and (code[i + 1] & 0xC7) == 0x05:
            hit = (6, 2)
        elif b0 == 0x66 and code[i + 1] == 0x89 and (code[i + 2] & 0xC7) == 0x05:
            hit = (7, 3)
        elif b0 == 0x88 and (code[i + 1] & 0xC7) == 0x05:
            hit = (6, 2)
        elif b0 == 0xC7 and code[i + 1] == 0x05:
            hit = (10, 2)
        elif b0 == 0xC6 and code[i + 1] == 0x05:
            hit = (7, 2)
        elif b0 == 0x48 and code[i + 1] == 0x83 and (code[i + 2] & 0xC7) == 0x05:
            hit = (8, 3)
        elif b0 in (0xF3, 0xF2) and code[i + 1] == 0x0F and code[i + 2] == 0x11 \
                and (code[i + 3] & 0xC7) == 0x05:
            hit = (8, 4)
        # INC/DEC [rip+d32] -- a read-modify-write, and the form the negative
        # control caught this scanner missing: ParticleInstance::_frameIndex is
        # INCREMENTED with `FF 05`, never assigned, so a store-only scan called
        # it unwritten. That is the FALSE-(c) direction the control exists for.
        elif b0 == 0xFF and (code[i + 1] & 0xC7) == 0x05 and ((code[i + 1] >> 3) & 7) in (0, 1):
            hit = (6, 2)
        elif b0 == 0x48 and code[i + 1] == 0xFF and (code[i + 2] & 0xC7) == 0x05 and ((code[i + 2] >> 3) & 7) in (0, 1):
            hit = (7, 3)
        elif b0 == 0xFE and (code[i + 1] & 0xC7) == 0x05 and ((code[i + 1] >> 3) & 7) in (0, 1):
            hit = (6, 2)
        if hit:
            length, off = hit
            disp = int.from_bytes(code[i + off:i + off + 4], "little", signed=True)
            yield base + i, base + i + length + disp
        i += 1


def find_crt_init_table(pe, ib):
    """The .CRT$XC array -- same structural rule as tools/rt_mint_gen.py."""
    text = next(s for s in pe.sections if s.Name.decode().rstrip(chr(0)) == ".text")
    rdata = next(s for s in pe.sections if s.Name.decode().rstrip(chr(0)) == ".rdata")
    ta, tb = text.VirtualAddress, text.VirtualAddress + max(text.Misc_VirtualSize,
                                                           text.SizeOfRawData)
    data = pe.get_data(rdata.VirtualAddress, rdata.SizeOfRawData)
    runs, cur = [], []
    for off in range(0, len(data) - 8, 8):
        qw = int.from_bytes(data[off:off + 8], "little")
        if qw and ta <= qw - ib < tb:
            cur.append(qw)
        else:
            if len(cur) >= 16:
                runs.append(cur)
            cur = []
    cand = [r for r in runs if all(r[i] < r[i + 1] for i in range(len(r) - 1))]
    return cand[0] if len(cand) == 1 else []


def main():
    pe = pefile.PE(DLL)
    ib = pe.OPTIONAL_HEADER.ImageBase
    text = next(s for s in pe.sections if s.Name.decode().rstrip(chr(0)) == ".text")
    code = pe.get_data(text.VirtualAddress, text.SizeOfRawData)
    base = ib + text.VirtualAddress

    by_rva = {e.address: e.name.decode()
              for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    srt = sorted(by_rva)

    # Attribution: nearest preceding EXPORT, with one exact carve-out.
    #
    # The first version credited ??0Scene@Picking@@ -- a PORTED function -- with
    # the writes to _firstTypeInfo, _loader and _rng. Those are in the CRT
    # initialisers, which are unexported and merely sit after Picking::Scene in
    # address order; the error moves rows from (c) into (a)/(b), i.e. it hides
    # the bug this tool exists to find.
    #
    # .pdata was the obvious fix and is NOT usable: it has 6,045 entries and does
    # not cover frame-less leaf functions, so ??0BaseTypeInfo's own store falls
    # outside every range and the row loses its real writer -- an error in the
    # same dangerous direction. The initialiser array, by contrast, is exact and
    # is the only region where the misattribution actually occurs, so stores
    # inside it are carved out by address and named as what they are.
    #
    # The first version of this tool attributed by nearest preceding export and
    # credited ??0Scene@Picking@@ -- a PORTED function -- with the writes to
    # _firstTypeInfo, _loader and _rng. Those writes are in the CRT initialisers,
    # which are unexported and merely happen to sit after Picking::Scene in
    # address order. That error moves rows from (c) into (a)/(b), i.e. it hides
    # the bug this tool exists to find, so it is worth the exception directory.
    inits = find_crt_init_table(pe, ib)
    init_lo, init_hi = (inits[0], inits[-1] + 0x70) if inits else (0, 0)

    def owner(va):
        if init_lo <= va < init_hi:
            return "(CRT initialiser)"
        i = bisect.bisect_right(srt, va - ib) - 1
        return by_rva[srt[i]] if i >= 0 else None

    # The minted storage rows, straight out of the generated table.
    rows = []
    text_inc = open(TABLES, encoding="utf-8").read()
    for m in re.finditer(
            r'\{ (nullptr|"[^"]*"), (0x[0-9a-f]+)ull, (\d+), &kStorageBytes\[\d+\], '
            r'"[^"]*", (nullptr|"[^"]*"), (nullptr|kInit\d+), (\d+) \}', text_inc):
        sym = None if m.group(1) == "nullptr" else m.group(1).strip('"')
        rows.append({"sym": sym, "va": int(m.group(2), 16),
                     "size": int(m.group(3)),
                     "ctor": m.group(4) != "nullptr",
                     "seeded": m.group(5) != "nullptr"})
    if not rows:
        print("no storage rows parsed — has the table format changed?", file=sys.stderr)
        return 3

    extents = {r["va"]: r["size"] for r in rows}
    bases = sorted(extents)
    ends = [b + extents[b] for b in bases]

    def row_of(addr):
        i = bisect.bisect_right(bases, addr) - 1
        return bases[i] if i >= 0 and addr < ends[i] else None

    writers = {r["va"]: {} for r in rows}
    found_ctrl = {a: set() for a in GHIDRA_WRITES}
    for ins, tgt in store_targets(code, base):
        r = row_of(tgt)
        if r is not None:
            writers[r].setdefault(owner(ins) or "(unexported/CRT internal)", set()).add(ins)
        if tgt in found_ctrl:
            found_ctrl[tgt].add(ins)

    # ---- negative control --------------------------------------------------
    missed = []
    for a, expect in GHIDRA_WRITES.items():
        for ins in expect:
            if ins not in found_ctrl.get(a, set()):
                missed.append((a, ins))
    print(f"negative control: {sum(len(v) for v in GHIDRA_WRITES.values())} store "
          f"sites read independently from Ghidra; {len(missed)} NOT reproduced by "
          f"the byte scan")
    if missed:
        for a, ins in missed:
            print(f"   MISSED {hex(ins)} -> {hex(a)}")
        print("The scan is blind to at least one store form, so a 'no writer' "
              "verdict below cannot be trusted. Refusing to report.", file=sys.stderr)
        return 2

    # ---- who is ported -----------------------------------------------------
    census_csv = os.path.join(tempfile.gettempdir(), "rt_writer_census.csv")
    r = subprocess.run([sys.executable, CENSUS_TOOL, "--csv", census_csv],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        sys.stderr.write(r.stdout.decode("utf-8", "replace"))
        return 3
    ported = {}
    with open(census_csv, encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            ported[row["key"]] = row["ported"] == "1"

    cat = {"a": [], "s": [], "b": [], "c": [], "?": []}
    print(f"\n{len(rows)} minted storage rows\n")
    for r in sorted(rows, key=lambda x: (x["sym"] or "", x["va"])):
        w = writers[r["va"]]
        named = {k: v for k, v in w.items() if k != "(unexported/CRT internal)"}
        any_ported = [k for k in named if ported.get(k)]
        if r["ctor"]:
            c = "a"
        elif r["seeded"]:
            c = "s"
        elif any_ported:
            c = "b"
        elif w:
            c = "c"
        else:
            c = "?"
        cat[c].append(r["sym"] or hex(r["va"]))
        label = r["sym"] or f"(unexported @{hex(r['va'])})"
        wl = ", ".join(f"{k}{'  [PORTED]' if ported.get(k) else ''}"
                       for k in sorted(named)) or "(no exported writer found)"
        print(f"({c}) {label}")
        print(f"      writers: {wl}")
        if "(unexported/CRT internal)" in w:
            print(f"      also written from {len(w['(unexported/CRT internal)'])} "
                  f"unexported internal site(s)")

    print(f"\n(a) reproduced CRT ctor : {len(cat['a'])}")
    print(f"(s) seeded with a known constant value: {len(cat[chr(115)])}")
    print(f"(b) a PORTED writer     : {len(cat['b'])}")
    print(f"(c) writer exists but is NOT ported -> ZERO FOREVER at cutover: "
          f"{len(cat['c'])}")
    for s in cat["c"]:
        print(f"      {s}")
    print(f"(?) NO writer found at all -> also zero forever: {len(cat['?'])}")
    for s in cat["?"]:
        print(f"      {s}")
    return 2 if (cat["c"] or cat["?"]) else 0


if __name__ == "__main__":
    sys.exit(main())
