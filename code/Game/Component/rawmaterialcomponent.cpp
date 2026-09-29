// code/Game/Component/rawmaterialcomponent.cpp
// Clean reimplementation of RawMaterialComponent lifecycle methods from Application-x64.exe.
//
// RawMaterialComponent (0x40 bytes) manages raw material lifecycle: spawning,
// decay timing, and collection marking.
//
// Vtable at 0x140164170. Four overrides:
//   Persist   (0x140129330) — versioned serialization of quantity, age, profession
//   Startup   (0x1401292c0) — quantity from the description, register the template with the town
//   OnAdded   (0x1401291d0) — post the Pickup work to the entity's WorkComponent
//   Update    (0x140129130) — accumulate time, check threshold, mark for collection
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/rawmaterialcomponent_funcs.h"
#include "Component/RawMaterialDescription.h"
#include "Component/ComponentDescription.h"
#include "Component/WorkComponent.h"
#include "Entity/ComponentEntity.h"
#include "Entity/CitizenBehavior.h"
#include "Work/Pickup.h"
#include "Component/coll_bridge.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"      // ActHunt_MarkEntityRemoval_c2ea0

// The exe helpers this file calls, by retail address:
//   0x140018020 Reimpl_ExternalPtr_Serialize_StringTable (ExternalPtr<Profession>, version >= 2)
//   0x140091e60 CitizenBehavior::RegisterRawMaterial (below)
//   0x1400c2ea0 ActHunt_MarkEntityRemoval_c2ea0 (mark entity for collection)
//   0x140003050 ComponentEntity::FindComponent<WorkComponent>
//   0x140129480 RawMaterialDescription::GetPickupCategory (below)
//   0x14012ec60 CitizenAI_WorkScheduler_Post (the work-scheduler POST, not a draw)
//   .rdata 0x14017225c = 3600.0f, the pile expiry threshold; the GameWorld cell is
//   g_pGameWorldSingleton.

// ============================================================
// RawMaterialComponent::Update (vtable slot [9], 0x140129130)
// ============================================================
// Accumulates delta time into the age. When threshold exceeded,
// marks entity for collection and adds to world pending list.

void RawMaterialComponent_Update(RawMaterialComponent* self, int64_t* pDeltaTime) {

    // Accumulate delta time into the age (a System::Time)
    self->age._ticks += *pDeltaTime;

    // The threshold: Time(3600.0f) (.rdata 0x14017225c)
    System::Time thresholdTime;
    RtSystem_Time_Ctor(&thresholdTime, 3600.0f);

    // Check if accumulated time exceeds threshold
    if (thresholdTime._ticks < self->age._ticks) {
        ComponentEntity* entity = self->_entity;

        if (GameUpdateLoop::_singleton == nullptr) return;
        // LEFT: GameUpdateLoop::m_entityList is void* (GAME-a): the removal list stays by offset.
        uintptr_t worldMgr = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton->m_entityList);

        if (entity != nullptr) {

            // Check bit 10 of entity flags at +0x40
            if (((entity->_entityFlags >> 10) & 1) == 0) {

                ActHunt_MarkEntityRemoval_c2ea0(entity);

                // Add to world pending list
                // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
                {
                    int currentCount = *(int*)(worldMgr + 0xd8);
                    GameColl::AsArray<GameColl::Ptr8>((void*)(worldMgr + 0xd0))
                        .SetSizeFill(currentCount + 1, &self->_entity);
                }
            }
        }
    }
}

// ============================================================
// CitizenBehavior::RegisterRawMaterial (0x140091e60): append a raw-material
// template to the town's known-material list (+0x110 data, +0x118 count,
// +0x11C capacity, stride 8) unless it is already there. Retail's receiver
// is GameUpdateLoop +0xF0, the CitizenBehavior.
// ============================================================
// Scans the existing array for the material; if present, returns. Otherwise grows
// (newCount = oldCount+1, tripled capacity if previously non-empty), copies the
// old elements, writes the material into the new tail slot, and frees the old buffer.
void CitizenBehavior::RegisterRawMaterial(ComponentDescription* material) {
    // Scan for existing entry
    int count = field_0x118;
    ComponentDescription** arr = field_0x110;
    for (int64_t i = 0; i < count; i++) {
        if (arr[i] == material) {
            return;
        }
    }

    int oldCount = field_0x118;
    int newCount = oldCount + 1;
    if (newCount == oldCount) return; // unreachable, matches original guard

    ComponentDescription** oldBuf = field_0x110;
    field_0x118 = newCount;

    if (field_0x11C < newCount) {
        field_0x11C = newCount;
        if (oldCount != 0) {
            field_0x11C = newCount * 3;
        }
        field_0x110 = static_cast<ComponentDescription**>(
            System::MemoryAllocator::MemAlloc((uint64_t)((int64_t)field_0x11C << 3), 8));
    }

    int start = 0;
    if (oldBuf == field_0x110) {
        start = oldCount;
    }
    ComponentDescription** dst = field_0x110;
    for (int i = start; i < newCount; i++) {
        ComponentDescription** slot = dst + i;
        if (slot) {
            if (i < oldCount) {
                *slot = oldBuf[i];
            } else {
                *slot = material;
            }
        }
    }

    if (oldBuf != field_0x110) {
        System::MemoryAllocator::MemFree(oldBuf);
    }
}

// B4 FORWARD: 5 outside callers (debugui.cpp, tradeui_checkout.cpp, starttown.cpp,
// tradecomponent_dock.cpp, and this file's Startup, whose receiver comes from the
// untyped GameUpdateLoop::m_citizenList).
void RawMaterialComponent_RegisterRawMaterial(void* worldMgr, void* entityChain) {
    reinterpret_cast<CitizenBehavior*>(worldMgr)->RegisterRawMaterial(
        reinterpret_cast<ComponentDescription*>(entityChain));
}

// ============================================================
// RawMaterialDescription::GetPickupCategory (0x140129480): the pickup work's
// category bits from the description's flag word (+0x5c)
// ============================================================
uint32_t RawMaterialDescription::GetPickupCategory() const {
    uint32_t flags = _flags;
    if ((flags >> 5) & 1)   return 2;
    if ((flags >> 6) & 1)   return 4;
    if ((flags >> 10) & 1)  return 0x10;
    if ((flags >> 12) & 1)  return 0x40;
    if ((flags >> 11) & 1)  return 0x20;
    if ((flags >> 13) & 1)  return 0x100;
    if ((flags >> 16) & 1)  return 0x200;
    if ((flags >> 9) & 1)   return 8;
    if ((flags >> 14) & 1)  return 0x80;
    return 1;
}

// ============================================================================
// ENDGAME B.3.8 VIS — RawMaterialComponent as a REAL class (RawMaterialComponent.h).
//
// Before: a 48-slot RawMaterialComponent_InertVtable filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal).
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/RawMaterialComponent.h"

// 0x140129400: Component() is the CALL 0x1401188b0; the 0x140164170 stamp is
// the compiler's; +0x30 = +0x38 = 0; flags |= 0x89 (bit 0 enrols it in the
// OnAddedPass, bit 7 in StartupComponents). Retail leaves +0x28 unwritten
// (Startup sets it); the tree's zero is kept.
RawMaterialComponent::RawMaterialComponent()
    : quantity(0), _pad2c{}, profession(nullptr), age{0} {
    OrFlagsInCtor(0x89);
}

// ============================================================
// RawMaterialComponent::Persist (vtable slot [3], 0x140129330)
// ============================================================
// Versioned serialization (version 2):
//   - quantity (int)
//   - age (Time accumulator)
//   - profession (version >= 2 only)

void RawMaterialComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 2);   // LEFT: Component::Persist takes the stream as void* (the base's signature)

    ser.Serialize(quantity, nullptr);
    age.Serialize(ser);

    // Version >= 2: the ExternalPtr<Profession>
    if (ser.GetVersion() >= 2) {
        Reimpl_ExternalPtr_Serialize_StringTable(&profession, &ser);   // 0x140018020
    }
}

// ============================================================
// RawMaterialComponent::Startup (vtable slot [4], 0x1401292c0)
// ============================================================
// Init the quantity from the description's _lowCreateCount (+0x40), then
// register the entity's template with the town's raw-material list.

void RawMaterialComponent::Startup() {
    Description* baseObj = _description;
    RawMaterialDescription* rawMatDesc =
        Object::IsDerivedFromNullSafe(baseObj, RawMaterialDescription::TypeId())
            ? static_cast<RawMaterialDescription*>(baseObj) : nullptr;

    if (rawMatDesc) {
        quantity = rawMatDesc->_lowCreateCount;
    }

    // Register the entity's template with the town manager.
    if (GameUpdateLoop::_singleton != nullptr) {
        // LEFT: GameUpdateLoop::m_citizenList is void* (GAME-a): the B4 FORWARD takes it.
        RawMaterialComponent_RegisterRawMaterial(GameUpdateLoop::_singleton->m_citizenList,
                                                 _entity->_description);
    }
}

// ============================================================
// RawMaterialComponent::OnAdded (vtable slot [6], 0x1401291d0)
// ============================================================
// ⚠ NOT a render call — the name is Ghidra's. This is THE PILE-PICKUP TRIGGER: it builds a
// Pickup WORK and posts it to the citizen work-scheduler, so a laborer comes and hauls the
// pile away. Slot [6] (+0x30) is the OnAdded hook that ComponentEntity_OnAddedPass dispatches
// when the entity is registered (flags 0x89 have bit 0 set), i.e. the moment a harvested pile
// joins the world via EntityList_RegisterEntity.
//
// Original logic (FUN_1401291d0):
//   1. CheckPtr on entity ref (+0x10)
//   2. FUN_140003050 on the owner entity — its WorkComponent (the post target)
//   3. IsDerivedFrom check on _unk18 for RawMaterialDescription
//   4. helper_9480 — the work kind/category code from the description
//   5. MemAlloc 0x60 bytes for the Pickup work
//   6. Work_Ctor25 (0x1400f72e0) to initialize the Work base
//   7. Set vtable to vtable_Pickup (0x140161810), zero fields at +0x48, +0x50, +0x58
//   8. InternalBasePtr_Ctor to wrap the work
//   9. CheckPtr on the WorkComponent ref
//  10. CitizenAI_impl_ec60 (0x14012ec60) — POST the work to the scheduler

void RawMaterialComponent::OnAdded() {
    ComponentEntity* entity = _entity;

    // The entity's WorkComponent (0x140003050)
    WorkComponent* workComp = entity->FindComponent<WorkComponent>();

    // Get RawMaterialDescription via IsDerivedFrom
    Description* baseObj = _description;
    RawMaterialDescription* rawMatDesc =
        Object::IsDerivedFromNullSafe(baseObj, RawMaterialDescription::TypeId())
            ? static_cast<RawMaterialDescription*>(baseObj) : nullptr;

    // A guard of the tree's (retail 0x1401291f7 calls 0x140129480 on the null).
    if (!rawMatDesc) return;

    // Get pickup category from material flags
    uint32_t category = rawMatDesc->GetPickupCategory();

    // Allocate pickup draw item (0x60 bytes)
    void* pickupItem = System::MemoryAllocator::MemAlloc(0x60, 8);

    Pickup* pickupItemResult = nullptr;
    if (pickupItem) {
        // Retail: `14012924b CALL 0x1400f72e0` (Work_Ctor25, the entity-seeded base
        // ctor), `LEA RAX,[0x140161810]; MOV [RBX],RAX` (the Pickup table), then
        // +0x48 = +0x50 = +0x58 = 0. B.3.8 (B8-5): the real Pickup ctor builds the same
        // bytes (the compiler stamps Pickup's table and zeroes the three fields), and
        // Work::InitFromEntity is Work_Ctor25's body.
        ComponentEntity* entityRef = entity;   // the InternalPtr copy retail hands the base ctor
        Pickup* pickup = new (pickupItem) Pickup();
        pickup->InitFromEntity(&entityRef);

        pickupItemResult = pickup;
    }

    // Wrap in InternalBasePtr
    Object::InternalPtr<Pickup> itemRef(pickupItemResult);

    // POST the Pickup work to the citizen work-scheduler — a laborer then claims it and
    // hauls this pile away. This is the pile-pickup trigger.
    // LEFT: CitizenAI_WorkScheduler_Post takes the WorkComponent as uintptr_t (COMP-b's declaration).
    CitizenAI_WorkScheduler_Post(reinterpret_cast<uintptr_t>(workComp), category, '\x01', &itemRef);
}

// [9] 0x140129130. PORT GAP (retail 0x140129130) — B38 D9: the body is ported
// above (RawMaterialComponent_Update, the pile expiry timer) but the tree's
// table never installed it; the inert stub is kept here. Installing it is its
// own titled commit.
void RawMaterialComponent::Update(void*) {}

// create 0x140037270: MemAlloc(0x40, 8), then the ctor.
void* RawMaterialComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(RawMaterialComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) RawMaterialComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* RawMaterialComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) RawMaterialComponent());
}

DEFINE_EXE_TYPE(RawMaterialComponent, "Component")
