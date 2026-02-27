// Multi-binary address resolution.
// Resolves runtime bases for Application-x64.exe, Runtime-x64.dll, VideoDX11-x64.dll.

#include "resolve.h"
#include <cstdio>

ModuleInfo g_modules[static_cast<int>(Module::COUNT)] = {
    // Exe: name=NULL means GetModuleHandle(NULL)
    { nullptr,              0x140000000ULL, 0, 0, nullptr },
    // Runtime-x64.dll
    { "Runtime-x64.dll",    0x180000000ULL, 0, 0, nullptr },
    // VideoDX11-x64.dll
    { "VideoDX11-x64.dll",  0x180000000ULL, 0, 0, nullptr },
};

bool ResolveInit() {
    // Exe: always loaded
    g_modules[0].handle = GetModuleHandleA(nullptr);
    if (!g_modules[0].handle) return false;
    g_modules[0].runtimeBase = reinterpret_cast<uintptr_t>(g_modules[0].handle);
    g_modules[0].delta = static_cast<intptr_t>(g_modules[0].runtimeBase)
                       - static_cast<intptr_t>(g_modules[0].preferredBase);

    // Runtime-x64.dll: required
    g_modules[1].handle = GetModuleHandleA(g_modules[1].name);
    if (!g_modules[1].handle) return false;
    g_modules[1].runtimeBase = reinterpret_cast<uintptr_t>(g_modules[1].handle);
    g_modules[1].delta = static_cast<intptr_t>(g_modules[1].runtimeBase)
                       - static_cast<intptr_t>(g_modules[1].preferredBase);

    // VideoDX11-x64.dll: optional
    g_modules[2].handle = GetModuleHandleA(g_modules[2].name);
    if (g_modules[2].handle) {
        g_modules[2].runtimeBase = reinterpret_cast<uintptr_t>(g_modules[2].handle);
        g_modules[2].delta = static_cast<intptr_t>(g_modules[2].runtimeBase)
                           - static_cast<intptr_t>(g_modules[2].preferredBase);
    }

#ifdef _DEBUG
    for (int i = 0; i < static_cast<int>(Module::COUNT); i++) {
        if (g_modules[i].handle) {
            printf("[Resolve] %s: base=0x%llx preferred=0x%llx delta=%+lld\n",
                   g_modules[i].name ? g_modules[i].name : "Application-x64.exe",
                   (unsigned long long)g_modules[i].runtimeBase,
                   (unsigned long long)g_modules[i].preferredBase,
                   (long long)g_modules[i].delta);
        } else {
            printf("[Resolve] %s: not loaded\n",
                   g_modules[i].name ? g_modules[i].name : "Application-x64.exe");
        }
    }
#endif

    return true;
}

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
