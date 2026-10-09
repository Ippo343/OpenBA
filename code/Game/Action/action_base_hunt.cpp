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
    if (obj->IsDerivedFrom(HuntDescription::TypeId())) desc = static_cast<const HuntDescription*>(obj);

    float scale = kRdata_140172234;   // 0x140172234 = 300.0f
    // B6 (14d G58): 0x140122ab0 CMOVNZ RDX,RBX over RDX = 0, 0x140122ab4 MOVSS XMM1,[RDX+0x20]:
    // no test.
    float holdSeconds = desc->_f20;

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
        int count = entity->_components._size;
        if (count >= 1) {
            ::Object::InternalPtr<Component>* base = entity->_components._data;
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

// (0x1400c3470 is ComponentEntity::RefreshEntityFlags, Entity/componententity.cpp: B6 COMP row 46 made
// the member retail's one body and this file's second copy, NotifyEntityChanged, went with it.)

// ============================================================
// CompleteRoad_helper_c640 (0x14009c640) — RETIRED duplicate. The port is
// Map::FinishRoadCell in action_pf_roots_road.cpp.
// ============================================================

