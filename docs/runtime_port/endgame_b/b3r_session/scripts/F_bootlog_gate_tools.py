#!/usr/bin/env python3
"""B3R block 0, inventory F (BOOTLOG_GATE_TOOLS): the boot-log lines, tools, unit tests and
env-gated harness code that depend on the registry, and what each becomes in the teardown.

Run from the repo root with no arguments:
    python docs/runtime_port/endgame_b/b3r_session/scripts/F_bootlog_gate_tools.py
It writes docs/runtime_port/endgame_b/b3r_session/data/F_bootlog_gate_tools.tsv and prints the
per-kind / per-verdict counts.

What is COMPUTED from the tree on every run: every source_file / source_line (the printf format
string is located in comment-stripped code), every read_by list (the gate kit, tools, skills,
rules, Makefile are grepped for the line's token or the tool's name), every binding count (tests,
harness files), every stub-address reader list.
What is EMBEDDED (a run cannot be made in block 0): the values the lines print today, taken from
the B3X close runs (gate.sh closeclean + stepclose.sh on 0419d0ce, session e971e316's
scratchpad kit/runs/, not committed). They are quoted in the note column as "now: ...".
"""
import glob
import os
import re
import sys
from collections import Counter, OrderedDict

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", ".."))
os.chdir(REPO)
sys.path.insert(0, os.path.join(REPO, "docs", "runtime_port", "endgame_b", "b3x_session"))
from codegrep import strip  # noqa: E402  comment-stripper that keeps string literals

OUT = "docs/runtime_port/endgame_b/b3r_session/data/F_bootlog_gate_tools.tsv"
PROD = ("code", "runtime", "standalone", "engine", "dx11")
EXCL = ("standalone/build", "tests/unit/build", ".claude/worktrees", "/build/", "/build-", "__pycache__")
KIT = "docs/runtime_port/endgame_b/b3x_session"


def rel(p):
    return p.replace("\\", "/").lstrip("./")


def walk(roots, exts):
    out = []
    for r in roots:
        if os.path.isfile(r):
            out.append(rel(r))
            continue
        for dp, _dn, fs in os.walk(r):
            for f in fs:
                q = rel(os.path.join(dp, f))
                if any(e in q for e in EXCL) or not q.endswith(exts):
                    continue
                out.append(q)
    return sorted(out)


_cache = {}


def code_of(path):
    if path not in _cache:
        try:
            _cache[path] = strip(open(path, encoding="utf-8", errors="replace").read())
        except OSError:
            _cache[path] = ""
    return _cache[path]


def raw_of(path):
    try:
        return open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return ""


CPP = (".cpp", ".h", ".inc", ".hpp", ".c")
PROD_FILES = walk(PROD, CPP)


def find_code(pattern, files=PROD_FILES, limit=None):
    """[(file, line)] of comment-stripped code lines matching pattern."""
    rx = re.compile(pattern)
    hits = []
    for f in files:
        for n, line in enumerate(code_of(f).split("\n"), 1):
            if rx.search(line):
                hits.append((f, n))
                if limit and len(hits) >= limit:
                    return hits
    return hits


# ---- who READS a log line / a tool: the gate kit, tools, skills, rules, Makefile ----------
READER_FILES = (walk([KIT], (".sh", ".py"))
                + walk(["tools"], (".py", ".sh", ".ps1", ".java"))
                + walk([".claude/skills", ".claude/rules"], (".md",))
                + ["Makefile", "CLAUDE.md"])


def readers(pattern, exclude=()):
    rx = re.compile(pattern)
    out = []
    for f in READER_FILES:
        if f in exclude:
            continue
        for n, line in enumerate(raw_of(f).split("\n"), 1):
            if rx.search(line):
                out.append(f"{f}:{n}")
    return out


def short(lst, k=6):
    if not lst:
        return "nobody"
    s = ", ".join(lst[:k])
    return s + (f" (+{len(lst) - k} more)" if len(lst) > k else "")


ROWS = []


def row(item, kind, src, line, dep, read_by, verdict, note):
    ROWS.append((item, kind, src, str(line), dep, read_by, verdict, note.replace("\t", " ")))


def raw_line(path, pattern):
    for n, line in enumerate(raw_of(path).split("\n"), 1):
        if re.search(pattern, line):
            return n
    return "-"


def first(pattern, files=PROD_FILES):
    h = find_code(pattern, files, limit=1)
    return h[0] if h else ("(no producer in code)", "-")


# =============================================================================================
# 1. BOOT / EXIT LOG LINES
# =============================================================================================
PROV = "B3X closeclean @0419d0ce"
LOG = [
    # item, producer regex (in code, strings kept), reader regex, depends_on, verdict, note
    ("[RtResolve] 0 misses", r'"\[RtResolve\] 0 misses', r"RtResolve\\?\]", "rt_name",
     "invariant",
     f"now: present on every route ({PROV}; mods/ui/tool routes at stepclose). Counts misses of the CACHING "
     "door only (RtResolveCached / RT_EXPORT_ONCE / RT_DATA_SLOT); a plain ResolveExport miss prints "
     "[RtUnresolved], a ResolveAddr(Runtime) miss after init prints NOTHING. gate.sh route() flags a route "
     "WITHOUT this line as a problem, so the commit that deletes rt_resolve_cache.cpp must re-cut gate.sh "
     "(the line disappears; it is not a regression)"),
    ("[RtResolve] MISS \"<sym>\" -> NULL", r'"\[RtResolve\] MISS', r"RtResolve\\?\]", "rt_name", "invariant",
     "now: 0 lines. Target 0 on every route through the teardown; dies with rt_resolve_cache.cpp"),
    ("[RtResolve] N MISSES across M symbols", r'"\[RtResolve\] %llu MISSES', r"RtResolve\\?\]", "rt_name",
     "invariant", "now: absent. Target absent; dies with rt_resolve_cache.cpp"),
    ("[RtResolveEarly] 0 registry-aware resolves before RuntimePortInit()", r'"\[RtResolveEarly\] 0 registry',
     r"RtResolveEarly", "rt_name|rt_addr", "delete",
     "now: the 0 line on every route. Read by nobody. Dies with RtResolveNoteEarly and RuntimePortInit"),
    ("[RtSlotAudit] 0 names ... CACHING door", r'"\[RtSlotAudit\] 0 names', r"RtSlotAudit", "rt_name", "delete",
     "now: the 0 line on every route. Read by nobody. Dies with RtResolveCached"),
    ("[RtMint] registered N by name, N by address", r'"\[RtMint\] registered', r"RtMint\\?\] \(minted\|registered\)|\[RtMint\\?\] \(minted",
     "mint", "recut",
     "now: 'registered 2 by name, 2 by address'. mergegate.sh prints it (display, no check). Re-cut 2/2 -> 1/1 -> 0/0 "
     "as MemoryAllocator::_singleton and Debug::_disableHandler become class statics; the line dies with rt_data_mint.cpp"),
    ("[RtMint] minted N storage symbols (B bytes)", r'"\[RtMint\] minted', r"RtMint\\?\] \(minted", "mint", "recut",
     "now: 'minted 2 storage symbols (32 bytes)'. Same re-cut as the registered line; non-increasing"),
    ("[RtMint] TRAP", r'RtMint\] TRAP', r"RtMint\\?\] TRAP", "mint", "delete",
     "NO PRODUCER in code since B.3.9 (the vtable mint that printed it is deleted). gate.sh / uiroutes.sh / "
     "stepclose.sh / mods_route.sh / devlost*.sh count it and can only ever read 0: a dead check. Delete from the kit"),
    ("GATE WARN", r'GATE WARN', r"GATE WARN", "none", "delete",
     "NO PRODUCER anywhere in product code. gate.sh counts it and flags != 0: a dead check. Delete from the kit"),
    ("[RtPort] runtime registry: N exports, M addrs", r'"\[RtPort\] runtime registry', r"runtime registry",
     "rt_name|rt_addr", "recut",
     "now: '1347 exports, 1817 addrs' (unchanged deaf604c -> 0419d0ce). Map SIZES incl. mirrored entries "
     "(rt_export_addrs.inc mirrors each name row into the address map and back), so a row deletion moves BOTH "
     "numbers by an amount a grep of Register* lines cannot predict. Read by nobody today. Re-cut per merge, "
     "non-increasing; dies with RuntimePortInit"),
    ("[RtPort] <family>: N exports registered (per-family lines)", r'"\[RtPort\][^"]*regist', r"RtPort\\?\]\.\*regist",
     "rt_name|rt_addr", "delete",
     "Print sites = code lines with '[RtPort]' and 'regist' in one string (count below). Read by nobody. Each dies with its "
     "Register function. WARNING: the same [RtPort] tag carries ATTENDANCE lines that are NOT registry lines and "
     "stepclose.sh reads one ('Audio::Stream ATTENDANCE'): a sweep keyed on the tag must not take them"),
    ("[RtPort] Engine lifecycle: REFUSING to register", r'"\[RtPort\] Engine lifecycle: REFUSING', r"REFUSING to register",
     "rt_name", "invariant", "now: absent. Target absent; dies with the Register call"),
    ("[RtUnresolved] <sym> (asked by ...)", r'"\[RtUnresolved\] %s  \(asked', r"RtUnresolved", "rt_name", "invariant",
     "now: 0 lines on newgame/tutorial/loadgame. NOT CHECKED BY ANY GATE SCRIPT today. This is the only line that "
     "names a by-name reader of a DELETED name row at run time: add 'count == 0' to gate.sh (a check on an "
     "existing line, not a new instrument). Dies with ResolveExport"),
    ("[RtUnresolved] ==== N distinct symbols resolved to NOTHING", r'"\[RtUnresolved\] ==== %zu', r"RtUnresolved",
     "rt_name", "invariant", "now: absent (DumpUnresolvedSymbols prints nothing when empty). Target absent"),
    ("[STANDALONE] NOT IMPLEMENTED: exe function at 0x...", r'NOT IMPLEMENTED: exe function', r"NOT IMPLEMENTED",
     "exe_addr", "recut",
     "now: newgame 10 addresses, tutorial 8, loadgame 1, menu routes 1 (0x140002280), tool routes 10-11, "
     "in-game save 9 (rows 'stub:' below). The ONLY run-time trace of an exe-address reader whose row is gone. "
     "stepclose.sh compares the SET against the baseline but its sed turns every address into ADDR, so the set "
     "collapses to one line and '0 new, 0 gone' is guaranteed: the comparison is broken (fix: keep the address). "
     "Re-cut: the set may only shrink; each shrink is written down (address, the reader that went direct, commit)"),
    ("[TRACE] ResolveAddr Exe 0x... -> (registered|NOT IMPLEMENTED)", r'"\[TRACE\] ResolveAddr Exe', r"TRACE\\?\] ResolveAddr",
     "exe_addr", "delete",
     "Printed only inside TraceExeBegin/End brackets (one caller: standalone/splash_loop_stubs.cpp:192). Not seen "
     "on the default routes. Read by nobody. Dies with ResolveAddr(Module::Exe)"),
    ("[Resolve-Standalone] <module>: base=/not loaded", r'"\[Resolve-Standalone\] %s: (base|not)', r"Resolve-Standalone",
     "none", "keep_changed",
     "_DEBUG only. Module table rows (Exe, Runtime-x64.dll 'not loaded'). Read by nobody. Goes with ModuleInfo when "
     "resolve_standalone.cpp is deleted (keep the 'Runtime-x64.dll: never loaded' FATAL check somewhere)"),
    ("[Resolve-Standalone] Address registry: N entries", r'"\[Resolve-Standalone\] Address registry', r"Resolve-Standalone",
     "exe_addr", "delete",
     "now: '3 entries' (registered BEFORE ResolveInit, i.e. before AddressRegistryInit; the three were not traced "
     "- blind spot). _DEBUG only. Read by nobody"),
    ("[AddressRegistry] N addresses registered", r'"\[AddressRegistry\] %zu', r"AddressRegistry\\?\]", "exe_addr", "recut",
     "now: '759 addresses registered', printed TWICE (AddressRegistryInit under _DEBUG, and main.cpp after INIT "
     "SUCCESS). Includes RegisterDispatchSlot cells and vtable/typeId storage. Read by nobody. Re-cut per merge, "
     "non-increasing; dies with address_registry.cpp"),
    ("[Standalone] Coverage: N / 582 exe addresses registered (P%)", r'"\[Standalone\] Coverage:', r"Coverage:",
     "exe_addr", "delete",
     "now: '759 / 582 (130.4%)'. The denominator is a stale constant; the number means nothing. Read by nobody. "
     "Delete in the first standalone/main.cpp commit"),
    ("[UnregCensus] phase -> / ==== N distinct unregistered addresses / 0x... first= total=", r'"\[UnregCensus\]',
     r"UNREG_CENSUS|UnregCensus|first=\(", "exe_addr", "keep_changed",
     "BANISHED_UNREG_CENSUS only (no gate route sets it). Parsed by tools/unreg_full.py and tools/unreg_loadpath.py. "
     "Useful while STUB rows exist; dies with ResolveAddr(Module::Exe) and SetUnregPhase (callers in "
     "code/Game/Game/gameupdateloop*.cpp)"),
    ("[ExtLoad] calls=N openFileFail=N ...", r'"\[ExtLoad\] calls=', r"ExtLoad\\?\] calls=", "none", "invariant",
     "now: newgame calls=5683 openFileFail=0, tutorial calls=5773 openFileFail=12, loadgame calls=4758. NOT a "
     "registry line; gate.sh prints it and NEXT_SESSION §1 pins it. Stays an invariant through the teardown"),
    ("typetable (FactoryReg / _firstTypeInfo / typeId= / ResolveTypeIdByNameLazy)", r'"\[FactoryReg\] ResolveTypeIdByNameLazy',
     r"ResolveTypeIdByNameLazy", "rt_name", "invariant",
     "now: 428 lines md5 b983f144. Registry-adjacent: a by-name lazy lookup converted to X::TypeId() drops its line, "
     "which gate.sh already tolerates. Invariant otherwise"),
    ("crash log 'Registry: N registered, M stubs hit'", r'"Registry: %zu registered', r"stubs hit", "exe_addr", "delete",
     "standalone_crash.log only (read by a person, triage-run skill). Dies with GetRegistryStats"),
    ("crash log 'MemoryAllocator::_singleton cell= value='", r'MemoryAllocator::_singleton cell=', r"_singleton cell",
     "mint", "keep_changed",
     "A READER of the minted storage row BY ADDRESS (ResolveAddr(Module::Runtime, 0x1801fc718)) in the crash "
     "handler. Must read the class static in the same commit that retires ?_singleton@MemoryAllocator (carry-over 1)"),
    ("[SeatShuffle] 0x140089470 registered", r'"\[SeatShuffle\]', r"SeatShuffle", "exe_addr", "keep_changed",
     "BANISHED_LIVE_SEAT_SHUFFLE only. The ONLY flag-conditional registry row: the flag decides whether "
     "0x140089470 is registered, and FindAvailableBuilding skips the shuffle when the door is null. Converting the "
     "reader to a direct call must become 'if (Opt().liveSeatShuffle)' at the call site, or the default changes"),
    ("[Standalone] WARNING: Some engine functions not resolved", r'Some engine functions not resolved',
     r"engine functions not resolved", "engine_funcs", "invariant",
     "now: absent. EngineFuncsInit() returned true. Target absent; dies with EngineFuncsInit"),
]
for item, prod_rx, read_rx, dep, verdict, note in LOG:
    f, ln = first(prod_rx)
    n_sites = len(find_code(prod_rx))
    rb = readers(read_rx)
    extra = f" [{n_sites} producing code line(s)]" if n_sites > 1 else ""
    row(item, "log_line", f, ln, dep, short(rb), verdict, note + extra)

# the two TOOL OUTPUT lines the gate reads (printed by a tool, not by the exe)
row("gate 'dispatch: checked N ResolveExport registrations, 0 name mismatch(es); 0 ... more than once'",
    "log_line", "tools/dispatch_slot_map.py", raw_line("tools/dispatch_slot_map.py", r'f"checked \{len'),
    "exe_addr", short(readers(r"dispatch_slot_map.py --check", exclude=("tools/dispatch_slot_map.py",))), "recut",
    "now: 'checked 35 ..., 0 name mismatch(es); 0 address(es) registered more than once'. 0 / 0 stay invariants; "
    "35 falls as RegisterDispatchSlot(addr, ResolveExport(...)) rows go and ends at 0 (then the check is vacuous "
    "and is deleted with address_registry.cpp)")
row("gate 'mintgen: --check: ... UP TO DATE (0 vtables, 0 BaseTypeInfo, N storage)'", "log_line",
    "tools/rt_mint_gen.py", raw_line("tools/rt_mint_gen.py", r"is UP TO DATE"), "mint",
    short(readers(r"rt_mint_gen.py --check", exclude=("tools/rt_mint_gen.py", "tools/runtime_census.py"))), "recut",
    "now: 'UP TO DATE (0 vtables, 0 BaseTypeInfo, 2 storage)'. Re-cut 2 -> 0; UP TO DATE stays the invariant while "
    "the generator exists; a RETIRED-ROW-STILL-READ refusal is a gate failure (keep that mechanism)")

# =============================================================================================
# 2. TOOLS
# =============================================================================================
TOK = OrderedDict([
    ("exe_addr", r"RegisterExeAddr|RegisterDispatchSlot|address_registry|reg_\*|reg_\w+\.cpp|Module::Exe|NOT IMPLEMENTED|UnregCensus|slotmap|first=\(\\w\+\)"),
    ("rt_name", r"RegisterRuntimeExport|ResolveExport|RT_EXPORT_ONCE|RtResolveCached|RT_DATA_SLOT"),
    ("rt_addr", r"RegisterRuntimeAddr|Module::Runtime|rt_export_addrs"),
    ("mint", r"rt_data_mint|rt_mint_gen|rt_mint_retired|RtMint"),
    ("engine_funcs", r"engine_funcs"),
])
TOOLS = OrderedDict([
    ("tools/dispatch_slot_map.py", ("keep_changed",
        "Parses standalone/address_registry.cpp + standalone/reg_*.cpp for RegisterDispatchSlot(addr, ResolveExport(\"...\")) "
        "and checks each name against Application-x64.exe's PE import table (pefile on the RETAIL exe in the game dir). "
        "--check: mismatches, not-an-import-slot, SHADOWED (registered twice). Gate: gate.sh every merge. As import-slot "
        "rows are deleted 'checked N' falls (35 now) and mismatches stay 0 trivially; at 0 rows --check is vacuous and "
        "goes with address_registry.cpp. KEEP the lookup mode (addr -> imported symbol): it is how an RSlot/ResolveAddr "
        "literal is mapped to the Runtime symbol to call directly (BLOCK0_BRIEF spelling 1), and ghidra skill + "
        ".claude/rules/address-registry.md + unreg_*.py + slot_name_audit.py import slotmap")),
    ("tools/rt_mint_gen.py", ("keep_changed",
        "Generates standalone/rt_data_mint_tables.inc (storage rows) AND standalone/rt_export_addrs.inc (all 4,926 DLL "
        "exports, name<->address, the mirror table). Shells out to runtime_census.py; reads tools/rt_mint_retired/*.txt and "
        "REFUSES a retired row that still has a reader (code, tests/unit, hex literal inside the row's extent). Gate: "
        "gate.sh (--check), mergegate.sh / finishmerge.sh (regenerate + commit the tables). DEFECT TO FIX FIRST: "
        "write_export_addrs() runs BEFORE the --check branch, so --check REWRITES rt_export_addrs.inc (the 'M' in git "
        "status, and why every kit script ends with 'git checkout -- standalone/rt_export_addrs.inc'); once the .inc is "
        "deleted, the next --check would re-create it untracked and mergegate.sh's 'TREE NOT CLEAN' would fire. Keep the "
        "retired-reader refusal as the permanent guard (NEXT_SESSION §3b.3); the storage emitter dies with the mint")),
    ("standalone/rt_export_addrs.inc", ("delete",
        "GENERATED (by rt_mint_gen.write_export_addrs, --export-addrs-only). Not a row list with readers: it is the whole DLL "
        "export table, read by resolve_standalone.cpp only to MIRROR each Register* call into the other door. Its 4,926 "
        "rows do not shrink as readers go; the file is deleted when no reader uses the opposite door from its "
        "registration (last mirror user), together with ExportNameByAddr/ExportAddrByName")),
    ("tools/rt_mint_retired/*.txt", ("keep",
        "302 retired keys (symbol or 0xVA) in 15 fragments; the input of the refusal. Keep; the teardown may add fragments "
        "for the two storage rows and, if the coordinator extends the mechanism, for deleted name/address rows")),
    ("tools/runtime_census.py", ("keep_changed",
        "4,626 lines. Scans code/ engine/ runtime/ standalone/ dx11/ for every door spelling (ResolveExport, RT_EXPORT_ONCE, "
        "RT_DATA_SLOT, ResolveAddr(Runtime), wrappers) and every RegisterRuntimeExport/Addr; 'ported' = a Register* row "
        "exists. Run by the gate TRANSITIVELY (rt_mint_gen.py subprocess; exits 2 -> generator refuses -> gate fails). "
        "As rows are deleted, called-and-registered symbols turn into 'called, unported' only if a door remains - which is "
        "exactly the reader list the teardown wants. Its door tables must learn any new spelling a block introduces. "
        "Dies with the mint emitter unless kept as the reader census")),
    ("tools/cutover_closure/", ("delete",
        "build_roots.py (census CSV + rt_data_mint tables -> roots), CutoverClosure.java (Ghidra), report.py "
        "(docs/runtime_port/closure/). A cutover-era progress number; not run by the gate or the Makefile. Retire with "
        "the census, or keep as history (it reads rt_data_mint_tables.inc, which will not exist)")),
    ("tools/export_name_audit.py", ("delete",
        "Checks every ResolveExport(\"...\") literal against the DLL export table (pefile). Not run by the gate. Useful "
        "DURING the teardown (a misspelled name is a DIRECT reader that looks DEAD); dies with the last ResolveExport")),
    ("tools/addr_door_audit.py", ("delete",
        "Every ResolveAddr(Module::Runtime, 0x...) minus every RegisterRuntimeAddr address (models the mint, reads "
        "engine_funcs.cpp). Not run by the gate. Useful during the teardown as the address-door reader list; dies with "
        "the last Module::Runtime door")),
    ("tools/dup_runtime_export.py", ("delete",
        "Duplicate RegisterRuntimeExport/RegisterRuntimeAddr rows (the later wins). Controls in "
        "dup_runtime_export_controls.sh. Not run by the gate. Dies with the last Register* row")),
    ("tools/dup_runtime_export_controls.sh", ("delete", "Self-test of dup_runtime_export.py. Dies with it")),
    ("tools/slot_name_audit.py", ("keep_changed",
        "Imports dispatch_slot_map.slotmap; reports every ResolveAddr(Module::Exe, 0x1401...) import-slot read with the PE "
        "symbol and the reader files (code/Game, engine_funcs.cpp, address_registry). Not run by the gate. The best "
        "existing READER LIST for spelling 1 (exe import slots); keep until the last slot read is converted")),
    ("tools/abi_retslot_audit.py", ("keep_changed",
        "Checks fn_* typedefs of by-value-returning exports for the hidden return slot (reads engine_funcs.h and the "
        "ResolveExport/RT_EXPORT_ONCE call sites). Not run by the gate. The teardown turns typedef+resolve into a "
        "declaration: the check then belongs to the compiler; delete when engine_funcs.h is dissolved")),
    ("tools/check_standing_notes.py", ("keep_changed",
        "Fails if a listed STANDING NOTE disappears; one note names RtPortLookupExport ('EXPECTED, NOT A FAULT'). Not run "
        "by the gate. When scene_watch.cpp's RtPortLookupAddr goes (carry-over 4), that NOTES entry must be edited in the "
        "same commit")),
    ("tools/unreg_full.py", ("delete",
        "Parses a [UnregCensus] dump ('0x... first= total=') and names each address via dispatch_slot_map.slotmap. Manual; "
        "dies with the census")),
    ("tools/unreg_loadpath.py", ("delete", "Same input as unreg_full.py, load-phase rows only. Manual; dies with the census")),
    ("tools/slot3_resolver_audit.py", ("keep_changed",
        "Finds component slot-3 (Serialize) wrappers whose lazy *_InitFuncs/Resolve* is never called on the load leg "
        "(code/Game). Not run by the gate. The lazy X_ResolveFuncs pattern (carry-over 11) is what it audits; dies when "
        "the last lazy resolver is converted")),
    ("tools/typeid_xcheck.py", ("keep",
        "Joins 0x1401cXXXX type-id storage literals against standalone/factory_registrations.csv. Exe type-id storage, "
        "registered by RegisterVtablesAndTypeIds in address_registry.cpp; still valid while those cells are addressed "
        "by literal. Not run by the gate")),
    ("tools/find_vtable.py", ("keep",
        "Finds the exe .rdata vtable containing a .text address, so it can be RegisterExeAddr'd. Reads the retail exe only. "
        "Not run by the gate. Unaffected until DEFINE_EXE_VTABLE (carry-over 18) goes")),
    ("tools/vtable_dump.py", ("keep", "Dumps a DLL vtable's slots with export names (pefile). Mentions rt_mint_gen only in prose. Not run by the gate")),
    ("tools/rt_data_row_refs.py", ("keep", "In-DLL .text read/write/LEA census for exported DATA rows (the retail DLL only). Useful for the two storage rows. Not run by the gate")),
    ("tools/rt_indll_callers.py", ("keep", "In-DLL rel32 caller scan (retail DLL only). Not run by the gate")),
    ("tools/rt_data_harvest.py", ("delete", "Wave 9 harvest of exported DATA bytes for the mint. Not run by the gate; dies with the mint")),
    ("tools/video_data_refs.py", ("keep", "In-DLL references to Video ?_first*@ / ??_7*Data rows (retail DLL). Relevant to carry-over 6, not to the registry")),
    ("tools/check_log_strip.py", ("keep",
        "Makefile target check-log-strip (a prerequisite of standalone / standalone-debug / standalone-perf): no qualified "
        "std::printf. Does NOT check 'Register*() inside printf' (reference_perf_build_strips_printf_arguments.md); the "
        "teardown deletes Register calls so that hazard only shrinks. Unaffected")),
    (f"{KIT}/codegrep.py", ("keep", "The counting tool of the acceptance (comment-stripped, strings kept). Unaffected")),
])
DEPS_OVERRIDE = {   # where the token scan reads prose rather than what the tool parses
    "standalone/rt_export_addrs.inc": ["rt_name", "rt_addr"],
    "tools/rt_mint_retired/*.txt": ["mint"],
    "tools/check_standing_notes.py": ["rt_addr"],
    "tools/slot3_resolver_audit.py": ["exe_addr", "engine_funcs"],
    "tools/typeid_xcheck.py": ["exe_addr"],
    "tools/vtable_dump.py": ["none"],
    "tools/rt_data_row_refs.py": ["mint"],
    "tools/rt_indll_callers.py": ["none"],
    "tools/rt_data_harvest.py": ["mint"],
    "tools/video_data_refs.py": ["none"],
    "tools/check_log_strip.py": ["none"],
    f"{KIT}/codegrep.py": ["none"],
}
for path, (verdict, note) in TOOLS.items():
    base = os.path.basename(path.rstrip("/*").rstrip("/"))
    if base.endswith(".txt"):
        base = "rt_mint_retired"
    if path.endswith("/"):
        files = walk([path], (".py", ".java", ".md"))
    elif "*" in path:
        files = sorted(glob.glob(path))
    else:
        files = [path]
    text = "".join(raw_of(f) for f in files)
    deps = DEPS_OVERRIDE.get(path) or [k for k, rx in TOK.items() if re.search(rx, text)]
    rb = readers(re.escape(base), exclude=tuple(files))
    make = [r for r in rb if r.startswith("Makefile") or r.startswith(KIT)]
    rbs = short(make, 8) + (f" | also named in {len(rb) - len(make)} tool/skill line(s)" if len(rb) > len(make) else "")
    nl = sum(raw_of(f).count("\n") for f in files)
    row(path, "tool", path, f"{len(files)} file(s), {nl} lines", "|".join(deps) or "none", rbs, verdict, note)

# =============================================================================================
# 3. UNIT TESTS
# =============================================================================================
TEST_RX = (r"\bg_unitResolveAddr\b|\bRegister(RuntimeExport|RuntimeAddr|ExeAddr|DispatchSlot)\s*\(|\bResolveExport\s*\(|"
           r"\bResolveAddr\s*\(|\bRT_EXPORT_ONCE\b|\bRtResolveCached\b|\bRtPortLookup\w*|"
           r"\b\w+::(InitFields|ReleaseFields)\s*\(|g_systemStub_(InitFields|ReleaseFields)")
TEST_NOTES = {
    "tests/unit/rt_port_stub.cpp": ("keep_changed", "rt_name|rt_addr|exe_addr",
        "THE unit build's resolve double (resolve_standalone.cpp is never linked): RegisterRuntimeExport/Addr are no-ops, "
        "ResolveExport answers null, ResolveAddr answers 0 unless a test installs g_unitResolveAddr. Every unit-linked "
        "product file that still registers or resolves links against this. It shrinks with them and is deleted when "
        "the last linked file (sources.mk) is registry-free. NOTE its Module enum (Exe, Runtime, VideoDX11, COUNT=3) "
        "differs from standalone/resolve.h (Exe, Runtime, COUNT=2); harmless (enum values do not mangle), comment says "
        "'byte-identical'"),
    "tests/unit/alloc_door_test.cpp": ("keep_changed", "exe_addr|rt_addr",
        "2 tests INJECTED THROUGH THE REGISTRY (g_unitResolveAddr): rtcoll::DoorAlloc/DoorFree prefer the exe dispatch-slot "
        "CELL 0x140152b60/0x140152b68 and RE-READ it every call, because standalone/redzone_alloc.cpp (BANISHED_REDZONE) "
        "rewrites the cell's contents to shim the allocator; fallback is ResolveAddr(Runtime, 0x1800c7a50/0x1800c7ad0). "
        "HARD PART: when the door goes direct, the dispatch-slot cell must survive as a named hook cell (STORAGE) that "
        "redzone writes and DoorAlloc reads; the test then sets that cell instead of installing a resolver"),
    "tests/unit/array_test.cpp": ("keep_changed", "rt_addr",
        "1 of 19 tests INJECTED THROUGH THE REGISTRY: array_pkgentry_shrink_arm_destroys_the_tail_in_place installs "
        "PkgResolver for 0x1800067d0 (Filename copy ctor) and 0x180026b50 (string-range release) to count kSpec_PkgEntry's "
        "element ops. When array.cpp calls those directly the seam is gone: build a test-local ArraySpec row whose "
        "assign/destroy pointers are the counting fakes (the spec is data), or observe the real Filename refcounts "
        "(filename.cpp is already linked). Note the product caches the fake in a function-local static (not reset)"),
    "tests/unit/system_abi_stubs.cpp": ("keep", "none",
        "LINK SEAM, not registry: defines System::Thread's out-of-line bodies (InitFields/ReleaseFields recording, "
        "OnCompleted counting, the rest abort) because thread_win.cpp includes <windows.h> AND stands on the registry "
        "(9 door lines). Carry-over 17. Survives the teardown unless thread_win.cpp becomes linkable; if it does, "
        "these definitions collide (duplicate symbols) and the tests need a virtual or injected seam instead"),
    "tests/unit/system_abi_stubs.h": ("keep", "none", "Declares the recording records of system_abi_stubs.cpp"),
    "tests/unit/b39_sys_thread_test.cpp": ("keep", "none", "Reads the Thread InitFields/ReleaseFields recordings (link seam above)"),
    "tests/unit/audio_abi_test.cpp": ("keep", "none", "Reads the Thread InitFields/ReleaseFields recordings (Audio::Stream on a 0x10000 thread)"),
    "tests/unit/rb_engine_abi_test.cpp": ("keep", "none",
        "LINK SEAM: defines Engine::UpdateLoop/LoaderLoop/DialogLoopT/DialogLoaderLoopT InitFields/ReleaseFields (product: "
        "runtime/src/engine/loops.cpp, 13 door lines, not linked) and reads the Thread recordings"),
    "tests/unit/rb_graphics_abi_test.cpp": ("keep", "none",
        "LINK SEAM: defines Graphics::Particle::ReleaseFields and SunLight::InitFields/ReleaseFields (product animation.cpp "
        "15 / sunlight.cpp 4 door lines, not linked)"),
    "tests/unit/rb_ui_abi_test.cpp": ("keep", "none",
        "LINK SEAM: defines UI DialogInstance/Element/ElementDescription InitFields/ReleaseFields (product dialog.cpp 49, "
        "dialog_create.cpp 10, element.cpp 43 door lines, not linked)"),
    "tests/unit/rb_video_abi_test.cpp": ("keep", "none",
        "LINK SEAM: defines Video::ImageBuffer ~ImageBuffer/InitFields/ReleaseFields (product buffers.cpp, 57 door lines, "
        "not linked)"),
}
TEST_FILES = walk(["tests/unit"], (".cpp", ".h"))
for f in TEST_FILES:
    hits = find_code(TEST_RX, [f])
    if not hits and f not in TEST_NOTES:
        continue
    verdict, dep, note = TEST_NOTES.get(f, ("keep", "none", "(no note: review)"))
    n_tests = len(re.findall(r"^UNIT_TEST\(", raw_of(f), re.M))
    row(f, "test", f, f"{len(hits)} code line(s); {n_tests} UNIT_TEST", dep, "make unit (gate.sh: unit N tests, 0 failures)",
        verdict, note)

# the product sources the unit build compiles in, with their door lines
mk = re.sub(r"#.*", "", raw_of("tests/unit/sources.mk")).replace("\\\n", " ")
unit_srcs = re.findall(r"\S+\.cpp", mk)
DOOR_RX = (r"\bRegister(RuntimeExport|RuntimeAddr|ExeAddr|DispatchSlot)\s*\(|\bResolveExport\s*\(|\bResolveAddr\s*\(|"
           r"\bRT_EXPORT_ONCE\b|\bRtResolveCached\b|\bRT_DATA_SLOT\b|\bRtAddr\s*<")
for s in unit_srcs:
    c = Counter(m.group(0).split("(")[0].split("<")[0].strip() for m in re.finditer(DOOR_RX, code_of(s)))
    seam = len(re.findall(r"BANISHED_UNIT_TEST", raw_of(s)))
    if not c and not seam:
        continue
    row(f"unit-linked {s}", "test", s, f"{sum(c.values())} door token(s)", "rt_name|rt_addr" if c else "none",
        "tests/unit/sources.mk -> make unit", "keep_changed" if c else "keep",
        f"doors: {dict(c) or '-'}; BANISHED_UNIT_TEST seam lines: {seam}. Linked into unit.exe against rt_port_stub.cpp "
        "(Register* are no-ops, resolves answer null/0, so every resolve-or-fall-back site takes its direct fallback). "
        "Converting these doors to direct calls needs no test change EXCEPT alloc_door.cpp and array.cpp (rows above); "
        "a door converted to a direct call into a file NOT in sources.mk (e.g. allocator.cpp) breaks the unit LINK: the "
        "BANISHED_UNIT_TEST malloc seams must then stay (or the callee joins sources.mk)")

# =============================================================================================
# 4. ENV-GATED HARNESS / DIAGNOSTIC CODE (standalone/ minus the registry files themselves)
# =============================================================================================
EF = re.findall(r"^extern\s+\S+\s+(\w+)\s*;", raw_of("engine/funcs/engine_funcs.h"), re.M)
EF_RX = re.compile(r"\b(" + "|".join(map(re.escape, EF)) + r")\b") if EF else None
HB = OrderedDict([
    ("exe_slot", r"ResolveAddr\s*\(\s*Module::Exe\s*,\s*(?:0x)?14015[12][0-9a-fA-F]{3}\b"),
    ("exe_addr", r"ResolveAddr\s*\(\s*Module::Exe\s*,\s*(?!(?:0x)?14015[12][0-9a-fA-F]{3}\b)"),
    ("rt_addr", r"ResolveAddr\s*\(\s*Module::Runtime|\bRtPortLookupAddr\s*\("),
    ("rt_name", r"\bResolveExport\s*\(|\bRT_EXPORT_ONCE\s*\(|\bRtResolveCached\s*\(|\bRT_DATA_SLOT\s*\(|\bRtPortLookupExport\s*\("),
    ("helper", r"\bRSlot\s*\(|\bResolveSlotPtr\b|\bResolveFn\s*<|\bResolveSlot\s*\(|\bRtAddr(Fn)?\s*<|\b(slot|SlotFn|R)\s*(<[^>]*>)?\s*\(\s*0x14"),
    ("register", r"\bRegister(ExeAddr|RuntimeExport|RuntimeAddr|DispatchSlot)\s*\("),
])
# which kit route exercises a flag (the kit's env lines are the source of truth; see route_env below)
ROUTE_OF_FLAG = {
    "pausemenu": "every gate.sh route (BANISHED_PAUSEMENU)", "pausemenuQuit": "every gate.sh route (BANISHED_PAUSEMENU)",
    "pausemenuButton": "uiroutes/stepclose goaldialog (BANISHED_PAUSEMENU_BUTTON=10)",
    "toolHarness": "toolroutes.sh + stepclose tool_* (BANISHED_TOOL_HARNESS)",
    "stress": "stepclose.sh sa-stress (BANISHED_STRESS)", "unattendedBoot": "every route",
    "saveTest": "tools/resave_gate (BANISHED_SAVE_TEST) = gate.sh resave xN + stepclose savefile",
    "modalDriver": "no kit route", "redzone": "no kit route (BANISHED_REDZONE)",
}
HARNESS = [f for f in walk(["standalone"], (".cpp",))
           if not re.search(r"standalone/(reg_|address_registry|resolve_standalone|rt_data_mint|options)", f)]
HARNESS += walk(["code/Game/Game"], (".cpp",))
NAMED = {"standalone/scene_watch.cpp", "standalone/hw_watch.cpp", "standalone/stall_debug.cpp",
         "standalone/entity_comp_validator.cpp", "standalone/sampler.cpp", "standalone/saveload_harness.cpp"}
for f in HARNESS:
    is_h = ("harness" in f) or f in NAMED or f.startswith("standalone/")
    if f.startswith("code/Game/Game") and "harness" not in f:
        continue
    t = code_of(f)
    c = OrderedDict((k, len(re.findall(rx, t, re.S))) for k, rx in HB.items())
    c["engine_funcs"] = len(EF_RX.findall(t)) if EF_RX else 0
    opts = sorted(set(re.findall(r"Opt\(\)\.(\w+)", t)))
    if not sum(c.values()) and f not in NAMED:
        continue
    if not is_h:
        continue
    routes = sorted({ROUTE_OF_FLAG[o] for o in opts if o in ROUTE_OF_FLAG})
    route = "; ".join(routes) if routes else ("no kit route" if opts else "every route (not flag-gated)")
    flags = ",".join(opts) if opts else "-"
    dep = "|".join(k for k in ("exe_slot", "exe_addr", "rt_addr", "rt_name", "helper", "register", "engine_funcs") if c[k])
    dep = dep.replace("exe_slot", "exe_addr(import slot)")
    counts = " ".join(f"{k}={v}" for k, v in c.items() if v)
    extra = ""
    if f == "standalone/scene_watch.cpp":
        extra = " Carry-over 4: RtPortLookupAddr(0x180100f20) used as an IDENTITY TOKEN (compared, never called)."
    if f == "standalone/redzone_alloc.cpp":
        extra = (" WRITES the allocator dispatch-slot CELLS 0x140152b60/0x140152b68 to shim MemAlloc/MemFree: a harness "
                 "that depends on registry STORAGE (see alloc_door_test). 30 more reader lines in code/Game (20 files), "
                 "most copying the cell's pointer into a static once.")
    if f == "standalone/main.cpp":
        extra = " The boot order RuntimePortInit -> AddressRegistryInit -> EngineFuncsInit and every exit summary live here."
    if f == "standalone/splash_loop_stubs.cpp":
        extra = " The one TraceExeBegin/End caller."
    row(f, "harness", f, counts or "0", dep or "none", route, "keep_changed" if sum(c.values()) else "keep",
        f"flags: {flags}.{extra}")

# =============================================================================================
# 5. THE NOT IMPLEMENTED STUB SET (observed; readers computed)
# =============================================================================================
STUBS = OrderedDict([
    # addr: (routes seen at the B3X close, what it is)
    ("0x140002280", ("every route incl. menu-only", "exe fn: component finder (engine_funcs.cpp:1141 binds it at boot)")),
    ("0x140022480", ("newgame tutorial tools savefile goaldialog", "exe fn")),
    ("0x140089470", ("newgame tools", "exe fn: TownRegistry_ShuffleProfessionList - PORTED, registered only under "
                                       "BANISHED_LIVE_SEAT_SHUFFLE (null = deterministic seat pick, the chosen default)")),
    ("0x1400938d0", ("newgame tutorial tools savefile goaldialog", "exe fn")),
    ("0x14009e8c0", ("tutorial savefile goaldialog", "exe fn")),
    ("0x1400fcc90", ("newgame tutorial tools savefile goaldialog", "exe fn")),
    ("0x140101ee0", ("newgame tutorial tools savefile goaldialog", "exe fn")),
    ("0x1401026a0", ("newgame tutorial tools savefile goaldialog", "exe fn")),
    ("0x140152a70", ("newgame tutorial tools savefile", "exe IMPORT SLOT: Math::Vector3::operator== (carry-over 9)")),
    ("0x1401c22d0", ("newgame tools", "exe .data TABLE (int8 LUT) read by address")),
    ("0x1401c23d0", ("newgame tools", "exe .data TABLE (uint16 index table) read by address")),
    ("0x140151ab8", ("savefile", "exe IMPORT SLOT (a delete helper; 4 Work/ readers copy it)")),
    ("0x1401513f8", ("tool_road", "exe IMPORT SLOT (dragtool.cpp)")),
])
for a, (routes, what) in STUBS.items():
    hits = find_code(re.escape(a) + r"\b")
    hits = [h for h in hits if not h[0].endswith("rt_export_addrs.inc")]
    readers_s = ", ".join(f"{f}:{n}" for f, n in hits[:6]) + (f" (+{len(hits) - 6})" if len(hits) > 6 else "")
    reg = [h for h in hits if re.search(r"RegisterExeAddr|RegisterDispatchSlot",
                                        code_of(h[0]).split("\n")[h[1] - 1])]
    verdict = "recut"
    row(f"stub: {a}", "log_line", hits[0][0] if hits else "-", hits[0][1] if hits else "-",
        "exe_addr", "stepclose.sh (count + set vs baseline: broken, see the NOT IMPLEMENTED row)", verdict,
        f"seen on: {routes}. {what}. readers(code): {readers_s or 'NONE FOUND (reader holds the address in data?)'}"
        + (f". registered at {reg[0][0]}:{reg[0][1]} (conditionally)" if reg else "")
        + ". Row class for inventory A: STUB; leaves the set only when its reader calls a C++ body")

# =============================================================================================
with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
    fh.write("item\tkind\tsource_file\tsource_line\tdepends_on\tread_by\tverdict\tnote\n")
    for r in ROWS:
        fh.write("\t".join(x.replace("\n", " ") for x in r) + "\n")
kinds = Counter(r[1] for r in ROWS)
verd = Counter((r[1], r[6]) for r in ROWS)
print(f"wrote {OUT}: {len(ROWS)} rows")
for k in sorted(kinds):
    print(f"  {k:9s} {kinds[k]:3d}  " + "  ".join(f"{v}={n}" for (kk, v), n in sorted(verd.items()) if kk == k))
