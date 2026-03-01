// engine/reimpl/plantorchard.cpp
// Reimplementation of PlantOrchard Work action from Application-x64.exe.
//
// PlantOrchard — orchard planting work. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> PlantOrchard
// Ctor: 0x1400f50b0, Vtable: 0x140161910
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x14002c540   <-- passthrough
//   [1]  VectorDtor    = 0x14002c4f0
//   [2]  BaseDtor      = 0x14002c4e0
//   [3]  Serialize     = 0x1400f4f50   <-- passthrough (identical pattern to PlantArea)
//   [4]  Execute       = 0x1400f4af0   <-- passthrough (complex)
//   [5]  Shared        = 0x1400f7540
//   [6]  OnComplete    = 0x1400f4a90   <-- passthrough
//   [7]  OnCancel      = 0x1400f4a20   <-- passthrough
//   [8]  Cleanup       = 0x1400f4e70   <-- passthrough
//   [9]  Validate      = 0x1400f47f0   <-- passthrough
//   [10] GetTarget     = 0x1400f4ef0   <-- passthrough
//   [11] GetPriority   = 0x140029860   (shared with ClearArea, PlantArea)
//
// Constructor reimplemented. All vtable methods passthrough.

#include "BuildActions.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================
typedef void* (*fn_ObjectInternal_Ctor_t)(void* self);
static fn_ObjectInternal_Ctor_t fn_ObjectInternal_Ctor = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookCtor;
static HookInfo g_hookScalarDtor;
static HookInfo g_hookSerialize;

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
// ScalarDtor (0x14002c540) — passthrough
// ============================================================
static void* Reimpl_ScalarDtor(PlantOrchard* self, uint32_t flags) {
    g_lastHook = "PlantOrchard::ScalarDtor";
    return ((void*(*)(void*, uint32_t))g_hookScalarDtor.trampoline)(self, flags);
}

// ============================================================
// Serialize (0x1400f4f50) — passthrough
// ============================================================
static void Reimpl_Serialize(PlantOrchard* self, void* stream) {
    g_lastHook = "PlantOrchard::Serialize";
    ((void(*)(void*, void*))g_hookSerialize.trampoline)(self, stream);
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    fn_ObjectInternal_Ctor = reinterpret_cast<fn_ObjectInternal_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152768)));
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
    uintptr_t target = ResolveAddr(Module::Exe, PlantOrchardAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[PlantOrchard] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemovePlantOrchardCtor() { RemoveHook(&g_hookCtor); }

void InstallPlantOrchardScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, PlantOrchardAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[PlantOrchard] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemovePlantOrchardScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallPlantOrchardSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, PlantOrchardAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[PlantOrchard] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemovePlantOrchardSerialize() { RemoveHook(&g_hookSerialize); }
