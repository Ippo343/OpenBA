// engine/reimpl/fishingcomponent.cpp
// Hook wrappers for FishingComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/fishingcomponent.cpp.
//
// FishingComponent (0x58 bytes) extends ResourceProduceComponent.
// Vtable at 0x140163230. One non-trivial hookable override:
//   Shutdown (0x14011f050) — base + bool + float serialization

#include "FishingComponent.h"
#include "Component/fishingcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Reimplementation wrapper (delegates to clean code)
// ============================================================
static void Reimpl_Shutdown(FishingComponent* self, void* stream) {
    g_lastHook = "FishingComponent::Shutdown";
    FishingComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallFishingComponentShutdown() {
    FishingComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14011f050);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[FishingComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveFishingComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
