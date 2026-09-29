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

#include "Component/TombstoneComponent.h"
#include "Entity/ComponentEntity.h"
#include "Component/TombstoneDescription.h"
#include "Component/coll_bridge.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
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


// ============================================================================
// ENDGAME B.3.8 CIT — TombstoneComponent as a REAL class (TombstoneComponent.h).
//
// Before: a 48-slot hand-built TombstoneComponent_LiveVtable filled at every
// create (FillTypedVtable + FinishLiveVtable), Inert_ScalarDtor in slot 0 (only
// ~Internal). After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2). Slot 4 keeps the tree's standalone-robust Startup body,
// which is what the table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/TombstoneComponent.h"

// 0x1400579a0. Component() is the CALL 0x1401188b0; the 0x14015a980 stamp is
// the compiler's; +0x28 = 0; flags |= 0x88 (0x88 with Component's 0x80).
TombstoneComponent::TombstoneComponent() : lifetime{0} {
    OrFlagsInCtor(0x88);
}

// ============================================================
// TombstoneComponent::Persist (0x1400578E0)
// ============================================================
void TombstoneComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 2);   // Persist's stream is void*

    lifetime.Serialize(ser);

    int version = ser.GetVersion();
    if (version < 2) {
        lifetime._ticks = lifetime._ticks / 2;
    }
}

// ============================================================
// TombstoneComponent::Startup (vtable slot [4], +0x20, original OnCreate 0x140057860)
// ============================================================
// Body-init dispatched at spawn by ComponentEntity_StartupComponents (vtable +0x20).
// Populates the countdown from the TombstoneDescription's [min, min+range] lifetime *
// secondsPerTick. _description is already the TombstoneDescription
// (ComponentEntity_CtorFromDescription's copy-ctor stores it there) — we TRUST it
// directly rather than going through IsDerivedFrom, which is unreliable in standalone
// (the factory tree is incomplete — see growthcomponent.cpp's note). Without this the
// inert factory leaves the countdown 0, so Update's `timer < 1` test fires on the FIRST
// tick and marks the tombstone for instant removal. Idempotent (only inits a 0 timer).
//
// Listed: retail 0x140057860 gates the description on IsDerivedFrom(DAT_1401c4a68) and
// always writes; this is the tree's form (its unreferenced faithful twin, the C function
// TombstoneComponent_Serialize, is deleted).
void TombstoneComponent::Startup() {

    // Already initialized (a live tombstone carries a positive lifetime) — leave it.
    if (lifetime._ticks != 0) return;

    Description* descRaw = _description;
    if (!descRaw) return;
    if (GameUpdateLoop::_singleton == nullptr) return;
    TombstoneDescription* desc = static_cast<TombstoneDescription*>(descRaw);

    // Random float in [0,1) from the world RNG.
    float randVal = GameUpdateLoop::_singleton->m_random->RandFloat();

    // lifetime seconds = (rand * range + minimum) * secondsPerTickMul  (DAT_140172234).
    float minTime = desc->_minLifetime;
    float range = desc->_lifetimeRange;
    float tickMul = kRdata_140172234;
    float seconds = (randVal * range + minTime) * tickMul;

    System::Time timeVal;
    RtSystem_Time_Ctor(&timeVal, seconds);
    lifetime = timeVal;
}

// [9] 0x1400577d0 — RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af: `LEA RDX,[RSP+0x40]; CALL [RAX+0x48]`).
// ============================================================
// TombstoneComponent::Update (0x1400577D0)
// ============================================================
void TombstoneComponent::Update(void* timeV) {
    const System::Time* pDeltaTime = reinterpret_cast<const System::Time*>(timeV);   // B.3.8 bridge (Update takes void*)

    // [LIVE-COMPONENT BRING-UP GUARD] On an inert-spawned standalone component the
    // countdown is zero unless Startup (slot +0x20) ran. If it is still zero but the
    // description is present, run the body-init now so the countdown starts from the
    // real random lifetime — otherwise the `timer < 1` test below fires on tick 1 and
    // marks the tombstone for instant removal.
    if (lifetime._ticks == 0 && _description != nullptr) {
        TombstoneComponent::Startup();
    }

    lifetime._ticks -= pDeltaTime->_ticks;


    if (lifetime._ticks < 1) {
        ComponentEntity* entityPtr = _entity;
        if (GameUpdateLoop::_singleton == nullptr) return;
        // LEFT: GameUpdateLoop::m_entityList is void* (GAME-a): its +0xD0 removal array by offset.
        uint8_t* entityList = reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton->m_entityList);

        if (entityPtr != nullptr) {

            // Check bit 10 of the entity flags
            uint32_t entityFlags = entityPtr->_entityFlags;
            if (((entityFlags >> 10) & 1) == 0) {
                ActHunt_MarkEntityRemoval_c2ea0(entityPtr);   // 0x1400c2ea0

                // Add to removal queue
                // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
                if (entityList) {
                    int count = *reinterpret_cast<int*>(entityList + 0xD8);
                    GameColl::AsArray<GameColl::Ptr8>(entityList + 0xD0)
                        .SetSizeFill(count + 1, &_entity);
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
