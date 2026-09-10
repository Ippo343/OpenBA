// Standalone address resolution for Banished reimplementation.
// In standalone mode, WE are the exe — so Module::Exe addresses resolve
// via a registry of our reimplemented functions. Module::Runtime and
// Module::VideoDX11 still use ASLR delta (they're loaded DLLs).


#include "resolve.h"
#include "../runtime/funcs/rt_port.h"
#include "rt_data_mint.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <algorithm>

// Return address of our own caller, for the BANISHED_RT_STRICT residue census.
// MSVC spells this `_ReturnAddress()` (an intrinsic in <intrin.h>); this tree
// builds with MinGW GCC, where the builtin is the portable spelling.
#if !defined(_MSC_VER)
#define _ReturnAddress() __builtin_return_address(0)
#endif

// ============================================================
// Module info — Exe slot unused
// ============================================================
ModuleInfo g_modules[static_cast<int>(Module::COUNT)] = {
    // Exe: WE are the exe — delta is meaningless, handle is us
    { nullptr,              0x140000000ULL, 0, 0, nullptr },
    // Runtime-x64.dll — still an external DLL
    { "Runtime-x64.dll",    0x180000000ULL, 0, 0, nullptr },
    // VideoDX11-x64.dll — still an external DLL
    { "VideoDX11-x64.dll",  0x180000000ULL, 0, 0, nullptr },
};

// ============================================================
// Address registry — maps Ghidra exe addresses to our code
// ============================================================
static std::unordered_map<uintptr_t, uintptr_t>& GetRegistry() {
    static std::unordered_map<uintptr_t, uintptr_t> registry;
    return registry;
}

// Track stats for progress reporting
static size_t g_stubHits = 0;
static size_t g_uniqueStubs = 0;

// ============================================================
// Bracket-window tracer — bumped by TraceExeBegin/End in callers.
// When > 0, ResolveAddr(Module::Exe, ...) logs every resolution.
// ============================================================
volatile int g_traceExeCallbacks = 0;

void TraceExeBegin(const char* label) {
    fprintf(stderr, "===== [TRACE BEGIN] %s =====\n", label ? label : "(null)");
    fflush(stderr);
    g_traceExeCallbacks++;
}

void TraceExeEnd(const char* label) {
    if (g_traceExeCallbacks > 0) g_traceExeCallbacks--;
    fprintf(stderr, "===== [TRACE END] %s =====\n", label ? label : "(null)");
    fflush(stderr);
}

// ============================================================
// Unregistered-address census (BANISHED_UNREG_CENSUS=1)
// ============================================================
// The NOT IMPLEMENTED warning below is once-per-address for the life of the
// process. That is fine as a nag but useless for "which unregistered slots does
// the LOAD path touch": a slot hit at boot and again mid-load is only ever
// attributed to boot. The census keeps a per-address, per-phase hit count so
// the load path can be read off directly.
static const char* g_unregPhase = "boot";

struct UnregEntry {
    const char* firstPhase = nullptr;              // phase of the very first hit
    std::unordered_map<const char*, size_t> hits;  // phase label -> count
};
static std::unordered_map<uintptr_t, UnregEntry>& GetUnregCensus() {
    static std::unordered_map<uintptr_t, UnregEntry> c;
    return c;
}
static bool CensusEnabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("BANISHED_UNREG_CENSUS");
        on = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }
    return on != 0;
}

void SetUnregPhase(const char* phase) {
    if (!phase) return;
    g_unregPhase = phase;
    if (CensusEnabled()) {
        fprintf(stderr, "[UnregCensus] phase -> %s\n", phase);
        fflush(stderr);
    }
}
const char* UnregPhase() { return g_unregPhase; }

void DumpUnregCensus() {
    if (!CensusEnabled()) return;
    auto& c = GetUnregCensus();
    // Sorted by address so two runs diff cleanly against each other.
    std::vector<uintptr_t> addrs;
    addrs.reserve(c.size());
    for (auto& kv : c) addrs.push_back(kv.first);
    std::sort(addrs.begin(), addrs.end());
    fprintf(stderr, "[UnregCensus] ==== %zu distinct unregistered addresses ====\n",
            addrs.size());
    for (uintptr_t a : addrs) {
        const UnregEntry& e = c[a];
        size_t total = 0;
        for (auto& h : e.hits) total += h.second;
        fprintf(stderr, "[UnregCensus] 0x%llx first=%s total=%zu",
                (unsigned long long)a, e.firstPhase ? e.firstPhase : "?", total);
        for (auto& h : e.hits) fprintf(stderr, " %s=%zu", h.first, h.second);
        fprintf(stderr, "\n");
    }
    fflush(stderr);
}

// NotImplemented stub — logs and returns 0
// Safe for most calling conventions (returns 0/nullptr/false)
static uintptr_t NotImplementedStub_impl(uintptr_t ghidra_addr) {
    // Only warn once per address to avoid log spam
    static std::unordered_map<uintptr_t, bool> warned;
    if (!warned[ghidra_addr]) {
        fprintf(stderr, "[STANDALONE] NOT IMPLEMENTED: exe function at 0x%llx\n",
                (unsigned long long)ghidra_addr);
        warned[ghidra_addr] = true;
        g_uniqueStubs++;
    }
    if (CensusEnabled()) {
        UnregEntry& e = GetUnregCensus()[ghidra_addr];
        if (!e.firstPhase) e.firstPhase = g_unregPhase;
        e.hits[g_unregPhase]++;
    }
    g_stubHits++;
    return 0;
}

void RegisterExeAddr(uintptr_t ghidra_addr, uintptr_t our_addr) {
    GetRegistry()[ghidra_addr] = our_addr;
}

void RegisterExeAddrs(const ExeAddrEntry* entries, size_t count) {
    auto& reg = GetRegistry();
    for (size_t i = 0; i < count; i++) {
        reg[entries[i].ghidra_addr] = entries[i].our_addr;
    }
}

RegistryStats GetRegistryStats() {
    return { GetRegistry().size(), g_uniqueStubs };
}

// ============================================================
// Runtime-x64.dll port registry (Wave 0 seam)
// ============================================================
// See the comment on RegisterRuntimeExport in resolve.h. Three lookup tables,
// consulted by ResolveExport / ResolveOrdinal / ResolveAddr(Module::Runtime)
// BEFORE the DLL is asked. Empty until a runtime port registers, so the
// default build resolves exactly what it always did.
namespace {

std::unordered_map<std::string, void*>& RtExportReg() {
    static std::unordered_map<std::string, void*> m;
    return m;
}
std::unordered_map<uintptr_t, void*>& RtAddrReg() {
    static std::unordered_map<uintptr_t, void*> m;
    return m;
}
std::unordered_map<uint16_t, void*>& RtOrdinalReg() {
    static std::unordered_map<uint16_t, void*> m;
    return m;
}
size_t g_rtHits = 0;

// BANISHED_RT_PORT=0 disables every runtime port at once (the master
// kill-switch for an A/B run). Individual ports keep their own
// BANISHED_RT_<CLASS> switches; this one short-circuits the whole seam.
bool RtPortEnabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("BANISHED_RT_PORT");
        on = (e && (e[0] == '0' || e[0] == 'f' || e[0] == 'F')) ? 0 : 1;
        if (!on) {
            fprintf(stderr, "[RtPort] BANISHED_RT_PORT=0 — every runtime port bypassed, "
                            "all calls go to Runtime-x64.dll\n");
            fflush(stderr);
        }
    }
    return on != 0;
}

bool RtCensusEnabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("BANISHED_RT_CENSUS");
        on = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }
    return on != 0;
}

// ---- Cutover residue census (BANISHED_RT_STRICT) --------------------------
// See the comment on RecordRuntimeResidue in resolve.h. Records every resolve
// that misses the port registry and falls through to the shipped DLL, so the
// wave 9 work list is what the PROCESS reaches rather than what the tree
// mentions. Off unless the env var is set; one map insert per distinct miss.
bool RtStrictEnabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("BANISHED_RT_STRICT");
        on = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }
    return on != 0;
}

struct ResidueEntry {
    unsigned long long count;
    void* firstCaller;   // return address of the first asker, for attribution
};
// Keyed by the printable form of the request so all three doors share one
// table and one sort order: the mangled name, "addr:0x1800xxxxx", or "ord:N".
std::map<std::string, ResidueEntry>& RtResidue() {
    static std::map<std::string, ResidueEntry> m;
    return m;
}

// ---- Unresolved-symbol census (the Wave 9 lane F null-resolve policy) ------
// See the comment on RecordResolveMiss in resolve.h. Always on: the only cost
// is on the path where a resolve returned NOTHING, which in a healthy pre-
// cutover run never happens.
std::map<std::string, ResidueEntry>& RtResolveMiss() {
    static std::map<std::string, ResidueEntry> m;
    return m;
}

bool RtFatalResolveEnabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("BANISHED_RT_FATAL_RESOLVE");
        on = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }
    return on != 0;
}

} // namespace

void RegisterRuntimeExport(const char* mangled_name, void* our_fn) {
    if (!mangled_name || !our_fn) return;
    RtExportReg()[mangled_name] = our_fn;
}

void RegisterRuntimeAddr(uintptr_t ghidra_addr, void* our_fn) {
    if (!ghidra_addr || !our_fn) return;
    RtAddrReg()[ghidra_addr] = our_fn;
}

void RegisterRuntimeOrdinal(uint16_t ordinal, void* our_fn) {
    if (!our_fn) return;
    RtOrdinalReg()[ordinal] = our_fn;
}

// Wave 9 lane H: did a PORT register this name? ResolveExport cannot answer
// that -- its return value cannot distinguish a registry hit from a DLL hit,
// and the whole early-snapshot question is exactly that distinction.
bool RuntimeExportIsRegistered(const char* mangled_name) {
    if (!mangled_name) return false;
    auto& reg = RtExportReg();
    return reg.find(mangled_name) != reg.end();
}

RuntimeRegistryStats GetRuntimeRegistryStats() {
    return { RtExportReg().size(), RtAddrReg().size(), RtOrdinalReg().size(), g_rtHits };
}

void DumpRuntimeRegistry() {
    if (!RtCensusEnabled()) return;
    RuntimeRegistryStats st = GetRuntimeRegistryStats();
    fprintf(stderr, "[RtCensus] ==== %zu exports, %zu addrs, %zu ordinals ported; %zu hits ====\n",
            st.exports, st.addrs, st.ordinals, st.hits);
    // Sorted so two runs diff cleanly.
    {
        std::vector<std::string> names;
        names.reserve(RtExportReg().size());
        for (auto& kv : RtExportReg()) names.push_back(kv.first);
        std::sort(names.begin(), names.end());
        for (auto& n : names)
            fprintf(stderr, "[RtCensus] export %s\n", n.c_str());
    }
    {
        std::vector<uintptr_t> addrs;
        addrs.reserve(RtAddrReg().size());
        for (auto& kv : RtAddrReg()) addrs.push_back(kv.first);
        std::sort(addrs.begin(), addrs.end());
        for (uintptr_t a : addrs)
            fprintf(stderr, "[RtCensus] addr 0x%llx\n", (unsigned long long)a);
    }
    {
        std::vector<uint16_t> ords;
        ords.reserve(RtOrdinalReg().size());
        for (auto& kv : RtOrdinalReg()) ords.push_back(kv.first);
        std::sort(ords.begin(), ords.end());
        for (uint16_t o : ords)
            fprintf(stderr, "[RtCensus] ordinal %u\n", (unsigned)o);
    }
    fflush(stderr);
}

void RuntimePortInit() {
    // Registrations happen in runtime/src/**, one *_PortInit() per wave, each
    // declared in runtime/funcs/rt_port.h and each gated by its own
    // BANISHED_RT_<WAVE> kill switch. They must run here, before
    // AddressRegistryInit and EngineFuncsInit cache their pointers.
    RtMathPortInit();     // Wave 1: Math
    RtVolumePortInit();   // Wave 1: Volume
    RtSystemPortInit();   // Wave 2: System
    RtObjectPortInit();   // Wave 3: Object
    RtIoPortInit();       // Wave 4: IO
    RtAchievementPortInit();  // Wave 5: Achievement
    RtSavePortInit();     // Wave 5: Save
    RtPickingPortInit();  // Wave 5: Picking
    RtVideoPortInit();    // Wave 6: Video
    RtGraphicsPortInit(); // Wave 6: Graphics
    RtUiPortInit();       // Wave 7: UI
    RtEnginePortInit();       // Wave 8: Engine (lanes A+B)
    RtAudioPortInit();        // Wave 8: Audio (lane C)
    RtInputPortInit();        // Wave 8: Input (lane D)
    RtApplicationPortInit();  // Wave 8: Application (lane E)
    RtUtilityPortInit();      // Wave 8: Utility (lane E)
    // LAST, deliberately. Wave 9 lane Q registers 27 ADDRESSES for the two
    // header-only Collection::Array bodies (~Array and SetSize), and those
    // addresses are already called by name-less ResolveAddr from graphics,
    // picking, ui, audio, achievement, engine and engine/funcs. Registering
    // them redirects all of those to us, so it happens at one identifiable
    // point in the init order rather than in the middle of it.
    RtCollectionPortInit();   // Wave 9: Collection (lane Q)
    RtCollectionDestroyPortInit();  // Wave 9: Collection destroy half (lane D2)

    // Wave 9 lane K: the 37 unnamed deleting destructors that are slot 0 of
    // (almost) every vtable the mint below is about to fill. Last of the port
    // inits, because it resolves each ~Class through the port registry and its
    // probe runs at the end of it -- so every other wave's destructors must
    // already be registered. Before RtDataMintInit(), which consumes the
    // registrations it makes.
    RtObjectDeletingDtorPortInit();  // Wave 9: deleting destructors (lane K)

    // Wave 9 lane X: the fourteen constructors that publish a `?_singleton@...`
    // DATA export. Here, between lane K and the mint, for two reasons. It must
    // be BEFORE RtDataMintInit() because the mint's verdict for a storage row
    // depends on whether its writer is registered. And it is AFTER every other
    // wave because its probe calls our OWN ??0Random@System@@QEAA@XZ through
    // the port-aware door, which only reaches the port once Wave 2 has run.
    RtSingletonCtorsPortInit();  // Wave 9: singleton constructors (lane X)

    // Wave 9 lane C2: mint the exported DATA symbols (??_7 vtables,
    // BaseTypeInfo, singletons and the other .data statics -- the count is a
    // moving target, see WAVE9_LANE_C2.md). AFTER every *_PortInit() above,
    // because a
    // vtable slot is filled with our port only if that port is registered by
    // now — anything registered later stays a trap until the next run.
    // Minting allocates and fills; it does not register anything unless
    // BANISHED_RT_MINT=1. See standalone/rt_data_mint.h.
    // Nothing to resolve at this point — the tables hold our own function
    // pointers, not DLL addresses — so the rest of this only reports.
    RuntimeRegistryStats st = GetRuntimeRegistryStats();
    printf("[RtPort] runtime registry: %zu exports, %zu addrs, %zu ordinals%s\n",
           st.exports, st.addrs, st.ordinals,
           RtPortEnabled() ? "" : "  (DISABLED via BANISHED_RT_PORT=0)");
    DumpRuntimeRegistry();

    // Wave 9 lane H: everything resolved from here on sees the full registry.
    // Anything recorded BEFORE this line resolved against a partly-built one,
    // and if its call site caches, it is frozen on the pre-registration answer.
    // WAVE 9: BUILD AND REGISTER THE MINTED DATA.
    //
    // Restored 2026-09-08.  Lane X's probe-reordering commit (4a60ccf) DELETED
    // this call while adding comments that refer to it, so the mint stopped
    // initialising entirely: no vtable, no BaseTypeInfo and no storage row was
    // ever built or registered.
    //
    // Nothing on an ordinary run could see that.  The mint REGISTERS nothing
    // unless BANISHED_RT_MINT=1 and its shadow only PRINTS under
    // BANISHED_RT_MINT_SHADOW=1, and lane X's verification cell set neither --
    // so a green BANISHED_RT_DIFF run was entirely consistent with the mint
    // being absent.  The FIRST controlled DLL-absent boot found it immediately:
    // ResolveAddr(Module::Runtime, 0x1801fc718) returned null and
    // engine_funcs.cpp:1623 dereferenced it, because
    // ?_singleton@MemoryAllocator@System@@ is a minted row that was never
    // registered.
    //
    // Placement, per lane X's own reasoning which the comments below preserve:
    // AFTER every *PortInit() above, because the mint reads the port registry
    // to decide a storage row's verdict; BEFORE RtResolveMarkPortInitDone() and
    // before lane X's probe, because the probe resolves the fourteen
    // ?_singleton@ names through a CACHING door and those are the very caches
    // the ported constructors publish through.
    RtDataMintInit();

    RtResolveMarkPortInitDone();

    // NOTE (Wave 17 lane CEIL): the door-attendance phase marker is DELIBERATELY
    // NOT here. RuntimePortInit is not the end of registration —
    // AddressRegistryInit() runs afterwards (standalone/main.cpp) and
    // reg_unreg_lane3.cpp resolves four ?_typeInfo@Graphics@@ rows by name to
    // fill dispatch slots. Marking here read all four as ATTENDANCE and would
    // have handed four rows a route that is really a registration.

    // Wave 9 lane X's oracle. LAST, after the mint AND after the mark above,
    // and both halves of that placement are deliberate.
    //
    //  * After RtDataMintInit(): its REGISTRATIONS ran earlier (the mint needs
    //    them to rank a storage row), but the probe RESOLVES all fourteen
    //    `?_singleton@...` names through a caching door, and those are the very
    //    caches the ported constructors publish through. Probing before the
    //    mint would freeze them on the DLL's .data, so on a minted run every
    //    one of the fourteen would write the copy nothing reads -- the exact
    //    failure lane X exists to close.
    //  * After RtResolveMarkPortInitDone(): with every registration done, the
    //    answers it caches are final, so they are not "early" in the sense lane
    //    H's recorder means and do not belong in that report. Placing it before
    //    the mark put twelve rows of this lane's own harness into a diagnostic
    //    other lanes read.
    //
    // No-ops unless BANISHED_RT_DIFF is on.
    RtLaneXSingletonCtorProbe();
}

// ============================================================
// ResolveAddr — standalone implementation
// ============================================================
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr) {
    if (mod == Module::Exe) {
        // Look up in our registry
        auto& reg = GetRegistry();
        auto it = reg.find(ghidra_addr);
        if (it != reg.end()) {
            if (g_traceExeCallbacks > 0) {
                fprintf(stderr,
                    "[TRACE] ResolveAddr Exe 0x%llx -> 0x%llx (registered)\n",
                    (unsigned long long)ghidra_addr,
                    (unsigned long long)it->second);
                fflush(stderr);
            }
            return it->second;
        }
        // Not registered — warn and return 0 (callers should null-check)
        if (g_traceExeCallbacks > 0) {
            fprintf(stderr,
                "[TRACE] ResolveAddr Exe 0x%llx -> NOT IMPLEMENTED\n",
                (unsigned long long)ghidra_addr);
            fflush(stderr);
        }
        NotImplementedStub_impl(ghidra_addr);
        return 0;
    }

    // Runtime/VideoDX11 — standard ASLR delta.
    //
    // Return 0 when the module is not mapped. Without this the delta is 0 and
    // the function hands back `ghidra_addr` unchanged — 0x1800187c8 and the
    // like — which is not null, looks like a perfectly good pointer, and points
    // at nothing. That is strictly worse than null for every caller, all of
    // which either null-check or crash immediately. It became reachable when
    // VideoDX11-x64.dll stopped being loaded in Standalone mode.
    //
    // Guard on the handle, not the delta: a module CAN legitimately load at its
    // preferred base and have delta == 0.
    // Runtime port seam: an address we have reimplemented resolves to OUR code.
    // Checked before the module handle, so a port stays live even for a module
    // that failed to load — that is the whole point of the endgame (§4.1).
    if (mod == Module::Runtime && RtPortEnabled()) {
        auto& reg = RtAddrReg();
        auto it = reg.find(ghidra_addr);
        if (it != reg.end()) {
            g_rtHits++;
            return reinterpret_cast<uintptr_t>(it->second);
        }
        RecordRuntimeResidue(nullptr, ghidra_addr, 0, _ReturnAddress());
        RtResolveNoteEarly(nullptr, ghidra_addr, 0, _ReturnAddress());   // Wave 9 lane H
    }

    const ModuleInfo& mi = g_modules[static_cast<int>(mod)];
    if (!mi.handle) return 0;
    return static_cast<uintptr_t>(
        static_cast<intptr_t>(ghidra_addr) + mi.delta
    );
}

// ============================================================
// ResolveInit — standalone version
// ============================================================
bool ResolveInit() {
    // Exe: we ARE the exe
    g_modules[0].handle = GetModuleHandleA(nullptr);
    g_modules[0].runtimeBase = reinterpret_cast<uintptr_t>(g_modules[0].handle);
    // Delta is meaningless for standalone exe, but set for completeness
    g_modules[0].delta = 0;

    // Runtime-x64.dll: required (we still depend on it)
    g_modules[1].handle = GetModuleHandleA(g_modules[1].name);
    if (!g_modules[1].handle) {
        // Try LoadLibrary — in standalone mode we load it ourselves
        g_modules[1].handle = LoadLibraryA(g_modules[1].name);
    }
    if (!g_modules[1].handle) {
        // WAVE 9 LANE G, the CONTROLLED CUTOVER BOOT.
        //
        // Normally a missing Runtime-x64.dll is fatal and main.cpp exits here.
        // That is correct for every ordinary run -- but it makes the ONE
        // experiment we cannot otherwise perform useless: with the DLL moved
        // aside the process would die BEFORE RuntimePortInit, before the mint
        // registers anything, and before a single ported body runs.  The
        // attempt would learn nothing and read as "cutover fails", which is not
        // a finding (lane C2, WAVE9_CUTOVER_BOOT_RUNBOOK.md).
        //
        // BANISHED_RT_NO_DLL=1 lets boot CONTINUE with a null Runtime handle so
        // the process gets far enough to say WHERE it dies.  That answers the
        // one question static analysis cannot: the ORDER in which the missing
        // pieces are actually reached.  Everything downstream already copes
        // with a null handle by design -- DllExport/DllAddr return null,
        // ResolveExportRawImpl returns null, and the mint shadow prints a
        // DLL=ABSENT banner.
        //
        // It is OFF by default and changes nothing when unset: an ordinary run
        // still takes the fatal exit above.
        const char* noDll = getenv("BANISHED_RT_NO_DLL");
        if (!noDll || noDll[0] == '0' || noDll[0] == 'f' || noDll[0] == 'F') return false;
        fprintf(stderr,
                "[Standalone] BANISHED_RT_NO_DLL=1 -- continuing with Runtime-x64.dll "
                "ABSENT.  This is the controlled cutover boot; a crash is the EXPECTED "
                "outcome and its location is the result.  See "
                "docs/runtime_port/WAVE9_CUTOVER_BOOT_RUNBOOK.md\n");
        g_modules[1].runtimeBase = 0;
        g_modules[1].delta = 0;
    } else {
    g_modules[1].runtimeBase = reinterpret_cast<uintptr_t>(g_modules[1].handle);
    g_modules[1].delta = static_cast<intptr_t>(g_modules[1].runtimeBase)
                       - static_cast<intptr_t>(g_modules[1].preferredBase);
    }

    // VideoDX11-x64.dll: never loaded. Our own vtables and our ports of the
    // two exports replace it entirely, and InstallVideoInterfacePrefill stops
    // Runtime LoadLibrary'ing it too. GetModuleHandle rather than LoadLibrary,
    // so that if something else in the process has pulled it in, ResolveAddr
    // still reports a truthful base instead of a zero delta — a wrong non-null
    // pointer into nothing is exactly the trap that cost a session before.
    g_modules[2].handle = GetModuleHandleA(g_modules[2].name);
    if (g_modules[2].handle) {
        g_modules[2].runtimeBase = reinterpret_cast<uintptr_t>(g_modules[2].handle);
        g_modules[2].delta = static_cast<intptr_t>(g_modules[2].runtimeBase)
                           - static_cast<intptr_t>(g_modules[2].preferredBase);
    }

#ifdef _DEBUG
    for (int i = 0; i < static_cast<int>(Module::COUNT); i++) {
        if (g_modules[i].handle) {
            printf("[Resolve-Standalone] %s: base=0x%llx preferred=0x%llx delta=%+lld\n",
                   g_modules[i].name ? g_modules[i].name : "Standalone-EXE",
                   (unsigned long long)g_modules[i].runtimeBase,
                   (unsigned long long)g_modules[i].preferredBase,
                   (long long)g_modules[i].delta);
        } else {
            printf("[Resolve-Standalone] %s: not loaded\n",
                   g_modules[i].name ? g_modules[i].name : "Standalone-EXE");
        }
    }
    printf("[Resolve-Standalone] Address registry: %zu entries\n", GetRegistry().size());
#endif

    return true;
}

// ============================================================
// Common resolution helpers
// ============================================================
const ModuleInfo* GetModuleInfo(Module mod) {
    return &g_modules[static_cast<int>(mod)];
}

// One residue key -> one row. Shared by the port-registry doors below and by
// the *Raw* doors, which use the "raw:" / "rawaddr:" prefixes.
static void RecordResidueKey(const char* key, void* caller) {
    ResidueEntry& e = RtResidue()[key];
    if (e.count == 0) e.firstCaller = caller;
    e.count++;
}

void RecordRuntimeResidue(const char* mangled_name, uintptr_t ghidra_addr,
                          uint16_t ordinal, void* caller) {
    if (!RtStrictEnabled()) return;
    char key[512];
    if (mangled_name) {
        snprintf(key, sizeof(key), "%s", mangled_name);
    } else if (ghidra_addr) {
        snprintf(key, sizeof(key), "addr:0x%llx", (unsigned long long)ghidra_addr);
    } else {
        snprintf(key, sizeof(key), "ord:%u", (unsigned)ordinal);
    }
    RecordResidueKey(key, caller);
}

// ---- The *Raw* doors --------------------------------------------------------
// Added 2026-09-07 (wave 9 lane 0) after lane D found that ResolveRuntimeRaw was
// invisible to strict mode: 0x18007fbfc is the DLL's own _beginthreadex, every
// System::Thread in the process goes through it, and it appeared in NO residue
// run. An absence produced by the instrument, read as a fact about the program
// -- the same shape as reference_truncated_tool_output_manufactures_calls_zero.md.
//
// The Raw doors BYPASS the port registry BY DESIGN (resolve.h): the diff oracle,
// the detour trampolines and RT_ORIGINAL_ONCE must reach the shipped code even
// when a port is registered. So a raw hit is NOT the same kind of event as a
// port-registry miss, and merging the two would make the port residue read as
// permanently non-empty. They are recorded under their own "raw:" / "rawaddr:"
// prefixes instead, counted separately, and reported in their own block.
//
// They are still cutover work: with the DLL gone, a raw resolve returns null,
// and whether that is fatal or merely disables an oracle has to be decided per
// call site. The dump says so rather than deciding it here.
static void RecordRawResidue(const char* mangled_name, uintptr_t ghidra_addr,
                             uint16_t ordinal, void* caller) {
    if (!RtStrictEnabled()) return;
    char key[512];
    if (mangled_name) {
        snprintf(key, sizeof(key), "raw:%s", mangled_name);
    } else if (ghidra_addr) {
        snprintf(key, sizeof(key), "rawaddr:0x%llx", (unsigned long long)ghidra_addr);
    } else {
        snprintf(key, sizeof(key), "raword:%u", (unsigned)ordinal);
    }
    RecordResidueKey(key, caller);
}

void DumpRuntimeResidue() {
    if (!RtStrictEnabled()) return;
    const std::map<std::string, ResidueEntry>& m = RtResidue();
    // Two populations, counted apart (see RecordRawResidue): a PORT-REGISTRY
    // MISS is a symbol we have not taken over; a RAW hit is a site that asked
    // for the shipped code on purpose. Both must be empty or accounted for
    // before the DLL can go, and only the first is "unported".
    size_t raw = 0;
    for (std::map<std::string, ResidueEntry>::const_iterator it = m.begin();
         it != m.end(); ++it) {
        const std::string& k = it->first;
        if (k.compare(0, 4, "raw:") == 0 || k.compare(0, 8, "rawaddr:") == 0 ||
            k.compare(0, 7, "raword:") == 0) raw++;
    }
    fprintf(stderr,
            "[RtResidue] ==== %zu distinct requests still fell through to "
            "Runtime-x64.dll ====\n", m.size() - raw);
    // Sorted (std::map) so two runs diff cleanly, same contract as [RtCensus].
    for (std::map<std::string, ResidueEntry>::const_iterator it = m.begin();
         it != m.end(); ++it) {
        const std::string& k = it->first;
        if (k.compare(0, 4, "raw:") == 0 || k.compare(0, 8, "rawaddr:") == 0 ||
            k.compare(0, 7, "raword:") == 0) continue;
        fprintf(stderr, "[RtResidue] %-8llu %-16p %s\n",
                it->second.count, it->second.firstCaller, k.c_str());
    }
    fprintf(stderr, "[RtResidue] ==== end (0 means no symbol is missing) ====\n");
    fprintf(stderr,
            "[RtRawResidue] ==== %zu distinct requests took a *Raw* door "
            "(ResolveExportRaw / ResolveOrdinalRaw / ResolveRuntimeRaw) ====\n", raw);
    for (std::map<std::string, ResidueEntry>::const_iterator it = m.begin();
         it != m.end(); ++it) {
        const std::string& k = it->first;
        if (k.compare(0, 4, "raw:") != 0 && k.compare(0, 8, "rawaddr:") != 0 &&
            k.compare(0, 7, "raword:") != 0) continue;
        fprintf(stderr, "[RtRawResidue] %-8llu %-16p %s\n",
                it->second.count, it->second.firstCaller, k.c_str());
    }
    fprintf(stderr, "[RtRawResidue] ==== end (these BYPASS the port registry by "
                    "design; each needs a per-site cutover verdict) ====\n");
    fflush(stderr);
}

// ---- The unresolved-symbol census -----------------------------------------
// Wave 9 lane F. See resolve.h for the policy and why it is not fatal by
// default.
void RecordResolveMiss(const char* mangled_name, uint16_t ordinal, void* caller) {
    char key[512];
    if (mangled_name) snprintf(key, sizeof(key), "%s", mangled_name);
    else              snprintf(key, sizeof(key), "ord:%u", (unsigned)ordinal);

    ResidueEntry& e = RtResolveMiss()[key];
    if (e.count == 0) {
        e.firstCaller = caller;
        // Name it AT RESOLVE TIME, once per distinct symbol. A pointer that is
        // null here faults much later, in a body that has no idea which symbol
        // it wanted, so the exit summary alone is not enough: a crash log has to
        // be readable next to a line that says what was missing.
        fprintf(stderr, "[RtUnresolved] %s  (asked by %p)\n", key, caller);
        fflush(stderr);
    }
    e.count++;

    if (RtFatalResolveEnabled()) {
        fprintf(stderr, "[RtUnresolved] BANISHED_RT_FATAL_RESOLVE is set -- "
                        "aborting on the first missing symbol.\n");
        fflush(stderr);
        abort();
    }
}

void DumpUnresolvedSymbols() {
    const std::map<std::string, ResidueEntry>& m = RtResolveMiss();
    if (m.empty()) return;   // the healthy case: say nothing
    fprintf(stderr, "[RtUnresolved] ==== %zu distinct symbols resolved to "
                    "NOTHING this run ====\n", m.size());
    for (std::map<std::string, ResidueEntry>::const_iterator it = m.begin();
         it != m.end(); ++it) {
        fprintf(stderr, "[RtUnresolved] %-8llu %-16p %s\n",
                it->second.count, it->second.firstCaller, it->first.c_str());
    }
    fprintf(stderr, "[RtUnresolved] ==== end (each of these is a null function "
                    "pointer waiting to be called) ====\n");
    fflush(stderr);
}

// The bodies, with no residue recording. ResolveExport / ResolveOrdinal call
// THESE on their fall-through path: recording there as well would file every
// port-registry miss twice, once under its own name and once under "raw:",
// and the raw block is meant to name the sites that ask for the shipped code
// DELIBERATELY.
static void* ResolveExportRawImpl(const char* mangled_name) {
    if (!g_modules[1].handle || !mangled_name) return nullptr;
    return reinterpret_cast<void*>(
        GetProcAddress(g_modules[1].handle, mangled_name)
    );
}

static void* ResolveOrdinalRawImpl(uint16_t ordinal) {
    if (!g_modules[1].handle) return nullptr;
    return reinterpret_cast<void*>(
        GetProcAddress(g_modules[1].handle, MAKEINTRESOURCEA(ordinal))
    );
}

// ---- Residue-free registry queries (Wave 9 lane C2) -----------------------
// See the comment in resolve.h. These deliberately do NOT fall through to the
// DLL and do NOT record residue: they answer "is this ported", not "give me
// something callable".
void* RtPortLookupExport(const char* mangled_name) {
    if (!mangled_name || !RtPortEnabled()) return nullptr;
    auto& reg = RtExportReg();
    auto it = reg.find(mangled_name);
    return it == reg.end() ? nullptr : it->second;
}

void* RtPortLookupAddr(uintptr_t ghidra_addr) {
    if (!ghidra_addr || !RtPortEnabled()) return nullptr;
    auto& reg = RtAddrReg();
    auto it = reg.find(ghidra_addr);
    return it == reg.end() ? nullptr : it->second;
}

// ---- Export-table walk (Wave 20 lane DOORS) --------------------------------
//
// See the comment in resolve.h. Parses the loaded module's PE export directory
// directly rather than going through any resolver, so that a screen wanting a
// subject can take one from the BINARY. Named exports only: an ordinal-only
// export has no name to hand back and is skipped, which is stated here because
// the count returned is the screen's denominator.
int RtEnumRuntimeExports(bool (*visit)(const char* name, void* addr,
                                       uintptr_t ghidraAddr, void* ctx),
                         void* ctx) {
    if (!visit) return 0;
    HMODULE h = g_modules[static_cast<int>(Module::Runtime)].handle;
    if (!h) return 0;

    const auto* base = reinterpret_cast<const unsigned char*>(h);
    const auto* dos  = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    const IMAGE_DATA_DIRECTORY& dd =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd.VirtualAddress || !dd.Size) return 0;

    const auto* ex = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dd.VirtualAddress);
    const auto* nameRvas = reinterpret_cast<const uint32_t*>(base + ex->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const uint16_t*>(base + ex->AddressOfNameOrdinals);
    const auto* funcRvas = reinterpret_cast<const uint32_t*>(base + ex->AddressOfFunctions);

    int seen = 0;
    for (uint32_t i = 0; i < ex->NumberOfNames; ++i) {
        const char* name = reinterpret_cast<const char*>(base + nameRvas[i]);
        const uint16_t ord = ordinals[i];
        void* addr = (ord < ex->NumberOfFunctions)
                   ? const_cast<unsigned char*>(base + funcRvas[ord])
                   : nullptr;
        const uintptr_t ghidra =
            addr ? (reinterpret_cast<uintptr_t>(addr) -
                    reinterpret_cast<uintptr_t>(base) +
                    static_cast<uintptr_t>(nt->OptionalHeader.ImageBase))
                 : 0;
        ++seen;
        if (!visit(name, addr, ghidra, ctx)) break;
    }
    return seen;
}

void* ResolveExportRaw(const char* mangled_name) {
    RecordRawResidue(mangled_name, 0, 0, _ReturnAddress());
    return ResolveExportRawImpl(mangled_name);
}

void* ResolveOrdinalRaw(uint16_t ordinal) {
    RecordRawResidue(nullptr, 0, ordinal, _ReturnAddress());
    return ResolveOrdinalRawImpl(ordinal);
}

uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr) {
    RecordRawResidue(nullptr, ghidra_addr, 0, _ReturnAddress());
    const ModuleInfo& mi = g_modules[static_cast<int>(Module::Runtime)];
    if (!mi.handle) return 0;
    return static_cast<uintptr_t>(
        static_cast<intptr_t>(ghidra_addr) + mi.delta
    );
}

void* ResolveExport(const char* mangled_name) {
    // WAVE 17 LANE CEIL — door attendance. FIRST, before the registry
    // short-circuit, so a row that IS registered is still counted; off unless
    // BANISHED_W17_ATTEND=1, and one strcmp against a 63-entry list when on.
    { extern void Wave17DoorAttendNote(const char*); Wave17DoorAttendNote(mangled_name); }
    if (mangled_name && RtPortEnabled()) {
        auto& reg = RtExportReg();
        auto it = reg.find(mangled_name);
        if (it != reg.end()) {
            g_rtHits++;
            return it->second;
        }
    }
    // Missed the port registry: this request is cutover residue.
    void* caller = _ReturnAddress();
    RecordRuntimeResidue(mangled_name, 0, 0, caller);
    RtResolveNoteEarly(mangled_name, 0, 0, caller);   // Wave 9 lane H
    void* fn = ResolveExportRawImpl(mangled_name);
    // Wave 9 lane F: nothing catches a null from here any more. Say so.
    if (!fn) RecordResolveMiss(mangled_name, 0, caller);
    return fn;
}

// ---- The ORDINAL door's bridge into the NAME registry ---------------------
// Wave 9, cutover attempt 07. `Device::Lock`/`Unlock` were PORTED (lane VID,
// registered by name and by address) and the residue list STILL carried
// `ord:3214` / `ord:4462` as null. The reason is structural and applies to
// every ordinal site, not to those two: `ResolveOrdinal` consulted only
// `RtOrdinalReg()`, a registry of its own, so a port registered by NAME is
// INVISIBLE to an ordinal caller. Registered, census-counted, audit-clean, and
// still null at cutover.
//
// Two lanes were each right and the combination was wrong: one identified the
// ordinals as Lock/Unlock and handed them on unported; the other ported those
// names and predicted they would leave the residue list. Neither could see the
// door between them.
//
// Fixed here rather than by asking every lane to also call
// RegisterRuntimeOrdinal, because that is a rule people forget exactly once and
// the failure is silent. The ordinal->name map is a LINK-TIME CONSTANT read out
// of the shipped export directory, so it still works with the DLL absent, which
// is the only moment it matters.
namespace {
struct OrdinalName { uint16_t ordinal; const char* mangled; };

// Every ordinal this tree passes to ResolveOrdinal, with the name the shipped
// export directory gives it. Verified against Runtime-x64.dll with pefile.
const OrdinalName kOrdinalNames[] = {
    {  443, "??0Thread@System@@QEAA@I@Z" },
    {  798, "??2GroupDescription@UI@@SAPEAX_KPEAX@Z" },
    { 2990, "?GetTypeIdForFileExtension@Factory@Object@@QEBAIPEBD@Z" },
    { 2991, "?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z" },
    { 3089, "?IsA@Base@Object@@QEBA_NI@Z" },
    { 3214, "?Lock@Device@Video@@QEAAXXZ" },
    { 4462, "?Unlock@Device@Video@@QEAAXXZ" },
};

const char* MangledForOrdinal(uint16_t ordinal) {
    for (const OrdinalName& o : kOrdinalNames)
        if (o.ordinal == ordinal) return o.mangled;
    return nullptr;
}
}  // namespace

void* ResolveOrdinal(uint16_t ordinal) {
    if (RtPortEnabled()) {
        auto& reg = RtOrdinalReg();
        auto it = reg.find(ordinal);
        if (it != reg.end()) {
            g_rtHits++;
            return it->second;
        }
        // Not registered by ordinal -- try the NAME this ordinal denotes.
        if (const char* mangled = MangledForOrdinal(ordinal)) {
            auto& nreg = RtExportReg();
            auto nit = nreg.find(mangled);
            if (nit != nreg.end()) {
                g_rtHits++;
                return nit->second;
            }
        } else {
            // An ordinal with no table row cannot reach the name registry, so a
            // port of that function would be silently bypassed here. Say so
            // ONCE rather than letting it pass as an ordinary DLL call.
            static std::set<uint16_t> warned;
            if (warned.insert(ordinal).second) {
                fprintf(stderr,
                        "[RtResolve] ordinal %u has no kOrdinalNames row -- a "
                        "PORT of this function registered by name would be "
                        "INVISIBLE here. Add the row (resolve_standalone.cpp).\n",
                        ordinal);
            }
        }
    }
    void* caller = _ReturnAddress();
    RecordRuntimeResidue(nullptr, 0, ordinal, caller);
    RtResolveNoteEarly(nullptr, 0, ordinal, caller);   // Wave 9 lane H
    void* fn = ResolveOrdinalRawImpl(ordinal);
    if (!fn) RecordResolveMiss(nullptr, ordinal, caller);
    return fn;
}

