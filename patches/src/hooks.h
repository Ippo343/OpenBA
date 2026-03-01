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
    uint8_t original[32]; // saved original bytes (up to 32 for variable prologue)
    size_t patchSize;     // bytes saved/restored (>= 12, must end on instruction boundary)
};

// Install a hook: overwrites target with a jump to detour, returns trampoline.
// prologueSize = number of bytes to save in trampoline. Must be >= 12 (the jump
// size) and MUST end on an instruction boundary. Default 12 works when the first
// 12 bytes of the target are complete instructions. Use a larger value when the
// 12-byte boundary falls mid-instruction (check with a disassembler).
bool InstallHook(uintptr_t targetAddr, void* detourFunc, HookInfo* info, size_t prologueSize = 12);

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

// ============================================================
// Crash diagnostics breadcrumb
// ============================================================

// Last hook that was entered — set by each Reimpl_*/Passthrough_* function.
// On crash, the VEH logs this to identify the last hook that ran.
extern volatile const char* g_lastHook;

// Access the reimplementation table for crash diagnostics (trampoline matching).
// Returns pointer to the ReimplEntry array and sets outCount to the number of entries.
const ReimplEntry* GetReimplTable(int* outCount);

// ============================================================
// Hook registry for crash diagnostics
// ============================================================

// Register a HookInfo with a name so the VEH can match trampoline addresses to hook names.
// Called automatically from each reimpl file's Install function.
void RegisterHookInfo(const char* name, HookInfo* info);

// Look up which hook owns a given address (checks target and trampoline ranges).
// Returns the hook name, or nullptr if no match.
const char* FindHookByAddress(uintptr_t addr);
