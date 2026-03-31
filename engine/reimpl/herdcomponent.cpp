// engine/reimpl/herdcomponent.cpp
// Hook wrapper for HerdComponent::Update.
// Delegates to clean reimplementation in code/Game/Component/herdcomponent.cpp.
//
// HerdComponent (0x60 bytes) manages wild animal herds.
// Vtable at 0x140163680. Update = slot 9.

#include "HerdComponent.h"
#include "Component/herdcomponent_funcs.h"
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

static void Reimpl_Update(HerdComponent* self, int64_t* deltaTime) {
    g_lastHook = "HerdComponent::Update";
    HerdComponent_Update(self, deltaTime);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallHerdComponentUpdate() {
    HerdComponent_InitFuncs();

    // Install hook
    uintptr_t target = ResolveAddr(Module::Exe, HerdComponentAddr::Update);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(2)+SUB(4) = 13 bytes
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);

#ifdef _DEBUG
    printf("[HerdComponent] Update hooked at %p (prologue=13)\n", (void*)target);
#endif
}

void RemoveHerdComponentUpdate() {
    RemoveHook(&g_hookUpdate);
}
