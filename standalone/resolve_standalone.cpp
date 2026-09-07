// Standalone address resolution for Banished reimplementation.
// In standalone mode, WE are the exe — so Module::Exe addresses resolve
// via a registry of our reimplemented functions. Module::Runtime and
// Module::VideoDX11 still use ASLR delta (they're loaded DLLs).


#include "resolve.h"
#include "../runtime/funcs/rt_port.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

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

    // Nothing to resolve at this point — the tables hold our own function
    // pointers, not DLL addresses — so the rest of this only reports.
    RuntimeRegistryStats st = GetRuntimeRegistryStats();
    printf("[RtPort] runtime registry: %zu exports, %zu addrs, %zu ordinals%s\n",
           st.exports, st.addrs, st.ordinals,
           RtPortEnabled() ? "" : "  (DISABLED via BANISHED_RT_PORT=0)");
    DumpRuntimeRegistry();
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
    if (!g_modules[1].handle) return false;
    g_modules[1].runtimeBase = reinterpret_cast<uintptr_t>(g_modules[1].handle);
    g_modules[1].delta = static_cast<intptr_t>(g_modules[1].runtimeBase)
                       - static_cast<intptr_t>(g_modules[1].preferredBase);

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

void* ResolveExportRaw(const char* mangled_name) {
    if (!g_modules[1].handle || !mangled_name) return nullptr;
    return reinterpret_cast<void*>(
        GetProcAddress(g_modules[1].handle, mangled_name)
    );
}

void* ResolveOrdinalRaw(uint16_t ordinal) {
    if (!g_modules[1].handle) return nullptr;
    return reinterpret_cast<void*>(
        GetProcAddress(g_modules[1].handle, MAKEINTRESOURCEA(ordinal))
    );
}

uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr) {
    const ModuleInfo& mi = g_modules[static_cast<int>(Module::Runtime)];
    if (!mi.handle) return 0;
    return static_cast<uintptr_t>(
        static_cast<intptr_t>(ghidra_addr) + mi.delta
    );
}

void* ResolveExport(const char* mangled_name) {
    if (mangled_name && RtPortEnabled()) {
        auto& reg = RtExportReg();
        auto it = reg.find(mangled_name);
        if (it != reg.end()) {
            g_rtHits++;
            return it->second;
        }
    }
    return ResolveExportRaw(mangled_name);
}

void* ResolveOrdinal(uint16_t ordinal) {
    if (RtPortEnabled()) {
        auto& reg = RtOrdinalReg();
        auto it = reg.find(ordinal);
        if (it != reg.end()) {
            g_rtHits++;
            return it->second;
        }
    }
    return ResolveOrdinalRaw(ordinal);
}

