// code/Game/Action/pickuprawmaterial.cpp
// Clean C++ reimplementation of PickupRawMaterial action methods.
// Source: Game/Action/PickupRawMaterial.cpp (from profile build debug strings)
//
// PickupRawMaterial -- 0x30 bytes. Picks up a raw material from a source entity.
// Inherits: Object::Internal -> Action (0x20) -> PickupRawMaterial
// Factory: 0x140022c60, Vtable: 0x14015ff10
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = (base Action)
//   [1] +0x08 VectorDtor    = (base Action, unhookable)
//   [2] +0x10 BaseDtor      = (base Action, unhookable)
//   [3] +0x18 Serialize     = 0x1400c99e0
//   [4] +0x20 AbstractImpl  = 0x1400c98d0 (picks up raw material)
//   [5] +0x28 Execute       = 0x1400c4a90 (base Action, returns 0)
//   [6] +0x30 Enter         = 0x1400c4b90 (base Action, void nop)
//   [7] +0x38 Exit          = 0x1400c4ba0 (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400c4a90 (base Action)
//   [9] +0x48 GetTarget     = 0x1400c9860
//
// Field layout (0x30 bytes):
//   +0x00-0x1F: Action base (vtable, Internal base, entity ptr, status byte)
//   +0x20: InternalBasePtr  sourceBuilding  (building to pick up from)
//   +0x28: InternalBasePtr  sourceEntity    (entity carrying the raw material)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "PickupRawMaterial.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Action/exe_bodies.h"   // ActRm_*, ActHunt_* helpers
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include "Component/CitizenComponent.h"
#include "Entity/ComponentEntity.h"
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// ============================================================
// Helpers, called by name (B3R)
//   0x1400223c0 ActRm_FindComponent_223c0 -- FindComponent for the RawMaterial component
//               (type ID DAT_1401c7c80): (entity, retval_IBP), returns the IBP
//   0x14012d000 ActRm_StorageAdd_d000 -- StorageComponent::AddItem(storage, itemType, count)
//   0x1400c2ea0 ActHunt_MarkEntityRemoval_c2ea0 -- entity flag set (sets bit 0x400 on entity
//               +0x40 flags, optionally exits components)
//
// FUN_140003110 (DynArrayPush 8-byte elements, used to add the entity to the game
// world list) is the exe-side Collection::Array<Ptr8>::SetSize(n,fill) mono-
// morphisation -- ENDGAME B.3.2 step 2 converted its one call site below to a
// GameColl::AsArray<GameColl::Ptr8> member call, so its resolved-pointer stand-in
// is gone.
// ============================================================

// ============================================================
// PickupRawMaterial::Serialize (0x1400c99e0)
// Serializes: status(+0x18), entity(+0x20) with type check DAT_1401c7158,
//             entity(+0x28) with type check ComponentEntity
// ============================================================
void PickupRawMaterial::Persist(void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status byte (+0x18)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize entity refs
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize source building (+0x20) with type check
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x20)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field20 != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(_field20, CitizenComponent::TypeId())) {
            _field20 = nullptr;
        }
    }

    // Serialize source entity (+0x28) with ComponentEntity type check
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x28)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field28 != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(_field28, ComponentEntity::TypeId())) {
            _field28 = nullptr;
        }
    }

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// PickupRawMaterial::AbstractImpl (0x1400c98d0) -- slot 4
// Picks up raw material from source entity (+0x28):
// 1. Finds RawMaterial description component on source entity's component array
// 2. Gets quantity (+0x28 on that component) and item type
// 3. Finds storage on source building (+0x20) -> parent (+0x10)
// 4. Adds item to storage via FUN_14012d000
// 5. Checks entity flags, marks entity for removal if needed
// Returns 0 (action complete).
// ============================================================
int  PickupRawMaterial::AbstractImpl(void* actionStack) {
    (void)actionStack;

    // Get source entity at +0x28
    void* sourceEntity = _field28;

    // Find RawMaterial description component on source entity via FUN_1400223c0
    // This is a FindComponent variant using DAT_1401c7c80 type ID
    uint8_t resultIBP[16]; // InternalBasePtr result
    ActRm_FindComponent_223c0((uintptr_t)sourceEntity, resultIBP);   // 0x1400223c0

    // CheckPtr on the result to resolve lazy pointer

    // Read quantity from the found component: *(int*)(component + 0x28)
    void* rawMatDesc = *(void**)resultIBP;
    int quantity = *(int*)((uintptr_t)rawMatDesc + 0x28);

    // Read raw material type from source entity (+0x28 field = ExternalPtr)
    int64_t itemType = *(int64_t*)((uintptr_t)(_field28) + 0x28);

    // Navigate to source building (+0x20) -> parent entity (+0x10)
    void* building = _field20;
    void* parentEntity = *(void**)((uintptr_t)building + 0x10);

    // Find StorageComponent on parent entity
    uintptr_t storageIBP = 0;
    fn_FindComponent_Storage((void*)parentEntity, &storageIBP);

    // CheckPtr + add item to storage
    ActRm_StorageAdd_d000(storageIBP, itemType, quantity);   // 0x14012d000

    // Get source entity again, check if it should be flagged for removal
    void* entityRef = _field28;
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t worldList = *(uintptr_t*)(gameWorld + 0xe8);

    if (entityRef != nullptr) {
        // Store entity ptr for flag check
        uintptr_t localEntity = (uintptr_t)entityRef;

        // Check bit 10 of flags at +0x40
        uint32_t flags = *(uint32_t*)(localEntity + 0x40);
        if (((flags >> 10) & 1) == 0) {
            // Entity not yet flagged -- call entity flag set and add to world removal list
            ActHunt_MarkEntityRemoval_c2ea0((void*)localEntity);   // 0x1400c2ea0

            // Push entity ref to world list at worldList + 0xd0.
            // B.3.2 bridge: FUN_140003110 == Collection::Array<Ptr8>::SetSize(n,fill)
            // (3-arg form; ActHunt_DynArraySetCount_3110 in action_base_hunt.cpp).
            int currentCount = *(int*)(worldList + 0xd8);
            GameColl::AsArray<GameColl::Ptr8>((void*)(worldList + 0xd0))
                .SetSize(currentCount + 1,
                         *reinterpret_cast<const GameColl::Ptr8*>(&localEntity));
        }
    }

    return 0;
}

// ============================================================
// PickupRawMaterial::GetTarget (0x1400c9860) -- slot 9
// Returns true if param matches entity at +0x28, or
// matches the parent entity (+0x10) of the building at +0x20.
// ============================================================
bool PickupRawMaterial::GetTarget(void* param) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    // Check direct match with +0x28
    uintptr_t paramVal = *(uintptr_t*)param;

    if (*(uintptr_t*)(s + 0x28) == paramVal) {
        return true;
    }

    // Check match with +0x20's parent (+0x10)
    uintptr_t building = *(uintptr_t*)(s + 0x20);
    uintptr_t parentEntity = *(uintptr_t*)(building + 0x10);
    return parentEntity == *(uintptr_t*)param;
}

// ============================================================
// class PickupRawMaterial (PickupRawMaterial.h) — ENDGAME B.3.8 ACT.
// The hand-built g_PickupRawMaterial_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x140022c60 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
PickupRawMaterial::PickupRawMaterial()
    : _field20(nullptr),
      _field28(nullptr) {}

// [3] 0x1400c99e0
// [4] 0x1400c98d0
// [9] 0x1400c9860

void* PickupRawMaterial::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PickupRawMaterial), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PickupRawMaterial());
}
void* PickupRawMaterial::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PickupRawMaterial());
}


DEFINE_EXE_TYPE(PickupRawMaterial, "Action")
