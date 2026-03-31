// Standalone address resolution for Banished reimplementation.
// In standalone mode, WE are the exe — so Module::Exe addresses resolve
// via a registry of our reimplemented functions. Module::Runtime and
// Module::VideoDX11 still use ASLR delta (they're loaded DLLs).
//
// This file replaces patches/src/resolve.cpp in standalone builds.

#ifdef STANDALONE_EXE

#include "resolve.h"
#include <cstdio>
#include <unordered_map>

// ============================================================
// Module info — same layout as DLL mode, but Exe slot unused
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
// ResolveAddr — standalone implementation
// ============================================================
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr) {
    if (mod == Module::Exe) {
        // Look up in our registry
        auto& reg = GetRegistry();
        auto it = reg.find(ghidra_addr);
        if (it != reg.end()) {
            return it->second;
        }
        // Not registered — warn and return 0 (callers should null-check)
        NotImplementedStub_impl(ghidra_addr);
        return 0;
    }

    // Runtime/VideoDX11 — standard ASLR delta (same as DLL mode)
    return static_cast<uintptr_t>(
        static_cast<intptr_t>(ghidra_addr) + g_modules[static_cast<int>(mod)].delta
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

    // VideoDX11-x64.dll: optional
    g_modules[2].handle = GetModuleHandleA(g_modules[2].name);
    if (!g_modules[2].handle) {
        g_modules[2].handle = LoadLibraryA(g_modules[2].name);
    }
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
// Shared functions — same in both modes
// ============================================================
const ModuleInfo* GetModuleInfo(Module mod) {
    return &g_modules[static_cast<int>(mod)];
}

void* ResolveExport(const char* mangled_name) {
    if (!g_modules[1].handle) return nullptr;
    return reinterpret_cast<void*>(
        GetProcAddress(g_modules[1].handle, mangled_name)
    );
}

void* ResolveOrdinal(uint16_t ordinal) {
    if (!g_modules[1].handle) return nullptr;
    return reinterpret_cast<void*>(
        GetProcAddress(g_modules[1].handle, MAKEINTRESOURCEA(ordinal))
    );
}

#endif // STANDALONE_EXE
