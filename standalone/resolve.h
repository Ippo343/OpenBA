#pragma once
// Multi-binary address resolution for Banished reimplementation.
// Translates Ghidra addresses to runtime addresses for any loaded module.
//
// Exe addresses are resolved via an address registry of reimplemented
// functions; Runtime-x64.dll and VideoDX11-x64.dll use the normal ASLR delta.

#include <windows.h>
#include <cstdint>

// Supported game modules
enum class Module : int {
    Exe       = 0,  // Application-x64.exe  (preferred base 0x140000000)
    Runtime   = 1,  // Runtime-x64.dll      (preferred base 0x180000000)
    VideoDX11 = 2,  // VideoDX11-x64.dll    (preferred base 0x180000000)
    COUNT     = 3
};

// Per-module info (populated by ResolveInit)
struct ModuleInfo {
    const char* name;           // module filename (NULL for exe)
    uintptr_t   preferredBase;  // PE preferred image base (from Ghidra)
    uintptr_t   runtimeBase;    // actual base at runtime
    intptr_t    delta;          // runtimeBase - preferredBase (signed, ASLR can go either way)
    HMODULE     handle;         // module handle
};

// Initialize address resolution for all loaded modules.
// Returns true if Runtime-x64.dll is resolved (the exe is us).
bool ResolveInit();

// Translate a Ghidra address to a runtime address.
// Module::Exe queries the address registry; others use the ASLR delta.
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// Get the ModuleInfo for a module (read-only).
const ModuleInfo* GetModuleInfo(Module mod);

// Resolve an exported function from Runtime-x64.dll by mangled name.
void* ResolveExport(const char* mangled_name);

// Resolve an exported function from Runtime-x64.dll by ordinal.
void* ResolveOrdinal(uint16_t ordinal);

// Read a value at a Ghidra address (for globals/singletons).
template<typename T>
inline T ReadGlobal(Module mod, uintptr_t ghidra_addr) {
    return *reinterpret_cast<T*>(ResolveAddr(mod, ghidra_addr));
}

// Write a value at a Ghidra address.
template<typename T>
inline void WriteGlobal(Module mod, uintptr_t ghidra_addr, T value) {
    *reinterpret_cast<T*>(ResolveAddr(mod, ghidra_addr)) = value;
}

// --- Standalone address registry ---

// Register a reimplemented function/vtable/global at a Ghidra exe address.
// Call during static init or from standalone main before game loop.
void RegisterExeAddr(uintptr_t ghidra_addr, uintptr_t our_addr);

// Batch registration helper
struct ExeAddrEntry {
    uintptr_t ghidra_addr;
    uintptr_t our_addr;
};
void RegisterExeAddrs(const ExeAddrEntry* entries, size_t count);

// --- Runtime-x64.dll port registry (the Wave 0 seam) -----------------------
//
// The runtime port uses the SAME progressive seam as the exe port: our ~1,180
// call sites into Runtime-x64.dll all go through ResolveExport /
// ResolveOrdinal / ResolveAddr(Module::Runtime, ...), so a port becomes live
// for every one of them the moment it is registered here. Nothing else has to
// change. Register during RuntimePortInit(), which runs after ResolveInit()
// and BEFORE EngineFuncsInit/AddressRegistryInit cache their pointers.
//
// IMPORTANT: registering only redirects OUR callers. The DLL's own internal
// calls never go through its export table, so they keep landing in the DLL.
// To redirect those too, install a detour (standalone/runtime_detour.h).
//
// Anything that patches, detours, trampolines to, or diff-tests against the
// ORIGINAL DLL code must resolve through the *Raw* variants below, or it will
// happily patch our own reimplementation once the port registers.
void RegisterRuntimeExport(const char* mangled_name, void* our_fn);
void RegisterRuntimeAddr(uintptr_t ghidra_addr, void* our_fn);
void RegisterRuntimeOrdinal(uint16_t ordinal, void* our_fn);

// Residue-free port-registry queries (Wave 9 lane C2, the DATA mint).
//
// "Is this symbol ported, and to what address" — WITHOUT the fallback to the
// DLL and WITHOUT recording a BANISHED_RT_STRICT residue entry. Returns our
// registered function, or nullptr if nothing is registered under that key.
//
// The mint's shadow check asks this about ~250 symbols on every pass. Asking
// through ResolveExport instead would have manufactured several hundred
// `[RtResidue]` lines and destroyed the one measurement the cutover gate
// depends on — the same shape of self-inflicted signal as
// reference_truncated_tool_output_manufactures_calls_zero.md. Anything that
// wants to INSPECT the registry rather than call through it belongs here.
void* RtPortLookupExport(const char* mangled_name);
void* RtPortLookupAddr(uintptr_t ghidra_addr);

// Original-DLL resolution, bypassing the port registry. Always the shipped code.
void*     ResolveExportRaw(const char* mangled_name);
void*     ResolveOrdinalRaw(uint16_t ordinal);
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

// Populated by the reg_rt_*.cpp wave files; called from standalone/main.cpp
// right after ResolveInit(). Safe to call when nothing is registered.
void RuntimePortInit();

// Progress reporting: how much of the DLL we have taken over.
struct RuntimeRegistryStats {
    size_t exports;   // RegisterRuntimeExport entries
    size_t addrs;     // RegisterRuntimeAddr entries
    size_t ordinals;  // RegisterRuntimeOrdinal entries
    size_t hits;      // resolutions that returned OUR code
};
RuntimeRegistryStats GetRuntimeRegistryStats();

// Machine-readable dump (one line per entry) for tools/runtime_census.py.
// Written to stderr when BANISHED_RT_CENSUS is set.
void DumpRuntimeRegistry();

// ---- Cutover residue census (BANISHED_RT_STRICT=1) ------------------------
// PLAN.md wave 9 removes `LoadLibrary("Runtime-x64.dll")` entirely. The gate on
// that is not "how many functions are ported" but "does anything still ASK the
// DLL for something at runtime". `tools/runtime_census.py` answers the first
// question from source; only the process can answer the second, because the
// source list is a superset -- it counts every resolve site in the tree,
// including ones no reachable path ever executes.
//
// With BANISHED_RT_STRICT set, every ResolveExport / ResolveOrdinal /
// ResolveAddr(Module::Runtime) that MISSES the port registry and falls through
// to the shipped DLL is recorded, with the return address of whoever asked.
// `DumpRuntimeResidue()` then prints one `[RtResidue]` line per distinct miss.
//
// The three *Raw* doors below are ALSO recorded, in a second population printed
// as `[RtRawResidue]` (keys prefixed "raw:" / "rawaddr:" / "raword:"). They
// bypass the port registry deliberately, so they are not "unported symbols" --
// but they ARE requests the process makes of the DLL, and until 2026-09-07 they
// were invisible here, which made an empty `[RtResidue]` list look like proof of
// something it did not prove (lane D's example: 0x18007fbfc, the DLL's own
// _beginthreadex, on the path of every System::Thread, in no residue run).
//
// So the gate is: `[RtResidue]` empty AND every `[RtRawResidue]` line carrying a
// written per-site verdict -- oracle-only (dies with the oracle) or a real
// dependency that must be replaced.
//
// LOG-ONLY BY DESIGN. It does not null the result or hand back a trap stub:
// a large share of the misses are exported DATA (`?_singleton@...`, `??_7...`
// vtables, `?_typeInfo@...`), and poisoning those corrupts the process long
// before the call that would name the culprit. Strict mode observes; it does
// not simulate the DLL being gone. Deleting the DLL is what does that.
void RecordRuntimeResidue(const char* mangled_name, uintptr_t ghidra_addr,
                          uint16_t ordinal, void* caller);
void DumpRuntimeResidue();

// ---- The null-resolve policy (Wave 9 lane F) ------------------------------
//
// Until 2026-09-07 the tree carried 163 sites shaped
//
//     fn = ResolveExport("?Mangled@...");
//     if (!fn) fn = ResolveAddr(Module::Runtime, 0x1800xxxxx);   // <- fallback
//
// Every one of those fallbacks was DEAD, because the primary always succeeded.
// They could not survive cutover: with the DLL gone no ResolveExport can
// succeed, so all 163 would have become the live path SIMULTANEOUSLY, each
// jumping at an address in a module that is not mapped. They were deleted
// rather than corrected -- a null is a better failure than a jump into the
// middle of a function (WAVE9_LANE_D.md found five whose addresses were already
// wrong, one of them a real entry point for the WRONG OVERLOAD, which would have
// run and done tree surgery with garbage in RDX).
//
// Deleting them leaves the question they were pretending to answer: what SHOULD
// happen when a resolve returns null? The answer this tree now gives is
// NAME IT LOUDLY, AT RESOLVE TIME, AND KEEP GOING.
//
//   * Loudly, because the alternative is a null function pointer called much
//     later from a body that has no idea which symbol it wanted -- this
//     project's EXEC-of-0 crash logs, which name an address and nothing else.
//     One `[RtUnresolved] <symbol>` line per distinct symbol at the moment of
//     the miss puts the name in the log ABOVE the crash.
//   * At resolve time, not at first call, because most of these are filled by
//     EngineFuncsInit at boot and never checked again.
//   * And KEEP GOING, not abort, for the reason that decides the whole policy:
//     ON CUTOVER DAY EVERY UNPORTED SYMBOL MISSES. Aborting on the first one
//     would print one name and teach nothing; running to the end prints the
//     complete list of what is missing, which is exactly the work list lane G
//     needs. `BANISHED_RT_FATAL_RESOLVE=1` opts into fail-fast for the narrower
//     case of "this run should have resolved everything -- stop where it
//     didn't".
//
// A miss is recorded ALWAYS, not under BANISHED_RT_STRICT: strict mode measures
// requests that reach the DLL, which is a different question, and a symbol that
// resolved to nothing is a defect in any run whether or not anyone is measuring.
// The cost is one map insert on a path that in a healthy pre-cutover run is
// never taken.
//
// This does NOT replace tools/export_name_audit.py, which catches a phantom
// mangling from source before the process runs; it is the runtime half, and it
// keeps working after cutover, when the export table the static audit reads is
// gone (`reference_resolveexport_phantom_names.md`).
void RecordResolveMiss(const char* mangled_name, uint16_t ordinal, void* caller);
void DumpUnresolvedSymbols();

// ---- Unregistered-address census (BANISHED_UNREG_CENSUS=1) ----------------
// ResolveAddr's NOT IMPLEMENTED warning fires ONCE per address for the whole
// process, so an address hit at boot AND again during a save load is only ever
// attributed to boot. That makes the log useless for asking "which unregistered
// slots does the LOAD path actually touch". The census records, per address,
// the phase of the first hit and a per-phase hit count, and dumps the table
// sorted at exit.
//
// Phase is a plain label pushed by the code that knows it has entered one;
// UnregPhase() is the current label ("boot" until someone sets it).
void SetUnregPhase(const char* phase);
const char* UnregPhase();
void DumpUnregCensus();

// Query how many exe addresses are registered vs referenced (for progress tracking)
struct RegistryStats {
    size_t registered;  // addresses with real implementations
    size_t stubs;       // addresses hitting NotImplemented stub
};
RegistryStats GetRegistryStats();

// ---- Bracket-window callback tracer ---------------------------------------
// When non-zero, ResolveAddr(Module::Exe, ...) prints a [TRACE] line for
// EVERY resolution (registered + unimplemented), and the bulk NullFactory
// stubs print [TRACE!] when hit. Counter (not bool) so nested brackets
// nest cleanly — each setter increments, each unsetter decrements.
//
// Usage:
//   TraceExeBegin("fn_DialogLoaderLoopT_Load(self=...)");
//   fn_DialogLoaderLoopT_Load(self);
//   TraceExeEnd("fn_DialogLoaderLoopT_Load");
//
// The label is echoed verbatim into the log so multiple bracketed regions
// in one run can be distinguished.
extern volatile int g_traceExeCallbacks;
void TraceExeBegin(const char* label);
void TraceExeEnd(const char* label);
