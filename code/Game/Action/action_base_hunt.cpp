// code/Game/Action/action_base_hunt.cpp
// Shared / high-reuse exe sub-helpers referenced from the HuntAnimal / RemoveAnimal /
// CompleteRoad action families (and, for 0x1400c3470 / 0x140003110, from many
// Component & Work files as well).
//
// Their callers call them by name through Action/exe_bodies.h (B3R); the registration
// (RegisterActionHuntBatch in standalone/reg_action_hunt.cpp) stays for readers other
// blocks still have.
#include "Action/exe_bodies.h"
//
// Wave 3 (Action). Each function is a 1:1 port of the decompiled exe helper; do not
// "improve" behavior.
//

#include <new>
#include "object/TypeInfo.h"
#include "Game/exe_constants.h"       // kRdata_140172234
#include "system/Value.h"             // RtSystem_Time_Ctor
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/AnimalComponent.h"
#include "Component/WildAnimalComponent.h"
#include "Entity/ComponentEntity.h"   // ComponentEntity::FindComponent<T>
#include "Component/HuntDescription.h"
#include "Component/HuntComponent.h"
#include "Entity/EntityList.h"
#include "Action/PathSearch.h"   // RawArray

// ============================================================
// HuntAnimal_helper_2a80 (0x140122a80) — is the hunt done: the HuntComponent's
// description, when a HuntDescription, gives the hunt time (_f20 * DAT_140172234 seconds);
// returns whether that Time (in raw ticks) is <= the component's huntProgress.
// ============================================================
bool HuntIsReady(const HuntComponent* hunt) {
    Description* obj = hunt->_description;
    const HuntDescription* desc = nullptr;
    if (::Object::IsDerivedFromNullSafe(obj, HuntDescription::TypeId())) desc = static_cast<const HuntDescription*>(obj);

    float scale = kRdata_140172234;   // 0x140172234 = 300.0f
    // The tree's guard: retail reads the description unconditionally.
    float holdSeconds = desc ? desc->_f20 : 0.0f;

    System::Time time = {};
    RtSystem_Time_Ctor(&time, holdSeconds * scale);   // ??0Time@System@@QEAA@M@Z
    return time._ticks <= hunt->huntProgress._ticks;
}

// ============================================================
// FUN_140003110 (0x140003110) — Array<qword>::SetSize(n, fill).
// ============================================================
// Grows the array to newCount elements (capacity grows: if first growth beyond a non-zero
// count by exactly 1, cap *= 3), then fills [start..newCount) — the rows below the old count
// are copied from the OLD buffer; new rows take *fill. Frees the old buffer if reallocated.
void QwordArraySetSize(RawArray<void*>* arr, int newCount, void* const* fill) {
    int oldCount = arr->size;
    if (newCount == oldCount) return;

    void** oldBase = arr->data;
    arr->size = newCount;
    if (arr->capacity < newCount) {
        arr->capacity = newCount;
        if (newCount == oldCount + 1 && oldCount != 0)
            arr->capacity = newCount * 3;
        arr->data = static_cast<void**>(System::MemoryAllocator::MemAlloc(
                       static_cast<uint64_t>(static_cast<int64_t>(arr->capacity)) << 3, 8));
    }

    int startIdx = (oldBase == arr->data) ? oldCount : 0;
    for (int64_t i = startIdx; i < newCount; i++) {
        void** dst = arr->data + i;
        if (dst) {
            *dst = nullptr;
            void* srcVal;
            if (i < oldCount)
                srcVal = oldBase[i];
            else
                srcVal = *fill;
            *dst = srcVal;
        }
    }

    if (oldBase != arr->data)
        System::MemoryAllocator::MemFree(oldBase);
}

// ============================================================
// FUN_1400c2ea0 (0x1400c2ea0) — mark an entity (and its removable components) for removal.
// ============================================================
// If the entity's flags have bit 1 (mask 2): walk its components; each whose _flags bit 1
// is set gets OnRemove (slot 7). Always finally OR 0x400 into the entity's flags.
void MarkEntityForRemoval(ComponentEntity* entity) {
    if ((static_cast<uint8_t>(entity->_entityFlags) & 2) != 0) {
        int count = entity->_componentCount;
        if (count >= 1) {
            ::Object::InternalPtr<Component>* base = entity->_components;
            for (int64_t i = 0; i < count; i++) {
                Component* node = base[i].Get();
                bool visits = ((node->_flags >> 1) & 1) != 0;
                if (visits) {
                    node->OnRemove();   // slot 7
                }
            }
        }
    }
    entity->_entityFlags |= 0x400u;
}

// ============================================================
// FUN_1400c3470 (0x1400c3470) — ComponentEntity::RefreshEntityFlags: recompute the entity's
// flags (the OR of every component's low 16 flag bits) and, if the entity is in an
// EntityList, stage it for the update tree when bit 4 changed and for the post-update tree
// when bit 0x10 changed (0x140003110 appends).
// HIGH-REUSE (called from many Component/Work files).
// The tree has a second body of this address, the member ComponentEntity::RefreshEntityFlags
// (Entity/componententity.cpp, block WORK-a), which skips null component slots and appends
// through Collection::Array::SetSize; the C spelling's callers keep this one (no null skip,
// the 0x140003110 port) until the two are reconciled.
// ============================================================
void NotifyEntityChanged(ComponentEntity* entity) {
    uint32_t oldFlags = entity->_entityFlags;
    int count = entity->_componentCount;
    entity->_entityFlags = 0;

    if (count > 0) {
        ::Object::InternalPtr<Component>* base = entity->_components;
        for (int64_t i = 0; i < count; i++) {
            Component* comp = base[i].Get();
            entity->_entityFlags |= static_cast<uint32_t>(static_cast<uint16_t>(comp->_flags));
        }
    }

    EntityList* owner = entity->_entityList;
    if (owner != nullptr) {
        uint32_t newFlags = entity->_entityFlags;
        if (((oldFlags ^ newFlags) & 4) != 0) {
            ComponentEntity* ibp = entity;
            ActHunt_DynArraySetCount_3110(&owner->stagingUpdate, owner->stagingUpdate.count + 1, &ibp);
        }
        if (((oldFlags ^ newFlags) & 0x10) != 0) {
            ComponentEntity* ibp = entity;
            ActHunt_DynArraySetCount_3110(&owner->stagingPostUpdate, owner->stagingPostUpdate.count + 1, &ibp);
        }
    }
}

// ============================================================
// CompleteRoad_helper_c640 (0x14009c640) — RETIRED duplicate. The port is
// Map::FinishRoadCell in action_pf_roots_road.cpp.
// ============================================================

// ===========================================================================
// B4 FORWARD: the C spellings other blocks' files still call, one line each.
// ===========================================================================


// B4 FORWARD: outside callers in many files (the arrays arrive as void*)
void ActHunt_DynArraySetCount_3110(void* arrV, int newCount, void* valuePtr) {
    QwordArraySetSize(reinterpret_cast<RawArray<void*>*>(arrV), newCount,   // LEFT: SIG ActHunt_DynArraySetCount_3110
                      reinterpret_cast<void* const*>(valuePtr));   // LEFT: SIG ActHunt_DynArraySetCount_3110
}

// B4 FORWARD: outside callers in many files
void ActHunt_MarkEntityRemoval_c2ea0(void* self) {
    MarkEntityForRemoval(reinterpret_cast<ComponentEntity*>(self));   // LEFT: SIG ActHunt_MarkEntityRemoval_c2ea0
}

// B4 FORWARD: outside callers in many files
void ActHunt_NotifyEntityChanged_c3470(void* self) {
    NotifyEntityChanged(reinterpret_cast<ComponentEntity*>(self));   // LEFT: SIG ActHunt_NotifyEntityChanged_c3470
}
