// engine/reimpl/cleararea.cpp
// Reimplementation of ClearArea Work action from Application-x64.exe.
//
// ClearArea — area clearing work action. Size = 0x78.
// Inherits: Object::Internal -> Work (0x48) -> ClearArea
// Ctor: 0x1400e68d0, Vtable: 0x140161110
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x140029870   <-- passthrough
//   [1]  VectorDtor    = 0x140029810
//   [2]  BaseDtor      = 0x140029800
//   [3]  Serialize     = 0x1400e6740   <-- passthrough (version-dependent, tree serialization)
//   [4]  Execute       = 0x1400e5ee0   <-- passthrough (complex)
//   [5]  Shared        = 0x1400f7540
//   [6]  OnComplete    = 0x1400e5e30   <-- passthrough
//   [7]  OnCancel      = 0x1400e5e00   <-- passthrough
//   [8]  Cleanup       = 0x1400e6650   <-- passthrough
//   [9]  Validate      = 0x1400e5b10   <-- passthrough
//   [10] GetTarget     = 0x1400e6720   <-- passthrough
//   [11] GetPriority   = 0x140029860
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
// Constructor (0x1400e68d0)
// ============================================================
static void* Reimpl_Ctor(ClearArea* self) {
    g_lastHook = "ClearArea::Ctor";
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

    // Set ClearArea vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, ClearAreaAddr::Vtable);

    // ClearArea fields
    self->field_48 = nullptr;
    self->field_50 = 0;
    self->field_58 = nullptr;
    self->field_60 = 0;
    self->field_68 = nullptr;

    return self;
}

// ============================================================
// ScalarDtor (0x140029870) — passthrough
// ============================================================
static void* Reimpl_ScalarDtor(ClearArea* self, uint32_t flags) {
    g_lastHook = "ClearArea::ScalarDtor";
    return ((void*(*)(void*, uint32_t))g_hookScalarDtor.trampoline)(self, flags);
}

// ============================================================
// Serialize (0x1400e6740) — passthrough
// ============================================================
static void Reimpl_Serialize(ClearArea* self, void* stream) {
    g_lastHook = "ClearArea::Serialize";
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
void InstallClearAreaCtor() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[ClearArea] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, ClearAreaAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[ClearArea] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaCtor() { RemoveHook(&g_hookCtor); }

void InstallClearAreaScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, ClearAreaAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[ClearArea] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallClearAreaSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, ClearAreaAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ClearArea] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaSerialize() { RemoveHook(&g_hookSerialize); }
