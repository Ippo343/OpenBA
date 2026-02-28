// engine/reimpl/createrawmaterial.cpp
// Reimplementation of CreateRawMaterial action from Application-x64.exe.
//
// CreateRawMaterial spawns raw material entities (e.g., logs, stone, food) from
// a resource-producing building. The AbstractImpl (257 instructions) handles:
//   - Looking up RawMaterialComponent config for yield amounts
//   - Checking building maturity level for yield scaling
//   - Creating new entity instances via allocator
//   - Placing entities near the building with random rotation
//   - Registering with the global resource tracker
//
// Struct size: 0x58, vtable at 0x14015fd10
// Inherits: Object::Internal -> Action -> CreateRawMaterial
//
// Fields from Serialize:
//   +0x20: resource type ID (serialized via FUN_140002340)
//   +0x28: InternalBasePtr (source building)
//   +0x30: InternalBasePtr (optional secondary entity)
//   +0x38: InternalBasePtr (optional tertiary entity)
//   +0x40: int (total quantity)
//   +0x44: int (material type index)
//   +0x48: uint (placement flags)
//   +0x4C: float (yield multiplier)
//   +0x50: bool (natural resource flag)
//   +0x51: bool (place near flag)
//
// Vtable overrides vs Action base:
//   Slot 3 (Serialize):    0x1400c6f30 -- PASSTHROUGH (versioned, 3 version checks)
//   Slot 4 (AbstractImpl): 0x1400c6b40 -- PASSTHROUGH (257 instr, entity creation)
//   Slot 9 (GetTarget):    0x1400c6aa0 -- checks +0x28->+0x10, +0x30, +0x38
//   Slots 5,6,7 (Execute, Enter, Exit): BASE
//   Slot 8 (CheckEntity):  BASE

#include "CreateRawMaterial.h"
#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookSerialize;
static HookInfo g_hookAbstractImpl;

// ============================================================
// Original function typedefs
// ============================================================

typedef void (*CreateRawMaterial_Serialize_t)(void* self, void* stream);
typedef uint64_t (*CreateRawMaterial_AbstractImpl_t)(void* self);

// ============================================================
// CreateRawMaterial::Serialize -- passthrough wrapper
// Versioned serialization with 3 version gates
// ============================================================

static void Reimpl_Serialize(void* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[CreateRawMaterial::Serialize] self=%p stream=%p\n", self, stream);
        callCount++;
    }
#endif
    ((CreateRawMaterial_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

// ============================================================
// CreateRawMaterial::AbstractImpl -- passthrough wrapper
// Complex entity creation: allocates raw material entities, places them
// with random rotation, registers with resource tracker
// ============================================================

static uint64_t Reimpl_AbstractImpl(void* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        uint8_t* act = (uint8_t*)self;
        int32_t qty = *(int32_t*)(act + 0x40);
        int32_t matIdx = *(int32_t*)(act + 0x44);
        float yieldMul = *(float*)(act + 0x4C);
        printf("[CreateRawMaterial::AbstractImpl] self=%p qty=%d matIdx=%d yield=%.2f\n",
               self, qty, matIdx, yieldMul);
        callCount++;
    }
#endif
    return ((CreateRawMaterial_AbstractImpl_t)g_hookAbstractImpl.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallCreateRawMaterialSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6f30);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[CreateRawMaterial] Serialize hooked at %p (prologue=20, passthrough)\n", (void*)target);
#endif
}

void RemoveCreateRawMaterialSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallCreateRawMaterialAbstractImpl() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6b40);
    // Prologue: PUSH(1)+PUSH(1)+PUSH(1)+PUSH(2)+PUSH(2)+PUSH(2)+MOV(3)+SUB(7) = 19 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 19);
#ifdef _DEBUG
    printf("[CreateRawMaterial] AbstractImpl hooked at %p (prologue=19, passthrough)\n", (void*)target);
#endif
}

void RemoveCreateRawMaterialAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}
