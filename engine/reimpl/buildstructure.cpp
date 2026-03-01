// engine/reimpl/buildstructure.cpp
// Reimplementation of BuildStructure Work action from Application-x64.exe.
//
// BuildStructure — structure construction work action. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> BuildStructure
// Ctor: 0x1400e5a90, Vtable: 0x1401610a0
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x140029680   <-- passthrough
//   [1]  VectorDtor    = 0x140029620
//   [2]  BaseDtor      = 0x140029610
//   [3]  Serialize     = 0x1400e5850   <-- passthrough (complex, array serialization)
//   [4]  Execute       = 0x1400e4fc0   <-- passthrough (very complex building logic)
//   [5]  Shared        = 0x1400f7540
//   [6]  OnComplete    = 0x1400e4f80   <-- passthrough
//   [7]  OnCancel      = 0x1400e4f40   <-- passthrough
//   [8]  Cleanup       = 0x1400e57a0   <-- passthrough (tree traversal)
//   [9]  Validate      = 0x1400e4be0   <-- passthrough (very complex)
//   [10] GetTarget     = 0x1400e5830
//   [11] GetPriority   = 0x140029670
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
// Constructor (0x1400e5a90)
// ============================================================
static void* Reimpl_Ctor(BuildStructure* self) {
    g_lastHook = "BuildStructure::Ctor";
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

    // Set BuildStructure vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, BuildStructureAddr::Vtable);

    // BuildStructure fields (ctor zeroes +0x50..+0x68)
    self->field_50 = nullptr;
    self->field_58 = 0;
    self->field_60 = nullptr;
    self->field_68 = 0;

    return self;
}

// ============================================================
// ScalarDtor (0x140029680) — passthrough
// ============================================================
static void* Reimpl_ScalarDtor(BuildStructure* self, uint32_t flags) {
    g_lastHook = "BuildStructure::ScalarDtor";
    return ((void*(*)(void*, uint32_t))g_hookScalarDtor.trampoline)(self, flags);
}

// ============================================================
// Serialize (0x1400e5850) — passthrough
// ============================================================
static void Reimpl_Serialize(BuildStructure* self, void* stream) {
    g_lastHook = "BuildStructure::Serialize";
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
void InstallBuildStructureCtor() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[BuildStructure] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, BuildStructureAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[BuildStructure] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildStructureCtor() { RemoveHook(&g_hookCtor); }

void InstallBuildStructureScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, BuildStructureAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[BuildStructure] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildStructureScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallBuildStructureSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, BuildStructureAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[BuildStructure] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildStructureSerialize() { RemoveHook(&g_hookSerialize); }
