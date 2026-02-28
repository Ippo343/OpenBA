// engine/reimpl/buildroad.cpp
// Reimplementation of BuildRoad Work action from Application-x64.exe.
//
// BuildRoad — road construction work action. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> BuildRoad
// Ctor: 0x1400e4ad0, Vtable: 0x140161020
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x1400295b0   <-- passthrough
//   [1]  VectorDtor    = 0x140029550
//   [2]  BaseDtor      = 0x140029540
//   [3]  Serialize     = 0x1400e4980   <-- passthrough (complex, version-dependent)
//   [4]  Execute       = 0x1400e4190   <-- passthrough (very complex road building logic)
//   [5]  Shared        = 0x1400f7540
//   [6]  OnComplete    = 0x1400e4150   <-- passthrough
//   [7]  OnCancel      = 0x1400e4120   <-- passthrough
//   [8]  Cleanup       = 0x1400e4910   <-- passthrough
//   [9]  Validate      = 0x1400e40c0   <-- passthrough
//   [10] GetTarget     = 0x1400e4960
//   [11] GetPriority   = 0x1400295a0
//
// Constructor reimplemented. All vtable methods passthrough (complex game logic).

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
// Constructor (0x1400e4ad0)
// Object::Internal::Internal() -> Work fields -> BuildRoad vtable + fields
// ============================================================
static void* Reimpl_Ctor(BuildRoad* self) {
    fn_ObjectInternal_Ctor(self);

    // Work base fields
    self->vtable = (void*)ResolveAddr(Module::Exe, 0x140161a80); // Work vtable (intermediate)
    self->field_10 = nullptr;
    self->field_18 = nullptr;
    self->field_20 = -1;
    self->field_24 = 0;
    self->field_28 = -1;
    self->field_2c = 0;
    self->field_30 = nullptr;
    self->field_38 = nullptr;
    self->field_40 = 0;

    // Set BuildRoad vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, BuildRoadAddr::Vtable);

    // BuildRoad fields
    self->field_48 = nullptr;
    self->field_50 = nullptr;
    self->field_58 = -1;
    self->field_5c = 2;

    return self;
}

// ============================================================
// ScalarDtor (0x1400295b0) — passthrough
// ============================================================
static void* Reimpl_ScalarDtor(BuildRoad* self, uint32_t flags) {
    return ((void*(*)(void*, uint32_t))g_hookScalarDtor.trampoline)(self, flags);
}

// ============================================================
// Serialize (0x1400e4980) — passthrough
// ============================================================
static void Reimpl_Serialize(BuildRoad* self, void* stream) {
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
void InstallBuildRoadCtor() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[BuildRoad] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[BuildRoad] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRoadCtor() { RemoveHook(&g_hookCtor); }

void InstallBuildRoadScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[BuildRoad] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRoadScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallBuildRoadSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[BuildRoad] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRoadSerialize() { RemoveHook(&g_hookSerialize); }
