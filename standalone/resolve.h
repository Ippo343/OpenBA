#pragma once
// Multi-binary address resolution for Banished reimplementation.
// Translates Ghidra addresses to runtime addresses for any loaded module.
//
// Exe addresses are resolved via an address registry of reimplemented
// functions; Module::Runtime addresses go through the port registry. Neither
// Runtime-x64.dll nor VideoDX11-x64.dll is ever loaded (ENDGAME.md A.1), and
// since ENDGAME.md A.3 no door is left that could reach them: the raw
// GetProcAddress doors and the ordinal registry are gone.

#include <windows.h>
#include <cstdint>

// Supported game modules
enum class Module : int {
    Exe       = 0,  // Application-x64.exe  (preferred base 0x140000000)
    Runtime   = 1,  // Runtime-x64.dll      (preferred base 0x180000000, never mapped)
    COUNT     = 2
};

// Per-module info (populated by ResolveInit)
struct ModuleInfo {
    const char* name;           // module filename (NULL for exe)
    uintptr_t   preferredBase;  // PE preferred image base (from Ghidra)
    uintptr_t   runtimeBase;    // actual base at runtime
    intptr_t    delta;          // runtimeBase - preferredBase (signed, ASLR can go either way)
    HMODULE     handle;         // module handle
};

// Initialize address resolution. Returns false only if Runtime-x64.dll turns
// out to be mapped in the process, which must never happen (ENDGAME.md A.1).
bool ResolveInit();

// Translate a Ghidra address to a runtime address.
// Module::Exe queries the address registry; Module::Runtime the port registry.
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// Get the ModuleInfo for a module (read-only).
const ModuleInfo* GetModuleInfo(Module mod);

// Resolve a Runtime function by mangled name through the port registry.
void* ResolveExport(const char* mangled_name);

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
// The runtime port uses the SAME progressive seam as the exe port: our call
// sites into what used to be Runtime-x64.dll all go through ResolveExport or
// ResolveAddr(Module::Runtime, ...), so a port becomes live for every one of
// them the moment it is registered here. Register during RuntimePortInit(),
// which runs after ResolveInit() and BEFORE EngineFuncsInit/AddressRegistryInit
// cache their pointers. A symbol nothing registers resolves to null and is
// named once by RecordResolveMiss below.
void RegisterRuntimeExport(const char* mangled_name, void* our_fn);
void RegisterRuntimeAddr(uintptr_t ghidra_addr, void* our_fn);

// Residue-free port-registry queries (Wave 9 lane C2, the DATA mint).
//
// "Is this symbol ported, and to what address" — for code that wants to INSPECT
// the registry rather than call through it. The mint's shadow check asks this
// about ~250 symbols on every pass; asking through ResolveExport instead would
// have manufactured several hundred diagnostic lines and destroyed the one
// measurement the cutover gate depended on.
void* RtPortLookupExport(const char* mangled_name);
void* RtPortLookupAddr(uintptr_t ghidra_addr);

// Populated by the reg_rt_*.cpp wave files; called from standalone/main.cpp
// right after ResolveInit(). Safe to call when nothing is registered.
void RuntimePortInit();

// Progress reporting: how much of the runtime we have taken over.
struct RuntimeRegistryStats {
    size_t exports;   // RegisterRuntimeExport entries
    size_t addrs;     // RegisterRuntimeAddr entries
    size_t hits;      // resolutions that returned OUR code
};
RuntimeRegistryStats GetRuntimeRegistryStats();

// Wave 9 lane H: did a PORT register this name? ResolveExport cannot answer
// that on its own, and the early-snapshot question is exactly that distinction.
bool RuntimeExportIsRegistered(const char* mangled_name);

// ---- The null-resolve policy (Wave 9 lane F) ------------------------------
//
// Until 2026-09-07 the tree carried 163 sites shaped
//
//     fn = ResolveExport(<the mangled name>);
//     if (!fn) fn = ResolveAddr(Module::Runtime, <a hardcoded address>);  // <- the fallback
//
// (spelled without a string literal on purpose: tools/export_name_audit.py
// harvests every ResolveExport call with a string-literal argument, and a
// worked example in a comment reads to it as one more symbol the DLL does not
// export -- a checker made to cry wolf by the documentation of the thing it
// checks)
//
// Every one of those fallbacks was DEAD, because the primary always succeeded,
// and none could have survived cutover: with the DLL gone they would all have
// jumped at an address in a module that is not mapped. They were deleted rather
// than corrected -- a null is a better failure than a jump into the middle of a
// function (WAVE9_LANE_D.md found five whose addresses were already wrong, one
// of them a real entry point for the WRONG OVERLOAD).
//
// Deleting them leaves the question they were pretending to answer: what SHOULD
// happen when a resolve returns null? The answer this tree gives is
// NAME IT LOUDLY, AT RESOLVE TIME, AND KEEP GOING.
//
//   * Loudly, because the alternative is a null function pointer called much
//     later from a body that has no idea which symbol it wanted -- this
//     project's EXEC-of-0 crash logs, which name an address and nothing else.
//     One `[RtUnresolved] <symbol>` line per distinct symbol at the moment of
//     the miss puts the name in the log ABOVE the crash.
//   * At resolve time, not at first call, because most of these are filled by
//     EngineFuncsInit at boot and never checked again.
//   * And KEEP GOING, not abort, because a boot with unported symbols misses
//     many at once: aborting on the first would print one name and teach
//     nothing, while running to the end prints the whole work list.
//
// With no DLL left to fall through to, this IS the residue census: a symbol
// nothing registered is a symbol nothing implements.
void RecordResolveMiss(const char* mangled_name, uint16_t ordinal, void* caller);
void DumpUnresolvedSymbols();

// Query how many exe addresses are registered vs referenced (for progress tracking)
struct RegistryStats {
    size_t registered;  // addresses with real implementations
    size_t stubs;       // addresses hitting NotImplemented stub
};
RegistryStats GetRegistryStats();
