// engine/reimpl/rawmaterialcomponent.cpp
// Hook wrappers for RawMaterialComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/rawmaterialcomponent.cpp.
//
// RawMaterialComponent (0x40 bytes) manages raw material lifecycle.
// Vtable at 0x140164170. Non-trivial overrides:
//   Shutdown  (0x140129330) — versioned serialization of field28, field38, field30
//   Serialize (0x1401292c0) — init field28 from building data, register with world
//   Draw      (0x1401291d0) — create pickup render object and register with scene
//   Update    (0x140129130) — accumulate time, check threshold, mark for collection

#include "RawMaterialComponent.h"
#include "Component/rawmaterialcomponent_funcs.h"
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
static HookInfo g_hookDraw;
static HookInfo g_hookUpdate;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(RawMaterialComponent* self, void* stream) {
    g_lastHook = "RawMaterialComponent::Shutdown";
    RawMaterialComponent_Shutdown(self, stream);
}

static void Reimpl_Serialize(RawMaterialComponent* self) {
    g_lastHook = "RawMaterialComponent::Serialize";
    RawMaterialComponent_Serialize(self);
}

static void Reimpl_Draw(RawMaterialComponent* self) {
    g_lastHook = "RawMaterialComponent::Draw";
    RawMaterialComponent_Draw(self);
}

static void Reimpl_Update(RawMaterialComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "RawMaterialComponent::Update";
    RawMaterialComponent_Update(self, pDeltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallRawMaterialComponentShutdown() {
    RawMaterialComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140129330);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV qword,-0x2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[RawMaterialComponent] Shutdown hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveRawMaterialComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallRawMaterialComponentSerialize() {
    RawMaterialComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401292c0);
    // Prologue: MOV [RSP+0x8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV RBX,[RCX+0x18](4) = 14
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[RawMaterialComponent] Serialize hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveRawMaterialComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallRawMaterialComponentDraw() {
    RawMaterialComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401291d0);
    // Prologue: PUSH RBX(1)+PUSH RSI(1)+PUSH RDI(1)+SUB RSP,0x30(4)+MOV qword,-0x2(9) = 16
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 17);
#ifdef _DEBUG
    printf("[RawMaterialComponent] Draw hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveRawMaterialComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallRawMaterialComponentUpdate() {
    RawMaterialComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140129130);
    // Prologue: PUSH RBX(2)+SUB RSP,0x20(4)+MOV RAX,[RDX](3)+MOVSS(8) = 17
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 17);
#ifdef _DEBUG
    printf("[RawMaterialComponent] Update hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveRawMaterialComponentUpdate() { RemoveHook(&g_hookUpdate); }
