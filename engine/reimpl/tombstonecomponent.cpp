// engine/reimpl/tombstonecomponent.cpp
// Reimplementation of TombstoneComponent lifecycle methods from Application-x64.exe.
//
// TombstoneComponent — Component subclass (0x30 bytes).
// Fields: field28 (void* / Time, +0x28, zeroed).
// Manages tombstone lifetime — countdown timer, then removes entity.
//
// Vtable at 0x14015a980:
//   [3] Shutdown        = 0x1400578E0  <-- HOOKED (serialize with Time + version halving)
//   [4] Serialize       = 0x140057860  <-- HOOKED (OnCreate: init random lifetime from data)
//   [9] Update          = 0x1400577D0  <-- HOOKED (timer countdown + entity removal)
//   Other slots: NO-OP (Draw, OnCreate, OnRemove, Slot5, Slot10, OnUpdate2)

#include "TombstoneComponent.h"
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

typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t fn_Serializer_GetVersion = nullptr;

// System::Time::Serialize(this, serializer)
typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t fn_Time_Serialize = nullptr;

// Base::IsDerivedFrom(this, typeId) — dispatched through [0x140152a98]
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* self, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t fn_IsDerivedFrom_d = nullptr;

// System::Random::RandFloat(random) — dispatched through [0x1401512f8]
typedef float (*fn_RandFloat_t)(void* random);
static fn_RandFloat_t fn_RandFloat = nullptr;

// System::Time::Time(this, float seconds) — dispatched through [0x140152b00]
typedef void* (*fn_Time_Ctor_Float_t)(void* self, float seconds);
static fn_Time_Ctor_Float_t fn_Time_Ctor_Float = nullptr;

// InternalBasePtr::CheckPtr — dispatched through [0x140152ab0]
typedef void (*fn_CheckPtr_t)(void* ptr);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

// FUN_1400c2ea0 — mark entity for removal
typedef void (*fn_MarkForRemoval_t)(void* entity);
static fn_MarkForRemoval_t fn_MarkForRemoval = nullptr;

// FUN_140003110 — dynamic array push
typedef void (*fn_ArrayPush_t)(void* arrayBase, int newCount, void* elementPtr);
static fn_ArrayPush_t fn_ArrayPush = nullptr;

// Global constant: seconds per tick multiplier (DAT_140172234)
static float* g_pSecondsPerTickMul = nullptr;

// Game state singleton + tombstone type ID
static void** g_pGameState = nullptr;
static uint32_t* g_pTombstoneTypeId = nullptr;  // DAT_1401c4a68

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookSerialize;
static HookInfo g_hookUpdate;

// ============================================================
// Shutdown (0x1400578E0)
// Serializes base + Time at +0x28. Version 2: if loading older save
// (version < 2), halves the stored time value (old format used
// double-length units).
// ============================================================
static void Reimpl_Shutdown(TombstoneComponent* self, void* stream) {
    fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    fn_Serializer_Ctor(serializer, stream, 2);

    int64_t* pTime = (int64_t*)&self->field28;
    fn_Time_Serialize(pTime, serializer);

    int version = fn_Serializer_GetVersion(serializer);
    if (version < 2) {
        *pTime = *pTime / 2;
    }

    fn_Serializer_Dtor(serializer);
}

// ============================================================
// Serialize (0x140057860) — actually OnCreate (vtable slot 4)
// Initializes tombstone lifetime from entity building data.
// Reads min/max lifetime floats from derived building data at +0x20/+0x24,
// generates random Time in that range.
// ============================================================
static void Reimpl_Serialize(TombstoneComponent* self) {
    void* baseObj = self->_unk18;

    // IsDerivedFrom check for tombstone data type
    bool isDerived = fn_IsDerivedFrom_d(baseObj, *g_pTombstoneTypeId);
    void* buildingData = nullptr;
    if (isDerived) {
        buildingData = baseObj;
    }

    // Random float in [0, 1)
    void* rng = *(void**)((uintptr_t)*g_pGameState + 0xD8);
    float randVal = fn_RandFloat(rng);

    // Compute lifetime: (randVal * max + min) * tickMultiplier
    float minTime = *(float*)((uintptr_t)buildingData + 0x20);
    float maxTime = *(float*)((uintptr_t)buildingData + 0x24);
    float seconds = (randVal * maxTime + minTime) * *g_pSecondsPerTickMul;

    // Construct Time from seconds, store at +0x28
    int64_t timeVal;
    fn_Time_Ctor_Float(&timeVal, seconds);
    *(int64_t*)&self->field28 = timeVal;
}

// ============================================================
// Update (0x1400577D0)
// Countdown timer. Decrements field28 by delta time.
// When timer expires (<= 0), marks entity for removal and adds
// it to the removal queue at GameState+0xE8.
// ============================================================
static void Reimpl_Update(TombstoneComponent* self, int64_t* pDeltaTime) {
    int64_t* pTimer = (int64_t*)&self->field28;
    *pTimer -= *pDeltaTime;

    if (*pTimer < 1) {
        // Get entity owner
        void* entityPtr = self->owner;
        void* gameWorld = *(void**)((uintptr_t)*g_pGameState + 0xE8);

        if (entityPtr != nullptr) {
            fn_CheckPtr((void*)((uintptr_t)self + 0x10));

            // Check bit 10 of entity flags at +0x40 — skip if already marked
            uint32_t entityFlags = *(uint32_t*)((uintptr_t)entityPtr + 0x40);
            if (((entityFlags >> 10) & 1) == 0) {
                fn_CheckPtr((void*)((uintptr_t)self + 0x10));
                fn_MarkForRemoval(entityPtr);

                // Add to removal queue: array at +0xD0, count at +0xD8
                int count = *(int*)((uintptr_t)gameWorld + 0xD8);
                fn_ArrayPush((void*)((uintptr_t)gameWorld + 0xD0), count + 1,
                             (void*)((uintptr_t)self + 0x10));
            }
        }
    }
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    fn_MarkForRemoval = reinterpret_cast<fn_MarkForRemoval_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));
    fn_ArrayPush = reinterpret_cast<fn_ArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140003110));
    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    g_pTombstoneTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4a68));
    g_pSecondsPerTickMul = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172234));

    // Dispatch slots
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad8)));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad0)));
    fn_Serializer_GetVersion = reinterpret_cast<fn_Serializer_GetVersion_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a48)));
    fn_Time_Serialize = reinterpret_cast<fn_Time_Serialize_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152aa8)));
    fn_IsDerivedFrom_d = reinterpret_cast<fn_IsDerivedFrom_dispatch_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a98)));
    fn_RandFloat = reinterpret_cast<fn_RandFloat_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x1401512f8)));
    fn_Time_Ctor_Float = reinterpret_cast<fn_Time_Ctor_Float_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152b00)));
    fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ab0)));

    return fn_Component_Serialize && fn_Serializer_Ctor && fn_Serializer_Dtor
        && fn_Time_Serialize && fn_Serializer_GetVersion
        && fn_IsDerivedFrom_d && fn_RandFloat && fn_Time_Ctor_Float
        && fn_CheckPtr && fn_MarkForRemoval && fn_ArrayPush;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTombstoneComponentShutdown() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[TombstoneComponent] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400578E0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 13);
#ifdef _DEBUG
    printf("[TombstoneComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTombstoneComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTombstoneComponentSerialize() {
    if (!fn_IsDerivedFrom_d) {
        if (!ResolveHelpers()) {
#ifdef _DEBUG
            printf("[TombstoneComponent] ERROR: Failed to resolve Serialize helpers\n");
#endif
            return;
        }
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140057860);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TombstoneComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTombstoneComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallTombstoneComponentUpdate() {
    if (!fn_CheckPtr) {
        if (!ResolveHelpers()) {
#ifdef _DEBUG
            printf("[TombstoneComponent] ERROR: Failed to resolve Update helpers\n");
#endif
            return;
        }
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400577D0);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 16);
#ifdef _DEBUG
    printf("[TombstoneComponent] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveTombstoneComponentUpdate() { RemoveHook(&g_hookUpdate); }
