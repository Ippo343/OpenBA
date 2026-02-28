// engine/reimpl/removeanimal.cpp
// Reimplementation of RemoveAnimal action methods from Application-x64.exe.
//
// RemoveAnimal — smallest action (0x28 bytes). Removes an animal entity from the world.
// Inherits: Object::Internal -> Action (0x20) -> RemoveAnimal
// Vtable at 0x14015ffe0 (10 slots).
//
// Overrides vs Action base:
//   Serialize  (0x1400ca870) — serializes status + entity ptr at +0x20 (with ComponentEntity type check)
//   _purecall  (0x1400ca7f0) — Execute: marks entity for removal, queues to cleanup list
//   CheckEntity(0x1400ca760) — clears "in use" bit on entity's model component
//   GetTarget  (0x1400ca720) — returns true if param matches _field20
//   Enter/Exit use base no-ops.
//   Execute uses base (0x1400c4a90).

#include "RemoveAnimal.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers (resolved at install time)
// ============================================================

// Object::InternalBasePtr::CheckPtr(this)
typedef void (*fn_CheckPtr_t)(void* ibp);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

// Object::InternalBasePtr::Serialize(this, Serializer&)
typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t fn_IBP_Serialize = nullptr;

// IO::Serializer::Serializer(this, Stream*, int)
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer(this)
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, void* field, const char* name)
typedef void (*fn_Serializer_Serialize_t)(void* self, void* field, const char* name);
static fn_Serializer_Serialize_t fn_Serializer_Serialize = nullptr;

// FUN_1400c2ea0 — marks entity for removal
typedef void (*fn_MarkEntityRemoval_t)(void* entity);
static fn_MarkEntityRemoval_t fn_MarkEntityRemoval = nullptr;

// FUN_140003110 — pushes entity to cleanup array
typedef void (*fn_ArrayPush_t)(void* arrayBase, int newCount, void* elementPtr);
static fn_ArrayPush_t fn_ArrayPush = nullptr;

// FUN_140005500 — get model component from entity
typedef void (*fn_GetModelComp_t)(void* entity, void* outIBP);
static fn_GetModelComp_t fn_GetModelComp = nullptr;

// FUN_1400c3470 — notify entity changed
typedef void (*fn_NotifyEntityChanged_t)(void* entity);
static fn_NotifyEntityChanged_t fn_NotifyEntityChanged = nullptr;

// TypeInfo_ComponentEntity (DAT_1401c4218 for RemoveAnimal, but 0x1401c7158 used in some others)
static uint32_t* g_TypeInfo_ComponentEntity = nullptr;

// Game state singleton
static void** g_pGameState = nullptr; // DAT_1401c3dd8

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookCheckEntity;
static HookInfo g_hookGetTarget;

// ============================================================
// RemoveAnimal::Serialize (0x1400ca870)
// Serializes status byte + entity ref at +0x20 with ComponentEntity type validation.
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t localSerializer1[16];
    uint8_t localSerializer2[24];

    // Serialize status byte
    fn_Serializer_Ctor(localSerializer1, stream, 0);
    fn_Serializer_Serialize(localSerializer1, s + 0x18, nullptr);
    fn_Serializer_Dtor(localSerializer1);

    // Serialize entity ref at +0x20
    fn_Serializer_Ctor(localSerializer2, stream, 0);
    fn_IBP_Serialize(s + 0x20, localSerializer2);
    // Type-check: if the deserialized ptr is not a ComponentEntity, null it
    void* entityRef = *(void**)(s + 0x20);
    if (entityRef != nullptr) {
        if (!fn_IsDerivedFrom(entityRef, *g_TypeInfo_ComponentEntity)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }
    fn_Serializer_Dtor(localSerializer2);
}

// ============================================================
// RemoveAnimal::_purecall / Execute (0x1400ca7f0)
// Marks the entity at +0x20 for removal, queues to cleanup list.
// Returns 0 (action complete).
// ============================================================
static uint64_t Reimpl_Purecall(void* self) {
    uint8_t* s = (uint8_t*)self;
    void* entityRef = *(void**)(s + 0x20);
    void* gameState = *g_pGameState;
    void* cleanupMgr = *(void**)((uint8_t*)gameState + 0xe8);

    if (entityRef != nullptr) {
        fn_CheckPtr(s + 0x20);
        // Check bit 10 of flags at entity+0x40
        uint32_t flags = *(uint32_t*)((uint8_t*)entityRef + 0x40);
        if (((flags >> 10) & 1) == 0) {
            fn_CheckPtr(s + 0x20);
            fn_MarkEntityRemoval(entityRef);
            int count = *(int*)((uint8_t*)cleanupMgr + 0xd8);
            void* localRef = entityRef;
            fn_ArrayPush((uint8_t*)cleanupMgr + 0xd0, count + 1, &localRef);
        }
    }
    return 0;
}

// ============================================================
// RemoveAnimal::CheckEntity (0x1400ca760)
// Clears "in use" bit (bit 20) on entity model component's flags.
// ============================================================
static void Reimpl_CheckEntity(void* self) {
    uint8_t* s = (uint8_t*)self;
    void* localIBP[4];

    fn_CheckPtr(s + 0x20);
    void* entity20 = *(void**)(s + 0x20);
    fn_GetModelComp(entity20, localIBP);

    fn_CheckPtr(localIBP);
    uint8_t* modelComp = (uint8_t*)(localIBP[0]);
    uint32_t flagsOld = *(uint32_t*)(modelComp + 0x20);
    if ((flagsOld >> 20) & 1) {
        uint32_t flagsNew = flagsOld & 0xFFEFFFFF; // clear bit 20
        *(uint32_t*)(modelComp + 0x20) = flagsNew;
        if (((flagsNew ^ flagsOld) & 0xFFFF) != 0) {
            void* parent = *(void**)(modelComp + 0x10);
            if (parent != nullptr) {
                fn_CheckPtr(modelComp + 0x10);
                fn_NotifyEntityChanged(*(void**)(modelComp + 0x10));
            }
        }
    }
}

// ============================================================
// RemoveAnimal::GetTarget (0x1400ca720)
// Returns true if param matches _field20.
// ============================================================
static bool Reimpl_GetTarget(void* self, void** param) {
    uint8_t* s = (uint8_t*)self;
    fn_CheckPtr(s + 0x20);
    return *param == *(void**)(s + 0x20);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallRemoveAnimalSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ca870);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[RemoveAnimal] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveRemoveAnimalSerialize() { RemoveHook(&g_hookSerialize); }

void InstallRemoveAnimalPurecall() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ca7f0);
    // Resolve helpers
    // CheckPtr is called indirectly via IAT at [0x140152ab0]
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(iatEntry);
    fn_MarkEntityRemoval = (fn_MarkEntityRemoval_t)ResolveAddr(Module::Exe, 0x1400c2ea0);
    fn_ArrayPush = (fn_ArrayPush_t)ResolveAddr(Module::Exe, 0x140003110);
    g_pGameState = (void**)ResolveAddr(Module::Exe, 0x1401c3dd8);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[RemoveAnimal] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemoveRemoveAnimalPurecall() { RemoveHook(&g_hookPurecall); }

void InstallRemoveAnimalCheckEntity() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ca760);
    fn_GetModelComp = (fn_GetModelComp_t)ResolveAddr(Module::Exe, 0x140005500);
    fn_NotifyEntityChanged = (fn_NotifyEntityChanged_t)ResolveAddr(Module::Exe, 0x1400c3470);
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 12);
#ifdef _DEBUG
    printf("[RemoveAnimal] CheckEntity hooked at %p\n", (void*)target);
#endif
}
void RemoveRemoveAnimalCheckEntity() { RemoveHook(&g_hookCheckEntity); }

void InstallRemoveAnimalGetTarget() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ca720);
    g_TypeInfo_ComponentEntity = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c4218);
    // Resolve serializer functions for Serialize method
    fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveExport("?Serializer@IO@@QEAA@PEAVStream@1@H@Z");
    fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveExport("??1Serializer@IO@@QEAA@XZ");
    fn_Serializer_Serialize = (fn_Serializer_Serialize_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
    fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z");
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[RemoveAnimal] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveRemoveAnimalGetTarget() { RemoveHook(&g_hookGetTarget); }
