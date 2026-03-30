// engine/reimpl/herbalistcomponent.cpp
// Reimplementation of HerbalistComponent lifecycle methods from Application-x64.exe.
//
// HerbalistComponent (0x50 bytes) is a ResourceProduceComponent subclass.
// Vtable at 0x140159860. Only non-trivial override: Shutdown (0x14004b200).

#include "HerbalistComponent.h"
#include "Component/herbalistcomponent_funcs.h"
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
// Shutdown (0x14004b200)
// ============================================================
static void Reimpl_Shutdown(HerbalistComponent* self, void* stream) {
    g_lastHook = "HerbalistComponent::Shutdown";
    HerbalistComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallHerbalistComponentShutdown() {
    HerbalistComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004b200);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[HerbalistComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveHerbalistComponentShutdown() { RemoveHook(&g_hookShutdown); }
