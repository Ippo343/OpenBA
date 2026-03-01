// engine/reimpl/pasturecomponent.cpp
// Reimplementation of PastureComponent lifecycle methods.
//
// PastureComponent (0x68 bytes) is UNUSUAL: Component base with own data before
// the controller sub-object (shifted to +0x48 instead of the usual +0x28).
//   +0x28: void*   field28       (zeroed — entity ref array ptr)
//   +0x30: void*   field30       (zeroed — entity ref array data)
//   +0x38: void*   field38       (zeroed — entity ref array)
//   +0x40: padding
//   +0x44: uint32  field44       (zeroed)
//   +0x48: void*   controllerVtable
//   +0x50..+0x60: controller sub-object data + selfPtr
//
// Vtable at 0x140163f60 (12 slots):
//   +0x00 Dtor           = 0x140036910 (too small to hook)
//   +0x08 GetDescription = 0x140036720 (7 bytes, unhookable)
//   +0x10 Startup        = 0x140036710 (7 bytes, unhookable)
//   +0x18 Shutdown       = 0x140127f40 (HOOKED — passthrough, very complex entity ref serialization)
//   +0x20 Serialize      = 0x140127a50 (9-byte MOV+RET, unhookable — zeroes field28)
//   +0x28 Slot5          = 0x140118a60 (no-op)
//   +0x30 Draw           = 0x140127a30 (HOOKED — passthrough, calculates capacity from entity refs)
//   +0x38 OnCreate       = 0x1401277e0 (HOOKED — passthrough, complex entity ref setup + registration)
//   +0x40 OnRemove       = 0x140118a70 (no-op)
//   +0x48 Update         = 0x140118a80 (no-op)
//   +0x50 Slot10         = 0x140118a90 (no-op)
//   +0x58 OnUpdate2      = 0x140118aa0 (no-op)
//
// All three non-trivial methods are complex:
// - Shutdown: Component_Serialize + two serializer blocks + entity ref array serialization
//   with InternalBasePtr validation loop and IsDerivedFrom checks
// - Draw: Calls helper that resolves entity data, computes sVar1*sVar2/capacity
// - OnCreate: Iterates entity ref array, registers with population manager lists,
//   reassigns entity refs, calls multiple helpers
// All hooked as passthroughs.

#include "PastureComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Shutdown (0x140127f40) — passthrough
// Complex: Component_Serialize, two serializer blocks, entity ref array
// with InternalBasePtr validation, IsDerivedFrom checks, conditional
// array resizing, and int serialization at +0x40/+0x44.
//
// Prologue: PUSH RDI(1) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 14 → 15 bytes
// ============================================================

typedef void (*fn_Shutdown_Orig_t)(PastureComponent* self, void* stream);

static void Reimpl_Shutdown(PastureComponent* self, void* stream) {
    g_lastHook = "PastureComponent::Shutdown";
    ((fn_Shutdown_Orig_t)g_hookShutdown.trampoline)(self, stream);
}

// ============================================================
// Draw (0x140127a30) — passthrough
// Calls FUN_140128570 to compute a capacity value from entity
// references and stores it at +0x40. The helper iterates entity
// component arrays, resolves types, does division.
//
// Prologue: PUSH RBX(1) + SUB RSP,0x20(4) + MOV RBX,RCX(3) + CALL(5) = 13
// ============================================================

typedef void (*fn_Draw_Orig_t)(PastureComponent* self);

static void Reimpl_Draw(PastureComponent* self) {
    g_lastHook = "PastureComponent::Draw";
    ((fn_Draw_Orig_t)g_hookDraw.trampoline)(self);
}

// ============================================================
// OnCreate (0x1401277e0) — passthrough
// Very complex: iterates entity ref array backwards, validates
// references, registers entities with population manager lists
// at multiple offsets (+0x80, +0x90), creates InternalBasePtrs,
// calls FUN_140105ed0, FUN_1401240a0.
//
// Need to check prologue size for this one.
// ============================================================

typedef void (*fn_OnCreate_Orig_t)(PastureComponent* self);

static void Reimpl_OnCreate(PastureComponent* self) {
    g_lastHook = "PastureComponent::OnCreate";
    ((fn_OnCreate_Orig_t)g_hookOnCreate.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallPastureComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140127f40);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[PastureComponent] Shutdown hooked (passthrough) at %p\n", (void*)target);
#endif
}

void RemovePastureComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}

void InstallPastureComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140127a30);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[PastureComponent] Draw hooked (passthrough) at %p\n", (void*)target);
#endif
}

void RemovePastureComponentDraw() {
    RemoveHook(&g_hookDraw);
}

void InstallPastureComponentOnCreate() {
    // Prologue: PUSH R13(2) + SUB RSP,0x60(4) + CMP [RCX+0x38],0(4) + MOV R13,RCX(3) = 13
    // All register-relative, no RIP-relative instructions. Safe boundary at 13.
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401277e0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[PastureComponent] OnCreate hooked (passthrough) at %p\n", (void*)target);
#endif
}

void RemovePastureComponentOnCreate() {
    RemoveHook(&g_hookOnCreate);
}
