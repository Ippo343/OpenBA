// code/Game/Component/tombstonecomponent.cpp
// Clean reimplementation of TombstoneComponent lifecycle methods from Application-x64.exe.
//
// TombstoneComponent (0x30 bytes) — manages tombstone lifetime.
// Fields: +0x28 = Time (int64, countdown timer).
//
// Vtable at 0x14015a980. Three non-trivial overrides:
//   Shutdown  (0x1400578E0) — Serialize with version halving
//   Serialize (0x140057860) — Actually OnCreate: init random lifetime
//   Update    (0x1400577D0) — Timer countdown + entity removal
//

#include "Component/tombstonecomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// System::Time::Serialize(this, serializer)
typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

// Base::IsDerivedFrom — dispatched through [0x140152a98]
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* self, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// System::Random::RandFloat(random)
typedef float (*fn_RandFloat_t)(void* random);
static fn_RandFloat_t s_fn_RandFloat = nullptr;

// FUN_1400c2ea0 — mark entity for removal
typedef void (*fn_MarkForRemoval_t)(void* entity);
static fn_MarkForRemoval_t s_fn_MarkForRemoval = nullptr;

// FUN_140003110 — dynamic array push

// Global: seconds per tick multiplier (DAT_140172234)
static float* s_pSecondsPerTickMul = nullptr;

// Game state singleton + tombstone type ID
static void** s_pGameState = nullptr;
static uint32_t* s_pTombstoneTypeId = nullptr;

static bool s_initialized = false;

// ============================================================
// TombstoneComponent::Shutdown (0x1400578E0)
// ============================================================
void TombstoneComponent_Shutdown(TombstoneComponent* self, void* stream) {
    // resolver call added at top, save/load wave 2026-08-28: Shutdown never resolved
    // its own helpers (only Startup/Update did), so on a load-from-save path where
    // Shutdown fires first every s_fn_* below could still be null.
    TombstoneComponent_InitFuncs();
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor)
        s_fn_Serializer_Ctor(serializer, stream, 2);

    int64_t* pTime = (int64_t*)&self->field28;
    if (s_fn_Time_Serialize)
        s_fn_Time_Serialize(pTime, serializer);

    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializer);
        if (version < 2) {
            *pTime = *pTime / 2;
        }
    }

    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// TombstoneComponent::Serialize (0x140057860) — actually OnCreate
// ============================================================
void TombstoneComponent_Serialize(TombstoneComponent* self) {
    void* baseObj = self->_unk18;

    // IsDerivedFrom check for tombstone data type
    void* buildingData = nullptr;
    if (s_fn_IsDerivedFrom_d && s_pTombstoneTypeId) {
        bool isDerived = s_fn_IsDerivedFrom_d(baseObj, *s_pTombstoneTypeId);
        if (isDerived) buildingData = baseObj;
    }

    if (!buildingData || !s_pGameState || !*s_pGameState) return;

    // Random float in [0, 1)
    void* rng = *(void**)((uintptr_t)*s_pGameState + 0xD8);
    float randVal = 0.0f;
    if (s_fn_RandFloat)
        randVal = s_fn_RandFloat(rng);

    // Compute lifetime: (randVal * max + min) * tickMultiplier
    float minTime = *(float*)((uintptr_t)buildingData + 0x20);
    float maxTime = *(float*)((uintptr_t)buildingData + 0x24);
    float seconds = (randVal * maxTime + minTime) * *s_pSecondsPerTickMul;

    // Construct Time from seconds, store at +0x28
    int64_t timeVal;
    if (fn_Time_Ctor_Float)
        fn_Time_Ctor_Float(&timeVal, seconds);
    *(int64_t*)&self->field28 = timeVal;
}

// ============================================================
// TombstoneComponent::Startup (vtable slot [4], +0x20, original OnCreate 0x140057860)
// ============================================================
// Body-init dispatched at spawn by ComponentEntity_StartupComponents (vtable +0x20).
// Populates the +0x28 countdown timer from the TombstoneDescription's [min,max]
// lifetime * secondsPerTick. _unk18 (+0x18) is already the TombstoneDescription
// (ComponentEntity_CtorFromDescription's copy-ctor stores it there) — we TRUST it
// directly rather than going through IsDerivedFrom, which is unreliable in standalone
// (the factory tree is incomplete — see growthcomponent.cpp's note). Without this the
// inert factory leaves +0x28 == 0, so Update's `timer < 1` test fires on the FIRST
// tick and marks the tombstone for instant removal. Idempotent (only inits a 0 timer).
//
// This is the standalone-robust form of the OnCreate (TombstoneComponent_Serialize
// above): identical arithmetic, but it trusts _unk18 instead of gating on
// IsDerivedFrom so the timer is reliably populated.
void TombstoneComponent_Startup(TombstoneComponent* self) {
    TombstoneComponent_InitFuncs();

    // Already initialized (a live tombstone carries a positive lifetime) — leave it.
    if (*(int64_t*)&self->field28 != 0) return;

    void* buildingData = self->_unk18;  // +0x18 = the TombstoneDescription
    if (!buildingData) return;
    if (!s_pGameState || !*s_pGameState) return;

    // Random float in [0,1) from the world RNG (world+0xD8). Prefer the project-wide
    // engine func (always resolved); fall back to the dispatch-slot pointer.
    void* rng = *(void**)((uintptr_t)*s_pGameState + 0xD8);
    float randVal = 0.0f;
    if (fn_Random_RandFloat) randVal = fn_Random_RandFloat(rng);
    else if (s_fn_RandFloat)  randVal = s_fn_RandFloat(rng);

    // lifetime seconds = (rand * maxTime + minTime) * secondsPerTickMul  (DAT_140172234).
    float minTime = *(float*)((uintptr_t)buildingData + 0x20);
    float maxTime = *(float*)((uintptr_t)buildingData + 0x24);
    float tickMul = s_pSecondsPerTickMul ? *s_pSecondsPerTickMul : 1.0f;
    float seconds = (randVal * maxTime + minTime) * tickMul;

    int64_t timeVal = 0;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&timeVal, seconds);
    *(int64_t*)&self->field28 = timeVal;
}

// ============================================================
// TombstoneComponent::Update (0x1400577D0)
// ============================================================
void TombstoneComponent_Update(TombstoneComponent* self, int64_t* pDeltaTime) {
    TombstoneComponent_InitFuncs();

    // [LIVE-COMPONENT BRING-UP GUARD] On an inert-spawned standalone component the
    // +0x28 countdown timer is zero unless Startup (slot +0x20) ran. If it is still
    // zero but the description (+0x18) is present, run the body-init now so the
    // countdown starts from the real random lifetime — otherwise the `timer < 1`
    // test below fires on tick 1 and marks the tombstone for instant removal.
    if (*(int64_t*)&self->field28 == 0 && self->_unk18 != nullptr) {
        TombstoneComponent_Startup(self);
    }

    int64_t* pTimer = (int64_t*)&self->field28;
    *pTimer -= *pDeltaTime;


    if (*pTimer < 1) {
        void* entityPtr = self->owner;
        if (!s_pGameState || !*s_pGameState) return;
        void* gameWorld = *(void**)((uintptr_t)*s_pGameState + 0xE8);

        if (entityPtr != nullptr) {

            // Check bit 10 of entity flags at +0x40
            uint32_t entityFlags = *(uint32_t*)((uintptr_t)entityPtr + 0x40);
            if (((entityFlags >> 10) & 1) == 0) {
                if (s_fn_MarkForRemoval)
                    s_fn_MarkForRemoval(entityPtr);

                // Add to removal queue
                // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
                if (gameWorld) {
                    int count = *(int*)((uintptr_t)gameWorld + 0xD8);
                    GameColl::AsArray<GameColl::Ptr8>((void*)((uintptr_t)gameWorld + 0xD0))
                        .SetSizeFill(count + 1, (void*)((uintptr_t)self + 0x10));
                }
            }
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void TombstoneComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_MarkForRemoval = reinterpret_cast<fn_MarkForRemoval_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_pTombstoneTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4a68));
    s_pSecondsPerTickMul = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172234));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_Time_Serialize = *reinterpret_cast<fn_Time_Serialize_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x1401512f8);
    if (slot) s_fn_RandFloat = *reinterpret_cast<fn_RandFloat_t*>(slot);

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Live bring-up (Phase 1; graduated unconditional 2026-09-13).
// Routes vtable slot 4 (+0x20) -> TombstoneComponent_Startup (populates the +0x28
// random-lifetime timer at spawn, dispatched by ComponentEntity_StartupComponents) and
// slot 9 (+0x48) -> TombstoneComponent_Update (per-tick countdown, the slot the
// EntityList bucket round-robin calls — NOT +0x40/slot 8 = empty OnRemove 0x140118a70,
// verified vs Ghidra vtable 0x14015a980). The only new exe-helper
// registration is the TombstoneDescription type-id global 0x1401c4a68
// (reg_component_tombstone.cpp); every other helper was already registered.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// hand-expanded (save/load wave 2026-08-28): COMPONENT_LIVE_FACTORY_S plus
// UNCONDITIONAL slot 3 (stream Serialize) wiring — save/load round-trip is not
// an opt-in feature. Everything else identical to the macro expansion.
static uint32_t TombstoneComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("TombstoneComponent");
    return cached;
}
static void* TombstoneComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];
static void TombstoneComponent_UpdateVSlot(void* s, void* t) {
    TombstoneComponent_Update(reinterpret_cast<TombstoneComponent*>(s),
                               reinterpret_cast<int64_t*>(t));
}
static void TombstoneComponent_StartupVSlot(void* s) {
    TombstoneComponent_Startup(reinterpret_cast<TombstoneComponent*>(s));
}
static void TombstoneComponent_ShutdownVSlot(void* s, void* stream) {
    TombstoneComponent_Shutdown(reinterpret_cast<TombstoneComponent*>(s), stream);
}
static void TombstoneComponent_FinishLiveVtable() {
    // slot 3 wired unconditionally, save/load wave 2026-08-28 (fields previously reset silently on load).
    TombstoneComponent_LiveVtable[3] =
        reinterpret_cast<void*>(&TombstoneComponent_ShutdownVSlot);
    TombstoneComponent_LiveVtable[4] =
        reinterpret_cast<void*>(&TombstoneComponent_StartupVSlot);
    TombstoneComponent_LiveVtable[9] =
        reinterpret_cast<void*>(&TombstoneComponent_UpdateVSlot);
}
extern "C" void* TombstoneComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x30, 0x88,
        TombstoneComponent_LiveVtable,
        reinterpret_cast<void*>(&TombstoneComponent_LiveGetTypeId));
    TombstoneComponent_FinishLiveVtable();
    return p;
}
static void* TombstoneComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x30, 0x88,
        TombstoneComponent_LiveVtable,
        reinterpret_cast<void*>(&TombstoneComponent_LiveGetTypeId));
    TombstoneComponent_FinishLiveVtable();
    return mem;
}
REGISTER_TYPE(TombstoneComponent, "Component",
              &TombstoneComponent_Factory, &TombstoneComponent_CreateInPlace);
