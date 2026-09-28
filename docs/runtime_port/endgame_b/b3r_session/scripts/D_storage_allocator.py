#!/usr/bin/env python3
"""B3R block 0, inventory D (STORAGE_ALLOCATOR): what it takes to delete the mint.

Re-run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b3r_session/scripts/D_storage_allocator.py
Writes docs/runtime_port/endgame_b/b3r_session/data/D_storage_allocator.tsv and prints
the per-spelling / per-directory counts the inventory quotes.

Method: every C/C++ source under the product and test trees is comment-stripped with
b3x_session/codegrep.py's strip() (strings kept), then matched line by line. Counts are
CODE lines. Build trees and worktrees are excluded (codegrep.py's EXCL).
"""
import os
import re
import sys
from collections import Counter, OrderedDict, defaultdict

REPO = os.getcwd()
sys.path.insert(0, os.path.join(REPO, "docs", "runtime_port", "endgame_b", "b3x_session"))
from codegrep import strip, EXCL  # noqa: E402

ROOTS = ["standalone", "runtime", "engine", "code", "dx11", "tests/unit"]
EXTS = (".cpp", ".h", ".hpp", ".inc", ".c")
OUT = os.path.join(REPO, "docs", "runtime_port", "endgame_b", "b3r_session", "data",
                   "D_storage_allocator.tsv")


def load_tree():
    files = OrderedDict()
    for r in ROOTS:
        for dp, dn, fs in os.walk(os.path.join(REPO, r)):
            dn.sort()
            for f in sorted(fs):
                p = os.path.join(dp, f)
                q = os.path.relpath(p, REPO).replace("\\", "/")
                if any(e in "/" + q for e in EXCL) or not q.endswith(EXTS):
                    continue
                try:
                    src = open(p, encoding="utf-8", errors="replace").read()
                except OSError:
                    continue
                files[q] = strip(src).split("\n")
    return files


FILES = load_tree()


def hits(pattern, skip=()):
    rx = re.compile(pattern)
    out = []
    for q, lines in FILES.items():
        if any(q.startswith(s) or q == s for s in skip):
            continue
        for i, ln in enumerate(lines, 1):
            if rx.search(ln):
                out.append((q, i, ln.strip()))
    return out


def dirkey(q):
    parts = q.split("/")
    if parts[0] == "code":
        return "/".join(parts[:3])
    if parts[0] in ("runtime",) and len(parts) > 2 and parts[1] == "src":
        return "/".join(parts[:3])
    if parts[0] == "tests":
        return "tests/unit"
    return "/".join(parts[:2]) if len(parts) > 2 else parts[0]


def per_dir(h):
    c = Counter(dirkey(q) for q, _, _ in h)
    return ";".join(f"{k}={v}" for k, v in sorted(c.items()))


def filelist(h, n=8):
    seen = []
    for q, i, _ in h:
        if q not in seen:
            seen.append(q)
    s = ";".join(seen[:n])
    return s + (f";(+{len(seen) - n} more)" if len(seen) > n else "")


def where(h):
    return ";".join(f"{q}:{i}" for q, i, _ in h[:8]) + (f";(+{len(h) - 8})" if len(h) > 8 else "")


ROWS = []
COLS = ["key", "kind", "storage_file", "storage_line", "writers", "readers",
        "reader_files", "becomes", "behaviour_change"]


def row(**kw):
    ROWS.append([str(kw.get(c, "")).replace("\t", " ").replace("\n", " ") for c in COLS])


def hexlit(addr):
    return r"\b0[xX]0*%x(?:[uU]?[lL]{0,2})\b" % addr


# ------------------------------------------------------------------------------------
# 1. The two minted storage rows
# ------------------------------------------------------------------------------------
MINT_INC = "standalone/rt_data_mint_tables.inc"
SING = "?_singleton@MemoryAllocator@System@@0PEAV12@EA"
DISH = "?_disableHandler@Debug@System@@0_NA"

sing_name = hits(re.escape(SING), skip=(MINT_INC, "standalone/rt_export_addrs.inc"))
sing_addr = hits(hexlit(0x1801fc718), skip=(MINT_INC, "standalone/rt_export_addrs.inc"))
sing_const = hits(r"\bAllocatorSingleton\b")
gpm = hits(r"\bg_pMemAllocSingleton\b")
gpm_w = hits(r"\bg_pMemAllocSingleton\s*=[^=]")
row(key=SING + " 0x1801fc718", kind="minted_storage", storage_file=MINT_INC, storage_line=13,
    writers="rtalloc::Construct/Destruct via SingletonSlot() (runtime/src/system/allocator.cpp:654/659), "
            "reached from Engine ctor/dtor door ??0/??1MemoryAllocator (engine_lifecycle.cpp:410/465)",
    readers=f"by name {len(sing_name)} ({where(sing_name)}); by address {len(sing_addr)} "
            f"({where(sing_addr)}); RtEngine::EngineAddr::AllocatorSingleton const {len(sing_const)-1} readers; "
            f"cached copy g_pMemAllocSingleton {len(gpm)} lines ({per_dir(gpm)})",
    reader_files=filelist(sing_name + sing_addr),
    becomes="static MemoryAllocator* System::MemoryAllocator::_singleton (zero-initialised, no dynamic "
            "initialiser); readers use it or MemoryAllocator::Get(); g_pMemAllocSingleton deleted",
    behaviour_change="no for the cell (same zeroed BSS, same writer); YES for g_pMemAllocSingleton: today it "
                     "is NULL until RunFactorySelfTest refreshes it (main.cpp:686) -- reading the live "
                     "static instead removes that dependency (separate titled commit)")

dis = hits(re.escape(DISH), skip=(MINT_INC, "standalone/rt_export_addrs.inc"))
dis_addr = hits(hexlit(0x1801fc708), skip=(MINT_INC, "standalone/rt_export_addrs.inc"))
row(key=DISH + " 0x1801fc708", kind="minted_storage", storage_file=MINT_INC, storage_line=12,
    writers="Video::Device::EnumerateVideoModes (runtime/src/video/enumerate_video_modes.cpp:144 =1, :152 =0)",
    readers=f"by name {len(dis)} ({where(dis)}); by address {len(dis_addr)}; value READ by nobody in the "
            "tree (retail reader Debug::ExceptionHandler 0x1800c5b6f is unported)",
    reader_files=filelist(dis),
    becomes="static bool System::Debug::_disableHandler (a minimal System::Debug class holding the three "
            "statics; functions stay unported STUB)",
    behaviour_change="no (write-only cell; the `if (disableHandler)` guards become dead -- remove them "
                     "with the conversion)")

# ------------------------------------------------------------------------------------
# 2. Everything else that reads or feeds the mint
# ------------------------------------------------------------------------------------
for q, i, ln in hits(r"\bRT_DATA_SLOT\b"):
    what = {"runtime/funcs/rt_port.h": "the macro definition: delete",
            "runtime/funcs/rt_resolve_cache.cpp": "a diagnostic string naming the macro: reword",
            "runtime/src/engine/singleton_ctors.cpp": "SlotMemoryAllocator(): DEFINED, NEVER CALLED -- delete",
            "runtime/src/system/allocator.cpp": "SingletonSlot(): becomes &MemoryAllocator::_singleton",
            "runtime/src/video/enumerate_video_modes.cpp":
                "DebugDisableHandlerSlot(): becomes &System::Debug::_disableHandler"}.get(q, "?")
    row(key=f"RT_DATA_SLOT {q}:{i}", kind="data_slot", storage_file=q, storage_line=i,
        readers=ln[:120], becomes=what, behaviour_change="no")

for pat, what in [
    (r"\bRtDataMintInit\b|rt_data_mint\.h", "call / include / declaration: delete with the mint"),
    (r"\bRtMintStorage\b|\bkMintStorage\b|\bkStorageBytes\b", "mint internals: delete with the files"),
    (r"\[RtMint\]", "boot-log lines: gone; re-cut the gate's expected lines in writing"),
    (r"rt_data_mint_tables\.inc", "a reader of the generated file"),
]:
    for q, i, ln in hits(pat):
        row(key=f"mint {q}:{i}", kind="other", storage_file=q, storage_line=i, readers=ln[:120],
            becomes=what, behaviour_change="no")

# ------------------------------------------------------------------------------------
# 3. Allocation doors: one row per spelling, counted per directory
# ------------------------------------------------------------------------------------
MA = r"\?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"
MF = r"\?MemFree@MemoryAllocator@System@@SAXPEAX@Z"
REGFILES = ("runtime/src/system/allocator.cpp", "standalone/address_registry.cpp")
SPELLINGS = [
    ("fn_MemAlloc(g_pMemAllocSingleton,...) -- engine_funcs global = member Allocate door 0x1800c6e60 "
     "(composed SwapAllocate)", r"\bfn_MemAlloc\b(?!_t)",
     "System::MemoryAllocator::_singleton->Allocate(size, align) through a standalone-locked member "
     "(or MemAlloc static)", "yes if mapped to MemAlloc: SwapAllocate also runs AllocStats_RecordAlloc and "
     "RedzoneAlloc_TeardownOp; BANISHED_REDZONE rewrites this pointer"),
    ("fn_MemFree(singleton, p) -- engine_funcs lambda over fn_MemFreeStatic", r"\bfn_MemFree\b(?!_t|Static)",
     "System::MemoryAllocator::MemFree(p)", "no (the lambda already ignores the singleton)"),
    ("fn_MemFreeStatic -- ?MemFree@ by name", r"\bfn_MemFreeStatic\b(?!_t)",
     "System::MemoryAllocator::MemFree(p)", "BANISHED_REDZONE rewrites this pointer (ShimFree)"),
    ("fn_MemAllocStatic -- ?MemAlloc@ by name", r"\bfn_MemAllocStatic\b(?!_t)",
     "System::MemoryAllocator::MemAlloc(size, align)", "no"),
    ("g_pMemAllocSingleton -- cached value of the minted cell", r"\bg_pMemAllocSingleton\b",
     "System::MemoryAllocator::_singleton (or the argument disappears with fn_MemAlloc)",
     "yes: cached NULL until main.cpp:686 refreshes it"),
    ("RT_EXPORT_ONCE / RtResolveCached / AtomicExport / ResolveExport / tables naming ?MemAlloc@", MA,
     "System::MemoryAllocator::MemAlloc(size, align)", "no"),
    ("RT_EXPORT_ONCE / RtResolveCached / AtomicExport / ResolveExport / tables naming ?MemFree@", MF,
     "System::MemoryAllocator::MemFree(p)", "no"),
    ("exe import slots 0x140152b60 / 0x140152b68 (ResolveAddr(Module::Exe) / RSlot / R<>)",
     r"\b0[xX]140152[bB]6[08](?:[uU]?[lL]{0,2})\b","System::MemoryAllocator::MemAlloc / MemFree",
     "yes: BANISHED_REDZONE rewrites the slot storage (redzone_alloc.cpp:346-349); rtcoll::DoorAlloc/DoorFree "
     "read the slot first"),
    ("Runtime allocator addresses 0x1800c7a50/7ad0/6e60/6c00 by address",
     r"\b0[xX]1800[cC](?:7[aA]50|7[aA][dD]0|6[eE]60|6[cC]00)(?:[uU]?[lL]{0,2})\b",
     "MemAlloc / MemFree / Allocate / Free direct", "no"),
    ("0x180009c30 FreeLockedIgnoringSelf (2-arg free, first ignored)", r"\b0[xX]180009[cC]30(?:[uU]?[lL]{0,2})\b",
     "a named 2-arg free helper next to MemoryAllocator, called directly", "no"),
    ("rtcoll::MemAlloc / MemFree / DoorAlloc / DoorFree", r"\brtcoll::(MemAlloc|MemFree|DoorAlloc|DoorFree)\b|"
     r"\b(DoorAlloc|DoorFree)\s*\(\s*\)", "System::MemoryAllocator::MemAlloc / MemFree",
     "yes while REDZONE rewrites the exe slot the door reads"),
    ("Object::detail::InternalMemAlloc / InternalMemFree (RT_EXPORT_ONCE door; unit build = malloc/free)",
     r"\bInternalMem(Alloc|Free)\b", "System::MemoryAllocator::MemAlloc / MemFree, with the unit build's "
     "host arm moved into the allocator (tests link malloc/free)", "no"),
    ("RtClass_OperatorDelete / RtUi_ClassOperatorDelete (bodies of the 28 + 60 ??3 rows)",
     r"\bRtClass_OperatorDelete\b|\bRtUi_ClassOperatorDelete\b",
     "System::MemoryAllocator::MemFree", "no"),
]
DOOR_WORDS = ["RegisterRuntimeExport", "RegisterDispatchSlot", "RT_EXPORT_ONCE", "RtResolveCached",
              "AtomicExport", "ResolveExport", "Dx11ResolveRuntimeImport", "Exp<", "const char*", "{"]


def door_of(q, i):
    """The binding a line naming a mangled literal belongs to: this line or the two above."""
    lines = FILES[q]
    for j in (i - 1, i - 2, i - 3):
        if j < 0:
            break
        for w in DOOR_WORDS:
            if w in lines[j]:
                return w
    return "?"


GENERATED = (MINT_INC, "standalone/rt_export_addrs.inc")
for label, pat, becomes, beh in SPELLINGS:
    h = hits(pat, skip=GENERATED)
    extra = ""
    if pat in (MA, MF):
        extra = " | by binding: " + ";".join(
            f"{k}={v}" for k, v in sorted(Counter(door_of(q, i) for q, i, _ in h).items()))
    row(key=label, kind="alloc_door", readers=f"{len(h)} code lines: {per_dir(h)}{extra}",
        reader_files=filelist(h), becomes=becomes, behaviour_change=beh)

# per-file allocator wrappers: file-local functions / typedef'd statics named MemAlloc/MemFree
wrap = hits(r"^(?:static\s+|inline\s+)*(?:void\s*\*|void)\s+(?:[A-Za-z_]\w*::)?(?:Mem(?:Alloc|Free)\w*|"
            r"\w*Mem(?:Alloc|Free))\s*\([^;]*\)\s*\{?\s*$|^(?:static\s+|inline\s+)*(?:void\s*\*|void)\s+"
            r"(?:[A-Za-z_]\w*::)?\w*Mem(?:Alloc|Free)\w*\s*\([^;]*\)\s*\{")
row(key="per-file allocator wrapper DEFINITIONS (void* XMemAlloc(...) { door } / void XMemFree(...))",
    kind="alloc_door", readers=f"{len(wrap)} definitions: {per_dir(wrap)}", reader_files=filelist(wrap, 12),
    becomes="deleted; callers call System::MemoryAllocator::MemAlloc/MemFree", behaviour_change="no")

# ------------------------------------------------------------------------------------
# 3b. operator new/delete copies registered by name and address
# ------------------------------------------------------------------------------------
OPDEL_TABLES = ["runtime/src/ui/ui_operator_delete.cpp", "runtime/src/object/class_operator_delete.cpp"]
rx_row = re.compile(r'\{\s*0[xX]([0-9a-fA-F]+)[^"]*"([^"]+)"')
for t in OPDEL_TABLES:
    rows = []
    for ln in FILES.get(t, []):
        m = rx_row.search(ln)
        if m:
            rows.append((int(m.group(1), 16), m.group(2)))
    rd_name = rd_addr = 0
    rfiles = []
    for a, nm in rows:
        hn = hits(re.escape('"' + nm + '"'), skip=(t, "standalone/rt_export_addrs.inc"))
        ha = hits(hexlit(a), skip=(t, "standalone/rt_export_addrs.inc"))
        rd_name += len(hn)
        rd_addr += len(ha)
        rfiles += hn + ha
    row(key=f"{t}: {len(rows)} ??3 operator delete rows (name+address, one shared MemFree body)",
        kind="alloc_door", storage_file=t,
        readers=f"by name {rd_name} lines, by address {rd_addr} lines ({where(rfiles)})",
        reader_files=filelist(rfiles),
        becomes="DEAD if 0 readers (the C++ classes declare their own operator delete / inherit the root's)",
        behaviour_change="no")
b2b = "runtime/src/system/closure_batch2b.cpp"
for a, nm in [(0x1800028a0, "??3MaterialInstance@Graphics@@SAXPEAX@Z"), (0x18001e990, "??3ImageBuffer@Video@@SAXPEAX@Z"),
              (0x1800251e0, "??3PackageFile@IO@@SAXPEAX@Z"), (0x180061ae0, None), (0x180011640, None),
              (0x180034d60, None)]:
    hn = hits(re.escape('"' + nm + '"'), skip=(b2b, "standalone/rt_export_addrs.inc")) if nm else []
    ha = hits(hexlit(a), skip=(b2b, "standalone/rt_export_addrs.inc"))
    row(key=f"0x{a:x} {nm or '(unnamed)'}", kind="alloc_door", storage_file=b2b,
        readers=f"by name {len(hn)}, by address {len(ha)} ({where(hn + ha)})", reader_files=filelist(hn + ha),
        becomes="DEAD if 0 readers, else the reader calls MemFree/MemAlloc", behaviour_change="no")

# allocator's own registrations
for q, i, ln in hits(r"Register(RuntimeExport|RuntimeAddr|DispatchSlot)\s*\(.*(MemoryAllocator@System|"
                     r"0x1800c7a50|0x1800c7ad0|0x1800c6e60|0x1800c6c00|0x180009c30|0x140152b6[08])"):
    m = re.search(r'"([^"]+)"', ln) or re.search(r"\(\s*(0[xX][0-9a-fA-F]+)", ln)
    k = m.group(1) if m else ""
    if k.startswith("0"):
        rd = [h for h in hits(hexlit(int(k, 16)), skip=GENERATED) if not re.search(r"Register\w+\s*\(", h[2])]
    else:
        rd = [h for h in hits(re.escape('"' + k + '"'), skip=GENERATED) if not re.search(r"Register\w+\s*\(", h[2])]
    row(key=f"registration {q}:{i} {k}", kind="alloc_door", storage_file=q, storage_line=i, writers=ln[:140],
        readers=f"by {'address' if k.startswith('0') else 'name'} {len(rd)} ({where(rd)})",
        reader_files=filelist(rd),
        becomes="deleted once its readers are direct" if rd else "deleted (no reader of this key)",
        behaviour_change="no")

# ------------------------------------------------------------------------------------
# 4. Other storage-like rows: RegisterExeAddr data rows, .rdata floats, singleton exrefs
# ------------------------------------------------------------------------------------
IMPORT_LO, IMPORT_HI = 0x140151000, 0x140152c18
data_rows = []
for q, i, ln in hits(r"\bRegisterExeAddr\s*\(\s*0[xX][0-9a-fA-F]+"):
    m = re.search(r"RegisterExeAddr\s*\(\s*0[xX]([0-9a-fA-F]+)\s*,\s*(.*)", ln)
    a = int(m.group(1), 16)
    if a >= 0x140151000:
        tgt = re.search(r"&\s*([A-Za-z_]\w*)", m.group(2))
        data_rows.append((a, q, i, tgt.group(1) if tgt else m.group(2)[:40]))
# the table-driven .rdata float block
rf = "standalone/reg_rdata_float_consts.cpp"
for i, ln in enumerate(FILES.get(rf, []), 1):
    m = re.search(r"\{\s*0[xX]([0-9a-fA-F]+)ULL\s*,\s*0[xX]([0-9a-fA-F]+)U", ln)
    if m:
        data_rows.append((int(m.group(1), 16), rf, i, f"g_storage[] bits=0x{m.group(2)}"))
count_addr = Counter(a for a, _, _, _ in data_rows)
for a, q, i, tgt in data_rows:
    reg_lines = {(q2, i2) for a2, q2, i2, _ in data_rows if a2 == a}
    ha = [h for h in hits(hexlit(a)) if (h[0], h[1]) not in reg_lines]
    hv = []
    if re.match(r"^[A-Za-z_]\w*$", tgt):
        hv = [h for h in hits(r"\b%s\b" % re.escape(tgt)) if (h[0], h[1]) != (q, i)]
    if a >= IMPORT_LO and a < IMPORT_HI:
        kind, becomes = "exe_data_row", "an exe IMPORT SLOT address registered through the EXE-ADDRESS door " \
                                        "(not a dispatch slot): the reader calls the Runtime function directly"
    elif a < 0x1401c0000:
        kind, becomes = "rdata_const", "a namespace-scope constexpr float/bits literal next to its reader"
    else:
        kind, becomes = "exe_data_row", "a class static / namespace-scope object in code/Game (singleton cell)"
    dup = f"; REGISTERED {count_addr[a]}x (last binding wins)" if count_addr[a] > 1 else ""
    row(key=f"0x{a:x} -> {tgt}", kind=kind, storage_file=q, storage_line=i, writers="registration" + dup,
        readers=f"by address {len(ha)} ({where(ha)}); by storage name {len(hv)}",
        reader_files=filelist(ha + hv), becomes=becomes,
        behaviour_change="no" if not dup else "no if the duplicate bindings hold the same bits -- check")

# engine_funcs data externs (storage exrefs bound by initialiser)
EF = "engine/funcs/engine_funcs.cpp"
for name in ["g_pMemAllocSingleton", "g_pGameWorldSingleton", "g_pKeyMapSingleton",
             "g_pInputConfigSingletonExref", "g_pAgeMultiplier", "g_pNotifyTimerDuration",
             "g_pNewGameSettings", "g_pGameSettingsPtr", "g_pGameSingleton", "g_fUVScale"]:
    init = [h for h in hits(r"\b%s\s*=" % name) if h[0] == EF]
    addr = ""
    for q, i, ln in init:
        m = re.search(r"0[xX][0-9a-fA-F]+", ln) or re.search(
            r"0[xX][0-9a-fA-F]+", FILES[EF][i] if i < len(FILES[EF]) else "")
        if m:
            addr = m.group(0)
    h = [x for x in hits(r"\b%s\b" % name) if x[0] not in ("engine/funcs/engine_funcs.h",) and
         not (x[0] == EF and re.search(r"\b%s\s*=" % name, x[2]))]
    row(key=f"{name} ({addr or 'no address'})", kind="singleton_exref", storage_file=EF,
        storage_line=init[-1][1] if init else "", writers=";".join(f"{q}:{i}" for q, i, _ in init),
        readers=f"{len(h)} code lines: {per_dir(h)}", reader_files=filelist(h),
        becomes="the class static / namespace object the registered cell already is (read it directly)",
        behaviour_change="no")

# named carry-overs and one-offs
ONE_OFFS = [
    ("CurrentBuildId free door", r"\bfn_Configuration_CurrentBuildId\b|\?CurrentBuildId@Configuration@System@@",
     "static System::String System::Configuration::CurrentBuildId() once 0x1800c4c60's slot-zeroing question "
     "is settled (Configuration.h:224)", "open question: retail Set()s the return slot without zeroing it"),
    ("Math::Vector3::operator== import slot 0x140152a70", r"\b0[xX]140152[aA]70\b|\bs_slot_Vec3Eq\b",
     "a == b on Math::Vector3 (RtMath_Vector3_Equals is ported)", "no"),
    ("0x1401c22d0 tile-shape LUT (unregistered exe .data)", r"\b0[xX]1401[cC]22[dD]0\b",
     "the local kTileShapeLut literal; delete the ResolveAddr", "yes (log only): each call today hits "
     "NotImplementedStub_impl before the fallback"),
    ("0x1401c23d0 road mask LUT (unregistered exe .data)", r"\b0[xX]1401[cC]23[dD]0\b",
     "the local kZwLutBytes literal; delete the ResolveAddr", "yes (log only): same NOT IMPLEMENTED hit"),
    ("0x14015d424 '.rsc' literal", r"\b0[xX]14015[dD]424\b", "already the literal \".rsc\" (spritecaptureui.cpp:200)",
     "no -- fixed since B.3.9 §10"),
    ("0x140151738 ?_singleton@Configuration@Input@@ import slot (data exref)", r"\b0[xX]140151738\b",
     "&Input::Configuration::_singleton", "no"),
    ("RtEngine::EngineAddr::AllocatorSingleton constant", r"\bAllocatorSingleton\b", "deleted (0 readers)", "no"),
]
for key, pat, becomes, beh in ONE_OFFS:
    h = hits(pat)
    row(key=key, kind="other", readers=f"{len(h)} code lines ({where(h)})", reader_files=filelist(h),
        becomes=becomes, behaviour_change=beh)

# The row class (BLOCK0_BRIEF words) leads the `becomes` column.
def row_class(r):
    key, kind, readers, becomes = r[0], r[1], r[5], r[7]
    if kind in ("minted_storage", "exe_data_row", "rdata_const", "singleton_exref"):
        return "STORAGE"
    if kind == "data_slot":
        return "DEAD" if ("definition" in becomes or "string" in becomes or "NEVER CALLED" in becomes) else "DIRECT"
    if key.startswith("mint "):
        return "DEAD"
    if kind == "alloc_door":
        if key.startswith("registration "):
            return "DEAD-after-DIRECT" if "no reader" not in becomes else "DEAD"
        if re.search(r"by name 0,? .*by address 0\b|by name 0 lines, by address 0 lines", readers):
            return "DEAD"
        return "DIRECT"
    if "LUT" in key:
        return "STORAGE"
    if key.startswith("CurrentBuildId"):
        return "DIRECT (open ABI question)"
    if readers.startswith("0 code lines") or key.startswith("RtEngine::EngineAddr::AllocatorSingleton"):
        return "DEAD"
    return "DIRECT"


for r in ROWS:
    r[7] = row_class(r) + ": " + r[7]

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("\t".join(COLS) + "\n")
    for r in ROWS:
        f.write("\t".join(r) + "\n")

# console summary
kinds = Counter(r[1] for r in ROWS)
print(f"{len(ROWS)} rows -> {os.path.relpath(OUT, REPO)}")
for k, v in sorted(kinds.items()):
    print(f"  {k:16s} {v}")
cls = Counter((r[7].split(":")[0], r[2].split("/")[0] if r[2] else "-") for r in ROWS)
print("row class x storage directory:")
for (c, d), v in sorted(cls.items()):
    print(f"  {c:28s} {d:12s} {v}")
print("MemoryAllocator code lines:", len(hits(r"MemoryAllocator")))
for label, pat, _, _ in SPELLINGS:
    h = hits(pat, skip=GENERATED)
    print(f"  {len(h):4d}  {label[:70]}  [{per_dir(h)}]")
    if pat in (MA, MF):
        print("        by binding:", dict(Counter(door_of(q, i) for q, i, _ in h)))
