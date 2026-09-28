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
#include "Component/TombstoneDescription.h"
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
#include "Action/exe_bodies.h"   // ActHunt_MarkEntityRemoval_c2ea0
#include "Game/exe_constants.h"

// ============================================================
// Helpers
// ============================================================
// Base::IsDerivedFrom — import slot 0x140152a98, called on the object.
// FUN_1400c2ea0 ActHunt_MarkEntityRemoval_c2ea0 — mark entity for removal.
// FUN_140003110 — dynamic array push (GameColl SetSizeFill).
// .rdata 0x140172234 — seconds-per-tick multiplier (kRdata_140172234).
// The GameWorld cell 0x1401c3dd8 is g_pGameWorldSingleton.


// ============================================================
// TombstoneComponent::Serialize (0x140057860) — actually OnCreate
// ============================================================
void TombstoneComponent_Serialize(TombstoneComponent* self) {
    void* baseObj = self->_description;

    // IsDerivedFrom check for tombstone data type
    void* buildingData = nullptr;
    {
        bool isDerived = reinterpret_cast<const Object::Base*>(baseObj)
                             ->IsDerivedFrom(TombstoneDescription::TypeId());   // B.3.x bridge
        if (isDerived) buildingData = baseObj;
    }

    if (!buildingData || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    // Random float in [0, 1)
    void* rng = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xD8);
    float randVal = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge

    // Compute lifetime: (randVal * max + min) * tickMultiplier
    float minTime = *(float*)((uintptr_t)buildingData + 0x20);
    float maxTime = *(float*)((uintptr_t)buildingData + 0x24);
    float seconds = (randVal * maxTime + minTime) * kRdata_140172234;

    // Construct Time from seconds, store at +0x28
    int64_t timeVal;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeVal), seconds);
    *(int64_t*)&self->field28 = timeVal;
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

// ============================================================
// TombstoneComponent::Persist (0x1400578E0)
// ============================================================
void TombstoneComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 2);   // B.3.4 bridge

    int64_t* pTime = (int64_t*)&field28;
    reinterpret_cast<System::Time*>(pTime)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge

    int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
    if (version < 2) {
        *pTime = *pTime / 2;
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
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
void TombstoneComponent::Startup() {

    // Already initialized (a live tombstone carries a positive lifetime) — leave it.
    if (*(int64_t*)&field28 != 0) return;

    void* buildingData = _description;  // +0x18 = the TombstoneDescription
    if (!buildingData) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    // Random float in [0,1) from the world RNG (world+0xD8).
    void* rng = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xD8);
    float randVal = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge

    // lifetime seconds = (rand * maxTime + minTime) * secondsPerTickMul  (DAT_140172234).
    float minTime = *(float*)((uintptr_t)buildingData + 0x20);
    float maxTime = *(float*)((uintptr_t)buildingData + 0x24);
    float tickMul = kRdata_140172234;
    float seconds = (randVal * maxTime + minTime) * tickMul;

    int64_t timeVal = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeVal), seconds);
    *(int64_t*)&field28 = timeVal;
}

// [9] 0x1400577d0 — RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af: `LEA RDX,[RSP+0x40]; CALL [RAX+0x48]`).
// ============================================================
// TombstoneComponent::Update (0x1400577D0)
// ============================================================
void TombstoneComponent::Update(void* timeV) {
    int64_t* pDeltaTime = static_cast<int64_t*>(timeV);

    // [LIVE-COMPONENT BRING-UP GUARD] On an inert-spawned standalone component the
    // +0x28 countdown timer is zero unless Startup (slot +0x20) ran. If it is still
    // zero but the description (+0x18) is present, run the body-init now so the
    // countdown starts from the real random lifetime — otherwise the `timer < 1`
    // test below fires on tick 1 and marks the tombstone for instant removal.
    if (*(int64_t*)&field28 == 0 && _description != nullptr) {
        TombstoneComponent::Startup();
    }

    int64_t* pTimer = (int64_t*)&field28;
    *pTimer -= *pDeltaTime;


    if (*pTimer < 1) {
        void* entityPtr = _entity;
        if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
        void* gameWorld = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xE8);

        if (entityPtr != nullptr) {

            // Check bit 10 of entity flags at +0x40
            uint32_t entityFlags = *(uint32_t*)((uintptr_t)entityPtr + 0x40);
            if (((entityFlags >> 10) & 1) == 0) {
                ActHunt_MarkEntityRemoval_c2ea0(entityPtr);   // 0x1400c2ea0

                // Add to removal queue
                // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
                if (gameWorld) {
                    int count = *(int*)((uintptr_t)gameWorld + 0xD8);
                    GameColl::AsArray<GameColl::Ptr8>((void*)((uintptr_t)gameWorld + 0xD0))
                        .SetSizeFill(count + 1, (void*)(reinterpret_cast<uintptr_t>(this) + 0x10));
                }
            }
        }
    }
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

DEFINE_EXE_TYPE(TombstoneComponent, "Component")
