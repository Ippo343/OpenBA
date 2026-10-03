// code/Game/Component/storagecomponent.cpp
// Clean reimplementation of StorageComponent lifecycle methods from Application-x64.exe.
//
// StorageComponent (0x40 bytes) manages storage slots for buildings.
// PERF HOTSPOT: QueryCount is O(n*m) per citizen per tick.
//
// Vtable at 0x140164720. Non-trivial overrides:
//   Shutdown  (0x14012c730) — versioned serialization of storage slot data
//   Draw      (0x14012c640) — rebuild storage slots from building template
//   QueryCount (0x14012cf10) — count items across all storage slots (44 xrefs)
//   Helper_c560 (0x14012c560) — check material flag intersection
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/storagecomponent_funcs.h"
#include "Component/RawMaterialDescription.h"
#include "Component/StorageDescription.h"
#include "Component/ComponentDescription.h"
#include "Component/MapComponent.h"
#include "Entity/ComponentEntity.h"
#include "Component/coll_bridge.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "system/MemoryAllocator.h"
#include "Object/factory_registration.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"   // ActRm_StorageAdd_d000

// Exe helpers, by retail address:
//   0x140002340 Reimpl_ExternalPtr_Serialize_StringTable (item type External ref)
//   0x14012d000 ActRm_StorageAdd_d000 (add storage slot entry)
//   0x140004680 FloorComponent_FindResData (MapComponent)
// IO::Serializer vtable slot +0x98 (field3c) is the member Serialize(unsigned int&).

// ============================================================
// Effective type-ids — cells 0x1401c7eb0 (StorageDescription) and 0x1401c7cb8
// (RawMaterialDescription) both fell back to Class::TypeId() unconditionally
// (D5 exception: mechanically equivalent).
// ============================================================
static uint32_t EffTid_StorageDescription() {
    return StorageDescription::TypeId();
}

// ============================================================
// Helper: Get StorageDescription sibling component
// ============================================================
static StorageDescription* GetStorageDescription(StorageComponent* self) {
    uint32_t tid = EffTid_StorageDescription();
    if (tid == 0) return nullptr;

    Description* obj = self->_description;
    if (!obj) return nullptr;

    bool derived = Object::IsDerivedFromNullSafe(obj, tid);
    return derived ? static_cast<StorageDescription*>(obj) : nullptr;
}

// ============================================================
// StorageComponent::QueryAvailable (0x14012ce70, "BuildRoad_helper_ce70")
// ============================================================
// Per-MATERIAL available count: scan the slot rows (comp+0x28, stride 0x10:
// {materialEntity(8), count(4), reserved(4)}) for the row whose material
// entity matches, return count - reserved; no row -> 0. Named BuildRoad_*
// in Ghidra because the road resource branch is its highest-profile caller
// (BuildRoad_Execute checks the citizen's HOME storage for the road
// material), but it is a plain StorageComponent accessor.
// Was never registered: the first stone road ever placed (the only work
// item with a non-null material at +0x50) called a null fn ptr and died
// at BuildRoad_Execute+0x7a7 (2026-07-26).

int StorageComponent::QueryAvailable(const ComponentDescription* material) const {
    for (int i = 0; i < slotCount; i++) {
        const Item& row = storageSlots[i];
        if (row._resource == material)
            return row._count - row._reserve;
    }
    return 0;
}

// B4 FORWARD: 1 outside caller (Work/buildroad.cpp). The null test is the tree's.
int StorageComponent_QueryAvailable(void* storageComp, int64_t materialEntity) {
    if (!storageComp) return 0;
    return reinterpret_cast<StorageComponent*>(storageComp)->QueryAvailable(
        reinterpret_cast<const ComponentDescription*>(materialEntity));
}

// ============================================================
// Reimpl of FUN_14000f090 (StorageComponent dynamic-array resize:
//   grow/shrink an array of 0x10-byte slot entries, plain memcpy of survivors)
// ============================================================
void StorageComponent_DynArrayResize(void* arrayBase, int newCount) {
    int64_t* field = (int64_t*)arrayBase;     // [0]=ptr, [1] low=count, high=capacity
    int oldCount = (int)field[1];
    if (newCount == oldCount) return;

    int64_t* oldBuf = (int64_t*)field[0];
    *(int*)((uintptr_t)field + 8) = newCount;
    if (*(int*)((uintptr_t)field + 0xc) < newCount) {
        *(int*)((uintptr_t)field + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *(int*)((uintptr_t)field + 0xc) = newCount * 3;
        }
        void* nb = System::MemoryAllocator::MemAlloc((uint64_t)((int64_t)*(int*)((uintptr_t)field + 0xc) << 4), 8);
        field[0] = (int64_t)nb;
    }

    int start = (oldBuf == (int64_t*)field[0]) ? oldCount : 0;
    if (start < newCount) {
        int64_t off = (int64_t)start * 0x10;
        for (int i = start; i < newCount; i++) {
            int64_t* dst = (int64_t*)(field[0] + off);
            if (dst) {
                if (i < oldCount) {
                    dst[0] = *(int64_t*)((uintptr_t)oldBuf + off);
                    dst[1] = *(int64_t*)((uintptr_t)oldBuf + off + 8);
                } else {
                    dst[0] = 0;
                    dst[1] = 0;
                }
            }
            off += 0x10;
        }
    }

    if (oldBuf != (int64_t*)field[0]) {
        if (oldBuf) System::MemoryAllocator::MemFree(oldBuf);
    }
}

// ============================================================================
// ENDGAME B.3.8 VIS — StorageComponent as a REAL class (StorageComponent.h).
//
// Before: a 48-slot StorageComponent_LiveVtable filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal).
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/StorageComponent.h"

// 0x14012c8d0: Component() is the CALL 0x1401188b0; the 0x140164720 stamp is
// the compiler's; +0x28 = +0x30 = 0; flags |= 0x81. (Retail leaves +0x38 to
// OnAdded / the build-complete init; the tree's zero is kept.)
StorageComponent::StorageComponent()
    : storageSlots(nullptr), slotCount(0), slotCapacity(0), capacity(0), rawMaterialFlags(0) {
    OrFlagsInCtor(0x81);
}

// ============================================================
// StorageComponent::Persist (vtable slot [3], 0x14012c730)
// ============================================================
// Versioned serialization (version 1):
//   - Serialize slot count
//   - If loading (version==0 path), resize slot array
//   - For each slot: serialize two ints (type fields) + External ref
//   - Serialize field38 (int) and field3c (via vtable call)
//   - Version 0 migration: copy StorageDescription+0x28 into field3c
//
// Original disassembly:
//   Component_Serialize(self, stream)
//   Serializer_Ctor(local, stream, 1)
//   Serialize_Int(&slotCount)
//   if loading: DynArrayResize(self+0x28, slotCount)
//   loop: Serialize_Int(slot+0x08), Serialize_Int(slot+0x0C), SerializeExternalRef(slot+0x00)
//   Serialize_Int(self+0x38)
//   vtable[+0x98](serializer, self+0x3C, nullptr)  -- serialize field 0x3C
//   if version==0 && loading: field3c = StorageDescription+0x28
//   Serializer_Dtor(local)

void StorageComponent::Persist(IO::Stream& stream) {
    // Call base Component_Serialize
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    IO::Serializer ser(stream, 1);

    // Serialize the slot count through a local
    int serSlotCount = slotCount;
    ser.Serialize(serSlotCount, nullptr);

    // If loading (encoding direction == 0), resize the slot array
    int encodingDir = static_cast<int>(ser.GetDirection());   // IO::Serializer::GetDirection, retail 0x180006210
    if (encodingDir == 0) {
        // B.3.2 bridge: FUN_14000f090 is Collection::Array<Item>::SetSize(n)
        // LEFT: the item array is the raw triple (H5).
        GameColl::AsArray<GameColl::Bytes<0x10, 8>>(&storageSlots).SetSize(serSlotCount);
    }

    // Iterate the items: _count, _reserve, then the ExternalPtr
    if (serSlotCount > 0) {
        Item* slotBase = storageSlots;
        for (int i = 0; i < serSlotCount; i++) {
            Item& slotEntry = slotBase[i];
            ser.Serialize(slotEntry._count, nullptr);
            ser.Serialize(slotEntry._reserve, nullptr);
            Reimpl_ExternalPtr_Serialize_StringTable(&slotEntry._resource, &ser);   // 0x140002340
        }
    }

    // Serialize the capacity (+0x38)
    ser.Serialize(capacity, nullptr);

    // Serialize field3c through the object at Serializer+0x08 — NOT the Stream at
    // Serializer+0x00. IO::Serializer::Serializer (Runtime 0x1800b9960) lays it out as
    // { Stream* @+0x00, ReaderWriter* @+0x08 }, and retail dispatches off the
    // READERWRITER (0x14012c7fc: MOV RCX,[RSP+0x30] = serializer+8; CALL [RAX+0x98]).
    // The comment below called that object the "Encoding"; it never was one —
    // IO::Encoding lives at stream+0x10 and has a five-slot table. +0x98 is
    // rw_vtbl::kScalarUInt on the ReaderWriter, i.e. IO::Serializer::Serialize(unsigned int&).
    //
    // ⚠ This read +0x00, so it called the STREAM's vtable slot 0x98 with this=Stream —
    // a different class, same slot index. Measured: the two slots resolve to different
    // functions, and the stream one writes EIGHT bytes at self+0x3c, running off the end
    // of a 0x40-byte StorageComponent and zeroing the boundary tag of the very next heap
    // block — which is this component's own slot array. That poisoned the allocator free
    // list on EVERY StorageComponent at load, and the game died much later inside
    // Allocate/UnlinkFreeTag (hence LOAD DONE, a healthy-looking run, then a crash with
    // no obvious connection). Probe evidence: tag@+0x40 0x0e -> 0, and
    // 0x00002000076e40be -> 0x0000200000000000 (low dword zeroed = an 8-byte write).
    //
    // Third arg mirrors retail's XOR R8D,R8D.
    ser.Serialize(rawMaterialFlags, nullptr);   // 0x98 = uint door


    // Version 0 migration: if loading and version==0, copy from StorageDescription
    int version = ser.GetVersion();
    if (version == 0 && encodingDir == 0) {
        StorageDescription* storageDesc = GetStorageDescription(this);
        if (storageDesc) {
            rawMaterialFlags = storageDesc->_rawMaterialFlags;
        }
    }
}
// [4] 0x14012c720 — the class's own `RET` body.
void StorageComponent::Startup() {}

// ============================================================
// StorageComponent::OnAdded (vtable slot [6], 0x14012c640)
// ============================================================
// Rebuilds storage slot descriptors from building template data.
//
// Original logic:
//   1. Get StorageDescription via IsDerivedFrom(self->_unk18, typeId)
//   2. If storageDesc[0x21] == 0, return (no storage defined)
//   3. If storageDesc[0x20] != 0, find MapComponent on entity,
//      compute tile area = map.width * map.height
//   4. Read maxPerSlot from storageDesc+0x24, defaultType from storageDesc+0x28
//   5. Resize slot array to 0
//   6. Set field38 = maxPerSlot * tileArea, field3c = defaultType
//   7. Iterate storageDesc template entries (at +0x30 ptr, +0x38 count),
//      each 0x10 bytes: [materialType(8), count(4), pad(4)]
//      Call StorageAddSlot(self, materialType, count) for each

void StorageComponent::OnAdded() {
    // Get StorageDescription
    StorageDescription* storageDesc = GetStorageDescription(this);
    if (!storageDesc) return;

    // Check if storage is defined (byte at +0x21). NOTE: this early return leaves +0x38
    // (max volume capacity) at its ctor ZERO, and a zero capacity makes CanAdd refuse
    // everything and makes PickupStoredRawMaterial compute a requested count of 0 — which
    // deposits a {resType, 0} stack that hangs the spill loop. Report the exit, don't
    // assume it: an empty log line here would otherwise read as "capacity was set fine".
    if (!storageDesc->_flagB) {
        return;
    }

    // Compute tile multiplier
    int tileArea = 1;
    if (storageDesc->_flagA) {
        // Find MapComponent on entity, get tile dimensions
        {
            MapComponent* mapComp = _entity->FindComponent<MapComponent>();   // 0x140004680
            if (mapComp) {
                int16_t w = mapComp->tileX1;
                int16_t h = mapComp->tileY1;
                tileArea = (int)w * (int)h;
            }
        }
    }

    // Read template data
    int maxPerSlot = storageDesc->_pickupValue;
    uint32_t defaultType = storageDesc->_rawMaterialFlags;

    // Resize slot array to 0 (clear existing)
    // B.3.2 bridge: FUN_14000f090 is Collection::Array<Item>::SetSize(n)
    // LEFT: the item array is the raw triple (H5).
    GameColl::AsArray<GameColl::Bytes<0x10, 8>>(&storageSlots).SetSize(0);

    // Set computed values
    capacity = maxPerSlot * tileArea;
    rawMaterialFlags = defaultType;

    // Iterate template storage entries
    StorageDescription::Volume* templateArray = storageDesc->_volumes;
    int templateCount = storageDesc->_volumeCount;

    if (templateCount > 0) {
        for (int i = 0; i < templateCount; i++) {
            const StorageDescription::Volume& entry = templateArray[i];
            // LEFT: ActRm_StorageAdd_d000 takes the storage and the item as integers (ACT-b's declaration).
            ActRm_StorageAdd_d000(reinterpret_cast<uintptr_t>(this), reinterpret_cast<int64_t>(entry._resource), entry._count);   // 0x14012d000
        }
    }
}

// create 0x140038b50: MemAlloc(0x40, 8), then the ctor.
void* StorageComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StorageComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StorageComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* StorageComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StorageComponent());
}

DEFINE_EXE_TYPE(StorageComponent, "Component")

// StorageComponent::QueryCount (0x14012cf10): the stored weight — every item's
// count times its RawMaterialDescription::_weight (+0x48).
// (moved from engine_funcs.h, ENDGAME B.3.9 TR)
int StorageComponent::QueryCount() const {
    int total = 0;
    for (int i = 0; i < slotCount; i++) {
        const Item& slot = storageSlots[i];
        const ComponentDescription* item = slot._resource;
        int itemQty = slot._count;
        // Scan the item's sub-descriptions for its RawMaterialDescription
        int compCount = item->_subDescCount;
        const RawMaterialDescription* resourceType = nullptr;
        for (int j = 0; j < compCount; j++) {
            Description* comp = item->_subDescs[j];
            uint32_t typeId = comp->Type();   // slot 2
            if (typeId == RawMaterialDescription::TypeId()) {
                if (Object::IsDerivedFromNullSafe(comp, RawMaterialDescription::TypeId())) {
                    resourceType = static_cast<const RawMaterialDescription*>(comp);
                }
                break;
            }
        }
        if (resourceType) {
            int quantity = resourceType->_weight;
            total += quantity * itemQty;
        }
    }
    return total;
}

// B4 FORWARD: outside callers in ~25 files (Action, Component, Dialog, Entity, Game, Work).
int fn_StorageComponent_QueryCount(void* storageComp) {
    return reinterpret_cast<StorageComponent*>(storageComp)->QueryCount();
}
