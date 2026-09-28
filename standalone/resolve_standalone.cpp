// Standalone address resolution for Banished reimplementation.
// WE are the exe, and since ENDGAME.md A.1 we are the runtime too: neither
// Runtime-x64.dll nor VideoDX11-x64.dll is ever mapped into the process.
// Module::Exe addresses resolve via a registry of our reimplemented functions;
// Module::Runtime addresses resolve via the port registry. ENDGAME.md A.3 then
// deleted the doors that could have reached the DLL at all — the raw
// GetProcAddress variants, the ordinal registry and the ASLR-delta path — so an
// unregistered runtime symbol resolves to null and is named once.


#include "resolve.h"
#include "options.h"
#include "../runtime/funcs/rt_port.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <algorithm>

// Return address of our own caller, for the unresolved-symbol census.
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
    // Runtime-x64.dll — never loaded (ENDGAME.md A.1). The row exists so
    // Module::Runtime keeps a preferred base for the registry lookups; the
    // handle stays null for the life of the process.
    { "Runtime-x64.dll",    0x180000000ULL, 0, 0, nullptr },
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
    return Opt().unregCensus;
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
// See the comment on RegisterRuntimeExport in resolve.h. Two lookup tables —
// by mangled name and by retail address — consulted by ResolveExport and
// ResolveAddr(Module::Runtime). There is nothing behind them: a name or address
// neither table answers resolves to null and is named by RecordResolveMiss.
namespace {

std::unordered_map<std::string, void*>& RtExportReg() {
    static std::unordered_map<std::string, void*> m;
    return m;
}
std::unordered_map<uintptr_t, void*>& RtAddrReg() {
    static std::unordered_map<uintptr_t, void*> m;
    return m;
}
size_t g_rtHits = 0;

// ---- Unresolved-symbol census (the Wave 9 lane F null-resolve policy) ------
// See the comment on RecordResolveMiss in resolve.h. Always on: the only cost
// is on the path where a resolve returned NOTHING, and with the DLL gone that
// path is the residue census -- a symbol nothing registered is a symbol
// nothing implements.
struct ResidueEntry {
    unsigned long long count;
    void* firstCaller;   // return address of the first asker, for attribution
};
std::map<std::string, ResidueEntry>& RtResolveMiss() {
    static std::map<std::string, ResidueEntry> m;
    return m;
}

} // namespace

// ---------------------------------------------------------------------------
// The two doors are two maps again (B3R W0-SM, 2026-09-28).
//
// From 2026-09-12 each Register* call was MIRRORED into the opposite map
// through the DLL's export directory (standalone/rt_export_addrs.inc), so a
// body registered by name only still answered its retail address. The mirror
// is deleted: the seven addresses whose only registration was the mirrored one
// and that a reader is bound to are registered explicitly by
// standalone/reg_rt_mirror16.cpp (the proof is
// docs/runtime_port/endgame_b/b3r_session/scripts/mirror_proof.py). A key is
// resolvable exactly when a Register* call names it; the last call wins.
// ---------------------------------------------------------------------------
void RegisterRuntimeExport(const char* mangled_name, void* our_fn) {
    if (!mangled_name || !our_fn) return;
    RtExportReg()[mangled_name] = our_fn;
}

void RegisterRuntimeAddr(uintptr_t ghidra_addr, void* our_fn) {
    if (!ghidra_addr || !our_fn) return;
    RtAddrReg()[ghidra_addr] = our_fn;
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
    return { RtExportReg().size(), RtAddrReg().size(), g_rtHits };
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
    RtSavePortInit();     // Wave 5: Save

    // Vtable batch 8 (2026-09-12): Collision2D::CollisionMesh/Surface,
    // Pathfinding::PointList, Picking::PickingMesh. Each registers
    // unconditionally (DIRECTION.md #2: no waves/lanes/flags for a port).
    RtVideoPortInit();    // Wave 6: Video
    RtGraphicsPortInit(); // Wave 6: Graphics
    RtUiPortInit();       // Wave 7: UI
    RtAudioPortInit();        // Wave 8: Audio (lane C)
    RtUtilityPortInit();      // Wave 8: Utility (lane E)

    // (Wave 28 lane FACTREST's RtFactoriesPortInit went with its last row at
    // ENDGAME B.3.9 OBJ: every factory pair is its class's own statics.)
    // LAST, deliberately. Wave 9 lane Q registers 27 ADDRESSES for the two
    // header-only Collection::Array bodies (~Array and SetSize), and those
    // addresses are already called by name-less ResolveAddr from graphics,
    // picking, ui, audio, achievement, engine and engine/funcs. Registering
    // them redirects all of those to us, so it happens at one identifiable
    // point in the init order rather than in the middle of it.

    // Cutover closure batch 2b (2026-09-12): the non-Collection half of one
    // WORKLIST.md batch — the four `operator delete` COMDAT copies, the
    // `operator new` copy, MemoryAllocator::Allocate's block-link encoder,
    // System::String::GetLength's missing registration, two copy constructors,
    // two serialize loops, a frustum support-vertex selector, Picking::Scene's
    // shared grid initialiser, Engine::Application's scalar deleting
    // destructor, Save::SavedState's delete and IO::Persistence's LOAD and
    // DELETE worker jobs. ONE hook: runtime/src/system/closure_batch2b.cpp
    // calls the other six namespaces' register functions.
    //
    // HERE, after the Collection inits, because two of its rows resolve
    // Collection addresses (0x18001b1a0 for Geometry's nested array,
    // 0x18007b960 for the Picking grid) — those are lazy, so the order is not
    // load-bearing, but putting it after keeps the "registrations that
    // redirect existing address doors" together at one point in the sequence.

    // (Wave 9 lane K's RtObjectDeletingDtorPortInit -- the RT_DELDTOR table
    // that filled slot 0 of the minted vtables -- went with the last minted
    // vtable at ENDGAME B.3.9 OBJ: every class declares its own DeletingDtor.)

    // Wave 9 lane X: the fourteen constructors that publish a `?_singleton@...`
    // DATA export. AFTER every other wave because its probe calls our OWN
    // ??0Random@System@@QEAA@XZ through the port-aware door, which only reaches
    // the port once Wave 2 has run.

    // B3R W0-SM: the seven addresses the deleted name->address mirror used to
    // serve. HERE, after every *PortInit() above, because each copies the body
    // its NAME holds (every one registered once, by RtUiPortInit /
    // RtGraphicsPortInit), and before the count below so the count includes them.
    // No other Register* call names these addresses, so nothing later can
    // change which body they hold. standalone/reg_rt_mirror16.cpp.

    // Registration is complete; this only reports. (The mint -- Wave 9 lane
    // C2's minted exported DATA, RtDataMintInit() -- was deleted at B3R W0-SM
    // M3: its last two storage rows are the class statics
    // System::MemoryAllocator::_singleton and System::Debug::_disableHandler.)
    RuntimeRegistryStats st = GetRuntimeRegistryStats();
    printf("[RtPort] runtime registry: %zu exports, %zu addrs\n",
           st.exports, st.addrs);

    // Wave 9 lane H: everything resolved from here on sees the full registry.
    // Anything recorded BEFORE this line resolved against a partly-built one,
    // and if its call site caches, it is frozen on the pre-registration answer.
    RtResolveMarkPortInitDone();

    // NOTE (Wave 17 lane CEIL): the door-attendance phase marker is DELIBERATELY
    // NOT here. RuntimePortInit is not the end of registration —
    // AddressRegistryInit() runs afterwards (standalone/main.cpp) and
    // reg_unreg_lane3.cpp resolves four ?_typeInfo@Graphics@@ rows by name to
    // fill dispatch slots. Marking here read all four as ATTENDANCE and would
    // have handed four rows a route that is really a registration.
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

    // Runtime port seam: an address we have reimplemented resolves to OUR code.
    // An address nothing registered resolves to 0, NOT to `ghidra_addr` itself:
    // 0x1800187c8 and the like are not null, look like perfectly good pointers,
    // and point at nothing, which is strictly worse than null for every caller.
    if (mod == Module::Runtime) {
        auto& reg = RtAddrReg();
        auto it = reg.find(ghidra_addr);
        if (it != reg.end()) {
            g_rtHits++;
            return reinterpret_cast<uintptr_t>(it->second);
        }
        RtResolveNoteEarly(nullptr, ghidra_addr, 0, _ReturnAddress());   // Wave 9 lane H
    }

    return 0;
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

    // Runtime-x64.dll is NEVER loaded (docs/runtime_port/ENDGAME.md A.1). Our
    // ports and our allocator are the runtime; a null handle is the
    // only correct state, and since A.3 no door exists that could use one.
    g_modules[1].handle      = nullptr;
    g_modules[1].runtimeBase = 0;
    g_modules[1].delta       = 0;

    // Nothing in the process may map it, either. Kept as a fail-fast rather
    // than dropped with the raw doors: a mapped copy means something in the
    // build is still pulling the DLL in, and that is worth stopping on.
    if (GetModuleHandleA("Runtime-x64.dll")) {
        fprintf(stderr, "[Standalone] FATAL: Runtime-x64.dll is mapped in the "
                        "process -- the exe must never load it (ENDGAME A.1)\n");
        fflush(stderr);
        return false;
    }
    fprintf(stderr, "[Standalone] Runtime-x64.dll: never loaded (ENDGAME A.1)\n");
    fflush(stderr);

    // Our class statics own every exported data symbol and our allocator owns
    // the heap, because nothing else exists; a cutover boot has no other meaning. That
    // used to be published as three BANISHED_RT_* variables set from here;
    // ENDGAME C.1 deleted the readers, so there is nothing left to announce.

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

// ---- The unresolved-symbol census -----------------------------------------
// Wave 9 lane F. See resolve.h for the policy and why it is not fatal.
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

// ---- Residue-free registry queries (Wave 9 lane C2) -----------------------
// See the comment in resolve.h. These record no miss and print nothing: they
// answer "is this ported", not "give me something callable".
void* RtPortLookupExport(const char* mangled_name) {
    if (!mangled_name) return nullptr;
    auto& reg = RtExportReg();
    auto it = reg.find(mangled_name);
    return it == reg.end() ? nullptr : it->second;
}

void* RtPortLookupAddr(uintptr_t ghidra_addr) {
    if (!ghidra_addr) return nullptr;
    auto& reg = RtAddrReg();
    auto it = reg.find(ghidra_addr);
    return it == reg.end() ? nullptr : it->second;
}

void* ResolveExport(const char* mangled_name) {
    if (mangled_name) {
        auto& reg = RtExportReg();
        auto it = reg.find(mangled_name);
        if (it != reg.end()) {
            g_rtHits++;
            return it->second;
        }
    }
    // Nothing registered this name and there is nothing behind the registry,
    // so the answer is null and the symbol is unimplemented. Name it once.
    void* caller = _ReturnAddress();
    RtResolveNoteEarly(mangled_name, 0, 0, caller);   // Wave 9 lane H
    RecordResolveMiss(mangled_name, 0, caller);
    return nullptr;
}
