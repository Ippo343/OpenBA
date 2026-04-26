#pragma once
// Multi-binary address resolution for Banished reimplementation.
// Translates Ghidra addresses to runtime addresses for any loaded module.
//
// Two build modes:
//   DLL mode (default):       ASLR delta against loaded original exe/DLLs.
//   Standalone mode (-DSTANDALONE_EXE): Exe addresses resolved via address
//       registry of reimplemented functions. Runtime/VideoDX11 still use ASLR.

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
// DLL mode: Returns true if exe + Runtime-x64.dll are resolved.
// Standalone mode: Returns true if Runtime-x64.dll is resolved (exe is us).
bool ResolveInit();

// Translate a Ghidra address to a runtime address.
// DLL mode: applies ASLR delta for all modules.
// Standalone mode: Module::Exe queries the address registry; others use ASLR delta.
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
#ifdef STANDALONE_EXE

// Register a reimplemented function/vtable/global at a Ghidra exe address.
// Call during static init or from standalone main before game loop.
void RegisterExeAddr(uintptr_t ghidra_addr, uintptr_t our_addr);

// Batch registration helper
struct ExeAddrEntry {
    uintptr_t ghidra_addr;
    uintptr_t our_addr;
};
void RegisterExeAddrs(const ExeAddrEntry* entries, size_t count);

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

#endif // STANDALONE_EXE

// --- Inline implementation (DLL mode only) ---

#ifndef STANDALONE_EXE

extern ModuleInfo g_modules[static_cast<int>(Module::COUNT)];

inline uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr) {
    return static_cast<uintptr_t>(
        static_cast<intptr_t>(ghidra_addr) + g_modules[static_cast<int>(mod)].delta
    );
}

#endif // !STANDALONE_EXE
