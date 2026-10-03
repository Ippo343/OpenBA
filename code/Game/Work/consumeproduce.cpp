// code/Game/Work/consumeproduce.cpp
// Clean C++ reimplementation of ConsumeProduce Work class.
// Source: Game/Work/ConsumeProduce.cpp (from profile build debug strings)
//
// ConsumeProduce -- consume/produce work cycle. Size = 0x78.
// Inherits: Object::Internal -> Work (0x48) -> ConsumeProduce
// Ctor: 0x1400e9340, Vtable: 0x140161220
//
// Methods implemented:
//   Ctor (0x1400e9340), ScalarDtor (0x14002a190), Serialize (0x1400e9020)
//   Execute (vtable slot 4, 0x1400e7840)
//   OnComplete (vtable slot 6, 0x1400e77c0)
//   OnCancel (vtable slot 7, 0x1400e7780)
//   Cleanup (vtable slot 8, 0x1400e8f30)
//   Validate (vtable slot 9, 0x1400e7530)
//   GetTarget (vtable slot 10, 0x1400e8fb0)
//

#include "Work/ConsumeProduce.h"
#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: bodies called by name
#include "object/Type.h"            // B3R: RtObject_Type_Serialize
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "redzone_alloc.h"   // RedzoneAlloc_PrintBacktrace (wild-write diagnostic)
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/ConsumeProduceComponent.h"     // ENDGAME B.3.9 D5
#include "Component/ConsumeProduceDescription.h"   // ENDGAME B.3.9 D5
#include "Component/StatusIconComponent.h"         // ENDGAME B.3.9 D5
#include "Component/StorageComponent.h"            // ENDGAME B.3.9 D5
#include "Component/RawMaterialDescription.h"      // ENDGAME B.3.9 D5
#include "Component/WorkComponent.h"               // ENDGAME B.3.9 D5
#include "Component/CitizenComponent.h"            // ENDGAME B.3.9 D5
#include "Component/CharacterComponent.h"
#include "Component/ModelComponent.h"
#include "Component/InteractComponent.h"
#include "Component/StorageLocationComponent.h"
#include "Component/WorkPlaceComponent.h"
#include "Entity/ComponentEntity.h"

// ============================================================
// Sub-functions, called by name (B3R; Work/Action/Component exe_bodies.h)
// ============================================================
//   slot 0x1401516b0 Object::Type::Serialize     RtObject_Type_Serialize (object/Type.h)
//   0x1400f7100 Work::ValidateTask                 WorkBase_ValidateTask_f7100
//   0x1400c5400 task list cleanup                  WorkBase_TaskListCleanup_c5400
//   0x140002e10 FindComponent (DWORD_1401c80e0)    AttendanceComponent_GetJobHostComponent
//   0x14012c3f0 ModelComponent::SetActiveState     CitizenComp_FUN_14012c3f0
//   0x1401193a0 ConsumeProduceAction_helper_93a0   WorkLeaf_ConsumeProduce_93a0
//               (the caller reads the low byte, as retail's AL)
//   0x14012b310 ConsumeProduceAction_helper_b310   WorkLeaf_ConsumeProduce_b310
//   0x14009c480 Map_RecalcZones (4 args)           Pf_ZoneRefresh_9c480
//   0x1400bef70 SetupConsumeProduceWorkChain       WorkAct_ConsumeProduceCtor_bef70
// Object::Internal ctor/dtor (IAT 0x140152768 / 0x140152770) and Work::Serialize
// (0x1400f7400) are the real base classes. 0x1400032e0 / 0x1400021d0 are
// GameColl::AsArray<T> member calls (ENDGAME B.3.2 step 2).

// ============================================================
// Constructor (0x1400e9340)
// Object::Internal::Internal() -> Work fields -> ConsumeProduce vtable + fields
// ============================================================
// The Work base init and the two stamps are the compiler's. field_48 (bool) and
// field_4c/field_54 (ints) are NOT initialized by retail's ctor.
ConsumeProduce::ConsumeProduce()
    : Work(),
      field_50(0),         // Type
      field_58(nullptr),   // IBP DynArray data ptr
      field_60(0),         // IBP DynArray count
      field_64(0),         // IBP DynArray capacity
      field_68(nullptr),   // int DynArray data ptr
      field_70(0),         // int DynArray count
      field_74(0) {        // int DynArray capacity
}

// BaseDtor 0x1400e9190. The stamps (ConsumeProduce, then Work) and the `~Internal`
// tail call are the compiler's.
ConsumeProduce::~ConsumeProduce() {
    // Free the int DynArray at +0x68 (original uses 8-byte writes to zero count+capacity)
    System::MemoryAllocator::MemFree(field_68);
    field_68 = nullptr;
    field_70 = 0;
    field_74 = 0;

    // Free the IBP DynArray at +0x58 (original uses 8-byte writes to zero count+capacity)
    System::MemoryAllocator::MemFree(field_58);
    field_58 = nullptr;
    field_60 = 0;
    field_64 = 0;
}

// ============================================================
// Serialize (0x1400e9020)
// Work::Serialize + Serializer(version=0) + IBP array + int array + fields
// ============================================================
void ConsumeProduce::Persist(IO::Stream& stream) {
    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create IO::Serializer with version 0 (destroyed at the end, as retail)
    IO::Serializer serializer(stream, 0);

    // Check if loading (direction 0 means loading)
    bool isLoading = (static_cast<int>(serializer.GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210

    // --- Serialize IBP array count (+0x60) ---
    // Use a local copy like the original does (original serializes into a stack local,
    // not directly into self->field_60). This avoids issues if ResizeIBPArray
    // modifies field_60 as a side-effect.
    int ibpCount = field_60;
    serializer.Serialize(ibpCount, nullptr);

    // If loading, resize the IBP array to match deserialized count.
    // B.3.2 bridge: FUN_1400032e0 == Collection::Array<Ptr8>::SetSize
    // (ImmigrationComp_DynArrayResize in immigrationcomponent.cpp).
    if (isLoading) {
        GameColl::AsArray<GameColl::Ptr8>((void*)&field_58).SetSize(ibpCount);
    }

    // Serialize each InternalBasePtr in the array
    CitizenComponent** ibpArray = field_58;
#ifdef _DEBUG
    if (ibpCount > 0 && ibpArray == nullptr) {
        printf("[ConsumeProduce::Serialize] BUG: ibpCount=%d but ibpArray=NULL! this=%p isLoading=%d field_58=%p field_60=%d\n",
               ibpCount, (void*)this, (int)isLoading, (void*)field_58, field_60);
    }
#endif
    for (int i = 0; i < ibpCount; i++) {
        reinterpret_cast<Object::InternalBasePtr*>(&ibpArray[i])->Serialize(serializer);   // B.3.4 bridge

        // After deserialize, validate the entity type.
        //
        // GUARDED because standalone GENUINELY DIFFERS from retail here, not to paper
        // over a faithful crash: in retail DAT_1401c7158 is always a live CitizenComponent
        // type id, but in our build that .rdata slot is registered only when
        // BANISHED_LIVE_CITIZENAI_CITID is set. That flag GRADUATED 2026-08-26 and is now
        // default-on (kill-switch =0), so the common case is a live id — but the guard
        // stays, because the kill-switch still produces a NULL slot and this is
        // Serialize-side, which autostart never reaches, so a regression here would only
        // ever surface as a save/load crash. See standalone/reg_citizenai_typeids.cpp.
        //
        // When the id is unavailable we KEEP the ref rather than clearing it. Retail's
        // intent is "drop refs that are not citizen entities"; with no id we cannot tell,
        // and these refs are citizens in the overwhelming majority, so clearing them would
        // destroy live state — strictly worse than skipping a check we cannot perform.
        // Same both-pointer-and-value shape as completetask.cpp's reader.
        CitizenComponent* ptr = ibpArray[i];
        if (ptr != nullptr) {
            bool valid = Object::IsDerivedFromNullSafe(ptr, CitizenComponent::TypeId());  // DAT_1401c7158
            if (!valid) {
                ibpArray[i] = nullptr;  // null out invalid refs
            }
        }
    }

    // --- Serialize int array count (+0x70) ---
    int intCount = field_70;
    serializer.Serialize(intCount, nullptr);

    // If loading, resize the int array.
    // B.3.2 bridge: FUN_1400021d0 == Collection::Array<int32>::SetSize (retail
    // ConsumeProduceAction_Shutdown calls it at 1400e90ff). I32NoInit — a grown tail is
    // left as the allocation found it, like retail. Safe: the resize runs only when
    // loading and the bulk int read below writes all intCount elements from the stream.
    if (isLoading) {
        GameColl::AsArray<GameColl::I32NoInit>((void*)&field_68).SetSize(intCount);
    }

    // Bulk-serialize the int array
    // Original: IO::Serializer::Serialize(serializer, count, intArrayPtr, name)
    serializer.Serialize(intCount, field_68, nullptr);

    // --- Serialize ConsumeProduce fields ---
    serializer.Serialize(*reinterpret_cast<bool*>(&field_48), nullptr);   // bool at +0x48 (the field is uint8_t storage)
    serializer.Serialize(field_4c, nullptr);    // int at +0x4C
    RtObject::RtObject_Type_Serialize(&field_50, &serializer);            // Type at +0x50
    serializer.Serialize(field_54, nullptr);    // int at +0x54
}

// ============================================================
// Helpers for Execute
// ============================================================

// Find a component by type ID on an entity (iterates component list at +0x30/+0x38).
// Returns the component pointer, or nullptr if not found.
// Find a DESCRIPTION by type id in an object's child list at +0x28 (count +0x30).
//
// This is NOT the same walk as ComponentEntity::FindComponent, and the difference is
// load-bearing: a ComponentEntity keeps its COMPONENT array at +0x30 with the count at
// +0x38, while a description keeps its CHILD list at +0x28 with the count at +0x30. A
// StorageComponent slot record holds a DESCRIPTION in its first qword, so the material
// lookup has to use this form. Retail inlines exactly this loop at 0x1400e7d0f
// (`*(int*)(uVar11 + 0x30)` / `*(longlong*)(uVar11 + 0x28)`), and HappinessComponent
// ::IsActive walks the same slot the same way.
//
// Reusing the component-list finder here is what made Execute fault on its very first
// live pass once slot-18 CreateWork started minting work: reading the count from
// matEntity+0x38 and the array from +0x30 landed inside the slot's (capacity, stored)
// int pair, so the "component pointer" came back as 0x0000000500000005 -- two adjacent
// int32 5s -- and the vtable read off it was the access violation.
static void* FindDescriptionOnObject(uintptr_t obj, uint32_t typeId) {
    int count = *(int*)(obj + 0x30);
    uintptr_t arr = *(uintptr_t*)(obj + 0x28);
    for (int i = 0; i < count; i++) {
        void* c = *(void**)(arr + (uintptr_t)i * 8);
        uint32_t t = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.x bridge (slot 2)
        if (t == typeId) {
            return Object::IsDerivedFromNullSafe(c, typeId) ? c : nullptr;
        }
    }
    return nullptr;
}

// Finalize action: call vtable +0x50, set citizen state flags, store citizen ref.
// The citizen slot handed to Execute is an InternalPtr<CitizenComponent> (its
// +0x10 entity, +0x30, +0x54, +0x88, +0xB0, +0x1F0 and +0x278 reads are
// CitizenComponent's fields).
static int FinalizeAction(ConsumeProduce* self, CitizenComponent** citizenIBP, void* actionIBP, int priority, int slotIndex) {
    CitizenComponent* citizen = *citizenIBP;

    // Call vtable slot +0x50 (GetTarget): the workplace's profession
    Profession* actionPtr = *self->GetTarget(static_cast<Profession**>(actionIBP));   // LEFT: the return buffer is the caller's reused scratch slot (FinalizeAction takes it as void*)

    citizen->statusFlags |= 4;
    citizen->field_0x30 = priority;
    citizen->field_0xB0 = 0;

    if (actionPtr == citizen->profession && priority >= 0) {
        citizen->happinessFlags |= 0x20000;
    }

    // Store citizen entity ref at self->field_58[slotIndex]
    self->field_58[slotIndex] = *citizenIBP;
    return 2;
}



// ============================================================
// Execute (0x1400e7840) -- vtable slot 4
//
// Main production work logic. Returns 1 (retry) or 2 (action pushed).
// ============================================================
int ConsumeProduce::Execute(CitizenComponent** citizenIBP, System::Time* /*context*/) {
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    // m_game (Map*) and m_citizenList (CitizenBehavior*) are still void* in
    // GameUpdateLoop.h: LEFT.
    int* mapGrid = (int*)gameWorld->m_game;
    void** worldEntityList = (void**)gameWorld->m_citizenList;

    // --- Find InteractComponent on work entity ---
    InteractComponent* posComp = field_10->FindComponent<InteractComponent>();

    // --- Find ConsumeProduceComponent (type 0x1401c7350) on work entity ---
    ConsumeProduceComponent* smComp = nullptr;
    fn_FindComponent_ConsumeProduce(field_10, &smComp);

    // --- Get ConsumeProduceDescription from the component's description (+0x18) ---
    void* descBase = smComp->_description;
    bool isDerived = Object::IsDerivedFromNullSafe(descBase, ConsumeProduceDescription::TypeId());
    // Component::_description is void*: LEFT for Component.h's owner.
    ConsumeProduceDescription* cpDesc = isDerived ? reinterpret_cast<ConsumeProduceDescription*>(descBase) : nullptr;

    // --- Find StorageLocationComponent (for storage slot lookup) ---
    StorageLocationComponent* storageLoc = nullptr;
    fn_FindComponent_Unknown7ee8(field_10, &storageLoc);

    // --- Check resource depletion (bVar6) ---
    // Get active product index from ConsumeProduce component
    bool resourcesDepleted = false;
    int activeProductIdx = WeightedRandomIndex_119470(reinterpret_cast<int64_t>(smComp));

    // The recipe array (+0x28, 0x28 stride) is raw storage in
    // ConsumeProduceDescription.h: its reads are LEFT.
    uintptr_t recipes = reinterpret_cast<uintptr_t>(cpDesc->_recipes);

    if (smComp->consumeResourceId < 0) {
        // Multi-product mode: check ALL products for depletion
        int productCount = cpDesc->_recipeCount;
        if (productCount > 0) {
            int* productPtr = (int*)(recipes + 0x18);
            for (int i = 0; i < productCount; i++) {
                int materialType = *productPtr;
                productPtr += 10;  // stride = 0x28 bytes = 10 ints
                // Check world material supply vs demand
                uintptr_t* matArray = (uintptr_t*)worldEntityList[0x2e];
                int supply = (int)matArray[(int64_t)materialType * 4];
                int demand = (int)matArray[(int64_t)materialType * 4 + 1];
                if (demand <= supply) {
                    resourcesDepleted = true;
                }
            }
        }
    } else {
        // Single product mode: check specific active product
        int materialType = *(int*)(recipes + 0x18 + (int64_t)activeProductIdx * 0x28);
        uintptr_t* matArray = (uintptr_t*)worldEntityList[0x2e];
        int supply = (int)matArray[materialType * 4];
        int demand = (int)matArray[materialType * 4 + 1];
        resourcesDepleted = (demand <= supply);
    }

    // --- Find StatusIconComponent (type 0x1401c7e08) on work entity ---
    StatusIconComponent* statusIcon = field_10->FindComponent<StatusIconComponent>();

    // Set active state = 2, enabled = resourcesDepleted
    CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>(statusIcon), 2, resourcesDepleted ? '\x01' : '\0');

    // --- Find ConsumeProduceComponent (type 0x1401c7350) for produce check ---
    ConsumeProduceComponent* smComp2 = field_10->FindComponent<ConsumeProduceComponent>();

    // Check produce threshold
    uint64_t canProduce = WorkLeaf_ConsumeProduce_93a0(reinterpret_cast<uintptr_t>(smComp2));
    // [CPWork] the gate that decides whether the tavern is allowed to make more at
    // all (retail 0x1401193a0 -- a stock-vs-limit test against the "Alcohol Limit 500"
    // the panel shows). Printed on CHANGE only; a permanent 0 here means the work runs
    // forever and produces nothing, which reads exactly like a broken produce path.
    {
        static int n = 0; static int last = -1;
        const int cp = (int)(char)canProduce;
        if (n < 8 && cp != last) { ++n; last = cp;
            fprintf(stderr, "[CPWork] canProduce(threshold)=%d depleted=%d\n",
                    cp, resourcesDepleted ? 1 : 0);
            fflush(stderr);
        }
    }

    // ============================================================
    // BRANCH: Not depleted
    // ============================================================
    if (!resourcesDepleted) {
        if ((char)canProduce == '\0') {
            goto handle_depleted_or_idle;
        }

        // --- Pick random empty output slot ---
        uint32_t startSlot = gameWorld->m_random->Rand((uint32_t)field_60);
        int totalSlots = field_60;
        uint32_t slot = startSlot;

        if (totalSlots < 1) {
            return 1;
        }

        // Find an empty slot (field_58[slot] == 0)
        CitizenComponent** slotCitizens = field_58;
        int attempts = 0;
        while (slotCitizens[slot] != nullptr) {
            slot++;
            if ((int)slot >= totalSlots) {
                slot = 0;
            }
            attempts++;
            if (attempts >= totalSlots) {
                return 1;  // All slots occupied
            }
        }

        if (slot == 0xffffffff) {
            return 1;
        }

        // --- Find StorageComponent on work entity ---
        StorageComponent* storage = field_10->FindComponent<StorageComponent>();

        // --- Setup loop vars for product iteration ---
        int64_t productStride = (int64_t)activeProductIdx * 5;
        uintptr_t productData = recipes;

        // Get citizen position data for cell index calculation. ModelComponent keeps
        // its transform as raw storage (H5): the position reads are LEFT.
        uintptr_t citizenPos = reinterpret_cast<uintptr_t>((*citizenIBP)->character->modelComponent);

        (void)*(float*)(citizenPos + 0x160);  // posHeight -- read but not used in reimpl
        int mapSize = *mapGrid;
        int halfMap = mapSize >> 1;
        int cellX = (int)(*(float*)(citizenPos + 0x158) + (float)halfMap);
        int cellY = (int)(*(float*)(citizenPos + 0x15c) + (float)halfMap);
        if (cellX < 0) cellX = 0;
        if (cellX >= mapSize) cellX = mapSize - 1;
        if (cellY < 0) cellY = 0;
        if (cellY >= mapSize) cellY = mapSize - 1;
        int citizenCell = cellY * mapSize + cellX;

        // --- Iterate input materials for active product ---
        // [CPWork] Execute had NEVER run before slot-18 CreateWork was wired, so its
        // first live pass faulted here: the component finder was handed matEntity
        // 0x0000000500000005 -- two adjacent int32 5s, i.e. a (capacity,stored) pair read
        // where a good POINTER belongs. Print the storage the loop is actually walking so
        // it can be compared against the one HappinessComponent::IsActive reports for the
        // same building; if they differ, the StorageComponent finder is the fault, not the
        // slot stride. Budgeted, outcome-only.
        {
            static int n = 0;
            if (n < 6) { ++n;
                void* const sd = storage ? storage->storageSlots : nullptr;
                fprintf(stderr, "[CPWork] Execute storageComp=%p ibp=%p data=%p count=%d "
                        "slot0=%p tid_storage=%u activeProduct=%d cpSel(+0x50)=%d\n",
                        static_cast<void*>(storage), static_cast<void*>(storage), sd,
                        storage ? storage->slotCount : -1,
                        sd ? *(void**)sd : nullptr,
                        StorageComponent::TypeId(),
                        activeProductIdx, smComp->consumeResourceId);
                fflush(stderr);
            }
        }
        // The storage item array (+0x28) is raw storage in StorageComponent.h (H5):
        // its reads are LEFT.
        int inputCount = storage->slotCount;
        if (inputCount > 0) {
            uintptr_t inputOffset = 0;
            for (int inputIdx = 0; inputIdx < inputCount; inputIdx++) {
                // Get material entity from storage component
                uintptr_t storageData = reinterpret_cast<uintptr_t>(storage->storageSlots);
                uintptr_t matEntity = *(uintptr_t*)(inputOffset + storageData);

                // Check if this material entity matches the required material for active product
                bool matMatches = (*(uintptr_t*)(productData + 0x10 + productStride * 8) == matEntity);
                if (!matMatches) {
                    int64_t matCheckCount = (int64_t)*(int*)(productData + 8 + productStride * 8);
                    if (matCheckCount > 0) {
                        uintptr_t* matCheckPtr = *(uintptr_t**)(productData + productStride * 8);
                        for (int64_t j = 0; j < matCheckCount; j++) {
                            if (matCheckPtr[j * 2] == matEntity) {
                                matMatches = true;
                                break;
                            }
                        }
                    }
                }

                if (matMatches) {
                    inputOffset += 0x10;
                    continue;
                }

                // Material doesn't match -- need to find and haul it
                // Get available amount from storage
                void* rawMatComp = FindDescriptionOnObject(matEntity, RawMaterialDescription::TypeId());

                // Check remaining amount in storage: capacity - currently stored
                int available = *(int*)(inputOffset + 8 + storageData) - *(int*)(inputOffset + 0xc + storageData);

                // Find StorageComponent on the citizen's entity
                StorageComponent* homeStorage =
                    (*citizenIBP)->_entity->FindComponent<StorageComponent>();

                // Check if home storage has capacity for the material
                int homeCapacity = homeStorage->capacity;
                int homeStored = fn_StorageComponent_QueryCount(homeStorage);

                if (homeCapacity - homeStored < available * *(int*)((uintptr_t)rawMatComp + 0x48)) {
                    // Not enough capacity -- check citizen's own storage
                    StorageComponent* citizenStorage =
                        (*citizenIBP)->_entity->FindComponent<StorageComponent>();

                    int cap2 = citizenStorage->capacity;
                    int stored2 = fn_StorageComponent_QueryCount(citizenStorage);
                    available = (cap2 - stored2) / *(int*)((uintptr_t)rawMatComp + 0x48);
                }

                if (available == 0) {
                    // No resources available -- replan citizen
                    CitizenAI_State_LongTermPlan(reinterpret_cast<int64_t>(*citizenIBP));
                    return 2;
                }

                // --- Search for storage with this resource ---
                float searchPos[3] = { 0.0f, 0.0f, 0.0f };

                // Get entity ref for pathfinding
                ComponentEntity* entityRef = field_10;

                uintptr_t foundEntity = 0;
                Pf_PickBestDropTarget_8d590(
                    reinterpret_cast<int64_t**>(worldEntityList), reinterpret_cast<int64_t*>(&foundEntity),
                    (uint32_t)field_28, matEntity,
                    available, '\x01', reinterpret_cast<uint32_t*>(searchPos), &field_10);

                if (foundEntity != 0) {
                    // Check passability
                    uint64_t passable = WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, (uint32_t)citizenCell,
                                                             (uint32_t)field_28);
                    if ((char)passable == '\0') {
                        return 1;
                    }

                    // Get terrain position for storage
                    float terrainPos[4] = {};
                    if (storageLoc == nullptr) {
                        if (posComp != nullptr) {
                            Pf_EntityResolvePos_2e20(posComp, reinterpret_cast<float*>(&foundEntity), 0, 0, nullptr);
                            terrainPos[0] = *(float*)&foundEntity;
                            terrainPos[1] = ((float*)&foundEntity)[1];
                            terrainPos[2] = ((float*)&foundEntity)[2];
                        }
                    } else {
                        // Use storage slot lookup
                        int slotMapSize = *mapGrid;
                        int slotCellX = (int)(searchPos[0] + (float)(slotMapSize >> 1));
                        int slotCellY = (int)(searchPos[1] + (float)(slotMapSize >> 1));
                        if (slotCellX < 0) slotCellX = 0;
                        if (slotCellX >= slotMapSize) slotCellX = slotMapSize - 1;
                        if (slotCellY < 0) slotCellY = 0;
                        if (slotCellY >= slotMapSize) slotCellY = slotMapSize - 1;

                        int slotResult = Pf_FindBestAdjacentCell_d5d0(
                            reinterpret_cast<int64_t>(storageLoc), slotMapSize * slotCellY + slotCellX,
                            (float*)&terrainPos, (uint32_t)field_28);
                        if (slotResult >= 0) {
                            // Set up pathfinding and create work action
                            uintptr_t destEntity = foundEntity;
                            CitizenComponent* destEntityRef = *citizenIBP;

                            WorkAct_GatherRawMatCtor_bf790(
                                &worldEntityList, &destEntityRef,
                                field_20, &destEntity,
                                searchPos, reinterpret_cast<void**>(&entityRef), (float*)&terrainPos,
                                (int64_t)matEntity, available, slot, 0, '\x01', 0, 0);

                            int priority = field_20;
                            return FinalizeAction(this, citizenIBP, &destEntityRef, priority, slot);
                        }
                    }
                }

                inputOffset += 0x10;
            }
        }

        // --- Second pass: check each input material for nearby storage ---
        bool allInputsAvailable = true;
        int inputMatCount = *(int*)(productData + 8 + productStride * 8);
        for (int matIdx = 0; matIdx < inputMatCount; matIdx++) {
            uintptr_t matEntity2 = *(uintptr_t*)(*(uintptr_t*)(productData + productStride * 8) + (uintptr_t)matIdx * 0x10);

            // Check if this material is already in our storage
            int existingAmount = 0;
            int storageSlotCount = storage->slotCount;
            if (storageSlotCount > 0) {
                uintptr_t* storageSlots = (uintptr_t*)storage->storageSlots;
                for (int si = 0; si < storageSlotCount; si++) {
                    if (storageSlots[si * 2] == matEntity2) {
                        if (si >= 0) {
                            existingAmount = (int)storageSlots[si * 2 + 1] - *(int*)((uintptr_t)storageSlots + si * 0x10 + 0xc);
                        }
                        break;
                    }
                }
            }

            int required = *(int*)(*(uintptr_t*)(productData + productStride * 8) + 8 + (uintptr_t)matIdx * 0x10);
            if (existingAmount < required) {
                allInputsAvailable = false;

                // Search for this material in nearby entities
                float searchPos2[3] = { 0.0f, 0.0f, 0.0f };

                // Get building position for search (ModelComponent transform: LEFT)
                uintptr_t bldgPos = reinterpret_cast<uintptr_t>((*citizenIBP)->character->modelComponent);

                mapSize = *mapGrid;
                halfMap = mapSize >> 1;
                int bx = (int)(*(float*)(bldgPos + 0x158) + (float)halfMap);
                int by = (int)(*(float*)(bldgPos + 0x15c) + (float)halfMap);
                if (bx < 0) bx = 0;
                if (bx >= mapSize) bx = mapSize - 1;
                if (by < 0) by = 0;
                if (by >= mapSize) by = mapSize - 1;

                ComponentEntity* foundStorage = nullptr;
                Pf_PickBestDropTarget_8d590(
                    reinterpret_cast<int64_t**>(worldEntityList), reinterpret_cast<int64_t*>(&foundStorage),
                    mapSize * by + bx, matEntity2, 1, '\0', reinterpret_cast<uint32_t*>(searchPos2),
                    &field_10);

                if (foundStorage != nullptr) {
                    // Found storage -- check if it has the material available
                    StorageComponent* fStorage = foundStorage->FindComponent<StorageComponent>();

                    // Check if storage has the material
                    int fSlotCount = fStorage->slotCount;
                    if (fSlotCount > 0) {
                        uintptr_t* fSlots = (uintptr_t*)fStorage->storageSlots;
                        for (int fi = 0; fi < fSlotCount; fi++) {
                            if (fSlots[fi * 2] == matEntity2) {
                                if (fi >= 0 && (int)fSlots[fi * 2 + 1] != *(int*)((uintptr_t)fSlots + fi * 0x10 + 0xc)) {
                                    // Material available -- find storage on the citizen's entity
                                    StorageComponent* homeStorage2 =
                                        (*citizenIBP)->_entity->FindComponent<StorageComponent>();

                                    int hs2Count = fn_StorageComponent_QueryCount(homeStorage2);
                                    if (hs2Count < 1) {
                                        // Storage empty -- set up path to fetch material
                                        ComponentEntity* fetchDest = foundStorage;
                                        float fetchPos[4] = {};
                                        Pf_EntityResolvePos_2e20(posComp, fetchPos, 0, slot, nullptr);

                                        ComponentEntity* pathSrc = field_10;

                                        // arg2 is the CITIZEN entity IBP, not the world entity
                                        // list. FindPath immediately does *(*arg2 + 0x278) to
                                        // reach the CitizenAIComponent and clean its task list;
                                        // handing it (GameWorld+0xf0) walked a non-canonical
                                        // "list" pointer and faulted in TaskListCleanup.
                                        // Retail (0x1400e87b9): [RBP] = *R12 (= *entityIBP) and
                                        // LEA RDX,[RBP] — the same &(*entityIBP) the sibling
                                        // call at 0x1400e83f4 passes.
                                        CitizenComponent* pathCitizen = *citizenIBP;

                                        WorkAct_GatherRawMatCtor_bf790(
                                            &foundStorage, &pathCitizen,
                                            field_20, reinterpret_cast<uint64_t*>(&pathSrc),
                                            (float*)&fetchPos, reinterpret_cast<void**>(&fetchDest), searchPos2,
                                            (int64_t)matEntity2, -1, slot, 1, '\0', 0, 0);

                                        int priority2 = field_20;
                                        return FinalizeAction(this, citizenIBP, &fetchDest, priority2, slot);
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        // --- Third pass: check assigned positions for validity ---
        // Retail 0x1400e7840 bounds BOTH position loops below by *(int*)(self+0x70) — the
        // status INT ARRAY's own count (param_1[0xe]) — not by the citizen-slot count at
        // +0x60 (param_1[0xc]). The port used field_60: a workplace with more worker slots
        // than produce positions (e.g. 20 vs 18) then read and WROTE statusArray[18..19],
        // which is the next heap block's boundary tag (+1 then +2 in its high dword, the
        // "free-list link with wrong high bits" teardown crash in the 800x10 stress run,
        // caught 2026-09-04 by a hardware watchpoint: Execute wrote 1, OnComplete wrote 2).
        int totalPositions = (int)field_70;  // number of output positions (= int array count)
        uint32_t firstFreePos = 0xffffffff;
        int firstFreeIndex = -1;
        uint32_t freeCount = 0;

        for (int posIdx = 0; posIdx < totalPositions; posIdx++) {
            float posVec[3];
            Pf_EntityResolvePos_2e20(posComp, posVec, 2, posIdx, nullptr);

            mapSize = *mapGrid;
            halfMap = mapSize >> 1;
            int px = (int)(posVec[0] + (float)halfMap);
            int py = (int)(posVec[1] + (float)halfMap);
            if (px < 0) px = 0;
            if (px >= mapSize) px = mapSize - 1;
            if (py < 0) py = 0;
            if (py >= mapSize) py = mapSize - 1;
            int posCell = py * mapSize + px;

            // Check if position has status 2 but no structure (Map cells: LEFT)
            int* statusArray = field_68;
            uintptr_t structBase = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
            if (statusArray[posIdx] == 2 &&
                *(uintptr_t*)(structBase + 8 + (int64_t)posCell * 0x30) == 0) {
                statusArray[posIdx] = 0;
            }

            if (statusArray[posIdx] == 0) {
                freeCount++;
                if ((int)firstFreePos < 0) {
                    firstFreePos = (uint32_t)posIdx;
                    firstFreeIndex = posCell;
                }
            }
        }

        // If all positions are free, mark as not producing
        if (freeCount == (uint32_t)totalPositions) {
            field_48 = 0;
        }

        // If no free position found, mark as producing
        if ((int)firstFreePos < 0) {
            field_48 = 1;
        } else if (field_48 == 0 && allInputsAvailable) {
            goto setup_produce_action;
        }

        // --- Check each position for workplace assignment ---
        for (int posIdx2 = 0; posIdx2 < totalPositions; posIdx2++) {
            float checkPos[3];
            Pf_EntityResolvePos_2e20(posComp, checkPos, 2, posIdx2, nullptr);

            mapSize = *mapGrid;
            halfMap = mapSize >> 1;
            int cx = (int)(checkPos[0] + (float)halfMap);
            int cy = (int)(checkPos[1] + (float)halfMap);
            if (cx < 0) cx = 0;
            if (cx >= mapSize) cx = mapSize - 1;
            if (cy < 0) cy = 0;
            if (cy >= mapSize) cy = mapSize - 1;
            int checkCell = cy * mapSize + cx;

            uintptr_t structBase2 = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
            ComponentEntity* structEntity = *(ComponentEntity**)(structBase2 + 8 + (int64_t)checkCell * 0x30);

            if (structEntity != nullptr) {
                if (structEntity != field_10) {
                    // Found a different entity at this position -- its WorkComponent
                    WorkComponent* locComp = structEntity->FindComponent<WorkComponent>();

                    CitizenComponent* citizenRef3 = *citizenIBP;
                    // GameUpdateLoop +0x118 (still void*): LEFT
                    uintptr_t assignMgr = (uintptr_t)gameWorld->m_tutorial;
                    uint64_t assigned = WorkGrid_TryAssignWorkplace_f8620(
                        assignMgr, &citizenRef3, locComp->workId, '\x01');
                    if ((char)assigned != '\0') {
                        return 2;
                    }
                }
            }
        }

        if ((int)firstFreePos < 0) {
            return 1;
        }

    setup_produce_action:
        // --- Set up produce action ---
        // Find StatusIconComponent again and toggle state 3
        StatusIconComponent* statusIcon2 = field_10->FindComponent<StatusIconComponent>();
        CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>(statusIcon2), 3, !allInputsAvailable ? '\x01' : '\0');

        if (allInputsAvailable) {
            // Get position for the output slot
            float producePos[3];
            float produceHeading[2] = {};
            Pf_EntityResolvePos_2e20(posComp, producePos, 0, slot, produceHeading);

            mapSize = *mapGrid;
            halfMap = mapSize >> 1;
            int prodCellX = (int)(producePos[1] + (float)halfMap);
            int prodCellY = (int)(producePos[0] + (float)halfMap);
            if (prodCellY < 0) prodCellY = 0;
            if (prodCellY >= mapSize) prodCellY = mapSize - 1;
            if (prodCellX < 0) prodCellX = 0;
            if (prodCellX >= mapSize) prodCellX = mapSize - 1;

            // Recalculate map zones
            Pf_ZoneRefresh_9c480(mapGrid);

            // Get cell structs for passability check (Map cells: LEFT)
            uintptr_t structBase3 = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
            uintptr_t srcCellStruct = structBase3 + (int64_t)citizenCell * 0x30;
            uintptr_t dstCellStruct = structBase3 + (int64_t)(prodCellX * mapSize + prodCellY) * 0x30;

            uint32_t srcMask = 1 << (*(uint8_t*)(dstCellStruct + 0x29) & 0x1f);
            uint32_t dstMask = 1 << (*(uint8_t*)(srcCellStruct + 0x29) & 0x1f);
            uint32_t combined = srcMask | dstMask;
            uint32_t passFlags = combined | 0xe1f;

            if ((passFlags & dstMask) != 0 && (passFlags & srcMask) != 0 &&
                ((combined & 0x1e0) != 0 ||
                 (*(int16_t*)(srcCellStruct + 0x26) != 0 &&
                  *(int16_t*)(dstCellStruct + 0x26) != 0 &&
                  *(int16_t*)(srcCellStruct + 0x26) == *(int16_t*)(dstCellStruct + 0x26)))) {

                // Check if enough consume cycles have passed
                char canFinish = '\0';
                if (cpDesc->_i40 <= field_4c) {
                    canFinish = '\x01';
                    field_4c -= cpDesc->_i40;

                    // Increment produce counter on ConsumeProduceComponent
                    ConsumeProduceComponent* smComp3 = field_10->FindComponent<ConsumeProduceComponent>();
                    smComp3->consumeState += 1;
                }

                // Create work action chain
                CitizenComponent* citizenRef4 = *citizenIBP;
                ComponentEntity* workEntityRef = field_10;
                // The tree's read of the qword at +0x50, truncated to int: field_50
                int actionPriority = field_50;

                WorkAct_ConsumeProduceCtor_bef70(
                    &citizenRef4,
                    field_20,
                    &workEntityRef,
                    producePos,
                    produceHeading[0],
                    reinterpret_cast<int64_t>(field_10->_description),
                    *(uint64_t*)(productData + 0x10 + productStride * 8),
                    (uint32_t)canFinish,
                    (int*)&actionPriority,
                    (uint32_t)field_54,
                    firstFreeIndex,
                    cpDesc->_f44,
                    (canFinish ? 0x8000 : 0) | ((firstFreePos | 0x100) << 8) | slot);

                // Update status array for the position
                if (canFinish != '\0') {
                    field_68[(int)firstFreePos] = 1;
                }

                int priority3 = field_20;
                return FinalizeAction(this, citizenIBP, &actionPriority, priority3, slot);
            }
        }
        return 1;
    }

    // ============================================================
    // BRANCH: Depleted resources or threshold not met
    // ============================================================
    if ((char)canProduce != '\0') {
    handle_depleted_or_idle:
        // Nothing to do here -- fall through
        (void)0;
    }

    // ResetProductionState IS 0x14012b310, and b310 is not a passive "reset": it drives
    // the ResourceProduce state machine 13 -> 14 (PAUSED). State 14's enter calls
    // SetAccepting(wpc, false), which raises the "work is disabled at this location"
    // sign AND evicts the entire worker roster — permanently, because state 14 has no
    // per-frame handler and the only 14 -> 13 requester is the panel's resume event.
    //
    // Retail GUARDS this call; we did not. At 0x1400e8eca:
    //     TEST AL,AL
    //     JNZ  0x1400e8ee1        ; skip straight to the SetActiveState(3,0) tail
    //     CALL 0x14012b310        ; only reached when AL == 0
    // AL is helper_93a0 (0x1401193a0) — our `canProduce`. So the (depleted &&
    // canProduce) arm must SKIP b310 and only set model state 3, keeping its workers.
    //
    // Symptom this caused: a woodcutter works normally until the depletion test flips
    // (~500-1000 frames), is still under its produce limit (canProduce=1), and where
    // retail would idle the model and keep the roster, we paused the building and
    // threw the worker back to laborer. The `[CPWork] canProduce=1 depleted=1` line
    // logged immediately before State14Enter is exactly this arm.
    //
    // The !depleted && !canProduce path (the goto above) still reaches b310, since
    // canProduce == 0 there — which is retail's intent: nothing to make, so stand down.
    if ((char)canProduce == '\0') {
        WorkLeaf_ConsumeProduce_b310(reinterpret_cast<uintptr_t>(smComp));
    }

    // Set active state = 3, disabled
    CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>(statusIcon), 3, '\0');

    return 1;
}

// ============================================================
// OnComplete (0x1400e77c0) -- vtable slot 6
// Stores null IBP into the assigned citizen slot, updates state tracking.
// param4 encodes: bits [6:0] = slot index, bit 15 = update int array,
//   bits [14:8] = int array index, bit 16 = increment field_4c
// ============================================================
int ConsumeProduce::OnComplete(CitizenComponent** /*param2*/, ComponentEntity** /*param3*/, int arg3) {
    const uint32_t param4 = static_cast<uint32_t>(arg3);
    // Clear the citizen assignment: store null IBP into slot
    uint32_t slotIdx = param4 & 0x7F;
    field_58[slotIdx] = nullptr;   // the InternalBasePtr ctor with null

    // If bit 15 set, mark the int array slot as state 2 (completed)
    if ((param4 >> 15) & 1) {
        int* intArray = field_68;
        uint32_t intIdx = (param4 >> 8) & 0x7F;
        // TEARDOWN_WILD_WRITE.md: retail (0x1400e77c0) stores this 2 unbounded, and a
        // 4-mod-8 store of a 2 past a small int array is exactly the teardown damage.
        // Report an index past the array's own count BEFORE the faithful store below,
        // with everything needed to say how the index and the array came apart.
        if ((int)intIdx >= field_70 || slotIdx >= (uint32_t)field_60) {
            std::fprintf(stderr,
                "[WILDWRITE] ConsumeProduce::OnComplete OUT OF RANGE: work=%p entity=%p "
                "workId=%d param4=0x%x -> posIdx=%u (count %d, cap %d, array %p) "
                "slotIdx=%u (count %d)\n",
                (void*)this, field_10, field_20, param4, intIdx,
                field_70, field_74, (void*)intArray, slotIdx, field_60);
            RedzoneAlloc_PrintBacktrace("ConsumeProduce::OnComplete");
        }
        intArray[intIdx] = 2;
    }

    // If bit 16 set, increment the produce counter
    if (param4 & 0x10000) {
        field_4c += 1;
    }

    return 0;
}

// ============================================================
// OnCancel (0x1400e7780) -- vtable slot 7
// Delegates to OnComplete with bit 16 cleared (no produce increment).
// ============================================================
int ConsumeProduce::OnCancel(CitizenComponent** param2, ComponentEntity** param3, int arg3) {
    const uint32_t param4 = static_cast<uint32_t>(arg3);
    // Retail (disassembly): copy *R8 and *RDX to stack slots, BTR R9D,0x10, then
    // dispatch vtable +0x30 = slot 6 OnComplete(&copy2, &copy3, R9) and return
    // its result.
    uint32_t modifiedParam4 = param4 & 0xFFFEFFFF;  // clear bit 16
    ComponentEntity* copy3 = *param3;
    CitizenComponent* copy2 = *param2;
    return OnComplete(&copy2, &copy3, static_cast<int>(modifiedParam4));
}

// ============================================================
// Cleanup (0x1400e8f30) -- vtable slot 8
// Iterates assigned citizens in the IBP array, clears their task state.
// ============================================================
int ConsumeProduce::Cleanup() {
    int count = field_60;
    if (count <= 0) return 0;

    CitizenComponent** ibpArray = field_58;

    for (int i = 0; i < count; i++) {
        if (ibpArray[i] == nullptr) continue;

        // CheckPtr on the citizen IBP; its CharacterComponent (+0x278)
        CharacterComponent* character = ibpArray[i]->character;

        // Call task list cleanup on the character's action list (+0x48)
        WorkBase_TaskListCleanup_c5400(&character->actions);
    }
    return 0;
}

// ============================================================
// Validate (0x1400e7530) -- vtable slot 9
// Checks if all assigned citizen tasks are still valid.
// Returns: 1 = all valid, 0 = some invalid (with cleanup).
// ============================================================
int ConsumeProduce::Validate() {
    int count = field_60;
    if (count <= 0) {
        // No assigned citizens -- return valid
        return 1;
    }

    CitizenComponent** ibpArray = field_58;
    bool allValid = true;
    int validCount = 0;
    int* tempTypes = nullptr;  // dynamic array of type IDs per valid citizen
    int tempCapacity = 0;

    for (int i = 0; i < count; i++) {
        CitizenComponent* entityIBP = ibpArray[i];
        if (entityIBP == nullptr) continue;

        // Validate task for this citizen
        uintptr_t taskResult = 0;
        CitizenComponent* ibpBuf = entityIBP;
        uint8_t valid = WorkBase_ValidateTask_f7100(this, nullptr, &ibpBuf, (void**)&taskResult);

        if (valid == 0) {
            // Task invalid -- clear citizen from slot (the InternalBasePtr ctor with null)
            ibpArray[i] = nullptr;
            allValid = false;
        } else {
            // Task valid -- track the type for cross-referencing
            int typeTag = *(int*)(taskResult + 0x34);

            int newCount = validCount + 1;
            if (newCount != validCount) {
                int newCap = tempCapacity;
                if (tempCapacity < newCount) {
                    newCap = newCount;
                    if (validCount != 0) {
                        newCap = newCount * 3;
                    }
                    int* newArr = (int*)System::MemoryAllocator::MemAlloc((uint64_t)newCap * 4, 4);
                    // Copy old data if buffer changed
                    int startIdx = (tempTypes == newArr) ? validCount : 0;
                    for (int j = startIdx; j < newCount; j++) {
                        if (j < validCount && tempTypes != newArr) {
                            newArr[j] = tempTypes[j];
                        } else if (j >= validCount) {
                            newArr[j] = (typeTag >> 4) & 0xF;
                        }
                    }
                    if (tempTypes != newArr) {
                        System::MemoryAllocator::MemFree(tempTypes);
                    }
                    tempTypes = newArr;
                    tempCapacity = newCap;
                } else {
                    // Buffer has capacity -- just set the new element
                    if (tempTypes) {
                        tempTypes[validCount] = (typeTag >> 4) & 0xF;
                    }
                }
                validCount = newCount;
            }
        }
    }

    // If some citizens were invalidated, update the int array state
    if (!allValid) {
        int* intArray = field_68;
        int intCount = field_70;
        for (int k = 0; k < intCount; k++) {
            // Check if this slot's type is still present in valid citizens
            bool found = false;
            for (int j = 0; j < validCount; j++) {
                if (tempTypes[j] == k) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // No valid citizen handles this slot -- mark as needs-redo
                if (intArray[k] == 1) {
                    intArray[k] = 2;
                }
            }
        }
    }

    System::MemoryAllocator::MemFree(tempTypes);
    return allValid ? 1 : 0;
}

// ============================================================
// GetTarget (0x1400e8fb0) -- vtable slot 10
// Returns the target position from the entity's position component.
// Hidden return ptr ABI: RCX=self, RDX=retbuf
// ============================================================
Profession** ConsumeProduce::GetTarget(Profession** retbuf) {
    // CheckPtr on the entity IBP at +0x10

    // Find the WorkPlaceComponent (type 0x1401c80e0) on the entity
    WorkPlaceComponent* workPlace = field_10->FindComponent<WorkPlaceComponent>();

    // CheckPtr on the found component

    // The workplace's profession (+0x48) into the return slot
    *retbuf = workPlace->profession;

    return retbuf;
}

// Slot11 (+0x58) — retail 0x14002a0f0 is `return 0 < *(int*)(self + 0x60)` (the shared
// body ClearStorage also uses: tree count at +0x60, not the +0x50 the generic
// Work_Slot11_HasActiveTasks reads). Was pinned to 0 by Work_SlotRet0 (2026-09-05).
int ConsumeProduce::GetPriority() {
    return field_60 > 0 ? 1 : 0;
}

// Slot12 (+0x60) — retail 0x1400e9010, this class's own `RET 0` (every other Work
// table shares 0x140006b10). The same behaviour; declared so the table matches.
void ConsumeProduce::Slot12(int /*version*/) {}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_ConsumeProduce_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over ConsumeProduce_Factory / _CreateInPlace.

// create 0x14002a150: MemAlloc(sizeof, 8), then the ctor.
void* ConsumeProduce::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ConsumeProduce), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ConsumeProduce());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* ConsumeProduce::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ConsumeProduce());
}


#include "Object/factory_registration.h"

DEFINE_EXE_TYPE(ConsumeProduce, "Work")
