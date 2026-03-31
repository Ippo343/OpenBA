// engine/reimpl/workplacecomponent.cpp
// Hook wrappers for WorkPlaceComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/workplacecomponent.cpp.
//
// WorkPlaceComponent (0x68 bytes) manages worker assignments to buildings.
// Vtable at 0x140164d30. Non-trivial overrides:
//   Shutdown  (0x14012f120) — worker array serialization with IsDerivedFrom validation
//   OnCreate  (0x14012f090) — BST lookup to register with profession list

#include "WorkPlaceComponent.h"
#include "Component/workplacecomponent_funcs.h"
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
static void Reimpl_Shutdown(WorkPlaceComponent* self, void* stream) {
    g_lastHook = "WorkPlaceComponent::Shutdown";
    WorkPlaceComponent_Shutdown(self, stream);
}

static void Reimpl_OnCreate(WorkPlaceComponent* self) {
    g_lastHook = "WorkPlaceComponent::OnCreate";
    WorkPlaceComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallWorkPlaceComponentShutdown() {
    WorkPlaceComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012f120);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[WorkPlaceComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveWorkPlaceComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallWorkPlaceComponentOnCreate() {
    WorkPlaceComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012f090);
    // Prologue: PUSH RBX(1) + SUB RSP,0x20(4) + MOV RAX,[RCX+0x48](4) + MOV RBX,RCX(3) = 13 (safe at 13)
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[WorkPlaceComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveWorkPlaceComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
