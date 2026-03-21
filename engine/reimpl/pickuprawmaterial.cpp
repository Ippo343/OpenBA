// engine/reimpl/pickuprawmaterial.cpp
// Reimplementation of PickupRawMaterial action methods from Application-x64.exe.
//
// PickupRawMaterial — 0x30 bytes. Picks up a raw material from a source entity.
// Inherits: Object::Internal -> Action (0x20) -> PickupRawMaterial
// Vtable at 0x14015ff10 (10 slots).
//
// Overrides vs Action base:
//   Serialize  (0x1400c99e0) — serializes status, entity(+0x20) with DAT_1401c7158, entity(+0x28) with ComponentEntity
//   _purecall  (0x1400c98d0) — complex: picks up raw material, transfers to state machine
//   GetTarget  (0x1400c9860) — returns true if param matches +0x28 or entity's parent
//   Execute uses base (0x1400c4a90). Enter/Exit/CheckEntity use base.

#include "PickupRawMaterial.h"
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

static uint32_t* g_TypeInfo_DAT_1401c7158 = nullptr;
static uint32_t* g_TypeInfo_ComponentEntity = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookGetTarget;

// ============================================================
// PickupRawMaterial::Serialize (0x1400c99e0)
// Serializes status(+0x18), entity(+0x20) with DAT_1401c7158, entity(+0x28) with ComponentEntity.
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "PickupRawMaterial::Serialize";
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[16];
    uint8_t ser2[24];

    // Serialize status
    fn_Serializer_Ctor(ser1, stream, 0);
    fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    fn_Serializer_Dtor(ser1);

    // Serialize entity refs
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
    fn_Serializer_Dtor(ser2);
}

// ============================================================
// PickupRawMaterial::_purecall (0x1400c98d0)
// Complex function — picks up raw material, does state machine transfer.
// Passthrough via trampoline.
// ============================================================
typedef uint64_t (*fn_PickupPurecall_t)(void* self, void* actionStack);

static uint64_t Reimpl_Purecall(void* self, void* actionStack) {
    g_lastHook = "PickupRawMaterial::Purecall";
    return ((fn_PickupPurecall_t)g_hookPurecall.trampoline)(self, actionStack);
}

// ============================================================
// PickupRawMaterial::GetTarget (0x1400c9860)
// Returns true if param matches entity at +0x28 or entity at +0x20's parent.
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    g_lastHook = "PickupRawMaterial::GetTarget";
    uint8_t* s = (uint8_t*)self;

    fn_CheckPtr(param);
    void* paramVal = *(void**)param;

    // Check direct match with +0x28
    if (*(void**)(s + 0x28) == paramVal) {
        return true;
    }

    // Check match with +0x20's parent (+0x10)
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
    fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveExport("??0Serializer@IO@@QEAA@AEAVStream@1@H@Z");
    fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveExport("??1Serializer@IO@@QEAA@XZ");
    fn_Serialize_Bool = (fn_Serialize_Bool_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
    fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z");
}

void InstallPickupRawMaterialSerialize() {
    ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c99e0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[PickupRawMaterial] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemovePickupRawMaterialSerialize() { RemoveHook(&g_hookSerialize); }

void InstallPickupRawMaterialPurecall() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c98d0);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[PickupRawMaterial] Purecall hooked (passthrough) at %p\n", (void*)target);
#endif
}
void RemovePickupRawMaterialPurecall() { RemoveHook(&g_hookPurecall); }

void InstallPickupRawMaterialGetTarget() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9860);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[PickupRawMaterial] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemovePickupRawMaterialGetTarget() { RemoveHook(&g_hookGetTarget); }
