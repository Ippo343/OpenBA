// code/Game/Action/consumerawmaterial.cpp
// Clean C++ reimplementation of ConsumeRawMaterial action methods.
// Source: Game/Action/ConsumeRawMaterial.cpp (from profile build debug strings)
//
// ConsumeRawMaterial -- 0x40 bytes. Consumes raw materials from entity's inventory.
// Inherits: Object::Internal -> Action (0x20) -> ConsumeRawMaterial
// Factory: 0x140022090, Vtable: 0x14015fc20
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = (base Action)
//   [1] +0x08 VectorDtor    = (base Action, unhookable)
//   [2] +0x10 BaseDtor      = (base Action, unhookable)
//   [3] +0x18 Serialize     = 0x1400c6120 (versioned, v0 legacy + v2 consume flag)
//   [4] +0x20 AbstractImpl  = 0x1400c5f50 (consume or check materials)
//   [5] +0x28 Execute       = 0x1400c4a90 (base Action, returns 0)
//   [6] +0x30 Enter         = 0x1400c4b90 (base Action, void nop)
//   [7] +0x38 Exit          = 0x1400c4ba0 (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400c5e30 (undo consumption if consume mode)
//   [9] +0x48 GetTarget     = 0x1400c5f10 (simple compare with +0x20)
//
// Field layout (0x40 bytes):
//   +0x00-0x1F: Action base (vtable, Internal base, entity ptr, status byte)
//   +0x20: InternalBasePtr  targetEntity    (entity whose storage to consume from)
//   +0x28: void*            materialList    (array of {ExternalPtr, int32_t count} pairs, 0x10 each)
//   +0x30: int32_t          materialCount   (number of entries in material list)
//   +0x38: bool             consumeMode     (true = subtract, false = just check availability)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "ConsumeRawMaterial.h"
#include "object/TypeInfo.h"
#include "Component/StorageComponent.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Action/exe_bodies.h"   // ActRm_* helpers
#include "Game/exe_bodies.h"     // Reimpl_ExternalPtr_Serialize_StringTable
#include "Entity/CitizenBehavior.h"   // professionResources (the +0x170 per-category counters)
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Entity/ComponentEntity.h"

// ============================================================
// Helpers, called by name (B3R)
//   0x14012d150 ActRm_StorageSubtract_d150      -- StorageComponent::SubtractItem(storage, itemType, count)
//   0x140088f90 ActRm_GetResourceCategory_88f90 -- GetResourceCategoryIndex(storageComp, materialExternalPtr)
//               -> int; the resource category index for the material, or -1 if not found
//   0x14000da70 ActRm_ArrayResize16_da70        -- array resize for 16-byte elements (material list)
//   0x140002340 Reimpl_ExternalPtr_Serialize_StringTable -- ExternalPtr serialize
// DAT_1401c7158 -- type ID for CitizenAI-like component (not used here; for reference)
// ============================================================

// ============================================================
// ConsumeRawMaterial::Serialize (0x1400c6120) -- slot 3
// Versioned serialization (version 2):
//   v0: legacy format (single ExternalPtr + int)
//   v1+: entity ref(+0x20), material count(+0x30), material list(+0x28)
//   v2+: consume mode flag(+0x38)
// ============================================================
void ConsumeRawMaterial::Persist(void* stream) {
    Action::Persist(stream);   // inlined in retail: Serializer(stream, 0), status +0x18

    // Versioned serializer (version 2)
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 2);   // B.3.4 bridge (Persist's stream is void*)

    // Serialize target entity (+0x20) with ComponentEntity type check
    _field20.InternalBasePtr::Serialize(ser);
    if (_field20.Get() != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(_field20.Get(), ComponentEntity::TypeId())) {
            _field20 = nullptr;
        }
    }

    // Check version for format selection
    int version = ser.GetVersion();

    if (version == 0) {
        // Legacy v0 format: single ExternalPtr + int (no material list)
        ComponentDescription* legacyMaterial = nullptr;
        Reimpl_ExternalPtr_Serialize_StringTable(&legacyMaterial, &ser);   // 0x140002340
        int legacyCount;
        ser.Serialize(legacyCount, nullptr);
    } else {
        // v1+ format: material count + material list array
        int matCount = _field30;
        ser.Serialize(matCount, nullptr);

        // If loading, resize the material array
        if (static_cast<int>(ser.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
            // LEFT: the shared 16-byte Array SetSize 0x14000da70 is ACT-a's file-local body,
            // reached only through its int64_t* forwarder.
            ActRm_ArrayResize16_da70(reinterpret_cast<int64_t*>(&_field28), matCount);   // 0x14000da70
        }

        // Serialize each material entry (ExternalPtr + int count, 0x10 bytes each)
        Entry* entry = _field28;
        if (matCount > 0) {
            uint64_t remaining = (uint64_t)matCount;
            do {
                Reimpl_ExternalPtr_Serialize_StringTable(&entry->material, &ser);   // 0x140002340
                ser.Serialize(entry->count, nullptr);
                ++entry;
                remaining--;
            } while (remaining != 0);
        }
    }

    // v2+: serialize consume mode flag (+0x38)
    int ver2 = ser.GetVersion();
    if (ver2 > 1) {
        ser.Serialize(_field38, nullptr);
    }
}

// ============================================================
// ConsumeRawMaterial::AbstractImpl (0x1400c5f50) -- slot 4
// If material count is 0: return 2 (nothing to consume).
// For each material in list:
//   If consumeMode == false (check only):
//     Look up available count in storage, subtract reserved.
//     If available < requested: return 2 (insufficient).
//   If consumeMode == true (consume):
//     Subtract reserved amount from storage reserved field.
//   Then: SubtractItem from storage, update global resource tracker.
// Returns 0 on success, 2 on failure.
// ============================================================
int  ConsumeRawMaterial::AbstractImpl(void* actionStack) {
    (void)actionStack;

    int matCount = _field30;
    if (matCount == 0) {
        return 2;
    }

    // Find StorageComponent on target entity (+0x20)
    StorageComponent* storage = _field20->FindComponent<StorageComponent>();   // 0x140006270

    int matIdx = 0;

    if (matCount <= 0) {
        return 0;
    }

    do {
        Entry* matEntry = &_field28[matIdx];
        int requestedCount = matEntry->count;

        if (!_field38) {
            // Check mode: verify sufficient availability
            int availableCount = 0;
            int slotCount = storage->slotCount;
            if (slotCount > 0) {
                StorageComponent::Item* itemArray = storage->storageSlots;
                for (int i = 0; i < slotCount; i++) {
                    if (itemArray[i]._resource == matEntry->material) {
                        if (i >= 0) {
                            // available = count - reserved
                            availableCount = itemArray[i]._count - itemArray[i]._reserve;
                        }
                        break;
                    }
                }
            }
            if (availableCount < requestedCount) {
                return 2;  // Insufficient
            }
        } else {
            // Consume mode: subtract from reserved field
            int slotCount = storage->slotCount;
            if (slotCount > 0) {
                StorageComponent::Item* itemArray = storage->storageSlots;
                for (int i = 0; i < slotCount; i++) {
                    if (itemArray[i]._resource == matEntry->material) {
                        if (i >= 0) {
                            itemArray[i]._reserve -= requestedCount;
                            if (itemArray[i]._reserve < 0) {
                                itemArray[i]._reserve = 0;
                            }
                        }
                        break;
                    }
                }
            }
        }

        // SubtractItem from storage
        StorageSubtract(storage, matEntry->material, requestedCount);   // 0x14012d150

        // Update the town's per-category consumption counter
        CitizenBehavior* town = GameUpdateLoop::_singleton->m_citizenList;
        int consumedCount = matEntry->count;

        int categoryIndex = ResourceCategory(matEntry->material);   // 0x140088f90
        if (categoryIndex >= 0) {
            town->professionResources[categoryIndex].field_0x0C += consumedCount;
        }

        matIdx++;
    } while (matIdx < _field30);

    return 0;
}

// ============================================================
// ConsumeRawMaterial::CheckEntity (0x1400c5e30) -- slot 8
// Undoes consumption when entity is removed (only if consume mode is true).
// Iterates material list, subtracts reserved amounts from storage.
// ============================================================
void ConsumeRawMaterial::CheckEntity() {
    // Only undo if consume mode is active
    if (!_field38) {
        return;
    }

    // Find StorageComponent on target entity (+0x20)
    StorageComponent* storage = _field20->FindComponent<StorageComponent>();   // 0x140006270

    int matCount = _field30;
    if (matCount <= 0) return;

    for (int i = 0; i < matCount; i++) {
        const Entry& entry = _field28[i];

        int slotCount = storage->slotCount;
        if (slotCount > 0) {
            StorageComponent::Item* itemArray = storage->storageSlots;
            for (int j = 0; j < slotCount; j++) {
                if (itemArray[j]._resource == entry.material) {
                    if (j >= 0) {
                        itemArray[j]._reserve -= entry.count;
                        if (itemArray[j]._reserve < 0) {
                            itemArray[j]._reserve = 0;
                        }
                    }
                    break;
                }
            }
        }
    }
}

// ============================================================
// ConsumeRawMaterial::GetTarget (0x1400c5f10) -- slot 9
// Simple compare: returns true if param matches entity at +0x20.
// ============================================================
bool ConsumeRawMaterial::GetTarget(void* param) {
    return *(void**)param == _field20.Get();   // LEFT: Action::GetTarget's parameter is void* (Action.h)
}

// ============================================================
// class ConsumeRawMaterial (ConsumeRawMaterial.h) — ENDGAME B.3.8 ACT.
// The hand-built g_ConsumeRawMaterial_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"

// create 0x1400220c0 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
ConsumeRawMaterial::ConsumeRawMaterial()
    : _field20(nullptr),
      _field28(nullptr),
      _field30(0),
      _field34(0),
      _field38(false),
      _pad39{} {}

// The dtor body of 0x140022130, between its own stamp and the Action stamp:
//   MOV RCX,[this+0x28]; CALL MemFree (IAT 0x140152b68, unconditional);
//   [this+0x28] = 0; [this+0x30] = 0 (the qword: count +0x30 and capacity +0x34).
// The tree's shared Action_ScalarDtor skipped it and leaked the consume list.
ConsumeRawMaterial::~ConsumeRawMaterial() {
    Object::detail::InternalMemFree(_field28);
    _field28 = nullptr;
    _field30 = 0;   // H7: retail clears count and capacity with one qword store
    _field34 = 0;
}

// [3] 0x1400c6120
// [4] 0x1400c5f50
// [8] 0x1400c5e30
// [9] 0x1400c5f10

void* ConsumeRawMaterial::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ConsumeRawMaterial), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ConsumeRawMaterial());
}
void* ConsumeRawMaterial::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ConsumeRawMaterial());
}


// ============================================================
// FUN_1400c6440 — ConsumeRawMaterial_Action_Ctor3 (2-arg variant)
//   RCX=self, RDX=param_2.  vtable ConsumeRawMaterial; +0x20=*param_2, +0x28=0, +0x30=0.
// ============================================================
void* ActChild_ConsumeRawCtor3_c6440(void* self, uint64_t* param_2) {
    // B4 FORWARD: 1 outside caller (Work/work_act_consume.cpp) holds the entity as uint64_t*.
    return static_cast<Object::Base*>(new (self) ConsumeRawMaterial(
        reinterpret_cast<ComponentEntity* const*>(param_2)));
}

// 0x1400c6440: +0x20 = *param_2 ; +0x28 = 0 ; +0x30 = 0 (one qword: count and capacity;
// +0x38 untouched).
ConsumeRawMaterial::ConsumeRawMaterial(ComponentEntity* const* storage)
    : _field20(*storage),
      _field28(nullptr),
      _field30(0),
      _field34(0) {}

// ============================================================
// FUN_1400c64b0 — ConsumeRawMaterial::AddMaterial (the consume-list append).
// Adds a (material, count) entry to the action's consume list (+0x28 array,
// +0x30 count, +0x34 capacity, stride 0x10: material, count, reserved). If +0x38 (the
// "reserve" flag) is set, first bumps the matching StorageComponent item's reserve by
// `count`.
// ============================================================
void ConsumeRawMaterial::AddMaterial(ComponentDescription* material, int count) {
    StorageComponent* storage = _field20->FindComponent<StorageComponent>();   // 0x140006270

    if (_field38) {
        int invCount = storage->slotCount;
        if (0 < invCount) {
            StorageComponent::Item* p = storage->storageSlots;
            int idx = 0;
            while (true) {
                if (p->_resource == material) {
                    if (idx >= 0) {
                        storage->storageSlots[idx]._reserve += count;
                    }
                    break;
                }
                idx++;
                p++;
                if ((int64_t)idx >= invCount) break;
            }
        }
    }

    // append {material, count} to +0x28 (count +0x30, cap +0x34, stride 0x10)
    uint32_t oldCount = static_cast<uint32_t>(_field30);
    uint32_t newCount = oldCount + 1;
    if (newCount == oldCount) return;  // overflow guard (never)

    // The new entry: material, count, reserved 0.
    Entry newEntry;
    newEntry.material = material;
    newEntry.count = count;
    newEntry.reserved = 0;

    Entry* oldArr = _field28;
    _field30 = static_cast<int32_t>(newCount);
    if (_field34 < (int)newCount) {
        _field34 = static_cast<int32_t>(newCount);
        if (oldCount != 0) _field34 = static_cast<int32_t>(newCount * 3);
        _field28 = System::MemoryAllocator::_singleton
            ? static_cast<Entry*>(System::MemoryAllocator::MemAlloc((uint64_t)((int64_t)_field34 << 4), 8))
            : nullptr;
    }
    int copyStart = 0;
    if (oldArr == _field28) copyStart = (int)oldCount;
    int64_t i = copyStart;
    if (i < (int)newCount) {
        do {
            Entry* dst = &_field28[i];
            if (dst != nullptr) {
                const Entry* src = (i < (int)oldCount) ? &oldArr[i] : &newEntry;
                *dst = *src;
            }
            i++;
        } while (i < (int)newCount);
    }
    if (oldArr != _field28) System::MemoryAllocator::MemFree(oldArr);
}

// B4 FORWARD: 1 outside caller (Work/work_act_consume.cpp) holds the action and the
// material as integers.
void Entity_ConsumeListAppend_64b0(int64_t self, int64_t resType, int count) {
    reinterpret_cast<ConsumeRawMaterial*>(self)->AddMaterial(
        reinterpret_cast<ComponentDescription*>(resType), count);
}

// ============================================================
// FUN_1400c63b0 — ConsumeRawMaterial_Action_CopyCtor
//   RCX=self, RDX=p2, R8=p3(material), R9=p4(u64, unused), [stack]=p5(byte).
//   +0x20=*p2 ; +0x28=0 ; +0x30=0 ; +0x38=p5(byte); then 64b0(self, p3, 1).
// ============================================================
void* ActChild_ConsumeRawCopyCtor_c63b0(void* self, uint64_t* p2, int64_t p3,
                                        uint64_t /*p4*/, uint8_t p5) {
    // B4 FORWARD: 3 outside callers (Component/citizenai_actionctors.cpp,
    // Work/work_act_perform.cpp) hold the entity as uint64_t* and the material as an integer.
    return static_cast<Object::Base*>(new (self) ConsumeRawMaterial(
        reinterpret_cast<ComponentEntity* const*>(p2),
        reinterpret_cast<ComponentDescription*>(p3), p5));
}

// 0x1400c63b0: +0x20 = *p2 ; +0x28 = 0 ; +0x30 = 0 (one qword) ; +0x38 = p5 (byte); then
// the consume-list append 0x1400c64b0(this, material, 1) (R8D = 1, retail 0x1400c6412).
ConsumeRawMaterial::ConsumeRawMaterial(ComponentEntity* const* storage,
                                       ComponentDescription* material, bool reserve)
    : _field20(*storage),
      _field28(nullptr),
      _field30(0),
      _field34(0),
      _field38(reserve) {
    AddMaterial(material, 1);
}

DEFINE_EXE_TYPE(ConsumeRawMaterial, "Action")
