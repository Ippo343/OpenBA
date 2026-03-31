// engine/reimpl/growth.cpp
// Hook wrapper for GrowthComponent::Update.
// Delegates to clean reimplementation in code/Game/Component/growthcomponent.cpp.
//
// GrowthComponent (0x50 bytes) manages tree/crop growth with seasonal
// temperature modulation, harvest threshold logic, and sub-resource spawning.
// Vtable at 0x140163590. Update address: 0x140120050.

#include "GrowthComponent.h"
#include "Component/growthcomponent_funcs.h"
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
// Reimplementation wrapper (delegates to clean code)
// ============================================================

static void Reimpl_Update(GrowthComponent* self, int64_t* deltaTime) {
    g_lastHook = "GrowthComponent::Update";
    GrowthComponent_Update(self, deltaTime);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallGrowthComponentUpdate() {
    GrowthComponent_InitFuncs();

    // Install hook
    uintptr_t target = ResolveAddr(Module::Exe, 0x140120050);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(2)+SUB(4) = 13 bytes
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);

#ifdef _DEBUG
    printf("[GrowthComponent] Update hooked at %p (prologue=13)\n", (void*)target);
#endif
}

void RemoveGrowthComponentUpdate() {
    RemoveHook(&g_hookUpdate);
}
