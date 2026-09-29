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
static uint32_t EffTid_RawMaterialDescription() {
    return RawMaterialDescription::TypeId();
}

// ============================================================
// Helper: Get StorageDescription sibling component
// ============================================================
static void* GetStorageDescription(StorageComponent* self) {
    uint32_t tid = EffTid_StorageDescription();
    if (tid == 0) return nullptr;

    void* obj = self->_description;
    if (!obj) return nullptr;

    bool derived = Object::IsDerivedFromNullSafe(obj, tid);
    return derived ? obj : nullptr;
}



// ============================================================
// Helper: find the RawMaterialDescription on a material TEMPLATE entity
// (descriptions at +0x28 raw-pointer array / +0x30 count) — the inline
// scan shared by QueryCount / Helper_c560 in the original.
// ============================================================
static void* FindRawMatDescription(uintptr_t materialEntity) {
    uint32_t tid = EffTid_RawMaterialDescription();
    if (!materialEntity || tid == 0) return nullptr;
    int compCount = *(int*)(materialEntity + 0x30);
    uintptr_t compArray = *(uintptr_t*)(materialEntity + 0x28);
    for (int i = 0; i < compCount; i++) {
        void* comp = *(void**)(compArray + (uintptr_t)i * 8);
        uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
        if (typeId == tid) {
            // Original: IsDerivedFrom-narrow, then break either way.
            return Object::IsDerivedFromNullSafe(comp, tid) ? comp : nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// StorageComponent::QueryCount (0x14012cf10) — faithful reimpl
// ============================================================
// VOLUME-weighted total (verified vs Ghidra): for each storage slot
// (comp+0x28, stride 0x10: {materialEntity(8), count(4), reserved(4)}), find
// the material's RawMaterialDescription and accumulate
//   desc(+0x48 unit volume) * slot count(+0x08).
// The previous port summed raw counts (UNFAITHFUL — capacity +0x38 is in
// volume units, _volumeLimit * area). Standalone-inert guard: a slot whose
// material has no RawMaterialDescription contributes 0 (retail reads
// *(NULL+0x48)).

int StorageComponent_QueryCount(void* storageComp) {
    if (!storageComp) return 0;

    uintptr_t sc = (uintptr_t)storageComp;
    int slotCount = *(int*)(sc + 0x30);
    uintptr_t slotArray = *(uintptr_t*)(sc + 0x28);
    int total = 0;

    for (int i = 0; i < slotCount; i++) {
        uintptr_t slotEntry = slotArray + (uintptr_t)i * 0x10;
        void* matDesc = FindRawMatDescription(*(uintptr_t*)slotEntry);
        int unitVol = matDesc ? *(int*)((uintptr_t)matDesc + 0x48) : 0;
        total += unitVol * *(int*)(slotEntry + 0x08);
    }

    return total;
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

int StorageComponent_QueryAvailable(void* storageComp, int64_t materialEntity) {
    if (!storageComp) return 0;

    uintptr_t sc = (uintptr_t)storageComp;
    int slotCount = *(int*)(sc + 0x30);
    uintptr_t slotArray = *(uintptr_t*)(sc + 0x28);

    for (int i = 0; i < slotCount; i++) {
        uintptr_t row = slotArray + (uintptr_t)i * 0x10;
        if (*(int64_t*)row == materialEntity)
            return *(int*)(row + 0x08) - *(int*)(row + 0x0c);
    }
    return 0;
}

// ============================================================
// StorageComponent::Helper_c560 (0x14012c560) — null-safe reimpl
// ============================================================
// Checks if (param_1[0x3C] & RawMaterialDescription[0x5C]) != 0.
// Searches material type entity for RawMaterialDescription component,
// guards against NULL.

uint32_t StorageComponent_Helper_c560(void* param_1, void* param_2) {
    // Type-id via EffTid (GetTypeIdByName in the standalone exe).
    void* rawMatDesc = FindRawMatDescription((uintptr_t)param_2);

    if (!rawMatDesc) return 0;

    uint32_t flags1 = *(uint32_t*)((uintptr_t)param_1 + 0x3C);
    uint32_t flags2 = *(uint32_t*)((uintptr_t)rawMatDesc + 0x5C);
    return (flags1 & flags2) != 0 ? 1 : 0;
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
    : storageSlots(nullptr), slotCount(0), _pad34{}, field38(nullptr) {
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

void StorageComponent::Persist(void* stream) {
    // Call base Component_Serialize
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    // Create local IO::Serializer (version 1)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    // Serialize slot count
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);
    int slotCount = *(int*)(selfAddr + 0x30);
    int serSlotCount = slotCount;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(serSlotCount, nullptr);   // B.3.4 bridge

    // If loading (encoding direction == 0), resize the slot array
    int encodingDir = static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection());   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
    if (encodingDir == 0) {
        // B.3.2 bridge: FUN_14000f090 is Collection::Array<Bytes<0x10,8>>::SetSize(n)
        GameColl::AsArray<GameColl::Bytes<0x10, 8>>((void*)(selfAddr + 0x28))
            .SetSize(serSlotCount);
    }

    // Iterate storage slots (each 0x10 bytes: [ptr(8), int(4), int(4)])
    if (serSlotCount > 0) {
        uintptr_t slotBase = *(uintptr_t*)(selfAddr + 0x28);
        for (int i = 0; i < serSlotCount; i++) {
            uintptr_t slotEntry = slotBase + (uintptr_t)i * 0x10;
            // Serialize two int fields at slot+0x08 and slot+0x0C
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(slotEntry + 0x08), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(slotEntry + 0x0C), nullptr);   // B.3.4 bridge
            // Serialize External reference at slot+0x00
            Reimpl_ExternalPtr_Serialize_StringTable((void*)slotEntry, serializer);   // 0x140002340
        }
    }

    // Serialize field38 (int at self+0x38)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(selfAddr + 0x38), nullptr);   // B.3.4 bridge

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
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned int*>(selfAddr + 0x3c), nullptr);   // B.3.4 bridge  (0x98 = uint door)


    // Version 0 migration: if loading and version==0, copy from StorageDescription
    int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
    if (version == 0 && encodingDir == 0) {
        void* storageDesc = GetStorageDescription(this);
        if (storageDesc) {
            *(uint32_t*)(selfAddr + 0x3c) = *(uint32_t*)((uintptr_t)storageDesc + 0x28);
        }
    }

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
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
    void* storageDesc = GetStorageDescription(this);
    if (!storageDesc) return;

    uintptr_t descAddr = (uintptr_t)storageDesc;
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    // Check if storage is defined (byte at +0x21). NOTE: this early return leaves +0x38
    // (max volume capacity) at its ctor ZERO, and a zero capacity makes CanAdd refuse
    // everything and makes PickupStoredRawMaterial compute a requested count of 0 — which
    // deposits a {resType, 0} stack that hangs the spill loop. Report the exit, don't
    // assume it: an empty log line here would otherwise read as "capacity was set fine".
    if (*(uint8_t*)(descAddr + 0x21) == 0) {
        return;
    }

    // Compute tile multiplier
    int tileArea = 1;
    if (*(uint8_t*)(descAddr + 0x20) != 0) {
        // Find MapComponent on entity, get tile dimensions
        {
            uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);
            uintptr_t mapCompBuf[2];
            FloorComponent_FindResData((void*)entity, (void*)mapCompBuf);   // 0x140004680
            uintptr_t mapComp = mapCompBuf[0];
            if (mapComp) {
                int16_t w = *(int16_t*)(mapComp + 0x2c);
                int16_t h = *(int16_t*)(mapComp + 0x2e);
                tileArea = (int)w * (int)h;
            }
        }
    }

    // Read template data
    int maxPerSlot = *(int*)(descAddr + 0x24);
    uint32_t defaultType = *(uint32_t*)(descAddr + 0x28);

    // Resize slot array to 0 (clear existing)
    // B.3.2 bridge: FUN_14000f090 is Collection::Array<Bytes<0x10,8>>::SetSize(n)
    GameColl::AsArray<GameColl::Bytes<0x10, 8>>((void*)(selfAddr + 0x28)).SetSize(0);

    // Set computed values
    *(int*)(selfAddr + 0x38) = maxPerSlot * tileArea;
    *(uint32_t*)(selfAddr + 0x3c) = defaultType;

    // Iterate template storage entries
    uintptr_t templateArray = *(uintptr_t*)(descAddr + 0x30);
    int templateCount = *(int*)(descAddr + 0x38);

    if (templateCount > 0) {
        for (int i = 0; i < templateCount; i++) {
            uintptr_t entry = templateArray + (uintptr_t)i * 0x10;
            uintptr_t materialType = *(uintptr_t*)entry;
            int count = *(int*)(entry + 8);
            ActRm_StorageAdd_d000(reinterpret_cast<uintptr_t>(this), static_cast<int64_t>(materialType), count);   // 0x14012d000
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

// moved from engine_funcs.h (ENDGAME B.3.9 TR): reads X::TypeId() of the cell's class.
int fn_StorageComponent_QueryCount(void* storageComp) {
    uintptr_t sc = (uintptr_t)storageComp;
    int slotCount = *(int*)(sc + 0x30);
    int total = 0;
    uintptr_t itemArray = *(uintptr_t*)(sc + 0x28);
    for (int i = 0; i < slotCount; i++) {
        uintptr_t slotBase = itemArray + (uintptr_t)i * 0x10;
        uintptr_t item = *(uintptr_t*)slotBase;
        int itemQty = *(int*)(slotBase + 0x8);
        // Scan item's component array for ResourceType
        int compCount = *(int*)(item + 0x30);
        uintptr_t compArray = *(uintptr_t*)(item + 0x28);
        void* resourceType = nullptr;
        for (int j = 0; j < compCount; j++) {
            void* comp = *(void**)(compArray + (uintptr_t)j * 8);
            uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)
            if (typeId == RawMaterialDescription::TypeId()) {
                if (Object::IsDerivedFromNullSafe(comp, RawMaterialDescription::TypeId())) {
                    resourceType = comp;
                }
                break;
            }
        }
        if (resourceType) {
            int quantity = *(int*)((uintptr_t)resourceType + 0x48);
            total += quantity * itemQty;
        }
    }
    return total;
}
