// engine/reimpl/consumerawmaterial.cpp
// Reimplementation of ConsumeRawMaterial action from Application-x64.exe.
//
// ConsumeRawMaterial is a state machine action that consumes raw materials
// from the entity's StateMachineComponent inventory. It checks available
// quantities and either subtracts them (consume mode) or verifies they exist
// (check mode), then records the consumption in the global resource tracker.
//
// Struct size: 0x40, vtable at 0x14015fc20
// Inherits: Object::Internal -> Action -> ConsumeRawMaterial
//
// Vtable overrides vs Action base:
//   Slot 3 (Serialize):    0x1400c6120 (override) -- PASSTHROUGH
//   Slot 4 (AbstractImpl): 0x1400c5f50 (override) -- PASSTHROUGH (complex inventory logic)
//   Slot 8 (CheckEntity):  0x1400c5e30 (override) -- PASSTHROUGH (undo consume on entity check)
//   Slot 9 (GetTarget):    0x1400c5f10 (override) -- simple compare
//   Slots 5,6,7 (Execute, Enter, Exit): BASE (inherited)

#include "ConsumeRawMaterial.h"
#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookSerialize;
static HookInfo g_hookAbstractImpl;
static HookInfo g_hookCheckEntity;

// ============================================================
// Original function typedefs
// ============================================================

typedef void (*ConsumeRawMaterial_Serialize_t)(void* self, void* stream);
typedef uint64_t (*ConsumeRawMaterial_AbstractImpl_t)(void* self);
typedef void (*ConsumeRawMaterial_CheckEntity_t)(void* self);

// ============================================================
// ConsumeRawMaterial::Serialize -- passthrough wrapper
// Complex serialization with versioned InternalBasePtr fields
// ============================================================

static void Reimpl_Serialize(void* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[ConsumeRawMaterial::Serialize] self=%p stream=%p\n", self, stream);
        callCount++;
    }
#endif
    ((ConsumeRawMaterial_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

// ============================================================
// ConsumeRawMaterial::AbstractImpl -- passthrough wrapper
// Complex inventory consumption: iterates material list, checks/subtracts
// quantities from StateMachineComponent, updates global resource tracker
// ============================================================

static uint64_t Reimpl_AbstractImpl(void* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        uint8_t* act = (uint8_t*)self;
        int32_t count = *(int32_t*)(act + 0x30);
        uint8_t consumeMode = *(uint8_t*)(act + 0x38);
        printf("[ConsumeRawMaterial::AbstractImpl] self=%p count=%d consume=%d\n",
               self, count, consumeMode);
        callCount++;
    }
#endif
    return ((ConsumeRawMaterial_AbstractImpl_t)g_hookAbstractImpl.trampoline)(self);
}

// ============================================================
// ConsumeRawMaterial::CheckEntity -- passthrough wrapper
// Undoes consumption when entity is removed (consume mode only)
// ============================================================

static void Reimpl_CheckEntity(void* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[ConsumeRawMaterial::CheckEntity] self=%p\n", self);
        callCount++;
    }
#endif
    ((ConsumeRawMaterial_CheckEntity_t)g_hookCheckEntity.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallConsumeRawMaterialSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6120);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[ConsumeRawMaterial] Serialize hooked at %p (prologue=20, passthrough)\n", (void*)target);
#endif
}

void RemoveConsumeRawMaterialSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallConsumeRawMaterialAbstractImpl() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5f50);
    // Prologue: PUSH(2)+SUB(4)+CMP(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 13);
#ifdef _DEBUG
    printf("[ConsumeRawMaterial] AbstractImpl hooked at %p (prologue=13, passthrough)\n", (void*)target);
#endif
}

void RemoveConsumeRawMaterialAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}

void InstallConsumeRawMaterialCheckEntity() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5e30);
    // Prologue: PUSH(2)+SUB(4)+CMP(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[ConsumeRawMaterial] CheckEntity hooked at %p (prologue=13, passthrough)\n", (void*)target);
#endif
}

void RemoveConsumeRawMaterialCheckEntity() {
    RemoveHook(&g_hookCheckEntity);
}
