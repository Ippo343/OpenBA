// engine/reimpl/destroycomponent.cpp
// Reimplementation of DestroyComponent lifecycle methods from Application-x64.exe.
//
// DestroyComponent — ControllerComponent subclass (0x68 bytes).
// Fields: field48 (Time), destroyFlags (uint16 +0x50), field51 (bool),
//         field54 (uint32), field58 (ptr), field60 (entity ref / mesh ID).
// Manages building demolition/destruction process.
//
// Vtable at 0x140163120:
//   [3] Shutdown        = 0x14011DB50  <-- HOOKED (serialize all destroy state fields)
//   [4] Serialize       = 0x14011DB00  <-- HOOKED (OnCreate: copy mesh ref from building data)
//   Other slots: NO-OP (Draw=base, OnCreate=base, OnRemove, Update, Slot10, OnUpdate2)

#include "DestroyComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t fn_Serializer_SerializeInt = nullptr;

typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t fn_Serializer_SerializeBool = nullptr;

typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t fn_Time_Serialize = nullptr;

// FUN_140002340 — serialize InternalBasePtr (field60)
typedef void (*fn_SerializeInternalBasePtr_t)(void* ptrField, void* serializer);
static fn_SerializeInternalBasePtr_t fn_SerializeInternalBasePtr = nullptr;

// IsDerivedFrom — dispatched through [0x140152a98]
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* self, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t fn_IsDerivedFrom_d = nullptr;

// Type ID for building component type (DAT_1401c7628)
static uint32_t* g_pBuildingCompTypeId = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookSerialize;

// ============================================================
// Shutdown (0x14011DB50)
// Full serialization of destroy state:
//   1. Component_Serialize
//   2. Empty serializer block (controller compat)
//   3. Serializer(version=0): field54 (int), field58 (int),
//      field51 (bool), field60 (InternalBasePtr via FUN_140002340),
//      destroyFlags at +0x50 (bool), field48 (Time)
// ============================================================
static void Reimpl_Shutdown(DestroyComponent* self, void* stream) {
    g_lastHook = "DestroyComponent::Shutdown";
    fn_Component_Serialize(self, stream);

    // Empty serializer block (controller format compat)
    uint8_t serializer1[24];
    fn_Serializer_Ctor(serializer1, stream, 0);
    fn_Serializer_Dtor(serializer1);

    // Serialize destroy state fields
    uint8_t serializer2[16];
    fn_Serializer_Ctor(serializer2, stream, 0);

    fn_Serializer_SerializeInt(serializer2, (int*)&self->field54, nullptr);
    fn_Serializer_SerializeInt(serializer2, (int*)((uintptr_t)self + 0x58), nullptr);

    fn_Serializer_SerializeBool(serializer2, (bool*)((uintptr_t)self + 0x51), nullptr);

    fn_SerializeInternalBasePtr((void*)((uintptr_t)self + 0x60), serializer2);

    fn_Serializer_SerializeBool(serializer2, (bool*)&self->destroyFlags, nullptr);

    fn_Time_Serialize((void*)&self->field48, serializer2);

    fn_Serializer_Dtor(serializer2);
}

// ============================================================
// Serialize (0x14011DB00) — actually vtable slot [4]
// Copies mesh reference from building data. Reads _unk18,
// does IsDerivedFrom check, copies 8 bytes from building+0x20
// into field60.
// ============================================================
static void Reimpl_Serialize(DestroyComponent* self) {
    g_lastHook = "DestroyComponent::Serialize";
    void* baseObj = self->_unk18;

    bool isDerived = fn_IsDerivedFrom_d(baseObj, *g_pBuildingCompTypeId);
    void* buildingData = nullptr;
    if (isDerived) {
        buildingData = baseObj;
    }

    // Copy mesh reference from building data +0x20 into field60
    *(uint64_t*)&self->field60 = *(uint64_t*)((uintptr_t)buildingData + 0x20);
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    fn_SerializeInternalBasePtr = reinterpret_cast<fn_SerializeInternalBasePtr_t>(
        ResolveAddr(Module::Exe, 0x140002340));
    g_pBuildingCompTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7628));

    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad8)));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad0)));
    fn_Serializer_SerializeInt = reinterpret_cast<fn_Serializer_SerializeInt_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ac8)));
    fn_Serializer_SerializeBool = reinterpret_cast<fn_Serializer_SerializeBool_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a58)));
    fn_Time_Serialize = reinterpret_cast<fn_Time_Serialize_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152aa8)));
    fn_IsDerivedFrom_d = reinterpret_cast<fn_IsDerivedFrom_dispatch_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a98)));

    return fn_Component_Serialize && fn_Serializer_Ctor && fn_Serializer_Dtor
        && fn_Serializer_SerializeInt && fn_Serializer_SerializeBool
        && fn_Time_Serialize && fn_SerializeInternalBasePtr
        && fn_IsDerivedFrom_d;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallDestroyComponentShutdown() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[DestroyComponent] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011DB50);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[DestroyComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveDestroyComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallDestroyComponentSerialize() {
    if (!fn_IsDerivedFrom_d) {
        if (!ResolveHelpers()) {
#ifdef _DEBUG
            printf("[DestroyComponent] ERROR: Failed to resolve Serialize helpers\n");
#endif
            return;
        }
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011DB00);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[DestroyComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveDestroyComponentSerialize() { RemoveHook(&g_hookSerialize); }
