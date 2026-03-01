// engine/reimpl/plantarea.cpp
// Reimplementation of PlantArea Work action from Application-x64.exe.
//
// PlantArea — crop field planting work. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> PlantArea
// Ctor: 0x1400f4770, Vtable: 0x140161890
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x14002c4b0   <-- passthrough
//   [1]  VectorDtor    = 0x14002c460
//   [2]  BaseDtor      = 0x14002c450
//   [3]  Serialize     = 0x1400f4540   <-- passthrough (tree + array serialization)
//   [4]  Execute       = 0x1400f4010   <-- passthrough (complex)
//   [5]  Shared        = 0x1400f7540
//   [6]  OnComplete    = 0x1400f3f60   <-- passthrough
//   [7]  OnCancel      = 0x1400f3ea0   <-- passthrough
//   [8]  Cleanup       = 0x1400f4460   <-- passthrough
//   [9]  Validate      = 0x1400f3c70   <-- passthrough
//   [10] GetTarget     = 0x1400f44e0   <-- passthrough
//   [11] GetPriority   = 0x140029860   (shared with ClearArea, PlantOrchard)
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
// ScalarDtor (0x14002c4b0) — passthrough
// ============================================================
static void* Reimpl_ScalarDtor(PlantArea* self, uint32_t flags) {
    g_lastHook = "PlantArea::ScalarDtor";
    return ((void*(*)(void*, uint32_t))g_hookScalarDtor.trampoline)(self, flags);
}

// ============================================================
// Serialize (0x1400f4540) — passthrough
// ============================================================
static void Reimpl_Serialize(PlantArea* self, void* stream) {
    g_lastHook = "PlantArea::Serialize";
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
void InstallPlantAreaCtor() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[PlantArea] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, PlantAreaAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[PlantArea] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemovePlantAreaCtor() { RemoveHook(&g_hookCtor); }

void InstallPlantAreaScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, PlantAreaAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[PlantArea] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemovePlantAreaScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallPlantAreaSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, PlantAreaAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[PlantArea] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemovePlantAreaSerialize() { RemoveHook(&g_hookSerialize); }
