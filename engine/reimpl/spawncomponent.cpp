// engine/reimpl/spawncomponent.cpp
// Hook wrappers for SpawnComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/spawncomponent.cpp.

#include "SpawnComponent.h"
#include "Component/spawncomponent_funcs.h"
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
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Update(SpawnComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "SpawnComponent::Update";
    SpawnComponent_Update(self, pDeltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallSpawnComponentUpdate() {
    SpawnComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140055ad0);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 12);
#ifdef _DEBUG
    printf("[SpawnComponent] Update hooked at %p (prologue=12)\n", (void*)target);
#endif
}

void RemoveSpawnComponentUpdate() { RemoveHook(&g_hookUpdate); }
