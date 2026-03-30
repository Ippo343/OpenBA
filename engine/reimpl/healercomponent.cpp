// engine/reimpl/healercomponent.cpp
// Reimplementation of HealerComponent lifecycle methods.
//
// HealerComponent (0x48 bytes) extends ControllerComponent with no additional fields.
// Constructor sets flags |= 0x2.
//
// Vtable at 0x140159730:
//   [3] Shutdown  = 0x14004aeb0 (two empty serializer blocks)
//   [7] OnCreate  = 0x14004ae70 (registers with healer list)

#include "HealerComponent.h"
#include "Component/healercomponent_funcs.h"
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
// Shutdown (0x14004aeb0)
// ============================================================
static void Reimpl_Shutdown(HealerComponent* self, void* stream) {
    g_lastHook = "HealerComponent::Shutdown";
    HealerComponent_Shutdown(self, stream);
}

// ============================================================
// OnCreate (0x14004ae70)
// ============================================================
static void Reimpl_OnCreate(HealerComponent* self) {
    g_lastHook = "HealerComponent::OnCreate";
    HealerComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallHealerComponentShutdown() {
    HealerComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004aeb0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[HealerComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveHealerComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallHealerComponentOnCreate() {
    HealerComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004ae70);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[HealerComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveHealerComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
