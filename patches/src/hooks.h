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

// Install a hook for functions whose prologue starts with MOV RAX,RSP or MOV R11,RSP.
//
// Standard trampoline is broken for these because CALL pushes a return address,
// making RSP 8 bytes lower than at the original call site. The RSP capture then
// gives the wrong value.
//
// This variant builds a trampoline that is meant to be reached by JMP (not CALL),
// so no return address is pushed and RSP is exactly what the original caller had.
// The detourFunc MUST be a naked function that tail-JMPs (not calls) into the
// trampoline. The trampoline then executes the prologue bytes normally and jumps
// back to (target + prologueSize) to continue the original function.
//
// Use the REIMPL_RSP_PASSTHROUGH macro in reimpl files to generate the naked stub.
bool InstallHookRSPSafe(uintptr_t targetAddr, void* detourFunc, HookInfo* info, size_t prologueSize);

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
// RSP-safe passthrough macro
// ============================================================
//
// For functions that start with MOV RAX,RSP or MOV R11,RSP, we cannot use
// a normal CALL-based trampoline because CALL shifts RSP by 8. Instead, the
// "reimpl" must tail-JMP directly into the trampoline (not CALL it).
//
// This macro generates a __declspec(naked) function that:
//   1. Sets g_lastHook for crash diagnostics (via a helper)
//   2. Tail-JMPs to g_hook.trampoline
//
// The trampoline was built by InstallHookRSPSafe to be JMP-safe:
// it contains the original prologue bytes + jump back to original+N.
// When entered by JMP (not CALL), RSP is exactly as the caller left it,
// so MOV RAX,RSP / MOV R11,RSP capture the correct value.
//
// Usage in reimpl files:
//   static HookInfo g_hookFoo;
//   REIMPL_RSP_PASSTHROUGH(Foo, g_hookFoo, "ClassName::FuncName")
//   // generates: void Reimpl_Foo()  (naked, tail-JMPs to trampoline)
//
// Note: The breadcrumb update is skipped in this macro to keep it truly
// naked (no C stack frame). Crash diagnostics still work via the hook
// registry address lookup (FindHookByAddress).
#ifdef _MSC_VER
#define REIMPL_RSP_PASSTHROUGH(name, hookInfo, debugName) \
    __declspec(naked) static void Reimpl_##name() { \
        __asm { mov rax, qword ptr [hookInfo.trampoline] } \
        __asm { jmp rax } \
    }
#else
// GCC/MinGW naked with asm
#define REIMPL_RSP_PASSTHROUGH(name, hookInfo, debugName) \
    __attribute__((naked)) static void Reimpl_##name() { \
        asm volatile( \
            "movq %0, %%rax\n\t" \
            "jmp *%%rax\n\t" \
            : : "m"(hookInfo.trampoline) : "rax" \
        ); \
    }
#endif

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
