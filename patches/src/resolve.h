#pragma once
// Multi-binary address resolution for Banished reimplementation.
// Translates Ghidra addresses to runtime addresses for any loaded module.
// Handles ASLR by computing delta at init time.

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
// Returns true if exe + Runtime-x64.dll are resolved.
// VideoDX11 is optional (may not be loaded yet).
// Call this after Runtime-x64.dll is confirmed loaded.
bool ResolveInit();

// Translate a Ghidra address to a runtime address.
inline uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

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

// --- Inline implementation ---

extern ModuleInfo g_modules[static_cast<int>(Module::COUNT)];

inline uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr) {
    return static_cast<uintptr_t>(
        static_cast<intptr_t>(ghidra_addr) + g_modules[static_cast<int>(mod)].delta
    );
}
