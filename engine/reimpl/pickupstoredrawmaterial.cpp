// engine/reimpl/pickupstoredrawmaterial.cpp
// Reimplementation of PickupStoredRawMaterial action from Application-x64.exe.
//
// PickupStoredRawMaterial handles picking up materials from a storage building.
// The AbstractImpl is extremely complex (238 instructions): it queries storage
// quantities, checks available amounts, subtracts from storage, and adds to
// the citizen's inventory.
//
// Struct size: 0x48, vtable at 0x14015ff80
// Inherits: Object::Internal -> Action -> PickupStoredRawMaterial
//
// Fields from Serialize:
//   +0x20: InternalBasePtr (work target building)
//   +0x28: InternalBasePtr (entity doing pickup)
//   +0x30: material list pointer (array of {typeId, count} pairs)
//   +0x38: material count
//   +0x40: bool (partial pickup allowed flag)
//
// Vtable overrides vs Action base:
//   Slot 3 (Serialize):    0x1400ca0d0 -- PASSTHROUGH
//   Slot 4 (AbstractImpl): 0x1400c9d40 -- PASSTHROUGH (238 instr, storage queries)
//   Slot 8 (CheckEntity):  0x1400c9c70 -- PASSTHROUGH (undo pickup on entity remove)
//   Slot 9 (GetTarget):    0x1400c9bf0 -- checks +0x28 and +0x20->+0x10
//   Slots 5,6,7 (Execute, Enter, Exit): BASE (inherited)

#include "PickupStoredRawMaterial.h"
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

typedef void (*PickupStoredRawMaterial_Serialize_t)(void* self, void* stream);
typedef uint64_t (*PickupStoredRawMaterial_AbstractImpl_t)(void* self);
typedef void (*PickupStoredRawMaterial_CheckEntity_t)(void* self);

// ============================================================
// PickupStoredRawMaterial::Serialize -- passthrough wrapper
// ============================================================

static void Reimpl_Serialize(void* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[PickupStoredRawMaterial::Serialize] self=%p stream=%p\n", self, stream);
        callCount++;
    }
#endif
    ((PickupStoredRawMaterial_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

// ============================================================
// PickupStoredRawMaterial::AbstractImpl -- passthrough wrapper
// Queries storage for available materials, transfers from storage to citizen
// ============================================================

static uint64_t Reimpl_AbstractImpl(void* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        uint8_t* act = (uint8_t*)self;
        int32_t count = *(int32_t*)(act + 0x38);
        uint8_t partial = *(uint8_t*)(act + 0x40);
        printf("[PickupStoredRawMaterial::AbstractImpl] self=%p matCount=%d partial=%d\n",
               self, count, partial);
        callCount++;
    }
#endif
    return ((PickupStoredRawMaterial_AbstractImpl_t)g_hookAbstractImpl.trampoline)(self);
}

// ============================================================
// PickupStoredRawMaterial::CheckEntity -- passthrough wrapper
// Undoes pickup when entity is removed
// ============================================================

static void Reimpl_CheckEntity(void* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[PickupStoredRawMaterial::CheckEntity] self=%p\n", self);
        callCount++;
    }
#endif
    ((PickupStoredRawMaterial_CheckEntity_t)g_hookCheckEntity.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallPickupStoredRawMaterialSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ca0d0);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[PickupStoredRawMaterial] Serialize hooked at %p (prologue=20, passthrough)\n", (void*)target);
#endif
}

void RemovePickupStoredRawMaterialSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallPickupStoredRawMaterialAbstractImpl() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9d40);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(1)+PUSH(2)+PUSH(2)+PUSH(2)+SUB(4) = 18 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 18);
#ifdef _DEBUG
    printf("[PickupStoredRawMaterial] AbstractImpl hooked at %p (prologue=18, passthrough)\n", (void*)target);
#endif
}

void RemovePickupStoredRawMaterialAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}

void InstallPickupStoredRawMaterialCheckEntity() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9c70);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(2)+SUB(4) = 13 bytes
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[PickupStoredRawMaterial] CheckEntity hooked at %p (prologue=13, passthrough)\n", (void*)target);
#endif
}

void RemovePickupStoredRawMaterialCheckEntity() {
    RemoveHook(&g_hookCheckEntity);
}
