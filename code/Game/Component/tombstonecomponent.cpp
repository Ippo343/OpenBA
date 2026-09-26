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
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// Base::IsDerivedFrom — dispatched through [0x140152a98]
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* self, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

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

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 2);   // B.3.4 bridge

    int64_t* pTime = (int64_t*)&self->field28;
    reinterpret_cast<System::Time*>(pTime)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge

    int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
    if (version < 2) {
        *pTime = *pTime / 2;
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// TombstoneComponent::Serialize (0x140057860) — actually OnCreate
// ============================================================
void TombstoneComponent_Serialize(TombstoneComponent* self) {
    void* baseObj = self->_description;

    // IsDerivedFrom check for tombstone data type
    void* buildingData = nullptr;
    if (s_fn_IsDerivedFrom_d && s_pTombstoneTypeId) {
        bool isDerived = s_fn_IsDerivedFrom_d(baseObj, *s_pTombstoneTypeId);
        if (isDerived) buildingData = baseObj;
    }

    if (!buildingData || !s_pGameState || !*s_pGameState) return;

    // Random float in [0, 1)
    void* rng = *(void**)((uintptr_t)*s_pGameState + 0xD8);
    float randVal = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge

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

    void* buildingData = self->_description;  // +0x18 = the TombstoneDescription
    if (!buildingData) return;
    if (!s_pGameState || !*s_pGameState) return;

    // Random float in [0,1) from the world RNG (world+0xD8).
    void* rng = *(void**)((uintptr_t)*s_pGameState + 0xD8);
    float randVal = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge

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
    if (*(int64_t*)&self->field28 == 0 && self->_description != nullptr) {
        TombstoneComponent_Startup(self);
    }

    int64_t* pTimer = (int64_t*)&self->field28;
    *pTimer -= *pDeltaTime;


    if (*pTimer < 1) {
        void* entityPtr = self->_entity;
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

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_initialized = true;
}

// ============================================================================
// ENDGAME B.3.8 CIT — TombstoneComponent as a REAL class (TombstoneComponent.h).
//
// Before: a 48-slot hand-built TombstoneComponent_LiveVtable filled at every
// create (FillTypedVtable + FinishLiveVtable), Inert_ScalarDtor in slot 0 (only
// ~Internal). After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2). Slot 4 keeps the tree's standalone-robust Startup body
// (TombstoneComponent_Startup above), which is what the table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/TombstoneComponent.h"

// 0x1400579a0. Component() is the CALL 0x1401188b0; the 0x14015a980 stamp is
// the compiler's; +0x28 = 0; flags |= 0x88 (0x88 with Component's 0x80).
TombstoneComponent::TombstoneComponent() : field28(nullptr) {
    OrFlagsInCtor(0x88);
}

// [0] 0x14000d480: ~TombstoneComponent (0x140057960: restamp + ~Internal), MemFree on &1.
Object::Base* TombstoneComponent::DeletingDtor(unsigned flags) {
    this->~TombstoneComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x1400578e0 — the body resolves its own helpers first (slot 3 is the
// first call into the class on a load-from-save).
void TombstoneComponent::Persist(void* stream) { TombstoneComponent_Shutdown(this, stream); }

// [4] 0x140057860 — the tree's standalone-robust lifetime roll.
void TombstoneComponent::Startup() { TombstoneComponent_Startup(this); }

// [9] 0x1400577d0 — RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af: `LEA RDX,[RSP+0x40]; CALL [RAX+0x48]`).
void TombstoneComponent::Update(void* time) {
    TombstoneComponent_Update(this, static_cast<int64_t*>(time));
}

// create 0x14000d280: MemAlloc(0x30, 8), then the ctor.
void* TombstoneComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TombstoneComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TombstoneComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* TombstoneComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TombstoneComponent());
}

DEFINE_EXE_VTABLE(TombstoneComponent, "_ZTV18TombstoneComponent")
DEFINE_EXE_TYPE(TombstoneComponent, "Component")
