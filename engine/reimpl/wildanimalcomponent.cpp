// engine/reimpl/wildanimalcomponent.cpp
// Hook wrappers for WildAnimalComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/wildanimalcomponent.cpp.
//
// WildAnimalComponent (0x30 bytes) -- simple Component subclass.
// Vtable at 0x140164af0:
//   [3] Shutdown = 0x14012e300
//   [7] OnCreate = 0x14012e2b0

#include "WildAnimalComponent.h"
#include "Component/wildanimalcomponent_funcs.h"
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
static void Reimpl_Shutdown(WildAnimalComponent* self, void* stream) {
    g_lastHook = "WildAnimalComponent::Shutdown";
    WildAnimalComponent_Shutdown(self, stream);
}

static void Reimpl_OnCreate(WildAnimalComponent* self) {
    g_lastHook = "WildAnimalComponent::OnCreate";
    WildAnimalComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallWildAnimalComponentShutdown() {
    WildAnimalComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012e300);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[WildAnimalComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveWildAnimalComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallWildAnimalComponentOnCreate() {
    WildAnimalComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012e2b0);
    // Prologue: 15 bytes
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 15);
#ifdef _DEBUG
    printf("[WildAnimalComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveWildAnimalComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
