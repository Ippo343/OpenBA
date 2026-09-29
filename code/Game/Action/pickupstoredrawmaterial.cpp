// code/Game/Action/pickupstoredrawmaterial.cpp
// Clean C++ reimplementation of PickupStoredRawMaterial action methods.
// Source: Game/Action/PickupStoredRawMaterial.cpp (from profile build debug strings)
//
// PickupStoredRawMaterial -- 0x48 bytes. Picks up materials from a storage building.
// Inherits: Object::Internal -> Action (0x20) -> PickupStoredRawMaterial
// Factory: 0x140022d60, Vtable: 0x14015ff80
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = (base Action)
//   [1] +0x08 VectorDtor    = (base Action, unhookable)
//   [2] +0x10 BaseDtor      = (base Action, unhookable)
//   [3] +0x18 Serialize     = 0x1400ca0d0
//   [4] +0x20 AbstractImpl  = 0x1400c9d40 (complex storage query/transfer)
//   [5] +0x28 Execute       = 0x1400c4a90 (base Action, returns 0)
//   [6] +0x30 Enter         = 0x1400c4b90 (base Action, void nop)
//   [7] +0x38 Exit          = 0x1400c4ba0 (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400c9c70 (undo pickup on entity remove)
//   [9] +0x48 GetTarget     = 0x1400c9bf0
//
// Field layout (0x48 bytes):
//   +0x00-0x1F: Action base (vtable, Internal base, entity ptr, status byte)
//   +0x20: InternalBasePtr  targetBuilding  (building with storage to pick from)
//   +0x28: InternalBasePtr  pickupEntity    (entity doing the pickup, e.g. citizen)
//   +0x30: void*            materialList    (array of {ExternalPtr, int32_t count} pairs, 0x10 each)
//   +0x38: int32_t          materialCount   (number of entries in material list)
//   +0x40: bool             partialAllowed  (if true, can pick up less than requested)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "PickupStoredRawMaterial.h"
#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"   // ActRm_* helpers
#include "Game/exe_bodies.h"     // Reimpl_ExternalPtr_Serialize_StringTable
#include "Work/exe_bodies.h"     // WorkBase_FindComponent_6270, WorkLeaf_FreeStorageCapacity_116540
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/CitizenComponent.h"
#include "Component/RawMaterialDescription.h"
#include "Component/StorageComponent.h"
#include "Entity/ComponentEntity.h"

// ============================================================
// Helpers, called by name (B3R)
//   0x14012d150 ActRm_StorageSubtract_d150 -- StorageComponent::SubtractItem(storage, itemType,
//               count); if the count reaches 0, removes the slot
//   0x14012d000 ActRm_StorageAdd_d000      -- StorageComponent::AddItem(storage, itemType, count)
//   0x14000da70 ActRm_ArrayResize16_da70   -- array resize for 16-byte elements (material list)
//   0x140002340 Reimpl_ExternalPtr_Serialize_StringTable -- ExternalPtr serialize
// ============================================================

// ============================================================
// Helper: FindComponent by type ID on an entity's component array
// Generic pattern: entity +0x30 = component IBP array, +0x38 = count
// Uses vtable+0x10 (GetTypeId) and IsDerivedFrom to match.
// ============================================================
// Generic FindComponent: arrayOffset/countOffset differ by object type.
// ComponentEntity: array=+0x30, count=+0x38
// External description objects (e.g. material types): array=+0x28, count=+0x30
static void* FindComponentByTypeId(void* entity, uint32_t wantTypeId,
                                   int arrayOffset, int countOffset) {
    uintptr_t ent = (uintptr_t)entity;
    int compCount = *(int*)(ent + countOffset);
    uintptr_t compArray = *(uintptr_t*)(ent + arrayOffset);

    for (int i = 0; i < compCount; i++) {
        void** compIBP = (void**)(compArray + (uintptr_t)i * 8);

        void* comp = *compIBP;
        // slot 2 Type()
        uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge

        if (typeId == wantTypeId) {
            void* found = *compIBP;
            if (::Object::IsDerivedFromNullSafe(found, wantTypeId)) {
                return found;
            }
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// PickupStoredRawMaterial::Serialize (0x1400ca0d0) -- slot 3
// Serializes: status(+0x18), building(+0x20), entity(+0x28),
//             material count(+0x38), material list(+0x30), partial flag(+0x40)
// ============================================================
void PickupStoredRawMaterial::Persist(void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status byte (+0x18)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize entity refs, material list, and flags
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize target building (+0x20) with type check
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x20)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field20 != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(_field20, CitizenComponent::TypeId())) {
            _field20 = nullptr;
        }
    }

    // Serialize pickup entity (+0x28) with ComponentEntity type check
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x28)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field28 != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(_field28, ComponentEntity::TypeId())) {
            _field28 = nullptr;
        }
    }

    // Serialize material count (+0x38)
    uint32_t matCount = *(uint32_t*)(s + 0x38);
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(&matCount), nullptr);   // B.3.4 bridge

    // If loading (version check: ser2 internal stream offset+8 == 0), resize material array
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(ser2)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        ActRm_ArrayResize16_da70(reinterpret_cast<int64_t*>(s + 0x30), (int)matCount);   // 0x14000da70
    }

    // Serialize each material entry (ExternalPtr + int count, 0x10 bytes each)
    uintptr_t* matList = *(uintptr_t**)(s + 0x30);
    if ((int)matCount > 0) {
        uint64_t remaining = (uint64_t)matCount;
        do {
            // Serialize ExternalPtr (material type)
            Reimpl_ExternalPtr_Serialize_StringTable(matList, ser2);   // 0x140002340
            // Serialize count (int at +0x08)
            reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(((uintptr_t)matList + 8)), nullptr);   // B.3.4 bridge
            matList = (uintptr_t*)((uintptr_t)matList + 0x10);
            remaining--;
        } while (remaining != 0);
    }

    // Serialize partial pickup flag (+0x40)
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<bool*>(s + 0x40), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// PickupStoredRawMaterial::AbstractImpl (0x1400c9d40) -- slot 4
// Complex storage query and transfer logic:
// For each material in the list (+0x30):
//   1. Find RawMaterialDescription on material to get quantity per unit
//   2. If requested count <= 0, compute from storage capacity
//   3. Check available quantity in storage
//   4. If partial not allowed and insufficient: return 2 (fail)
//   5. If partial allowed, clamp to available; if 0: return 2
//   6. Subtract reserved amount from storage
//   7. Add to citizen's parent's storage via SubtractItem + AddItem
// Returns 0 on success, 2 on insufficient materials.
// ============================================================
int  PickupStoredRawMaterial::AbstractImpl(void* actionStack) {
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);


    // Find storage component on pickup entity (+0x28)
    uintptr_t storageIBP = 0;
    fn_FindComponent_Storage(_field28, &storageIBP);

    int matIdx = 0;
    if (matIdx >= *(int*)(s + 0x38)) {
        return 0;
    }

    int64_t matOffset = 0;
    do {
        uintptr_t matList = *(uintptr_t*)(s + 0x30);
        int64_t materialType = *(int64_t*)(matList + matOffset);

        // Find RawMaterialDescription component on the material type
        // Material type is an External description object: components at +0x28/+0x30
        void* rawMatDesc = FindComponentByTypeId((void*)materialType, RawMaterialDescription::TypeId(), 0x28, 0x30);

        // Get requested count from material list (+0x08 in each entry)
        int requestedCount = *(int*)(matList + matOffset + 8);

        if (requestedCount < 1) {
            // Count not specified: compute from storage capacity
            void* targetBuilding = _field20;
            void* parentEntity = *(void**)((uintptr_t)targetBuilding + 0x10);

            // Find StorageComponent on parent entity
            void* parentStorage = FindComponentByTypeId(parentEntity, StorageComponent::TypeId(), 0x30, 0x38);

            // Create IBP for parent storage, get max capacity and current count
            uintptr_t parentStorageIBP = 0;
            new (&parentStorageIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(parentStorage));   // B.3.3 bridge
            int maxCapacity = *(int*)(parentStorageIBP + 0x38);
            int currentTotal = fn_StorageComponent_QueryCount((void*)parentStorageIBP);

            // Compute how many units we can pick up based on remaining capacity
            int quantityPerUnit = rawMatDesc ? *(int*)((uintptr_t)rawMatDesc + 0x48) : 1;
            requestedCount = (maxCapacity - currentTotal) / quantityPerUnit;
        }

        // Look up available quantity of this material in citizen's storage
        uintptr_t matList2 = *(uintptr_t*)(s + 0x30);
        int availableCount = 0;
        int slotCount = *(int*)(storageIBP + 0x30);
        if (slotCount > 0) {
            int64_t* itemArray = *(int64_t**)(storageIBP + 0x28);
            for (int i = 0; i < slotCount; i++) {
                if (itemArray[i * 2] == *(int64_t*)(matList2 + matOffset)) {
                    if (i >= 0) {
                        availableCount = (int)itemArray[i * 2 + 1];
                    }
                    break;
                }
            }
        }

        // Check availability
        bool partialAllowed = _field40 != 0;
        if (!partialAllowed) {
            if (availableCount < requestedCount) {
                return 2;  // Insufficient, fail
            }
        } else {
            if (availableCount < requestedCount) {
                requestedCount = availableCount;
            }
            if (requestedCount == 0) {
                return 2;  // Nothing to pick up
            }
        }

        // Subtract reserved amount from citizen's storage
        uintptr_t matList3 = *(uintptr_t*)(s + 0x30);
        slotCount = *(int*)(storageIBP + 0x30);
        if (slotCount > 0) {
            int64_t* itemArray = *(int64_t**)(storageIBP + 0x28);
            for (int i = 0; i < slotCount; i++) {
                if (itemArray[i * 2] == *(int64_t*)(matList3 + matOffset)) {
                    if (i >= 0) {
                        int* reservedPtr = (int*)((uintptr_t)itemArray + (uintptr_t)i * 0x10 + 0xc);
                        *reservedPtr -= requestedCount;
                        if (*(int*)((uintptr_t)itemArray + 0xc + (uintptr_t)i * 0x10) < 0) {
                            *(int*)((uintptr_t)itemArray + 0xc + (uintptr_t)i * 0x10) = 0;
                        }
                    }
                    break;
                }
            }
        }

        // Subtract from citizen's storage via SubtractItem
        uintptr_t matList4 = *(uintptr_t*)(s + 0x30);
        ActRm_StorageSubtract_d150(storageIBP, *(int64_t*)(matList4 + (int64_t)matIdx * 0x10), requestedCount);   // 0x14012d150

        // Add to target building's parent storage
        uintptr_t matList5 = *(uintptr_t*)(s + 0x30);
        void* targetBuilding = _field20;
        int64_t itemType = *(int64_t*)(matList5 + matOffset);
        void* parentEntity = *(void**)((uintptr_t)targetBuilding + 0x10);

        // Find StorageComponent on parent entity
        void* parentStorage = FindComponentByTypeId(parentEntity, StorageComponent::TypeId(), 0x30, 0x38);

        uintptr_t parentStorageIBP2[2];
        new (parentStorageIBP2) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(parentStorage));   // B.3.3 bridge
        ActRm_StorageAdd_d000(parentStorageIBP2[0], itemType, requestedCount);   // 0x14012d000

        matOffset += 0x10;
        matIdx++;
    } while (matIdx < *(int*)(s + 0x38));

    return 0;
}

// ============================================================
// PickupStoredRawMaterial::CheckEntity (0x1400c9c70) -- slot 8
// Undoes pickup when entity is removed.
// Iterates material list, subtracts reserved amounts from citizen's storage.
// ============================================================
void PickupStoredRawMaterial::CheckEntity() {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    // Find storage component on pickup entity (+0x28)
    uintptr_t storageIBP = 0;
    fn_FindComponent_Storage(_field28, &storageIBP);

    int matCount = *(int*)(s + 0x38);
    if (matCount <= 0) return;

    int64_t matOffset = 0;
    for (int i = 0; i < matCount; i++) {
        uintptr_t matList = *(uintptr_t*)(s + 0x30);

        int slotCount = *(int*)(storageIBP + 0x30);
        if (slotCount > 0) {
            int64_t* itemArray = *(int64_t**)(storageIBP + 0x28);
            for (int j = 0; j < slotCount; j++) {
                if (itemArray[j * 2] == *(int64_t*)(matList + matOffset)) {
                    if (j >= 0) {
                        // Subtract the requested count from the reserved field (+0xc)
                        int* reservedPtr = (int*)((uintptr_t)itemArray + (uintptr_t)j * 0x10 + 0xc);
                        *reservedPtr -= *(int*)(matList + matOffset + 8);
                        if (*(int*)((uintptr_t)itemArray + 0xc + (uintptr_t)j * 0x10) < 0) {
                            *(int*)((uintptr_t)itemArray + 0xc + (uintptr_t)j * 0x10) = 0;
                        }
                    }
                    break;
                }
            }
        }

        matOffset += 0x10;
    }
}

// ============================================================
// PickupStoredRawMaterial::GetTarget (0x1400c9bf0) -- slot 9
// Returns true if param matches entity at +0x28, or
// matches the parent entity (+0x10) of the building at +0x20.
// ============================================================
bool PickupStoredRawMaterial::GetTarget(void* param) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    // Check match with pickup entity at +0x28
    uintptr_t entityVal = *(uintptr_t*)(s + 0x28);
    if (*(uintptr_t*)param == entityVal) {
        return true;
    }

    // Check match with +0x20's parent (+0x10)
    uintptr_t building = *(uintptr_t*)(s + 0x20);
    uintptr_t parentEntity = *(uintptr_t*)(building + 0x10);
    return parentEntity == *(uintptr_t*)param;
}

// ============================================================
// class PickupStoredRawMaterial (PickupStoredRawMaterial.h) — ENDGAME B.3.8 ACT.
// The hand-built g_PickupStoredRawMaterial_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"

// create 0x140022d60 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
PickupStoredRawMaterial::PickupStoredRawMaterial()
    : _field20(nullptr),
      _field28(nullptr),
      _field30(nullptr),
      _field38(nullptr),
      _field40(0),
      _pad41{} {}

// The dtor body of 0x140022dd0, between its own stamp and the Action stamp:
//   MOV RCX,[this+0x30]; CALL MemFree (IAT 0x140152b68, unconditional);
//   [this+0x30] = 0; [this+0x38] = 0 (the qword: count +0x38 and capacity +0x3c).
// The tree's shared Action_ScalarDtor skipped it and leaked the carrier list.
PickupStoredRawMaterial::~PickupStoredRawMaterial() {
    Object::detail::InternalMemFree(_field30);
    _field30 = nullptr;
    _field38 = nullptr;
}

// [3] 0x1400ca0d0
// [4] 0x1400c9d40
// [8] 0x1400c9c70
// [9] 0x1400c9bf0

void* PickupStoredRawMaterial::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PickupStoredRawMaterial), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PickupStoredRawMaterial());
}
void* PickupStoredRawMaterial::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PickupStoredRawMaterial());
}


// ============================================================
// FUN_1400ca230 — Resource_TransferToCarrier
// Add `count` of resource `resType` to the carrier list at self+0x30 (count +0x38,
// cap +0x3c, stride 0x10: [+0]=resType u64, [+8]=count int, [+0xc]=reserved int),
// and bump the matching storage-inventory reserved field (+0xc) by `count`.
//   self = the PickupStored action; resType = resource type id; count = amount.
// ============================================================
void ActChild_TransferToCarrier_ca230(int64_t self, int64_t resType, int count) {
    int64_t storageIbp = 0;
    WorkBase_FindComponent_6270(*reinterpret_cast<uintptr_t*>(self + 0x28), &storageIbp);

    // bump storage inventory reserved(+0xc) for the matching resType
    int invCount = *reinterpret_cast<int*>(storageIbp + 0x30);
    if (0 < invCount) {
        int64_t* p = *reinterpret_cast<int64_t**>(storageIbp + 0x28);
        int idx = 0;
        while (true) {
            if (*p == resType) {
                if (idx >= 0) {
                    int64_t base = *reinterpret_cast<int64_t*>(storageIbp + 0x28);
                    *reinterpret_cast<int*>(base + (int64_t)idx * 0x10 + 0xc) += count;
                }
                break;
            }
            idx++; p += 2;
            if (idx >= invCount) break;
        }
    }

    // carrier-list find: if resType already present, add to its count(+8) and return
    int carCount = *reinterpret_cast<int*>(self + 0x38);
    if (0 < carCount) {
        int64_t* p = *reinterpret_cast<int64_t**>(self + 0x30);
        int idx = 0;
        while (true) {
            if (*p == resType) {
                int64_t base = *reinterpret_cast<int64_t*>(self + 0x30);
                *reinterpret_cast<int*>(base + (int64_t)idx * 0x10 + 8) += count;
                return;
            }
            idx++; p += 2;
            if (idx >= carCount) break;
        }
    }

    // not found: append {resType, count, reserved=0} to the carrier list (grow-if-full)
    uint32_t oldCount = *reinterpret_cast<uint32_t*>(self + 0x38);
    uint32_t newCount = oldCount + 1;
    if (newCount == oldCount) return;
    int64_t newEntry[2];
    newEntry[0] = resType;
    reinterpret_cast<int*>(&newEntry[1])[0] = count;
    reinterpret_cast<int*>(&newEntry[1])[1] = 0;
    void* oldArr = *reinterpret_cast<void**>(self + 0x30);
    *reinterpret_cast<uint32_t*>(self + 0x38) = newCount;
    if (*reinterpret_cast<int*>(self + 0x3c) < (int)newCount) {
        *reinterpret_cast<uint32_t*>(self + 0x3c) = newCount;
        if (oldCount != 0) *reinterpret_cast<uint32_t*>(self + 0x3c) = newCount * 3;
        void* na = System::MemoryAllocator::_singleton
            ? System::MemoryAllocator::MemAlloc(
                          (uint64_t)((int64_t)*reinterpret_cast<int*>(self + 0x3c) << 4), 8)
            : nullptr;
        *reinterpret_cast<void**>(self + 0x30) = na;
    }
    int copyStart = 0;
    if (oldArr == *reinterpret_cast<void**>(self + 0x30)) copyStart = (int)oldCount;
    int64_t i = copyStart;
    if (i < (int)newCount) {
        int64_t byteOff = i << 4;
        do {
            int64_t* dst = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(self + 0x30) + byteOff);
            if (dst != nullptr) {
                int64_t* src = (i < (int)oldCount)
                    ? reinterpret_cast<int64_t*>((int64_t)i * 0x10 + reinterpret_cast<int64_t>(oldArr))
                    : newEntry;
                dst[0] = src[0];
                dst[1] = src[1];
            }
            i++;
            byteOff += 0x10;
        } while (i < (int)newCount);
    }
    if (oldArr != *reinterpret_cast<void**>(self + 0x30) ) System::MemoryAllocator::MemFree(oldArr);
}

// ============================================================
// FUN_1400ca520 — PickupStoredRawMaterial_Action_CopyCtor
//   RCX=self, RDX=param_2, R8=param_3, R9=param_4(resType u64),
//   [stack]=param_5(int requested), param_6(byte @ +0x40).
// Field-init then, if param_4!=0: locate the source entity's RawMaterialDescription
// (component-array scan vs type-id 0x1401c7cb8) to get its per-unit size at +0x48;
// if requested(param_5)<=0 compute it from FUN_140116540(self+0x20)/perUnitSize;
// clamp to the storage availability (count-reserved) for resType; then transfer.
// ============================================================
void* ActChild_PickupStoredCopyCtor_ca520(void* self, uint64_t* param_2, uint64_t* param_3,
                                          int64_t param_4, int param_5, uint8_t param_6) {
    // ENDGAME B.3.8 ACT: the real ctor (PickupStoredRawMaterial.h). A by-address door.
    return static_cast<Object::Base*>(
        new (self) PickupStoredRawMaterial(param_2, param_3, param_4, param_5, param_6));
}

// 0x1400ca520 — the ctor body, moved from the door above: the field init is the
// initialiser list; everything after it is the retail body verbatim.
PickupStoredRawMaterial::PickupStoredRawMaterial(uint64_t* param_2, uint64_t* param_3,
                                                 int64_t param_4, int param_5, uint8_t param_6)
    : _field20(reinterpret_cast<void*>(*param_2)),
      _field28(reinterpret_cast<void*>(*param_3)),
      _field30(nullptr),
      _field38(nullptr),
      _field40(param_6) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    int64_t storageIbp = 0;
    WorkBase_FindComponent_6270(*reinterpret_cast<uintptr_t*>(s + 0x28), &storageIbp);

    if (param_4 == 0) return;

    uint32_t wantTid = RawMaterialDescription::TypeId();

    // component-array scan of param_4 for child GetTypeId == wantTid, IsDerivedFrom-confirm
    void* rawMatDesc = nullptr;
    int srcCount = *reinterpret_cast<int*>(param_4 + 0x30);
    if (0 < srcCount) {
        int idx = 0;
        int64_t byteOff = 0;
        while (true) {
            int64_t arr = *reinterpret_cast<int64_t*>(param_4 + 0x28);
            void* child = *reinterpret_cast<void**>(arr + byteOff);
            uint32_t tid = reinterpret_cast<Object::Base*>(child)->Type();   // B.3.x bridge (slot 2)
            if (tid == wantTid) {
                int64_t arr2 = *reinterpret_cast<int64_t*>(param_4 + 0x28);
                void* match = *reinterpret_cast<void**>(arr2 + (int64_t)idx * 8);
                if (::Object::IsDerivedFromNullSafe(match, RawMaterialDescription::TypeId()))
                    rawMatDesc = match;
                break;
            }
            idx++; byteOff += 8;
            if (idx >= srcCount) break;
        }
    }

    int requested = param_5;
    if (requested <= 0) {
        int total = WorkLeaf_FreeStorageCapacity_116540(*reinterpret_cast<intptr_t*>(s + 0x20));   // 0x140116540
        // perUnitSize = *(int*)(rawMatDesc+0x48); guard the null-find (original assumes found).
        int perUnit = rawMatDesc ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawMatDesc) + 0x48) : 0;
        requested = (perUnit != 0) ? total / perUnit : 0;
    }

    // storage availability for resType param_4 = count(+8) - reserved(+0xc); clamp requested down to it
    int avail = 0;
    {
        int invCount = *reinterpret_cast<int*>(storageIbp + 0x30);
        if (0 < invCount) {
            int64_t* inv = *reinterpret_cast<int64_t**>(storageIbp + 0x28);
            int idx = 0; int64_t* p = inv;
            while (true) {
                if (*p == param_4) {
                    if (idx >= 0)
                        avail = reinterpret_cast<int*>(inv)[(int64_t)idx * 4 + 2]
                              - reinterpret_cast<int*>(reinterpret_cast<int64_t>(inv) + (int64_t)idx * 0x10 + 0xc)[0];
                    break;
                }
                idx++; p += 2;
                if (idx >= invCount) break;
            }
        }
    }
    if (avail < requested) {
        // re-scan to set requested = avail (count - reserved)
        requested = 0;
        int invCount = *reinterpret_cast<int*>(storageIbp + 0x30);
        if (0 < invCount) {
            int64_t* inv = *reinterpret_cast<int64_t**>(storageIbp + 0x28);
            int idx = 0; int64_t* p = inv;
            while (true) {
                if (*p == param_4) {
                    if (idx >= 0)
                        requested = reinterpret_cast<int*>(inv)[(int64_t)idx * 4 + 2]
                                  - reinterpret_cast<int*>(reinterpret_cast<int64_t>(inv) + (int64_t)idx * 0x10 + 0xc)[0];
                    break;
                }
                idx++; p += 2;
                if (idx >= invCount) break;
            }
        }
    }

    ActChild_TransferToCarrier_ca230(reinterpret_cast<int64_t>(this), param_4, requested);
}

DEFINE_EXE_TYPE(PickupStoredRawMaterial, "Action")
