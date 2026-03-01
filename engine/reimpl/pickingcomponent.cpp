// engine/reimpl/pickingcomponent.cpp
// Reimplementation of PickingComponent lifecycle methods.
//
// PickingComponent (0x38 bytes) — direct Component subclass.
// Manages 3D picking (mouse click -> entity selection) via Picking::Scene.
// pickingId at +0x28 (int32, -1 = unregistered), entity ref at +0x30.
//
// Vtable at 0x140164040:
//   [3] Shutdown  = 0x140128bb0 (passthrough — serialization with InternalBasePtr)
//   [4] Serialize = 0x140128c30 (9 bytes — too small to hook)
//   [6] Draw      = 0x140128b90 (5-byte JMP thunk — too small to hook)
//   [7] OnCreate  = 0x140128b50 (passthrough — uses runtime dispatch table for RemoveMesh)
//   [11] OnUpdate2 = 0x140128ba0 (5-byte JMP thunk — too small to hook)
//
// Note: Serialize(0x140128c30), Draw(0x140128b90), and OnUpdate2(0x140128ba0) are all
// too small to hook (5-9 bytes each, under the 12-byte minimum for inline hooks).

#include "PickingComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Shutdown (0x140128bb0) — passthrough
// Serializes Component base + InternalBasePtr at +0x30 (entity ref).
// After deserialization, validates with IsDerivedFrom(TypeInfo_ComponentEntity)
// and nulls out if type check fails.
// ============================================================
typedef void (*PickingShutdown_t)(void* self, void* stream);
static HookInfo g_hookShutdown;

static void Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "PickingComponent::Shutdown";
    ((PickingShutdown_t)g_hookShutdown.trampoline)(self, stream);
}

void InstallPickingComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140128bb0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[PickingComponent] Shutdown hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemovePickingComponentShutdown() { RemoveHook(&g_hookShutdown); }

// ============================================================
// OnCreate (0x140128b50) — passthrough
// If pickingId >= 0, calls Picking::Scene::RemoveMesh via runtime dispatch
// table (CALL [0x140152438]) and resets pickingId to -1.
// Uses runtime dispatch table — must passthrough.
// ============================================================
typedef void (*PickingOnCreate_t)(void* self);
static HookInfo g_hookOnCreate;

static void Reimpl_OnCreate(void* self) {
    g_lastHook = "PickingComponent::OnCreate";
    ((PickingOnCreate_t)g_hookOnCreate.trampoline)(self);
}

void InstallPickingComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140128b50);
    // Prologue: PUSH RBX(1) + SUB RSP,0x20(4) + MOV EDX,[RCX+0x28](4) + MOV RBX,RCX(3) = 12
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[PickingComponent] OnCreate hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemovePickingComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
