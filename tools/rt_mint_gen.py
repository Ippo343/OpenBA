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
  3. The exported-DATA set is re-derived from the census dump by SECTION
     (anything not in .text), not by name prefix. That is how the ten extra
     .data statics below were found: lane C's harvest filtered on the three
     name prefixes ??_7 / ?_singleton@ / ?_typeInfo@ and so could not see
     ?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA — the head of the very
     chain the 14 BaseTypeInfo mints live on — nor System::Timer's four
     calibration statics. 60 was the count of three prefixes; 70 is the count
     of exported DATA.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_mint_gen.py
"""
import argparse
import ast
import bisect
import collections
import csv
import os
import re
import sys

import pefile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
HARVEST = os.path.join(REPO, "docs", "runtime_port", "wave9_data_harvest.csv")
UNPORTED = os.path.join(REPO, "docs", "runtime_port", "wave9_unported_20260907.txt")
OUT = os.path.join(REPO, "standalone", "rt_data_mint_tables.inc")

ROW_RE = re.compile(r'^\s*(export|addr|ordinal)\s+(\S+)\s+(\S+)\s*$')

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


def load_unported_exports(path):
    """(namespace, symbol, consumers) for every 'export' row in the census dump."""
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    out = []
    for i, line in enumerate(lines):
        m = ROW_RE.match(line)
        if m and m.group(1) == "export":
            consumers = lines[i + 1].strip() if i + 1 < len(lines) else ""
            out.append((m.group(2), m.group(3), consumers))
    return out


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

    # ---- vtables -----------------------------------------------------------
    rows = list(csv.DictReader(open(HARVEST, encoding="utf-8", newline="")))
    vt = collections.defaultdict(list)
    for r in rows:
        if r["kind"] == "vtable":
            vt[r["symbol"]].append(r)

    vtables = []
    for sym in sorted(vt):
        rs = sorted(vt[sym], key=lambda r: int(r["slot_index"]))
        rva = int(rs[0]["rva"], 16)
        n = len(rs)
        # (2) re-read every slot from the DLL and compare with the CSV
        raw = pe.get_data(rva, n * 8)
        slots = []
        for i, r in enumerate(rs):
            live = int.from_bytes(raw[i * 8:i * 8 + 8], "little")
            if hex(live) != r["slot_va"]:
                errors.append(f"{sym} slot {i}: CSV {r['slot_va']} != DLL {hex(live)}")
            name = r["slot_name"]
            if name == "_purecall":
                kind = "RT_MINT_PURECALL"
                name = None
            elif name.startswith("FUN_"):
                kind = "RT_MINT_UNNAMED"
                name = None
            else:
                kind = "RT_MINT_NAMED"
            slots.append((i, name, live, kind))
        # (1) slot count, two independent ways
        end = rva + 8 * n
        j = bisect.bisect_right(sorted_rvas, rva)
        nxt = sorted_rvas[j] if j < len(sorted_rvas) else None
        if nxt == end:
            proof = "adjacent-export:" + by_rva[nxt]
        else:
            tail = int.from_bytes(pe.get_data(end, 8), "little")
            if ib <= tail < ib + isize:
                errors.append(f"{sym}: slot count {n} SUSPECT — the qword after the "
                              f"last slot ({hex(tail)}) is a valid image pointer")
                proof = "SUSPECT"
            else:
                proof = "tail-not-a-pointer"
        vtables.append({"symbol": sym, "va": ib + rva, "slots": slots, "proof": proof})

    # ---- typeinfo ----------------------------------------------------------
    typeinfos = []
    for r in rows:
        if r["kind"] != "typeinfo":
            continue
        f = ast.literal_eval(r["slot_name"])
        rva = int(r["rva"], 16)
        fac_rva = by_name.get(f["factoryFn"] or "")
        inp_rva = by_name.get(f["inPlaceFactoryFn"] or "")
        typeinfos.append({
            "symbol": r["symbol"], "va": ib + rva,
            "name": f["name"], "parent": f["parentName"], "ext": f["fileExtension"],
            "factory": f["factoryFn"], "factory_va": (ib + fac_rva) if fac_rva else 0,
            "inplace": f["inPlaceFactoryFn"], "inplace_va": (ib + inp_rva) if inp_rva else 0,
        })
        if f["next_raw"] != "0x0":
            errors.append(f"{r['symbol']}: next is {f['next_raw']} in the file, expected 0x0")
    typeinfos.sort(key=lambda t: t["symbol"])

    # ---- storage: every exported DATA symbol in the unported set -----------
    storage = []
    seen_vt = {v["symbol"] for v in vtables}
    seen_ti = {t["symbol"] for t in typeinfos}
    for ns, sym, consumers in load_unported_exports(UNPORTED):
        rva = by_name.get(sym)
        if rva is None or section_of(rva) == ".text":
            continue
        if sym in seen_vt or sym in seen_ti:
            continue
        if sym.startswith("?_singleton@"):
            size, note = 8, "singleton pointer"
        elif sym in EXTRA_SIZES:
            size, note = EXTRA_SIZES[sym]
        else:
            errors.append(f"{sym}: exported DATA in {section_of(rva)} with no known size")
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
