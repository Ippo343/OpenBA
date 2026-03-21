// engine/reimpl/plantorchard.cpp
// Reimplementation of PlantOrchard Work action from Application-x64.exe.
//
// PlantOrchard — orchard planting work. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> PlantOrchard
// Ctor: 0x1400f50b0, Vtable: 0x140161910
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x14002c540   <-- REIMPLEMENTED
//   [1]  VectorDtor    = 0x14002c4f0   (too small, clean code only)
//   [2]  BaseDtor      = 0x14002c4e0   (too small, clean code only)
//   [3]  Serialize     = 0x1400f4f50   <-- REIMPLEMENTED
//   [4]  Slot4/Execute = 0x1400f4af0   <-- REIMPLEMENTED (complex orchard planting)
//   [5]  Execute       = 0x1400f7540   (3 bytes, clean code only)
//   [6]  OnComplete    = 0x1400f4a90   <-- REIMPLEMENTED
//   [7]  OnCancel      = 0x1400f4a20   <-- REIMPLEMENTED
//   [8]  Cleanup       = 0x1400f4e70   <-- REIMPLEMENTED
//   [9]  Validate      = 0x1400f47f0   <-- REIMPLEMENTED
//   [10] GetTarget     = 0x1400f4ef0   <-- REIMPLEMENTED
//   [11] GetPriority   = 0x140029860   (9 bytes, shared, clean code only)
//
// Constructor reimplemented. All hookable vtable methods reimplemented.

#include "BuildActions.h"
#include "Work/plantorchard_funcs.h"
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
// Constructor (0x1400f50b0)
// ============================================================
static void* Reimpl_Ctor(PlantOrchard* self) {
    g_lastHook = "PlantOrchard::Ctor";
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

    // Set PlantOrchard vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, PlantOrchardAddr::Vtable);

    // PlantOrchard fields
    self->field_48 = nullptr;
    self->field_50 = 0;
    self->field_58 = nullptr;
    self->field_60 = 0;
    self->field_68 = nullptr;

    return self;
}

// ============================================================
// ScalarDtor (0x14002c540) — REIMPLEMENTED
// ============================================================
static void* Reimpl_ScalarDtor(PlantOrchard* self, uint32_t flags) {
    g_lastHook = "PlantOrchard::ScalarDtor";
    return PlantOrchard_ScalarDtor_Impl(self, flags);
}

// ============================================================
// Serialize (0x1400f4f50) — REIMPLEMENTED
// ============================================================
static void Reimpl_Serialize(PlantOrchard* self, void* stream) {
    g_lastHook = "PlantOrchard::Serialize";
    PlantOrchard_Serialize_Impl(self, stream);
}

// ============================================================
// Slot4 (0x1400f4af0) — REIMPLEMENTED (complex orchard planting)
// ============================================================
static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    g_lastHook = "PlantOrchard::Slot4";
    return (uint64_t)PlantOrchard_Slot4_Impl((PlantOrchard*)self, param2, param3, param4);
}

// ============================================================
// OnComplete (Slot6, 0x1400f4a90) — REIMPLEMENTED
// ============================================================
static int Reimpl_OnComplete(void* self, void* param2, void* param3, int param4) {
    g_lastHook = "PlantOrchard::OnComplete";
    return PlantOrchard_OnComplete_Impl((PlantOrchard*)self, param2, param3, param4);
}

// ============================================================
// OnCancel (Slot7, 0x1400f4a20) — REIMPLEMENTED
// ============================================================
static int Reimpl_OnCancel(void* self, void* param2, void* param3, int param4) {
    g_lastHook = "PlantOrchard::OnCancel";
    return PlantOrchard_OnCancel_Impl((PlantOrchard*)self, param2, param3, param4);
}

// ============================================================
// Cleanup (Slot8, 0x1400f4e70) — REIMPLEMENTED
// ============================================================
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "PlantOrchard::Cleanup";
    PlantOrchard_Cleanup_Impl((PlantOrchard*)self);
}

// ============================================================
// Validate (Slot9, 0x1400f47f0) — REIMPLEMENTED
// ============================================================
static bool Reimpl_Validate(void* self, void* param2) {
    g_lastHook = "PlantOrchard::Validate";
    return PlantOrchard_Validate_Impl((PlantOrchard*)self, param2);
}

// ============================================================
// GetTarget (Slot10, 0x1400f4ef0) — REIMPLEMENTED
// ============================================================
static void* Reimpl_GetTarget(void* self, void* outPtr) {
    g_lastHook = "PlantOrchard::GetTarget";
    return PlantOrchard_GetTarget_Impl((PlantOrchard*)self, outPtr);
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
void InstallPlantOrchardCtor() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[PlantOrchard] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }
    PlantOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, PlantOrchardAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[PlantOrchard] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemovePlantOrchardCtor() { RemoveHook(&g_hookCtor); }

void InstallPlantOrchardScalarDtor() {
    PlantOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, PlantOrchardAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[PlantOrchard] ScalarDtor hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantOrchardScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallPlantOrchardSerialize() {
    PlantOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, PlantOrchardAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[PlantOrchard] Serialize hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantOrchardSerialize() { RemoveHook(&g_hookSerialize); }

void InstallPlantOrchardSlot4() {
    PlantOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f4af0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 12);
#ifdef _DEBUG
    printf("[PlantOrchard] Slot4 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantOrchardSlot4() { RemoveHook(&g_hookSlot4); }

void InstallPlantOrchardOnComplete() {
    PlantOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f4a90);
    InstallHook(target, (void*)Reimpl_OnComplete, &g_hookOnComplete, 14);
#ifdef _DEBUG
    printf("[PlantOrchard] OnComplete hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantOrchardOnComplete() { RemoveHook(&g_hookOnComplete); }

void InstallPlantOrchardOnCancel() {
    PlantOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f4a20);
    InstallHook(target, (void*)Reimpl_OnCancel, &g_hookOnCancel, 14);
#ifdef _DEBUG
    printf("[PlantOrchard] OnCancel hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantOrchardOnCancel() { RemoveHook(&g_hookOnCancel); }

void InstallPlantOrchardCleanup() {
    PlantOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f4e70);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 13);
#ifdef _DEBUG
    printf("[PlantOrchard] Cleanup hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantOrchardCleanup() { RemoveHook(&g_hookCleanup); }

void InstallPlantOrchardValidate() {
    PlantOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f47f0);
    InstallHook(target, (void*)Reimpl_Validate, &g_hookValidate, 12);
#ifdef _DEBUG
    printf("[PlantOrchard] Validate hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantOrchardValidate() { RemoveHook(&g_hookValidate); }

void InstallPlantOrchardGetTarget() {
    PlantOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f4ef0);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 14);
#ifdef _DEBUG
    printf("[PlantOrchard] GetTarget hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePlantOrchardGetTarget() { RemoveHook(&g_hookGetTarget); }
