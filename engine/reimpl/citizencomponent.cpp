// engine/reimpl/citizencomponent.cpp
// Reimplementation of CitizenComponent functions from Application-x64.exe.
//
// Hooks:
//   1. Update (0x140112e70) — PASSTHROUGH (largest function, 711 instructions)
//   2. ShouldSwitchWorkplace (0x1401109e0) — REIMPLEMENTED in code/Game/Component/
//
// Original: 0x140112e70 (Update), 0x1401109e0 (ShouldSwitchWorkplace)

#include "CitizenComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration from code/Game/Component/citizencomponent.cpp
extern bool CitizenComponent_ShouldSwitchWorkplace(CitizenComponent* self);

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;
static HookInfo g_hookShouldSwitch;

// ============================================================
// Original function typedefs
// ============================================================
typedef void (*CitizenComponent_Update_t)(void* self, void* deltaTime);
typedef bool (*CitizenComponent_ShouldSwitch_t)(CitizenComponent* self);

// ============================================================
// Update (0x140112e70) — PASSTHROUGH WRAPPER
// Parameters: RCX = this (CitizenComponent*), RDX = Time* (pointer to int64)
// ============================================================
static void Reimpl_Update(CitizenComponent* self, void* deltaTime) {
    g_lastHook = "CitizenComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        int64_t dt = *reinterpret_cast<int64_t*>(deltaTime);
        printf("[CitizenComponent::Update] self=%p dt=%lld age=%.2f\n",
               (void*)self, dt, *reinterpret_cast<float*>(
                   reinterpret_cast<uint8_t*>(self) + 0x3C));
        callCount++;
    }
#endif

    // Call original via trampoline
    ((CitizenComponent_Update_t)g_hookUpdate.trampoline)(self, deltaTime);
}

// ============================================================
// ShouldSwitchWorkplace (0x1401109e0) — REIMPLEMENTED
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
    uintptr_t target = ResolveAddr(Module::Exe, CitizenComponentAddr::Update);
    // Prologue: MOV [RSP+8],RCX(5) + PUSH RBP(1) + PUSH RBX(1) + PUSH RSI(1) +
    //           PUSH RDI(1) + PUSH R12(2) + PUSH R14(2) + PUSH R15(2) = 15 bytes
    // But MOV RAX,RSP is 3 bytes at start. Using 13 bytes:
    //   MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RBX(1) + PUSH RSI(1) +
    //   PUSH RDI(1) + PUSH R12(2) + PUSH R14(2) + PUSH R15(2) = 13 bytes.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);
#ifdef _DEBUG
    printf("[CitizenComponent] Update hooked at %p (prologue=13, passthrough)\n", (void*)target);
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

void RemoveCitizenComponentUpdate() { RemoveHook(&g_hookUpdate); }
void RemoveCitizenComponentShouldSwitch() { RemoveHook(&g_hookShouldSwitch); }
