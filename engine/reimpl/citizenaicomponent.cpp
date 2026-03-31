// engine/reimpl/citizenaicomponent.cpp
// Reimplementation of CitizenAIComponent::Update from Application-x64.exe.
//
// Core citizen AI decision loop. Runs every game tick per citizen.
// Delegates to clean reimplementation in code/Game/Component/citizenaicomponent.cpp.
//
// Original: 0x14010f3e0

#include "CitizenAIComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration from code/Game/Component/citizenaicomponent.cpp
extern void CitizenAIComponent_Update(CitizenAIComponent* self, int64_t* pDeltaTime);
extern void CitizenAIComponent_InitFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Update (0x14010f3e0)
// Delegates to clean reimplementation.
// Parameters: RCX = this, RDX = pointer to Time (int64*)
// ============================================================
static void Reimpl_Update(CitizenAIComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "CitizenAIComponent::Update";
    CitizenAIComponent_Update(self, pDeltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCitizenAIComponentUpdate() {
    // Initialize clean code function pointers
    CitizenAIComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, CitizenAIComponentAddr::Update);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x60],RBX(5) +
    //           MOV [RSP+0x68],RBP(5) = 16 bytes.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 16);
#ifdef _DEBUG
    printf("[CitizenAIComponent] Update hooked at %p (prologue=16)\n", (void*)target);
#endif
}

void RemoveCitizenAIComponentUpdate() { RemoveHook(&g_hookUpdate); }
