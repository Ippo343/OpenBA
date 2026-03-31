// engine/reimpl/citizencomponent.cpp
// Reimplementation of CitizenComponent functions from Application-x64.exe.
//
// Hooks:
//   1. Update (0x140112e70) — REIMPLEMENTED in code/Game/Component/
//   2. ShouldSwitchWorkplace (0x1401109e0) — REIMPLEMENTED in code/Game/Component/
//
// Original: 0x140112e70 (Update), 0x1401109e0 (ShouldSwitchWorkplace)

#include "CitizenComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declarations from code/Game/Component/citizencomponent.cpp
extern void CitizenComponent_Update(CitizenComponent* self, void* deltaTime);
extern bool CitizenComponent_ShouldSwitchWorkplace(CitizenComponent* self);
extern void CitizenComponent_InitFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;
static HookInfo g_hookShouldSwitch;

// ============================================================
// Update (0x140112e70) — Delegates to clean reimplementation.
// Parameters: RCX = this (CitizenComponent*), RDX = Time* (pointer to int64)
// ============================================================
static void Reimpl_Update(CitizenComponent* self, void* deltaTime) {
    g_lastHook = "CitizenComponent::Update";
    CitizenComponent_Update(self, deltaTime);
}

// ============================================================
// ShouldSwitchWorkplace (0x1401109e0) — Delegates to clean reimplementation.
// Parameters: RCX = CitizenComponent*
// Returns: bool (should citizen switch workplace)
// ============================================================
static bool Reimpl_ShouldSwitchWorkplace(CitizenComponent* self) {
    g_lastHook = "CitizenComponent::ShouldSwitchWorkplace";
    return CitizenComponent_ShouldSwitchWorkplace(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCitizenComponentUpdate() {
    // Initialize clean code function pointers
    CitizenComponent_InitFuncs();

    // VTable hook: MOV RAX,RSP in prologue breaks with RSP compensation.
    // vtable at 0x140162580, Update = slot 9 (+0x48)
    InstallHookVTable(0x140162580, 9, (void*)Reimpl_Update, &g_hookUpdate);
    RegisterHookInfo("CitizenComponent::Update", &g_hookUpdate);
#ifdef _DEBUG
    printf("[CitizenComponent] Update vtable-hooked (original=%p)\n", g_hookUpdate.trampoline);
#endif
}

void InstallCitizenComponentShouldSwitch() {
    // 0x1401109e0: MOV [RSP+8],RCX (5) + PUSH RBP (1) + PUSH RDI (1) +
    //              PUSH R13 (2) + PUSH R14 (2) + PUSH R15 (2) = 13 bytes
    // NOTE: Prologue has MOV [RSP+8],RCX — RSP-relative store (bug class 4).
    // Trampoline is NOT safe to call (RSP mismatch). Full reimpl only.
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401109e0);
    InstallHook(target, (void*)Reimpl_ShouldSwitchWorkplace, &g_hookShouldSwitch, 13);
#ifdef _DEBUG
    printf("[CitizenComponent] ShouldSwitchWorkplace hooked at %p (prologue=13, reimpl)\n", (void*)target);
#endif
}

void RemoveCitizenComponentUpdate() { RemoveHookVTable(&g_hookUpdate); }
void RemoveCitizenComponentShouldSwitch() { RemoveHook(&g_hookShouldSwitch); }
