// engine/reimpl/consumeproduce.cpp
// Reimplementation of ConsumeProduce Work action from Application-x64.exe.
//
// ConsumeProduce — consume/produce work cycle. Size = 0x78.
// Inherits: Object::Internal -> Work (0x48) -> ConsumeProduce
// Ctor: 0x1400e9340, Vtable: 0x140161220
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x14002a190   <-- passthrough
//   [1]  VectorDtor    = 0x14002a140
//   [2]  BaseDtor      = 0x14002a130
//   [3]  Serialize     = 0x1400e9020   <-- passthrough (complex: array of InternalBasePtrs + ints)
//   [4]  Execute       = 0x1400e7840   <-- passthrough (complex)
//   [5]  Shared        = 0x1400f7540
//   [6]  OnComplete    = 0x1400e77c0   <-- passthrough
//   [7]  OnCancel      = 0x1400e7780   <-- passthrough
//   [8]  Cleanup       = 0x1400e8f30   <-- passthrough
//   [9]  Validate      = 0x1400e7530   <-- passthrough
//   [10] GetTarget     = 0x1400e8fb0   <-- passthrough
//   [11] GetPriority   = 0x14002a0f0
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
// Constructor (0x1400e9340)
// ============================================================
static void* Reimpl_Ctor(ConsumeProduceAction* self) {
    g_lastHook = "ConsumeProduce::Ctor";
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

    // Set ConsumeProduce vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, ConsumeProduceActionAddr::Vtable);

    // ConsumeProduce fields
    self->field_50 = 0;
    self->field_58 = nullptr;
    self->field_60 = 0;
    self->field_68 = nullptr;
    self->field_70 = 0;

    return self;
}

// ============================================================
// ScalarDtor (0x14002a190) — passthrough
// ============================================================
static void* Reimpl_ScalarDtor(ConsumeProduceAction* self, uint32_t flags) {
    g_lastHook = "ConsumeProduce::ScalarDtor";
    return ((void*(*)(void*, uint32_t))g_hookScalarDtor.trampoline)(self, flags);
}

// ============================================================
// Serialize (0x1400e9020) — passthrough
// ============================================================
static void Reimpl_Serialize(ConsumeProduceAction* self, void* stream) {
    g_lastHook = "ConsumeProduce::Serialize";
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
void InstallConsumeProduceCtor() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[ConsumeProduce] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, ConsumeProduceActionAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[ConsumeProduce] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveConsumeProduceCtor() { RemoveHook(&g_hookCtor); }

void InstallConsumeProduceScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, ConsumeProduceActionAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[ConsumeProduce] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveConsumeProduceScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallConsumeProduceSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, ConsumeProduceActionAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ConsumeProduce] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveConsumeProduceSerialize() { RemoveHook(&g_hookSerialize); }
