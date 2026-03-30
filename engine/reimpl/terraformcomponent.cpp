// engine/reimpl/terraformcomponent.cpp
// Hook wrappers for TerraformComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/terraformcomponent.cpp.

#include "TerraformComponent.h"
#include "Component/terraformcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookOnCreate;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(TerraformComponent* self, void* stream) {
    g_lastHook = "TerraformComponent::Shutdown";
    TerraformComponent_Shutdown(self, stream);
}

static void Reimpl_OnCreate(TerraformComponent* self) {
    g_lastHook = "TerraformComponent::OnCreate";
    TerraformComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTerraformComponentShutdown() {
    TerraformComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140057500);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[TerraformComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTerraformComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTerraformComponentOnCreate() {
    TerraformComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140057350);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 15);
#ifdef _DEBUG
    printf("[TerraformComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveTerraformComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
