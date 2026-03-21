// engine/reimpl/plantarea.cpp
// Reimplementation of PlantArea Work action from Application-x64.exe.
//
// PlantArea — crop field planting work. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> PlantArea
// Ctor: 0x1400f4770, Vtable: 0x140161890
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x14002c4b0   <-- REIMPLEMENTED
//   [1]  VectorDtor    = 0x14002c460   (too small, clean code only)
//   [2]  BaseDtor      = 0x14002c450   (too small, clean code only)
//   [3]  Serialize     = 0x1400f4540   <-- REIMPLEMENTED
//   [4]  Slot4/Execute = 0x1400f4010   <-- REIMPLEMENTED (complex planting logic)
//   [5]  Execute       = 0x1400f7540   (3 bytes, clean code only)
//   [6]  OnComplete    = 0x1400f3f60   <-- REIMPLEMENTED
//   [7]  OnCancel      = 0x1400f3ea0   <-- REIMPLEMENTED
//   [8]  Cleanup       = 0x1400f4460   <-- REIMPLEMENTED
//   [9]  Validate      = 0x1400f3c70   <-- REIMPLEMENTED
//   [10] GetTarget     = 0x1400f44e0   <-- REIMPLEMENTED
//   [11] GetPriority   = 0x140029860   (9 bytes, shared, clean code only)
//
// Constructor reimplemented. All hookable vtable methods reimplemented.

#include "BuildActions.h"
#include "Work/plantarea_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// fn_ObjectInternal_Ctor is now declared globally in engine_funcs.h

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookCtor;
static HookInfo g_hookScalarDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookSlot4;
static HookInfo g_hookOnComplete;
static HookInfo g_hookOnCancel;
static HookInfo g_hookCleanup;
static HookInfo g_hookValidate;
static HookInfo g_hookGetTarget;

// ============================================================
// Constructor (0x1400f4770)
// ============================================================
static void* Reimpl_Ctor(PlantArea* self) {
    g_lastHook = "PlantArea::Ctor";
    fn_ObjectInternal_Ctor(self);

    // Work base fields
    self->vtable = (void*)ResolveAddr(Module::Exe, 0x140161a80);
    self->field_10 = nullptr;
    self->field_18 = nullptr;
    self->field_20 = -1;
    self->field_24 = 0;
    self->field_28 = -1;
    self->field_2c = 0;
    self->field_30 = nullptr;
    self->field_38 = nullptr;
    self->field_40 = 0;

    // Set PlantArea vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, PlantAreaAddr::Vtable);

    // PlantArea fields
    self->field_48 = nullptr;
    self->field_50 = 0;
    self->field_58 = nullptr;
    self->field_60 = 0;
    self->field_68 = nullptr;

    return self;
}

// ============================================================
// ScalarDtor (0x14002c4b0) — REIMPLEMENTED
// ============================================================
static void* Reimpl_ScalarDtor(PlantArea* self, uint32_t flags) {
    g_lastHook = "PlantArea::ScalarDtor";
    return PlantArea_ScalarDtor_Impl(self, flags);
}

// ============================================================
// Serialize (0x1400f4540) — REIMPLEMENTED
// ============================================================
static void Reimpl_Serialize(PlantArea* self, void* stream) {
    g_lastHook = "PlantArea::Serialize";
    PlantArea_Serialize_Impl(self, stream);
}

// ============================================================
// Slot4 (0x1400f4010) — REIMPLEMENTED (complex planting logic)
// ============================================================
static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    g_lastHook = "PlantArea::Slot4";
    return (uint64_t)PlantArea_Slot4_Impl((PlantArea*)self, param2, param3, param4);
}

// ============================================================
// OnComplete (Slot6, 0x1400f3f60) — REIMPLEMENTED
// ============================================================
static int Reimpl_OnComplete(void* self, void* param2, void* param3, uint32_t param4) {
    g_lastHook = "PlantArea::OnComplete";
    return PlantArea_OnComplete_Impl((PlantArea*)self, param2, param3, param4);
}

// ============================================================
// OnCancel (Slot7, 0x1400f3ea0) — REIMPLEMENTED
// ============================================================
static int Reimpl_OnCancel(void* self, void* param2, void* param3, uint32_t param4) {
    g_lastHook = "PlantArea::OnCancel";
    return PlantArea_OnCancel_Impl((PlantArea*)self, param2, param3, param4);
}

// ============================================================
// Cleanup (Slot8, 0x1400f4460) — REIMPLEMENTED
// ============================================================
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "PlantArea::Cleanup";
    PlantArea_Cleanup_Impl((PlantArea*)self);
}

// ============================================================
// Validate (Slot9, 0x1400f3c70) — REIMPLEMENTED
// ============================================================
static bool Reimpl_Validate(void* self, void* param2) {
    g_lastHook = "PlantArea::Validate";
    return PlantArea_Validate_Impl((PlantArea*)self, param2);
}

// ============================================================
// GetTarget (Slot10, 0x1400f44e0) — REIMPLEMENTED
// ============================================================
static void* Reimpl_GetTarget(void* self, void* outPtr) {
    g_lastHook = "PlantArea::GetTarget";
    return PlantArea_GetTarget_Impl((PlantArea*)self, outPtr);
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    // fn_ObjectInternal_Ctor is now resolved globally in EngineFuncsInit()
    return fn_ObjectInternal_Ctor != nullptr;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallPlantAreaCtor() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[PlantArea] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }
    PlantArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, PlantAreaAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[PlantArea] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemovePlantAreaCtor() { RemoveHook(&g_hookCtor); }

void InstallPlantAreaScalarDtor() {
    PlantArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, PlantAreaAddr::ScalarDtor);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[PlantArea] ScalarDtor hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantAreaScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallPlantAreaSerialize() {
    PlantArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, PlantAreaAddr::Serialize);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[PlantArea] Serialize hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantAreaSerialize() { RemoveHook(&g_hookSerialize); }

void InstallPlantAreaSlot4() {
    PlantArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f4010);
    // Prologue: MOV [RSP+0x10],RDX(5) + MOV [RSP+0x8],RCX(5) + PUSH RBP(1) + PUSH RSI(1) = 12
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 12);
#ifdef _DEBUG
    printf("[PlantArea] Slot4 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantAreaSlot4() { RemoveHook(&g_hookSlot4); }

void InstallPlantAreaOnComplete() {
    PlantArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f3f60);
    // Prologue: MOV [RSP+0x20],R9D(5) + PUSH RBX(1) + SUB RSP,0x20(4) = 10... verify
    InstallHook(target, (void*)Reimpl_OnComplete, &g_hookOnComplete, 14);
#ifdef _DEBUG
    printf("[PlantArea] OnComplete hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantAreaOnComplete() { RemoveHook(&g_hookOnComplete); }

void InstallPlantAreaOnCancel() {
    PlantArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f3ea0);
    InstallHook(target, (void*)Reimpl_OnCancel, &g_hookOnCancel, 14);
#ifdef _DEBUG
    printf("[PlantArea] OnCancel hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantAreaOnCancel() { RemoveHook(&g_hookOnCancel); }

void InstallPlantAreaCleanup() {
    PlantArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f4460);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 13);
#ifdef _DEBUG
    printf("[PlantArea] Cleanup hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantAreaCleanup() { RemoveHook(&g_hookCleanup); }

void InstallPlantAreaValidate() {
    PlantArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f3c70);
    InstallHook(target, (void*)Reimpl_Validate, &g_hookValidate, 12);
#ifdef _DEBUG
    printf("[PlantArea] Validate hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantAreaValidate() { RemoveHook(&g_hookValidate); }

void InstallPlantAreaGetTarget() {
    PlantArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f44e0);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 14);
#ifdef _DEBUG
    printf("[PlantArea] GetTarget hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantAreaGetTarget() { RemoveHook(&g_hookGetTarget); }
