// engine/reimpl/consumeproduce.cpp
// Hook wrappers for ConsumeProduce Work action from Application-x64.exe.
//
// ConsumeProduce -- consume/produce work cycle. Size = 0x78.
// Inherits: Object::Internal -> Work (0x48) -> ConsumeProduce
// Ctor: 0x1400e9340, Vtable: 0x140161220
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x14002a190   <-- reimplemented
//   [1]  VectorDtor    = 0x14002a140
//   [2]  BaseDtor      = 0x14002a130
//   [3]  Serialize     = 0x1400e9020   <-- reimplemented
//   [4]  Execute       = 0x1400e7840   <-- passthrough (extremely complex ~500 lines)
//   [5]  Shared        = 0x1400f7540
//   [6]  OnComplete    = 0x1400e77c0   <-- passthrough (not hooked in hooks.cpp)
//   [7]  OnCancel      = 0x1400e7780   <-- passthrough (not hooked in hooks.cpp)
//   [8]  Cleanup       = 0x1400e8f30   <-- passthrough (not hooked in hooks.cpp)
//   [9]  Validate      = 0x1400e7530   <-- passthrough (not hooked in hooks.cpp)
//   [10] GetTarget     = 0x1400e8fb0   <-- passthrough (not hooked in hooks.cpp)
//   [11] GetPriority   = 0x14002a0f0
//
// Ctor, ScalarDtor, Serialize reimplemented in code/Game/Work/consumeproduce.cpp.
// Slots 6-10 have clean code in code/Game/ but are not hooked yet.

#include "BuildActions.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Work/consumeproduce_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool ConsumeProduce_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookCtor;
static HookInfo g_hookScalarDtor;
static HookInfo g_hookSerialize;

// ============================================================
// Constructor (0x1400e9340) -- reimplemented
// ============================================================
static void* Reimpl_Ctor(ConsumeProduceAction* self) {
    g_lastHook = "ConsumeProduce::Ctor";
    return ConsumeProduce_Ctor_Impl(self);
}

// ============================================================
// ScalarDtor (0x14002a190) -- reimplemented
// ============================================================
static void* Reimpl_ScalarDtor(ConsumeProduceAction* self, uint32_t flags) {
    g_lastHook = "ConsumeProduce::ScalarDtor";
    return ConsumeProduce_ScalarDtor_Impl(self, flags);
}

// ============================================================
// Serialize (0x1400e9020) -- reimplemented
// ============================================================
static void Reimpl_Serialize(ConsumeProduceAction* self, void* stream) {
    g_lastHook = "ConsumeProduce::Serialize";
    ConsumeProduce_Serialize_Impl(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallConsumeProduceCtor() {
    if (!ConsumeProduce_ResolveFuncs()) {
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
