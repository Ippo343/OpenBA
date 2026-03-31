// engine/reimpl/residencecomponent.cpp
// Hook wrappers for ResidenceComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/residencecomponent.cpp.
//
// ResidenceComponent (0x60 bytes) — ControllerComponent subclass.
// Manages citizen residence assignments (housing).
//
// Vtable at 0x1401642c0:
//   [3] Shutdown  = 0x14012a170 — serialization with Time + arrays
//   [4] Serialize = 0x14012a120 — IsDerivedFrom check + field init
//   [7] OnCreate  = 0x14012a0e0 — calls init + registers with game state

#include "ResidenceComponent.h"
#include "Component/residencecomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookSerialize;
static HookInfo g_hookOnCreate;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(ResidenceComponent* self, void* stream) {
    g_lastHook = "ResidenceComponent::Shutdown";
    ResidenceComponent_Shutdown(self, stream);
}

static void Reimpl_Serialize(ResidenceComponent* self) {
    g_lastHook = "ResidenceComponent::Serialize";
    ResidenceComponent_Serialize(self);
}

static void Reimpl_OnCreate(ResidenceComponent* self) {
    g_lastHook = "ResidenceComponent::OnCreate";
    ResidenceComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallResidenceComponentShutdown() {
    ResidenceComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012a170);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ResidenceComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveResidenceComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallResidenceComponentSerialize() {
    ResidenceComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012a120);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV RBX,[RCX+0x18](4) = 14
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[ResidenceComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveResidenceComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallResidenceComponentOnCreate() {
    ResidenceComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012a0e0);
    // Prologue: PUSH RBX(1) + SUB RSP,0x20(4) + MOV RBX,RCX(3) + CALL(5) = 13
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 14);
#ifdef _DEBUG
    printf("[ResidenceComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveResidenceComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
