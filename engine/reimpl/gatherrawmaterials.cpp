// engine/reimpl/gatherrawmaterials.cpp
// Reimplementation of GatherRawMaterials Work action from Application-x64.exe.
//
// GatherRawMaterials — raw material gathering work. Size = 0x78.
// Inherits: Object::Internal -> Work (0x48) -> GatherRawMaterials
// Ctor: 0x1400ec8a0, Vtable: 0x140161420
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x14002a410   <-- passthrough
//   [1]  VectorDtor    = 0x14002a3b0
//   [2]  BaseDtor      = 0x14002a3a0
//   [3]  Serialize     = 0x1400ec560   <-- passthrough (2 arrays + tree serialization)
//   [4]  Execute       = 0x1400ebe40   <-- passthrough (complex)
//   [5]  Shared        = 0x1400f7540
//   [6]  OnComplete    = 0x1400ebdf0   <-- passthrough
//   [7]  OnCancel      = 0x1400ebdb0   <-- passthrough
//   [8]  Cleanup       = 0x1400ec480   <-- passthrough
//   [9]  Validate      = 0x1400ebb50   <-- passthrough
//   [10] GetTarget     = 0x1400ec510   <-- passthrough
//   [11] GetPriority   = 0x14002a400
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
// Constructor (0x1400ec8a0)
// ============================================================
static void* Reimpl_Ctor(GatherRawMaterials* self) {
    g_lastHook = "GatherRawMaterials::Ctor";
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

    // Set GatherRawMaterials vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, GatherRawMaterialsAddr::Vtable);

    // GatherRawMaterials fields
    self->field_48 = nullptr;
    self->field_50 = 0;
    self->field_58 = nullptr;
    self->field_60 = 0;
    self->field_68 = nullptr;
    self->field_70 = 0;

    return self;
}

// ============================================================
// ScalarDtor (0x14002a410) — passthrough
// ============================================================
static void* Reimpl_ScalarDtor(GatherRawMaterials* self, uint32_t flags) {
    g_lastHook = "GatherRawMaterials::ScalarDtor";
    return ((void*(*)(void*, uint32_t))g_hookScalarDtor.trampoline)(self, flags);
}

// ============================================================
// Serialize (0x1400ec560) — passthrough
// ============================================================
static void Reimpl_Serialize(GatherRawMaterials* self, void* stream) {
    g_lastHook = "GatherRawMaterials::Serialize";
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
void InstallGatherRawMaterialsCtor() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[GatherRawMaterials] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, GatherRawMaterialsAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[GatherRawMaterials] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveGatherRawMaterialsCtor() { RemoveHook(&g_hookCtor); }

void InstallGatherRawMaterialsScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, GatherRawMaterialsAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[GatherRawMaterials] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveGatherRawMaterialsScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallGatherRawMaterialsSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, GatherRawMaterialsAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[GatherRawMaterials] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveGatherRawMaterialsSerialize() { RemoveHook(&g_hookSerialize); }
