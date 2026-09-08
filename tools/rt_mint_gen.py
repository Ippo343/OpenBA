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
import re
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
# THESE TWO ARE OBJECTS, NOT SCALARS, and BOTH are constructed at load.
# Both are BSS in the file, which is all a file-based harvest can see, so the
# file cannot tell you which. The .CRT$XC sweep (find_crt_init_table below)
# answers it against the DLL's own initialiser array:
#
#   _loader  IS constructed, by exactly one initialiser (0x180148fe0): Thread
#            ctor with a 256 KB stack, then the Loader vtable at +0x00, then
#            _loop = 0. Reproduced by ConstructLoader().
#   _rng     IS constructed too, by initialiser 0x180149650, and this file
#            said the opposite for a day. The base-address-only version of the
#            sweep missed it because that initialiser never mentions the
#            object's base: it writes mt[0] at +0x04 and walks upward. The
#            mint's own shadow report is what caught it -- "DLL=WRITTEN" on a
#            2500-byte BSS object at process init, before any game code could
#            have touched it. It seeds MT19937 with `(uint32)_time64(NULL)`,
#            so the VALUE cannot and need not match retail's; what matters is
#            that it is seeded at all, because an all-zero MT19937 twists to
#            all-zero forever and Particle's every draw would silently be 0.
#            Reproduced by ConstructParticleRng().

# The one CRT trap that appears as a vtable slot. Named by Ghidra
# (get_function_by_address at 0x18007ec28 -> "_purecall"); its decompile is the
# stock MSVC pure-virtual-call handler. A slot pointing here is a pure virtual
# the concrete class never overrides -- retail relies on it never being called
# -- so it is minted as a NAMED trap, not as a function to port.
KNOWN_INTERNAL_THUNKS = {
    0x18007ec28: "_purecall",
}

EXTRA_SIZES = {
    "?_loader@LoaderLoop@Engine@@1VLoader@2@A": (0x38, "Engine::Loader OBJECT, CONSTRUCTED at load — reproduced by ConstructLoader()"),
    "?_rng@Particle@Graphics@@1VRandom@System@@A": (2500, "System::Random OBJECT (MT19937), SEEDED FROM time() at load by 0x180149650"),
    "?_frameIndex@ParticleInstance@Graphics@@1HA": (4, "int"),
    "?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA": (1, "bool"),
    "?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA": (8, "BaseTypeInfo* — head of the reflection chain"),
    "?_errorString@Error@System@@0PADA": (8, "char*"),
    "?_secondsPerTick@Timer@System@@0NA": (8, "double"),
    "?_systemInitialTick@Timer@System@@0_JA": (8, "int64"),
    "?_systemTicksPerSecond@Timer@System@@0_JA": (8, "int64"),
    "?_ticksPerSecond@Timer@System@@0_JA": (8, "int64"),
    # Wave 9 lane W: the three class-static heads of the intrusive live-object
    # lists Video::{Image,Index,Vertex}Buffer::Platform{Construct,Destruct}
    # maintain. Each is one pointer; the size is pinned by
    # runtime/types/video/{ImageBuffer,Buffers}.h, whose link fields sit at
    # +0x50/+0x58 and +0x10/+0x18 of the objects these heads point at.
    "?_firstSurface@ImageBuffer@Video@@0PEAV12@EA":
        (8, "ImageBuffer* -- head of the live-surface list (Video_ImageBuffer_PlatformConstruct)"),
    "?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA":
        (8, "IndexBuffer* -- head of the live-index-buffer list"),
    "?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA":
        (8, "VertexBuffer* -- head of the live-vertex-buffer list"),
}

# Vtables the census CANNOT see, because the only thing that needs them is this
# mint. ??_7Loader@Engine@@6B@ is referenced by exactly one thing in the whole
# process: the CRT initialiser that constructs ?_loader@LoaderLoop@Engine@@,
# which stamps it at +0x00. Our tree never resolves it, so it is absent from the
# census's `called` set -- and the moment we reproduce that construction (see
# KNOWN_CTORS) we become its consumer. Lane C2's own instance of "porting grows
# the cutover surface".
EXTRA_VTABLES = {
    "??_7Loader@Engine@@6B@":
        "stamped by the ?_loader construction; no other consumer exists",
}

# Storage symbols the DLL's CRT CONSTRUCTS at load, and where the construction
# lives on our side. A symbol that the sweep finds a constructor for and that is
# NOT listed here is new work: the generator refuses rather than emitting a
# symbol that would be minted as zeroed bytes and silently differ from the
# loaded DLL.
KNOWN_CTORS = {
    "?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA":
        "MintTypeInfos() — head of our own BaseTypeInfo chain",
    "?_loader@LoaderLoop@Engine@@1VLoader@2@A":
        "MintConstructedStatics() — Thread ctor + Loader vtable + _loop=0",
    "?_rng@Particle@Graphics@@1VRandom@System@@A":
        "ConstructParticleRng() - MT19937 init_genrand seeded from time(), "
        "transcribed from initialiser 0x180149650",
}


def find_crt_init_table(pe, ib, text, rdata):
    """The DLL's .CRT$XC initialiser array: the ground truth for what is
    CONSTRUCTED at load, as opposed to what merely lives in a writable section.

    Section flags cannot answer this — every one of these statics is BSS in the
    file whether or not a constructor runs over it at load. The array is found
    structurally: a run of qwords in .rdata that all point into .text, bounded
    by a zero qword at each end, strictly increasing, and long. Vtable regions
    also produce long runs of .text pointers, so `strictly increasing` and the
    zero terminators are what separate the two; the result is asserted to be
    unique.
    """
    ta, tb = text
    ra, rb = rdata
    data = pe.get_data(ra, rb - ra)
    runs, cur = [], []
    for off in range(0, len(data) - 8, 8):
        qw = int.from_bytes(data[off:off + 8], "little")
        if qw and ta <= qw - ib < tb:
            cur.append(qw)
        else:
            if len(cur) >= 16:
                runs.append(cur)
            cur = []
    if len(cur) >= 16:
        runs.append(cur)
    cand = [r for r in runs if all(r[i] < r[i + 1] for i in range(len(r) - 1))]
    if len(cand) != 1:
        raise SystemExit(f"CRT initialiser table: expected exactly one "
                         f"zero-bounded increasing run, found {len(cand)}")
    return cand[0]


def scan_ctor_refs(pe, ib, inits, extents):
    """target VA -> [initialiser VAs that reference it].

    `extents` maps a symbol's base VA to its SIZE, and a hit anywhere inside
    [base, base+size) counts. Matching the base alone is not enough and the
    difference was a wrong conclusion published for a day: the initialiser that
    seeds ?_rng@Particle@Graphics@@ (0x180149650) never mentions the object's
    base -- it writes mt[0] at +0x04 and walks upward -- so a base-only scan
    reported the object as "not constructed", and lane C2 wrote that zeroed
    storage was faithful for it. A constructor referencing a FIELD rather than
    the base is ordinary; the scan has to allow for it.

    Each initialiser is tiny (~0x70 bytes) and reaches its static through a
    rip-relative displacement. Rather than disassemble, every 4-byte window is
    tried as a disp32 against the known target set: a false positive would have
    to be a random 4 bytes landing on one of ~80 specific addresses, and every
    hit this produced was confirmed against Ghidra's own xrefs.
    """
    hits = {}
    import bisect as _b
    bases = sorted(extents)
    ends = [b + extents[b] for b in bases]

    def owner(addr):
        i = _b.bisect_right(bases, addr) - 1
        return bases[i] if i >= 0 and addr < ends[i] else None

    bounds = [(inits[i], inits[i + 1] if i + 1 < len(inits) else inits[i] + 0x70)
              for i in range(len(inits))]
    for start, end in bounds:
        b = pe.get_data(start - ib, end - start)
        for o in range(0, len(b) - 4):
            disp = int.from_bytes(b[o:o + 4], "little", signed=True)
            t = start + o + 4 + disp
            base = owner(t)
            if base is not None:
                hits.setdefault(base, set()).add(start)
    return {k: sorted(v) for k, v in hits.items()}


# Files whose mentions of a DATA symbol must NOT be treated as evidence that
# the tree needs it: the mint's own generated table and machinery name every
# symbol by construction, so including them would be circular.
SCAN_SKIP = ("standalone/rt_data_mint_tables.inc", "standalone/rt_data_mint.cpp",
             "standalone/rt_data_mint.h")
SCAN_DIRS = ("code", "runtime", "standalone", "engine", "dx11")
DATA_LITERAL = re.compile(r'"((?:\?\?_7|\?_)[^"]{4,})"')


def strip_comments(text):
    """Drop // and /* */ comments before scanning.

    Not cosmetic: this tree DOCUMENTS mangled names heavily -- a mangled name
    in a comment is a note about a symbol, not a dependency on it. Without this,
    code/Game/Work/marketwork.cpp's comment about ?_typeInfo@TableElement would
    have minted a BaseTypeInfo nothing reads, and the count would overstate the
    cutover surface in exactly the direction this lane has been correcting it
    downward for.
    """
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':                      # keep string literals intact
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1]); i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(c); i += 1
    return "".join(out)


def scan_source_for_data_symbols(repo):
    """Every exported-DATA-shaped mangled name that appears as a LITERAL in the
    tree, wherever it appears.

    This exists because the census cannot see all of them. `runtime_census.py`
    recognises a resolve only when the literal sits directly inside
    `ResolveExport(...)` / `ResolveExportRaw(...)` (RE_EXPORT requires the quote
    immediately after the paren), so three shapes are invisible to it:

        RtVtable("??_7MeshInstance@Graphics@@6B@")     a wrapper
        ResolveExportRaw(kFrameIndexData)             a named constant
        ResolveExport(s.mangled)                      a table walked in a loop

    All three are live in this tree, and all three are exactly the symbols a
    cutover needs minted -- the last one is standalone/reg_element_typeids.cpp,
    21 UI ?_typeInfo@ symbols whose own header comment says "a partial table
    costs one crash per missing entry". Scanning for the literal finds them
    regardless of the door they go through.
    """
    out = {}
    for d in SCAN_DIRS:
        for root, _dirs, files in os.walk(os.path.join(repo, d)):
            for f in files:
                if not f.endswith((".cpp", ".h", ".inc")):
                    continue
                path = os.path.join(root, f)
                rel = os.path.relpath(path, repo).replace(os.sep, "/")
                if rel in SCAN_SKIP:
                    continue
                text = strip_comments(open(path, encoding="utf-8",
                                           errors="replace").read())
                for m in DATA_LITERAL.finditer(text):
                    out.setdefault(m.group(1), set()).add(rel)
    return out


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

    secs_text = secs_rdata = None
    for name, lo, hi in sections:
        if name == ".text":
            secs_text = (lo, hi)
        elif name == ".rdata":
            secs_rdata = (lo, hi)
    if not secs_text or not secs_rdata:
        raise SystemExit("Runtime-x64.dll has no .text/.rdata?")

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
    # The census is a superset of what runs but a SUBSET of what is referenced:
    # it cannot see a symbol reached through a wrapper, a named constant or a
    # table walked in a loop. Union in the source scan, which sees all three.
    census_syms = {d[0] for d in data_syms}
    scanned = scan_source_for_data_symbols(REPO)
    scan_added = []
    for sym, where in sorted(scanned.items()):
        if sym in census_syms:
            continue
        rva = by_name.get(sym)
        if rva is None or section_of(rva) == ".text":
            continue          # not an exported DATA symbol; a comment or a code export
        data_syms.append((sym, "scan", rva, ";".join(sorted(where))))
        scan_added.append((sym, sorted(where)[0]))

    for sym, why in EXTRA_VTABLES.items():
        rva = by_name.get(sym)
        if rva is None:
            errors.append(f"{sym}: EXTRA_VTABLES names a symbol the DLL does not export")
        elif all(sym != d[0] for d in data_syms):
            data_syms.append((sym, "Engine", rva, "mint:" + why))
    data_syms.sort()

    # ---- the construction sweep (see find_crt_init_table) -------------------
    inits = find_crt_init_table(pe, ib, secs_text, secs_rdata)

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

    # ---- the construction sweep, now that every symbol's EXTENT is known ---
    # A constructor may reference any field, not the base (see scan_ctor_refs),
    # so the scan needs sizes: a vtable is slots*8, a BaseTypeInfo is 0x38, and
    # a storage symbol's size is the one EXTRA_SIZES / the singleton rule gives.
    extents = {v["va"]: len(v["slots"]) * 8 for v in vtables}
    for sym, _ns, rva, _c in data_syms:
        if sym.startswith("?_typeInfo@"):
            extents[ib + rva] = 0x38
        elif sym.startswith("?_singleton@"):
            extents[ib + rva] = 8
        elif sym in EXTRA_SIZES:
            extents[ib + rva] = EXTRA_SIZES[sym][0]
    ctor_of = scan_ctor_refs(pe, ib, inits, extents)

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
    for t in typeinfos:
        if t["va"] not in ctor_of:
            errors.append(f"{t['symbol']}: NOT built by any CRT initialiser — "
                          f"every BaseTypeInfo is supposed to be, so either the "
                          f"sweep is broken or this one is built somewhere else")
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
        ctor = ctor_of.get(ib + rva, [])
        if ctor and sym not in KNOWN_CTORS:
            errors.append(
                f"{sym}: the DLL CONSTRUCTS this at load (initialiser "
                f"{hex(ctor[0])}) and nothing in rt_data_mint.cpp reproduces it. "
                f"Minting it as zeroed storage would silently differ from the "
                f"loaded DLL — add it to KNOWN_CTORS and implement it.")
        storage.append({"symbol": sym, "va": ib + rva, "size": size,
                        "note": note, "namespace": ns,
                        "ctor": (hex(ctor[0]) + (f" (+{len(ctor)-1} more)" if len(ctor) > 1 else ""))
                                if ctor else ""})
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
          f"&kStorageBytes[{so}], {cstr(s['note'])}, {cstr(s['ctor'] or None)} }},")
    w("};")
    w("")

    open(args.out, "w", encoding="utf-8", newline="\n").write("\n".join(o) + "\n")
    print(f"wrote {args.out}")
    print(f"  scan       +{len(scan_added)} exported DATA symbols the CENSUS CANNOT SEE "
          f"(wrapper / named constant / table loop)")
    for sym, where in scan_added:
        print(f"                {sym}   [{where}]")
    print(f"  census     {len(unported)} unported exports -> {len(data_syms)} exported DATA "
          f"({checked} rows cross-checked against the round-1 harvest, 0 disagreements)")
    print(f"  vtables    {len(vtables):3d}  ({sum(len(v['slots']) for v in vtables)} slots, "
          f"{sum(1 for v in vtables if v['proof'].startswith('adjacent'))} count-proved by adjacency, "
          f"{sum(1 for v in vtables if v['proof'] == 'tail-not-a-pointer')} by tail probe)")
    print(f"  typeinfo   {len(typeinfos):3d}")
    print(f"  crt init   {len(inits):3d} initialisers; {len(ctor_of)} of the exported "
          f"DATA symbols are CONSTRUCTED at load "
          f"({sum(1 for s in storage if s['ctor'])} storage + {len(typeinfos)} BaseTypeInfo)")
    print(f"  storage    {len(storage):3d}  ({total} bytes; "
          f"{sum(1 for s in storage if s['symbol'].startswith('?_singleton@'))} singletons, "
          f"{sum(1 for s in storage if not s['symbol'].startswith('?_singleton@'))} other .data statics)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
