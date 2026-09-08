#!/usr/bin/env python3
r"""Generate standalone/rt_data_mint_tables.inc — Wave 9 lane C2's minted DATA.

WHY THIS EXISTS

Lane C's harvest (docs/runtime_port/wave9_data_harvest.csv, WAVE9_LANE_C.md)
recorded what every exported DATA symbol in Runtime-x64.dll contains while the
DLL is still on disk. This script turns that harvest into C++ the exe can
carry after the DLL is gone: storage for every singleton, a BaseTypeInfo per
?_typeInfo@, and a slot-by-slot descriptor for every ??_7 vtable.

IT DOES NOT DECIDE WHAT GOES IN A VTABLE SLOT. The descriptor records the DLL's
ground truth (this slot holds function X, named or FUN_<addr>, or _purecall);
standalone/rt_data_mint.cpp fills the actual table AT RUNTIME from the port
registry, so a slot is our port the moment lane B registers it and a named trap
until then. That is the only way to keep a minted vtable honest without
regenerating this file per port.

THREE THINGS THIS SCRIPT VERIFIES, not just transcribes
  1. Slot COUNT, two independent ways. `sizeProof` is "adjacent-export" when
     the vtable's last slot ends exactly where the next exported symbol starts
     (exact, 13 of 28), or "tail-not-a-pointer" when the qword one past the end
     is not a valid address inside the DLL image (15 of 28). A vtable that is
     neither would be SUSPECT — the decode may have been truncated by the
     harvest's next-export bound — and this script fails rather than emit one.
  2. Every slot target VA is re-read from the DLL here, not copied from the
     CSV, and compared against the CSV. A mismatch fails the run.
  3. The exported-DATA set is re-derived by SECTION (any unported export whose
     RVA is not in .text), not by name prefix. That is how the ten extra .data
     statics were found: lane C's harvest filtered on the three name prefixes
     ??_7 / ?_singleton@ / ?_typeInfo@ and so could not see
     ?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA — the head of the very
     chain the 14 BaseTypeInfo mints live on — nor System::Timer's four
     calibration statics.

THE SET IS A MOVING TARGET, AND THAT IS WHY THIS RUNS THE CENSUS ITSELF

The symbol list is not read from a frozen dump. This script SHELLS OUT to
tools/runtime_census.py on every run and takes the unported exports from its
fresh output. The set is "the DLL data our tree references today", so porting a
class can ADD rows to it: lane B2's twelve Video::*Data companion classes took
the vtable count from 28 to 38 in one afternoon, because ten of those vtables
had no reference in the tree until that port existed (the same shape as
reference_porting_can_grow_the_cutover_surface.md, now measured three times in
Wave 9). A generator driven by a hard-coded list would under-report silently
for the rest of the wave; this one cannot.

The harvest CSV is still read, but only as an independent CROSS-CHECK of the
slot bytes for the symbols it covers — never as the source of the set.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_mint_gen.py
"""
import argparse
import ast
import bisect
import csv
import os
import subprocess
import sys
import tempfile

import pefile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
HARVEST = os.path.join(REPO, "docs", "runtime_port", "wave9_data_harvest.csv")
CENSUS_TOOL = os.path.join(REPO, "tools", "runtime_census.py")
OUT = os.path.join(REPO, "standalone", "rt_data_mint_tables.inc")

# Storage sizes for the exported .data statics that are NOT plain pointers.
# Each is pinned by a type header in our tree, named here so the number can be
# re-checked rather than trusted:
#   Engine::Loader  0x38  runtime/types/engine/UpdateLoop.h (static_assert)
#                         — corroborated independently by the DLL itself: the
#                         gap from ?_loader@ to the next exported symbol is
#                         exactly 56 bytes.
#   System::Random  2500  runtime/types/system/Value.h (static_assert)
#
# THESE TWO ARE OBJECTS, NOT SCALARS, and that is not a size footnote. Both are
# BSS in the file, but the DLL's CRT runs their constructors at load: Loader's
# ctor stores ??_7Loader@Engine@@6B@ into +0x00 (UpdateLoop.h) and Random's
# seeds 624 words of MT19937 state. Minting zeroed storage for them reproduces
# the FILE, not the loaded DLL — at cutover something has to run those two
# constructors or the first virtual call on _loader dispatches through a null
# vtable and Particle's _rng returns zeros forever. Recorded in the `note`
# field and in WAVE9_LANE_C2.md; no other storage symbol has this problem
# (singletons are pointers written on first use).
# The one CRT trap that appears as a vtable slot. Named by Ghidra
# (get_function_by_address at 0x18007ec28 -> "_purecall"); its decompile is the
# stock MSVC pure-virtual-call handler. A slot pointing here is a pure virtual
# the concrete class never overrides -- retail relies on it never being called
# -- so it is minted as a NAMED trap, not as a function to port.
KNOWN_INTERNAL_THUNKS = {
    0x18007ec28: "_purecall",
}

EXTRA_SIZES = {
    "?_loader@LoaderLoop@Engine@@1VLoader@2@A": (0x38, "Engine::Loader OBJECT — NEEDS A CTOR AT STARTUP, zeroed storage is not enough"),
    "?_rng@Particle@Graphics@@1VRandom@System@@A": (2500, "System::Random OBJECT (MT19937) — NEEDS A CTOR AT STARTUP to seed mt[]"),
    "?_frameIndex@ParticleInstance@Graphics@@1HA": (4, "int"),
    "?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA": (1, "bool"),
    "?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA": (8, "BaseTypeInfo* — head of the reflection chain"),
    "?_errorString@Error@System@@0PADA": (8, "char*"),
    "?_secondsPerTick@Timer@System@@0NA": (8, "double"),
    "?_systemInitialTick@Timer@System@@0_JA": (8, "int64"),
    "?_systemTicksPerSecond@Timer@System@@0_JA": (8, "int64"),
    "?_ticksPerSecond@Timer@System@@0_JA": (8, "int64"),
}


def run_census(csv_path):
    """Run tools/runtime_census.py and return its unported 'export' rows.

    Deliberately a subprocess and not an import: the census is the authority on
    what our tree references (it has been wrong three times this project and
    each fix landed there), and it EXITS 2 rather than degrading when it cannot
    answer. Re-running it here is what keeps this generator's symbol set from
    going stale as other lanes land ports.
    """
    r = subprocess.run([sys.executable, CENSUS_TOOL, "--csv", csv_path],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        sys.stderr.write(r.stdout.decode("utf-8", "replace"))
        raise SystemExit(f"runtime_census.py exited {r.returncode}; refusing to "
                         f"generate from a census that could not answer")
    with open(csv_path, encoding="utf-8", newline="") as f:
        return [row for row in csv.DictReader(f)
                if row["kind"] == "export" and row["ported"] == "0"]


def cstr(s):
    if s is None:
        return "nullptr"
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", default=DEFAULT_DLL)
    ap.add_argument("--out", default=OUT)
    args = ap.parse_args()

    pe = pefile.PE(args.dll)
    ib = pe.OPTIONAL_HEADER.ImageBase
    isize = pe.OPTIONAL_HEADER.SizeOfImage
    sections = [(s.Name.decode().rstrip("\0"), s.VirtualAddress,
                 s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData))
                for s in pe.sections]

    def section_of(rva):
        for name, a, b in sections:
            if a <= rva < b:
                return name
        return "?"

    by_name, by_rva = {}, {}
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.name:
            n = e.name.decode()
            by_name[n] = e.address
            by_rva[e.address] = n
    sorted_rvas = sorted(by_rva)

    errors = []

    # ---- the SET: whatever the census says we still ask the DLL for ---------
    # Scratch, not a repo artefact: it is regenerated on every run and would
    # otherwise churn a 843-row CSV into every commit that touches the mint.
    census_csv = os.path.join(tempfile.gettempdir(), "rt_mint_census.csv")
    unported = run_census(census_csv)
    data_syms = []          # (symbol, namespace, rva, consumers)
    for row in unported:
        rva = by_name.get(row["key"])
        if rva is None or section_of(rva) == ".text":
            continue
        data_syms.append((row["key"], row["namespace"], rva, row.get("callers", "")))
    data_syms.sort()

    # The harvest, keyed for cross-checking only. Slots first, then typeinfo.
    harvest_slots = {}      # (symbol, index) -> target VA
    harvest_ti = {}
    for r in csv.DictReader(open(HARVEST, encoding="utf-8", newline="")):
        if r["kind"] == "vtable" and r["slot_index"] != "":
            harvest_slots[(r["symbol"], int(r["slot_index"]))] = int(r["slot_va"], 16)
        elif r["kind"] == "typeinfo":
            harvest_ti[r["symbol"]] = ast.literal_eval(r["slot_name"])
    checked = 0

    # ---- vtables -----------------------------------------------------------
    vtables = []
    for sym, ns, rva, _consumers in data_syms:
        if not sym.startswith("??_7"):
            continue
        # Decode the slots straight out of the DLL. Bound: the next exported
        # symbol (a vtable is never longer than that), stopping early on a
        # zero or a qword that is not an address in this image.
        j = bisect.bisect_right(sorted_rvas, rva)
        nxt = sorted_rvas[j] if j < len(sorted_rvas) else None
        max_slots = min(64, ((nxt - rva) // 8) if nxt else 64)
        raw = pe.get_data(rva, max_slots * 8)
        slots = []
        for i in range(len(raw) // 8):
            qw = int.from_bytes(raw[i * 8:i * 8 + 8], "little")
            if qw == 0 or not (ib <= qw < ib + isize):
                break
            name = by_rva.get(qw - ib)
            if qw in KNOWN_INTERNAL_THUNKS:
                kind, name = "RT_MINT_PURECALL", None
            elif name:
                kind = "RT_MINT_NAMED"
            else:
                kind = "RT_MINT_UNNAMED"
            # cross-check against the harvest where it has an opinion
            h = harvest_slots.get((sym, i))
            if h is not None:
                checked += 1
                if h != qw:
                    errors.append(f"{sym} slot {i}: harvest {hex(h)} != DLL {hex(qw)}")
            slots.append((i, name, qw, kind))
        if not slots:
            errors.append(f"{sym}: decoded 0 slots")
            continue
        n = len(slots)
        end_rva = rva + 8 * n
        if nxt == end_rva:
            proof = "adjacent-export:" + by_rva[nxt]
        else:
            tail = int.from_bytes(pe.get_data(end_rva, 8), "little")
            if ib <= tail < ib + isize:
                errors.append(f"{sym}: slot count {n} SUSPECT — the qword after the "
                              f"last slot ({hex(tail)}) is a valid image pointer")
                proof = "SUSPECT"
            else:
                proof = "tail-not-a-pointer"
        # A harvested vtable that changed LENGTH would mean the decode rule and
        # the harvest disagree about the class, which is worth a hard stop.
        hn = sum(1 for (hs, _hi) in harvest_slots if hs == sym)
        if hn and hn != n:
            errors.append(f"{sym}: harvest has {hn} slots, this decode has {n}")
        vtables.append({"symbol": sym, "va": ib + rva, "slots": slots, "proof": proof})

    # ---- typeinfo (Object::BaseTypeInfo, 0x38) -----------------------------
    def cstr_at(va):
        if not va or not (ib <= va < ib + isize):
            return None
        d = pe.get_data(va - ib, 256)
        if not d:
            return None
        e = d.find(b"\0")
        return d[:e if e >= 0 else len(d)].decode("latin1")

    typeinfos = []
    for sym, ns, rva, _consumers in data_syms:
        if not sym.startswith("?_typeInfo@"):
            continue
        raw = pe.get_data(rva, 0x38)
        if len(raw) != 0x38:
            errors.append(f"{sym}: could not read 0x38 bytes")
            continue
        q = lambda o: int.from_bytes(raw[o:o + 8], "little")
        name_p, parent_p, ext_p = q(8), q(16), q(24)
        fac_p, inp_p, next_p = q(32), q(40), q(48)
        if next_p != 0:
            errors.append(f"{sym}: next is {hex(next_p)} in the file, expected 0x0")
        t = {
            "symbol": sym, "va": ib + rva,
            "name": cstr_at(name_p), "parent": cstr_at(parent_p), "ext": cstr_at(ext_p),
            "factory": by_rva.get(fac_p - ib) if fac_p else None, "factory_va": fac_p,
            "inplace": by_rva.get(inp_p - ib) if inp_p else None, "inplace_va": inp_p,
        }
        h = harvest_ti.get(sym)
        if h:
            checked += 1
            for k_here, k_there in (("name", "name"), ("parent", "parentName"),
                                    ("ext", "fileExtension"), ("factory", "factoryFn"),
                                    ("inplace", "inPlaceFactoryFn")):
                if t[k_here] != h[k_there]:
                    errors.append(f"{sym}.{k_here}: harvest {h[k_there]!r} != "
                                  f"DLL {t[k_here]!r}")
        typeinfos.append(t)
    typeinfos.sort(key=lambda t: t["symbol"])

    # ---- storage: everything else in the exported-DATA set -----------------
    storage = []
    for sym, ns, rva, _consumers in data_syms:
        if sym.startswith("??_7") or sym.startswith("?_typeInfo@"):
            continue
        if sym.startswith("?_singleton@"):
            size, note = 8, "singleton pointer"
        elif sym in EXTRA_SIZES:
            size, note = EXTRA_SIZES[sym]
        else:
            # A new exported .data static nobody has sized. Refusing is the
            # point: guessing 8 for an object would mint a truncated one.
            errors.append(f"{sym}: exported DATA in {section_of(rva)} with no known "
                          f"size — add it to EXTRA_SIZES with the header that pins it")
            continue
        storage.append({"symbol": sym, "va": ib + rva, "size": size,
                        "note": note, "namespace": ns})
    storage.sort(key=lambda s: s["symbol"])

    if errors:
        print("REFUSING TO GENERATE — verification failed:", file=sys.stderr)
        for e in errors:
            print("  " + e, file=sys.stderr)
        return 2

    # ---- emit --------------------------------------------------------------
    o = []
    w = o.append
    w("// GENERATED by tools/rt_mint_gen.py — DO NOT EDIT BY HAND.")
    w("// Source of truth: docs/runtime_port/wave9_data_harvest.csv (lane C's")
    w("// harvest of Runtime-x64.dll) re-verified against the DLL at generation")
    w("// time. Regenerate with:")
    w("//   tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_mint_gen.py")
    w(f"// Counts: {len(vtables)} vtables, {len(typeinfos)} BaseTypeInfo, "
      f"{len(storage)} storage symbols.")
    w("")

    # per-slot trap stubs
    w("// ---- Per-slot traps. One function per (vtable, slot) so an unreachable")
    w("// slot that DOES get called names itself instead of aborting anonymously.")
    for vi, v in enumerate(vtables):
        for (si, _n, _va, _k) in v["slots"]:
            w(f"RT_MINT_TRAP({vi},{si})")
    w("")

    for vi, v in enumerate(vtables):
        w(f"// {v['symbol']}  DLL {v['va']:#x}  {len(v['slots'])} slots  "
          f"[{v['proof']}]")
        w(f"static const RtMintVSlot kVSlots{vi}[] = {{")
        for (si, name, va, kind) in v["slots"]:
            w(f"    {{ {cstr(name)}, {va:#x}ull, {kind} }},")
        w("};")
        w(f"static void* kVTable{vi}[{len(v['slots'])}];")
        w(f"static const RtMintTrapFn kVTraps{vi}[] = {{ "
          + ", ".join(f"&RtMintTrapStub_{vi}_{si}" for (si, _, _, _) in v["slots"]) + " };")
        w("")

    w("static RtMintVtable kMintVtables[] = {")
    for vi, v in enumerate(vtables):
        w(f"    {{ {cstr(v['symbol'])}, {v['va']:#x}ull, {len(v['slots'])}, "
          f"kVSlots{vi}, kVTable{vi}, kVTraps{vi}, {cstr(v['proof'])} }},")
    w("};")
    w("")

    w("// ---- Object::BaseTypeInfo. Fields are link-time constants copied from")
    w("// the DLL; `next` is deliberately NOT copied (it reads 0x0 on disk and is")
    w("// built by running constructors) and is rebuilt from OUR init order.")
    w(f"static RtObject::BaseTypeInfo kTypeInfoStorage[{len(typeinfos)}];")
    for ti_i, t in enumerate(typeinfos):
        for si in range(2):
            w(f"RT_MINT_TI_TRAP({ti_i},{si})")
    w("static RtMintTypeInfo kMintTypeInfos[] = {")
    for ti_i, t in enumerate(typeinfos):
        w(f"    {{ {cstr(t['symbol'])}, {t['va']:#x}ull, &kTypeInfoStorage[{ti_i}],")
        w(f"      {cstr(t['name'])}, {cstr(t['parent'])}, {cstr(t['ext'])},")
        w(f"      {cstr(t['factory'])}, {t['factory_va']:#x}ull, &RtMintTiTrapStub_{ti_i}_0,")
        w(f"      {cstr(t['inplace'])}, {t['inplace_va']:#x}ull, &RtMintTiTrapStub_{ti_i}_1 }},")
    w("};")
    w("")

    w("// ---- Plain storage: the singleton pointers plus the ten other exported")
    w("// .data statics. All are BSS in the DLL — written only at runtime — so")
    w("// there is nothing to copy: minting one means OWNING the bytes.")
    # Each entry gets a 16-byte-aligned slice. Two of these are OBJECTS, not
    # scalars (Engine::Loader and System::Random), and the singletons are
    # pointers; carving them out of a byte array at packed offsets would put
    # pointer fields on odd addresses. 16 matches what the DLL's own .data
    # gives them and costs a few dozen bytes.
    offsets = []
    off = 0
    for s in storage:
        off = (off + 15) & ~15
        offsets.append(off)
        off += s["size"]
    total = (off + 15) & ~15
    w(f"alignas(16) static unsigned char kStorageBytes[{total}];  // one block, zero-initialised")
    w("static RtMintStorage kMintStorage[] = {")
    for s, so in zip(storage, offsets):
        w(f"    {{ {cstr(s['symbol'])}, {s['va']:#x}ull, {s['size']}, "
          f"&kStorageBytes[{so}], {cstr(s['note'])} }},")
    w("};")
    w("")

    open(args.out, "w", encoding="utf-8", newline="\n").write("\n".join(o) + "\n")
    print(f"wrote {args.out}")
    print(f"  census     {len(unported)} unported exports -> {len(data_syms)} exported DATA "
          f"({checked} rows cross-checked against the round-1 harvest, 0 disagreements)")
    print(f"  vtables    {len(vtables):3d}  ({sum(len(v['slots']) for v in vtables)} slots, "
          f"{sum(1 for v in vtables if v['proof'].startswith('adjacent'))} count-proved by adjacency, "
          f"{sum(1 for v in vtables if v['proof'] == 'tail-not-a-pointer')} by tail probe)")
    print(f"  typeinfo   {len(typeinfos):3d}")
    print(f"  storage    {len(storage):3d}  ({total} bytes; "
          f"{sum(1 for s in storage if s['symbol'].startswith('?_singleton@'))} singletons, "
          f"{sum(1 for s in storage if not s['symbol'].startswith('?_singleton@'))} other .data statics)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
