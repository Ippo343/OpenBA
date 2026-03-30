// engine/reimpl/diseasecomponent.cpp
// Reimplementation of DiseaseComponent::Update from Application-x64.exe.
//
// The Update vtable entry at 0x140048b90 is a small 16-byte dispatch thunk:
//   if (field30 > 0) goto ActiveDisease (0x140048840)
//   else goto InfectionTick (0x140048610)
//
// Original thunk: 0x140048b90 (16 bytes, CMP+JG+JMP)

#include "DiseaseComponent.h"
#include "Component/diseasecomponent_funcs.h"
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
// Update (0x140048b90) — dispatch thunk
// ============================================================
static void Reimpl_Update(DiseaseComponent* self, int64_t* deltaTime) {
    g_lastHook = "DiseaseComponent::Update";
    DiseaseComponent_Update(self, deltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallDiseaseComponentUpdate() {
    DiseaseComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140048b90);
    // The thunk is exactly 16 bytes (CMP[5] + JG[6] + JMP[5]).
    // Both jumps are relative so the trampoline won't work, but we never call it.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 16);
#ifdef _DEBUG
    printf("[DiseaseComponent] Update hooked at %p (prologue=16)\n", (void*)target);
#endif
}

void RemoveDiseaseComponentUpdate() { RemoveHook(&g_hookUpdate); }
