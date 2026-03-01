// engine/reimpl/citizencomponent.cpp
// Reimplementation of CitizenComponent::Update from Application-x64.exe.
//
// PASSTHROUGH WRAPPER — this is the largest function in the game (711 instructions,
// 80+ function calls). It handles the entire citizen lifecycle: aging, health,
// food/fuel consumption, housing, workplace assignment, partner finding, happiness,
// and more. Full reimpl requires mapping ~30 helper functions.
//
// Currently hooks with logging and delegates to original via trampoline.
//
// Original: 0x140112e70

#include "CitizenComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Original function typedef
// ============================================================
typedef void (*CitizenComponent_Update_t)(void* self, void* deltaTime);

// ============================================================
// Update (0x140112e70) — PASSTHROUGH WRAPPER
// Logs entry then delegates to original implementation.
//
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
// Install / Remove
// ============================================================
void InstallCitizenComponentUpdate() {
    uintptr_t target = ResolveAddr(Module::Exe, CitizenComponentAddr::Update);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RBX(1) + PUSH RSI(1) +
    //           PUSH RDI(1) + PUSH R12(2) + PUSH R14(2) + PUSH R15(2) = 13 bytes.
    // Instruction boundary at 13 (after PUSH R15).
    // NOTE: Trampoline IS needed here (passthrough). The prologue does not contain
    // RIP-relative instructions, so the trampoline is safe.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);
#ifdef _DEBUG
    printf("[CitizenComponent] Update hooked at %p (prologue=13, passthrough)\n", (void*)target);
#endif
}

void RemoveCitizenComponentUpdate() { RemoveHook(&g_hookUpdate); }
