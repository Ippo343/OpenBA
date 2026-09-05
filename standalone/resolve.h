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
