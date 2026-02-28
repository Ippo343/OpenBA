// engine/reimpl/removenaturalresource.cpp
// Reimplementation of RemoveNaturalResource action methods from Application-x64.exe.
//
// RemoveNaturalResource — 0x38 bytes. Removes a natural resource entity.
// Inherits: Object::Internal -> Action (0x20) -> RemoveNaturalResource
// Vtable at 0x140160050 (10 slots).
//
// Overrides vs Action base:
//   Serialize   (0x1400cac50) — serializes status, entity(+0x20) DAT_1401c7158, entity(+0x28) ComponentEntity, bool(+0x30)
//   _purecall   (0x1400cab10) — checks resource ownership, sets harvest flags, calls removal
//   CheckEntity (0x1400caaa0) — clears "in use" bit on model component at +0x28
//   GetTarget   (0x1400caa20) — matches +0x28 or entity parent at +0x20
//   Execute uses base. Enter/Exit use base.
//
// Field layout (0x38 bytes):
//   +0x20: void*    citizenEntity  (entity ref, DAT_1401c7158 type)
//   +0x28: void*    resourceEntity (entity ref, ComponentEntity type)
//   +0x30: uint8_t  harvestFlag
//   +0x31-0x37: padding

#include "RemoveNaturalResource.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================

typedef void (*fn_CheckPtr_t)(void* ibp);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t fn_IBP_Serialize = nullptr;

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;
typedef void (*fn_Serialize_Bool_t)(void* self, void* field, const char* name);
static fn_Serialize_Bool_t fn_Serialize_Bool = nullptr;

// FUN_1400052c0 — get NaturalResourceComponent from entity
typedef void* (*fn_GetNRComp_t)(void* entity, void* outIBP);
static fn_GetNRComp_t fn_GetNRComp = nullptr;

// FUN_1400c3470 — notify entity changed
typedef void (*fn_NotifyEntityChanged_t)(void* entity);
static fn_NotifyEntityChanged_t fn_NotifyEntityChanged = nullptr;

static uint32_t* g_TypeInfo_DAT_1401c7158 = nullptr;
static uint32_t* g_TypeInfo_ComponentEntity = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookCheckEntity;
static HookInfo g_hookGetTarget;

// ============================================================
// RemoveNaturalResource::Serialize (0x1400cac50)
// Serializes: status(+0x18), entity(+0x20), entity(+0x28), bool(+0x30)
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[16];
    uint8_t ser2[24];

    // Serialize status
    fn_Serializer_Ctor(ser1, stream, 0);
    fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    fn_Serializer_Dtor(ser1);

    // Serialize entity refs + flag
    fn_Serializer_Ctor(ser2, stream, 0);
    fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *g_TypeInfo_DAT_1401c7158)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }
    fn_IBP_Serialize(s + 0x28, ser2);
    if (*(void**)(s + 0x28) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x28), *g_TypeInfo_ComponentEntity)) {
            *(void**)(s + 0x28) = nullptr;
        }
    }
    fn_Serialize_Bool(ser2, s + 0x30, nullptr);
    fn_Serializer_Dtor(ser2);
}

// ============================================================
// RemoveNaturalResource::_purecall (0x1400cab10)
// Complex function (66 instructions). Checks ownership, sets harvest flags, removes resource.
// Passthrough via trampoline.
// ============================================================
typedef uint64_t (*fn_RNRPurecall_t)(void* self);

static uint64_t Reimpl_Purecall(void* self) {
    return ((fn_RNRPurecall_t)g_hookPurecall.trampoline)(self);
}

// ============================================================
// RemoveNaturalResource::CheckEntity (0x1400caaa0)
// Clears "in use" bit (bit 20) on entity's model component at +0x28.
// ============================================================
static void Reimpl_CheckEntity(void* self) {
    uint8_t* s = (uint8_t*)self;
    void* localIBP[4];

    fn_CheckPtr(s + 0x28);
    fn_GetNRComp(*(void**)(s + 0x28), localIBP);

    fn_CheckPtr(localIBP);
    uint8_t* modelComp = (uint8_t*)(localIBP[0]);
    uint32_t flagsOld = *(uint32_t*)(modelComp + 0x20);
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

// ============================================================
// RemoveNaturalResource::GetTarget (0x1400caa20)
// Returns true if param matches entity at +0x28 or entity at +0x20's parent.
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    uint8_t* s = (uint8_t*)self;

    fn_CheckPtr(s + 0x28);
    void* entity28 = *(void**)(s + 0x28);

    if (*(void**)param == entity28) {
        return true;
    }

    fn_CheckPtr(s + 0x20);
    void* parentEntity = *(void**)(*(uint8_t**)(s + 0x20) + 0x10);
    fn_CheckPtr(param);
    return parentEntity == *(void**)param;
}

// ============================================================
// Install / Remove
// ============================================================

static void ResolveHelpers() {
    // CheckPtr is called indirectly via IAT at [0x140152ab0]
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(iatEntry);
    g_TypeInfo_DAT_1401c7158 = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7158);
    g_TypeInfo_ComponentEntity = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c4218);
    fn_GetNRComp = (fn_GetNRComp_t)ResolveAddr(Module::Exe, 0x1400052c0);
    fn_NotifyEntityChanged = (fn_NotifyEntityChanged_t)ResolveAddr(Module::Exe, 0x1400c3470);
    fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveExport("?Serializer@IO@@QEAA@PEAVStream@1@H@Z");
    fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveExport("??1Serializer@IO@@QEAA@XZ");
    fn_Serialize_Bool = (fn_Serialize_Bool_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
    fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z");
}

void InstallRemoveNaturalResourceSerialize() {
    ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400cac50);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[RemoveNaturalResource] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveRemoveNaturalResourceSerialize() { RemoveHook(&g_hookSerialize); }

void InstallRemoveNaturalResourcePurecall() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400cab10);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[RemoveNaturalResource] Purecall hooked (passthrough) at %p\n", (void*)target);
#endif
}
void RemoveRemoveNaturalResourcePurecall() { RemoveHook(&g_hookPurecall); }

void InstallRemoveNaturalResourceCheckEntity() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400caaa0);
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 12);
#ifdef _DEBUG
    printf("[RemoveNaturalResource] CheckEntity hooked at %p\n", (void*)target);
#endif
}
void RemoveRemoveNaturalResourceCheckEntity() { RemoveHook(&g_hookCheckEntity); }

void InstallRemoveNaturalResourceGetTarget() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400caa20);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[RemoveNaturalResource] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveRemoveNaturalResourceGetTarget() { RemoveHook(&g_hookGetTarget); }
