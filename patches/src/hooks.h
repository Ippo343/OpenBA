#pragma once
// Hook infrastructure and reimplementation registration.
//
// Two systems:
//   1. Low-level inline hooking (InstallHook/RemoveHook/HookInfo)
//   2. Reimplementation registry (ReimplEntry table iterated by InstallHooks/RemoveHooks)

#include <windows.h>
#include <cstdint>

// Translate Ghidra address -> runtime address (defined in dllmain.cpp, legacy wrapper)
extern uintptr_t ghidra_addr(uintptr_t ghidra_address);

// ============================================================
// Low-level hook: 12-byte absolute jump for x64 (mov rax, imm64; jmp rax)
// ============================================================

struct HookInfo {
    void* trampoline;     // call this to invoke the original function
    void* target;         // the address we hooked
    uint8_t original[16]; // saved original bytes
    size_t patchSize;     // bytes overwritten (always 12 for x64 abs jump)
};

// Install a hook: overwrites target with a jump to detour, returns trampoline
bool InstallHook(uintptr_t targetAddr, void* detourFunc, HookInfo* info);

// Remove a hook: restores original bytes
void RemoveHook(HookInfo* info);

// ============================================================
// Reimplementation registry
// ============================================================

// Each reimplemented function registers an entry.
// InstallHooks() iterates these; RemoveHooks() tears them down.
struct ReimplEntry {
    const char* name;     // human-readable name (e.g., "MainLoop::Frame")
    void (*install)();    // install the hook
    void (*remove)();     // remove the hook
    bool enabled;         // set false to skip (compile-time or runtime toggle)
};

// Called from DllMain — set up all hooks and reimplementations
void InstallHooks();

// Called from DllMain — tear down all hooks
void RemoveHooks();
