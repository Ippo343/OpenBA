// engine/reimpl/populationcomponent.cpp
// Hook wrappers for PopulationComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/populationcomponent.cpp.

#include "PopulationComponent.h"
#include "Component/populationcomponent_funcs.h"
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
static void Reimpl_Update(PopulationComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "PopulationComponent::Update";
    PopulationComponent_Update(self, pDeltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallPopulationComponentUpdate() {
    PopulationComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140054070);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 17);
#ifdef _DEBUG
    printf("[PopulationComponent] Update hooked at %p (prologue=17)\n", (void*)target);
#endif
}

void RemovePopulationComponentUpdate() { RemoveHook(&g_hookUpdate); }
