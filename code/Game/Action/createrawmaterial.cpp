// code/Game/Action/createrawmaterial.cpp
// Clean C++ reimplementation of CreateRawMaterial action methods.
// Source: Game/Action/CreateRawMaterial.cpp (from profile build debug strings)
//
// CreateRawMaterial -- 0x58 bytes. Spawns raw material entities from a resource building.
// Inherits: Object::Internal -> Action (0x20) -> CreateRawMaterial
// Vtable at 0x14015fd10 (10 slots).
//
// Fields from Serialize:
//   +0x20: description entity (ExternalPtr/type ref, serialized via FUN_140002340)
//   +0x28: InternalBasePtr (source building)
//   +0x30: InternalBasePtr (optional secondary entity)
//   +0x38: InternalBasePtr (optional tertiary entity)
//   +0x40: int (total quantity)
//   +0x44: int (material type index)
//   +0x48: uint (placement flags)
//   +0x4c: float (yield multiplier)
//   +0x50: bool (natural resource flag)
//   +0x51: bool (place near flag)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "CreateRawMaterial.h"
#include "engine_funcs.h"
#include "Action/exe_bodies.h"            // Pf_FindPlacementCell_bd40, ActRm_* helpers
#include "Work/exe_bodies.h"              // WorkAcc_FindComponent_22520, WorkBase_FindComponent_3050
#include "Game/exe_bodies.h"              // Reimpl_ExternalPtr_Serialize_StringTable
#include "Entity/componententity_funcs.h" // ComponentEntity_CtorFromDescription, EntityList_RegisterEntity_Impl
#include "system/MemoryAllocator.h"       // System::MemoryAllocator::MemAlloc
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Component/CitizenComponent.h"
#include "Entity/ComponentEntity.h"
#include "Component/RawMaterialDescription.h"
#include "Component/RawMaterialComponent.h"
#include "Component/WorkPlaceDescription.h"

// ============================================================
// Helpers, called by name (B3R)
//   0x140002340 Reimpl_ExternalPtr_Serialize_StringTable -- serialize StringHash/type ref
//   0x1400c3720 ComponentEntity_CtorFromDescription      -- clone entity from description
//   0x14009bd40 Pf_FindPlacementCell_bd40 -- find placement cell
//               (mapGrid*, cellIndex, naturalResourceFlag, outEntityPtr*, placementFlags)
//   0x140129760 ActRm_PositionEntity_9760 -- position entity at cell with rotation
//   0x140088ef0 ActRm_RegisterRawMaterial_88ef0 -- register raw material with resource tracker
//               (entityList, sourceIBP, placeNearFlag, descEntity, quantity)
//   0x1400c48a0 EntityList_RegisterEntity_Impl -- register entity with entity list
//   0x140022520 WorkAcc_FindComponent_22520 -- get child entities from source building
//               (member ABI: RCX=entity, RDX=retbuf)
//   0x140003050 WorkBase_FindComponent_3050 -- find component on entity (ResourceDescription?)
// ============================================================

// Constant: rotation scale factor (90.0f) at 0x140172204
static const float ROTATION_SCALE = 90.0f;

// ============================================================
// Helper: find RawMaterialDescription component in description entity
// Searches the component array at desc+0x28 (count at desc+0x30)
// for a component matching type TypeId_RawMaterialDescription.
// ============================================================
static void* FindRawMaterialDesc(void* descEntity) {
    uintptr_t desc = (uintptr_t)descEntity;
    int count = *(int*)(desc + 0x30);
    uintptr_t compArray = *(uintptr_t*)(desc + 0x28);

    for (int i = 0; i < count; i++) {
        void* comp = *(void**)(compArray + (int64_t)i * 8);
        // slot 2 Type()
        uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
        if (typeId == RawMaterialDescription::TypeId()) {
            // Verify with IsDerivedFrom
            if (::Object::IsDerivedFromNullSafe(comp, RawMaterialDescription::TypeId())) {
                return comp;
            }
        }
    }
    return nullptr;
}

// ============================================================
// Helper: find component by type ID on new entity
// Searches the component array at entity+0x30 (count at entity+0x38)
// using InternalBasePtr for each component.
// ============================================================
static void* FindComponentOnEntity(int64_t entity, uint32_t typeId) {
    int count = *(int*)(entity + 0x38);
    uintptr_t compArray = *(uintptr_t*)(entity + 0x30);

    for (int i = 0; i < count; i++) {
        void* compIBP = (void*)(compArray + (int64_t)i * 8);
        void* comp = *(void**)compIBP;

        // slot 2 Type()
        uint32_t compTypeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
        if (compTypeId == typeId) {
            comp = *(void**)compIBP;
            if (::Object::IsDerivedFromNullSafe(comp, typeId)) {
                return comp;
            }
        }
    }
    return nullptr;
}

// ============================================================
// Serialize (0x1400c6f30)
// Versioned serialization with version 3 serializer.
// Version gates at 0, 1, 2, and 3 control which fields are serialized.
// ============================================================
void CreateRawMaterial::Persist(void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status (version 0)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize resource data (version 3)
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 3);   // B.3.4 bridge

    // Type reference (+0x20)
    Reimpl_ExternalPtr_Serialize_StringTable(s + 0x20, ser2);   // 0x140002340

    // Source building IBP (+0x28)
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x28)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (sourceBuilding != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(sourceBuilding, CitizenComponent::TypeId())) {
            sourceBuilding = nullptr;
        }
    }

    // Material type index (+0x44) as int
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(s + 0x44), nullptr);   // B.3.4 bridge

    // Yield multiplier (+0x4c) as float
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(yieldMultiplier, nullptr);   // B.3.4 bridge

    // Natural resource flag (+0x50) as bool
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<bool*>(s + 0x50), nullptr);   // B.3.4 bridge

    // Total quantity (+0x40) as int
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(s + 0x40), nullptr);   // B.3.4 bridge

    // Place near flag (+0x51) as bool
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<bool*>(s + 0x51), nullptr);   // B.3.4 bridge

    // Version > 0: secondary entity IBP (+0x30)
    int version = reinterpret_cast<IO::Serializer*>(ser2)->GetVersion();   // B.3.4 bridge
    if (version > 0) {
        reinterpret_cast<Object::InternalBasePtr*>(s + 0x30)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
        if (secondaryEntity != nullptr) {
            if (!::Object::IsDerivedFromNullSafe(secondaryEntity, ComponentEntity::TypeId())) {
                secondaryEntity = nullptr;
            }
        }
    }

    // Version < 2: default placement flags; >= 2: serialize uint (+0x48)
    version = reinterpret_cast<IO::Serializer*>(ser2)->GetVersion();   // B.3.4 bridge
    if (version < 2) {
        placementFlags = 4;
    } else {
        reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x48), nullptr);   // B.3.4 bridge
    }

    // Version > 2: tertiary entity IBP (+0x38)
    version = reinterpret_cast<IO::Serializer*>(ser2)->GetVersion();   // B.3.4 bridge
    if (version > 2) {
        reinterpret_cast<Object::InternalBasePtr*>(s + 0x38)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
        if (tertiaryEntity != nullptr) {
            if (!::Object::IsDerivedFromNullSafe(tertiaryEntity, ComponentEntity::TypeId())) {
                tertiaryEntity = nullptr;
            }
        }
    }

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}


int  CreateRawMaterial::AbstractImpl(void* actionStack) {
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    // Get description entity at +0x20
    uintptr_t descEntity = *(uintptr_t*)(s + 0x20);

    // Find RawMaterialDescription component in the description
    void* rawMatDesc = FindRawMaterialDesc((void*)descEntity);

    // Check building maturity to determine yield
    uintptr_t building = *(uintptr_t*)(s + 0x28);

    int yield;
    // Compare building age (+0x210) with maturity threshold from description (+0x288 -> +0x118)
    int buildingAge = *(int*)(building + 0x210);
    uintptr_t buildDesc = *(uintptr_t*)(building + 0x288);
    int maturityAge = *(int*)(buildDesc + 0x118);

    if (buildingAge < maturityAge) {
        // Young: use yield at +0x40
        yield = *(int*)((uintptr_t)rawMatDesc + 0x40);
    } else {
        // Mature: use yield at +0x44
        yield = *(int*)((uintptr_t)rawMatDesc + 0x44);
    }

    // Apply yield multiplier
    float yieldMul = yieldMultiplier;
    int totalItems = (int)((float)yield * yieldMul);

    if (totalItems == 0) {
        return 0;
    }

    // Max items per PILE = the carrier's volume capacity / this material's per-unit volume.
    // Retail 0x1400c6c22-2c, verified at instruction level:
    //     MOV EAX,[R15+0x40]     ; R15 = this, +0x40 = ctor arg7
    //     CDQ
    //     IDIV dword ptr [RBX+0x48]  ; RBX = rawMatDesc, +0x48 = per-unit VOLUME
    //     MOV ECX,EAX            ; ECX = cap, loop-invariant
    //     MOV [RBP+0x50],EAX     ; stashed; reloaded into ECX at the loop tail (6c6ef3)
    // then inside the loop at 6c6c40-46: MOV EAX,R13D / CMP R13D,ECX / CMOVG EAX,ECX
    // = min(totalItems, cap). The division is done ONCE, before the loop — matched here.
    //
    // This port previously used rawMatDesc+0x48 (the per-unit volume) DIRECTLY as the cap,
    // i.e. it dropped the division entirely, so every pile produced or culled has been the
    // wrong size since the function was written.
    //
    // action+0x40 is CreateRawMaterial's ctor arg7, and all four retail construction sites
    // pass StorageComponent+0x38 — the carrier's max volume capacity (0x1400bed3f loads it
    // as MOV R12D,[RAX+0x38] and 0x1400bed9f stores it to RSP+0x30 = arg7; our ported site
    // in action_pf_roots_drop.cpp does the same). So this is "how many whole units fit in
    // the hauler", which is why it caps the per-entity batch.
    int unitVolume = *(int*)((uintptr_t)rawMatDesc + 0x48);
    int carrierVolume = *(int*)(s + 0x40);
    // STANDALONE-ONLY TRIPWIRE (retail has no equivalent and needs none). Retail's IDIV
    // relies on +0x48 being non-zero and its cap being >= 1; a cap of 0 makes batchSize 0,
    // the loop tail's `SUB R13D,EDI` subtract nothing, and 0x1400c6efa jump back forever —
    // an in-world FREEZE, not a crash. We can still reach that state because a not-yet-
    // ported construction path would leave +0x40 at its zeroed ctor value, so fail LOUDLY
    // with the operands instead of hanging. If this ever fires, fix the ctor site that
    // passed a zero capacity — do not soften this into a clamp.
    if (unitVolume <= 0 || carrierVolume < unitVolume) {   // i.e. the quotient would be <= 0
        fprintf(stderr, "[CreateRawMaterial] ABORT: batch cap would be 0 — "
                "carrierVolume(action+0x40)=%d unitVolume(rawMatDesc+0x48)=%d totalItems=%d. "
                "Retail would spin here forever; the ctor site that built this action passed "
                "a zero/short capacity as arg7.\n", carrierVolume, unitVolume, totalItems);
        fflush(stderr);
        return 0;
    }
    int maxPerEntity = carrierVolume / unitVolume;

    // Spawn loop: create entities for all items
    while (totalItems > 0) {
        int batchSize = totalItems;
        if (maxPerEntity < totalItems) {
            batchSize = maxPerEntity;
        }

        // Check if natural resource flag needs to be set
        if (*(char*)(s + 0x50) == '\0') {
            int32_t matIdx = materialCellIdx;
            uintptr_t mapGrid = *(uintptr_t*)(gameWorld + 0xf8);
            uintptr_t cellArrayBase = *(uintptr_t*)(mapGrid + 0x80);
            int64_t existingEntity = *(int64_t*)(cellArrayBase + 8 + (int64_t)matIdx * 0x30);
            if (existingEntity != 0) {
                naturalResFlag = 1;
            }
        }

        // Find placement cell
        int32_t matIdx = materialCellIdx;
        int cellIndex;
        if (*(char*)(s + 0x50) != '\0') {
            // Natural resource placement: use helper
            void* thirdEntity = tertiaryEntity;
            cellIndex = Pf_FindPlacementCell_bd40(   // 0x14009bd40
                *(int**)(gameWorld + 0xf8), matIdx, '\x01',
                (int64_t*)&thirdEntity, placementFlags);
        } else {
            cellIndex = matIdx;
        }

        if (cellIndex < 0) {
            // No valid cell found, still consume items
            batchSize = batchSize;  // keep batchSize as is
        } else {
            // Allocate new ComponentEntity from description
            void* newEntityMem = System::MemoryAllocator::MemAlloc(0x48, 8);
            void* newEntity = nullptr;
            if (newEntityMem != nullptr) {
                newEntity = ComponentEntity_CtorFromDescription(static_cast<ComponentEntity*>(newEntityMem), (void*)descEntity);   // 0x1400c3720
            }

            // Wrap in InternalBasePtr
            int64_t entityIBP = 0;
            new (&entityIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(newEntity));   // B.3.3 bridge

            // Find component on new entity (DAT_1401c7c80 type)
            void* comp = FindComponentOnEntity(entityIBP, RawMaterialComponent::TypeId());

            // Wrap component in IBP
            int64_t compIBP = 0;
            new (&compIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(comp));   // B.3.3 bridge

            // Position entity at cell with random rotation
            void* randomPtr = *(void**)(gameWorld + 0xd8);
            int randVal = reinterpret_cast<System::Random*>(randomPtr)->RandTol(2);   // B.3.4b bridge
            ActRm_PositionEntity_9760(compIBP, cellIndex, (float)randVal * ROTATION_SCALE);   // 0x140129760

            // Handle secondary entity (+0x30) if present
            if (*(int64_t*)(s + 0x30) != 0) {
                uintptr_t secondaryEntity = *(uintptr_t*)(s + 0x30);
                uintptr_t secDesc = *(uintptr_t*)(secondaryEntity + 0x28);

                // Search secondary entity's description for DAT_1401c8118 component
                int secCount = *(int*)(secDesc + 0x30);
                for (int i = 0; i < secCount; i++) {
                    void* secComp = *(void**)(*(uintptr_t*)(secDesc + 0x28) + (int64_t)i * 8);
                    uint32_t secTypeId = reinterpret_cast<Object::Base*>(secComp)->Type();   // B.3.8 bridge
                    if (secTypeId == WorkPlaceDescription::TypeId()) {
                        ::Object::IsDerivedFromNullSafe(secComp, WorkPlaceDescription::TypeId());
                        break;
                    }
                }
            }

            // Set quantity on new entity component
            *(int32_t*)(compIBP + 0x28) = batchSize;
            *(int64_t*)(compIBP + 0x30) = 0;


            // Register raw material with resource tracker
            int64_t sourceIBP = *(int64_t*)(s + 0x30);
            ActRm_RegisterRawMaterial_88ef0(   // 0x140088ef0
                *(int64_t*)(gameWorld + 0xf0), &sourceIBP,
                *(char*)(s + 0x51), descEntity, batchSize);

            // Register entity with entity list
            int64_t entityRef = entityIBP;
            int typeGroup = *(int*)(descEntity + 0x20);
            EntityList_RegisterEntity_Impl(*(void**)(gameWorld + 0xe8), &entityRef, typeGroup);   // 0x1400c48a0

            // Handle place-near behavior: link to secondary entity
            if (*(char*)(s + 0x51) != '\0' && *(int64_t*)(s + 0x30) != 0) {
                int64_t childIBP = 0;
                WorkAcc_FindComponent_22520(*(uintptr_t*)(s + 0x30), &childIBP);   // 0x140022520
                if (childIBP != 0) {
                    void* rdCompIBP = nullptr;
                    WorkBase_FindComponent_3050(static_cast<uintptr_t>(entityIBP), &rdCompIBP);   // 0x140003050
                    uint32_t rdValue = *(uint32_t*)(*(uintptr_t*)&rdCompIBP + 0x28);

                    // B.3.2 bridge: FUN_140044810 == Collection::Array<int32>::
                    // SetSize(n, fill) (3-arg form; ActRm_DynArrayPush4_44810 in
                    // action_base_rawmat.cpp), gated on the byte flag at +0x28 and
                    // appending rdValue as the new tail element.
                    if (*reinterpret_cast<char*>(childIBP + 0x28) != 0) {
                        int newCount = *reinterpret_cast<int*>(childIBP + 0x38) + 1;
                        GameColl::AsArray<GameColl::I32>(reinterpret_cast<void*>(childIBP + 0x30))
                            .SetSize(newCount, GameColl::I32{ static_cast<int32_t>(rdValue) });
                    }
                }
            }
        }

        totalItems -= batchSize;
    }

    return 0;
}

// ============================================================
// GetTarget (0x1400c6aa0)
// Returns true if param matches entity at +0x28->+0x10, or +0x30, or +0x38.
// ============================================================
bool CreateRawMaterial::GetTarget(void* param) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    // Check +0x28 -> building entity -> +0x10 (the actual target entity)
    uintptr_t building = *(uintptr_t*)(s + 0x28);
    uintptr_t targetEntity = *(uintptr_t*)(building + 0x10);

    // Use a local IBP to CheckPtr the target
    uintptr_t targetIBP = targetEntity;

    if (*(uintptr_t*)param == targetIBP) {
        return 1;
    }

    // Check +0x30 (secondary entity)
    if (*(int64_t*)(s + 0x30) == *(int64_t*)param) {
        return 1;
    }

    // Check +0x38 (tertiary entity)
    if (*(int64_t*)(s + 0x38) == *(int64_t*)param) {
        return 1;
    }

    return 0;
}

// ============================================================
// class CreateRawMaterial (CreateRawMaterial.h) — ENDGAME B.3.8 ACT.
// The hand-built g_CreateRawMaterial_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x1400222d0 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
CreateRawMaterial::CreateRawMaterial()
    : descEntity(nullptr),
      sourceBuilding(nullptr),
      secondaryEntity(nullptr),
      tertiaryEntity(nullptr),
      totalQuantity(0),
      materialCellIdx(0),
      placementFlags(0),
      yieldMultiplier(0.0f),
      naturalResFlag(0),
      placeNearFlag(0),
      _pad52{} {}

// [3] 0x1400c6f30
// [4] 0x1400c6b40
// [9] 0x1400c6aa0

void* CreateRawMaterial::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CreateRawMaterial), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CreateRawMaterial());
}
void* CreateRawMaterial::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CreateRawMaterial());
}


DEFINE_EXE_TYPE(CreateRawMaterial, "Action")
