// code/Game/Work/buildroad.cpp
// Clean C++ reimplementation of BuildRoad Work class methods.
// Source: Game/Work/BuildRoad.cpp (from profile build debug strings)
//
// BuildRoad — road construction work action. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> BuildRoad
// Ctor: 0x1400e4ad0, Vtable: 0x140161020
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/BuildRoad.h"
#include "buildroad_funcs.h"
#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "math/Math.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: StorageComponent_QueryAvailable, CitizenAI_State_LongTermPlan
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 D5: CitizenComponent::TypeId()
#include "Component/CharacterComponent.h"
#include "Component/ModelComponent.h"
#include "Component/RawMaterialComponent.h"
#include "Component/NaturalResourceComponent.h"
#include "Component/WorkComponent.h"
#include "Component/StorageComponent.h"
#include "Entity/ComponentEntity.h"
#include "Game/ProfessionList.h"

// ============================================================
// Helpers this file reaches (ENDGAME B.3.8, B38_RULES D7)
// ============================================================
// The file used to keep six function-pointer statics filled by a lazy, non-atomic
// `BuildRoad_ResolveFuncs()` (called from the factory, the 8-argument ctor door and
// the dtor — i.e. from both the loader and the main thread). Every one is gone:
//   Object::Internal ctor/dtor (IAT 0x140152768/70) -> the compiler's base chain
//   Work::Serialize 0x1400f7400                     -> Work::Persist (qualified)
//   Work::ValidateTask 0x1400f7100                  -> WorkBase_ValidateTask_f7100
//   task-list cleanup 0x1400c5400                   -> WorkBase_TaskListCleanup_c5400
//     (both ported in work_base_reimpl.cpp and registered at those addresses by
//      standalone/reg_work_base.cpp — the function ResolveAddr answered)
//   type-id cell 0x1401c7158 (CitizenComponent)     -> read where it is used
extern uint8_t WorkBase_ValidateTask_f7100(void* self, void* citizenAI, void* taskIBP, void** out);
extern void    WorkBase_TaskListCleanup_c5400(void* self);

// ============================================================
// Constructor (0x1400e4ad0)
// Object::Internal::Internal() -> Work fields -> BuildRoad vtable + fields. The two
// stamps are the compiler's. +0x60..+0x68 are not written (retail's ctor leaves
// them to Persist / the 8-argument ctor, as the old C ctor did).
// ============================================================
BuildRoad::BuildRoad()
    : Work(),
      field_48(nullptr), field_50(nullptr), field_58(-1), field_5c(2) {}

// ============================================================
// Parameterised constructor (0x1400e4b50)
// This is the ctor RoadTool's commit path uses (FUN_14009c960): it chains the shared
// Work base ctor (Work_Ctor25 @ 0x1400f72e0) with the owner InternalBasePtr, installs
// the BuildRoad vtable, and seeds the cell + road-description fields.
//
// Retail copies *ownerIbp into a LOCAL and passes that local to Work_Ctor25 — the
// caller's IBP is not handed over directly, and 9c960 goes on to reuse its copy for the
// pending-road BST insert. Work(void*) reads the value once and never writes through
// the pointer, so the caller's copy is untouched either way.
// ============================================================
BuildRoad::BuildRoad(uint64_t* ownerIbp, uint32_t cellIdx, uint32_t mapDescVal,
                     uint64_t roadDescExtra, uint32_t roadTypeExisting,
                     uint32_t roadState, uint32_t terrainType)
    : Work(ownerIbp),
      field_48(nullptr),
      field_50(reinterpret_cast<void*>(roadDescExtra)),
      field_58(static_cast<int32_t>(cellIdx)),
      field_5c(static_cast<int32_t>(mapDescVal)),
      field_60(static_cast<int32_t>(roadTypeExisting)),
      field_64(static_cast<int32_t>(roadState)),
      field_68(static_cast<int32_t>(terrainType)) {
    field_28 = static_cast<int32_t>(cellIdx);   // retail also mirrors the cell into the Work base
}

// The by-address door RoadTool's commit path calls (roadtool_calcpath.cpp resolves
// 0x1400e4b50): the real ctor on the caller's memory (B8-5 — the same bytes).
void* BuildRoad_CtorParams_Impl(BuildRoad* self, uint64_t* ownerIbp, uint32_t cellIdx,
                                uint32_t mapDescVal, uint64_t roadDescExtra,
                                uint32_t roadTypeExisting, uint32_t roadState,
                                uint32_t terrainType) {
    if (!self) return nullptr;
    return static_cast<Object::Base*>(new (static_cast<void*>(self)) BuildRoad(
        ownerIbp, cellIdx, mapDescVal, roadDescExtra, roadTypeExisting, roadState, terrainType));
}

// ============================================================
// Serialize (0x1400e4980)
// Work_Serialize + Serializer(version=1) + serialize fields
// Serializes: road type (+0x5c via stream vtable +0x98), cell index (+0x58),
//   entity ref (+0x48), building desc (+0x50), material fields (+0x60..+0x68)
// ============================================================
void BuildRoad::Persist(IO::Stream& stream) {
    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create IO::Serializer with version 1 (destroyed at the end, as retail)
    IO::Serializer serializer(stream, 1);

    // Serialize road type (+0x5c) through the UINT door. Serializer+0x08 is the
    // READERWRITER, not the stream (the Stream is at +0x00), and its vtable
    // +0x98 is rw_vtbl::kScalarUInt. The old two-argument call form left the
    // `const char* name` register unset; every retail body ignores it, so
    // passing nullptr is wire-identical.
    serializer.Serialize(*reinterpret_cast<unsigned int*>(&field_5c), nullptr);   // an int through the uint door (0x98), as retail

    // Serialize cell index (+0x58) via IO::Serializer::Serialize
    serializer.Serialize(field_58, nullptr);

    // InternalBasePtr::Serialize for entity ref (+0x48)
    reinterpret_cast<Object::InternalBasePtr*>(&field_48)->Serialize(serializer);   // B.3.4 bridge

    // Validate entity ref type after deserialize
    if (field_48 != nullptr) {
        // TypeId at DAT_1401c7158 = CitizenComponent (ENDGAME B.3.9 D5: by cell).
        bool valid = Object::IsDerivedFromNullSafe(field_48, CitizenComponent::TypeId());
        if (!valid) {
            field_48 = nullptr;
        }
    }

    // ExternalPtr serialize for building description (+0x50)
    Reimpl_ExternalPtr_Serialize_StringTable(&field_50, &serializer);

    // Version-dependent fields
    int version = serializer.GetVersion();
    if (version < 1) {
        // Default values for old save format
        field_60 = 0xff;
        field_64 = 0;
        field_68 = 2;
    } else {
        // Serialize material fields
        serializer.Serialize(field_60, nullptr);
        serializer.Serialize(field_64, nullptr);
        serializer.Serialize(field_68, nullptr);
    }
}

// ============================================================
// OnComplete (0x1400e4150) — slot 6
// Clears target entity ref, returns (param4 == 1).
// ============================================================
int BuildRoad::OnComplete(CitizenComponent** /*citizen*/, ComponentEntity** /*arg2*/, int param4) {
    // Clear the citizen ref (the InternalBasePtr ctor with null)
    field_48 = nullptr;

    return (param4 == 1) ? 1 : 0;
}

// ============================================================
// OnCancel (0x1400e4120) — slot 7
// Clears target entity ref.
// ============================================================
int BuildRoad::OnCancel(CitizenComponent**, ComponentEntity**, int) {
    field_48 = nullptr;   // the InternalBasePtr ctor with null
    return 0;
}

// ============================================================
// Cleanup (0x1400e4910) — slot 8
// If target entity exists, clean up its pending task list.
// Walks entity -> +0x278 (StateMachineComponent) -> +0x48 (task list)
// ============================================================
int BuildRoad::Cleanup() {
    if (field_48 == nullptr) return 0;

    // The citizen's CharacterComponent (+0x278) action list (+0x48)
    WorkBase_TaskListCleanup_c5400(&field_48->character->actions);
    return 0;
}

// ============================================================
// Validate (0x1400e40c0) — slot 9
// Checks if the assigned task (from field_48) is still valid.
// Uses shared Work::ValidateTask helper (FUN_1400f7100).
// ============================================================
int BuildRoad::Validate() {
    CitizenComponent* entityRef = field_48;

    if (entityRef != nullptr) {
        // Call shared validate helper with a copy of the citizen ref
        CitizenComponent* ibpCopy = entityRef;
        uint8_t result = WorkBase_ValidateTask_f7100(this, nullptr, &ibpCopy, nullptr);
        if (result == 0) {
            // Task invalid — clear the citizen ref
            field_48 = nullptr;
            return 0;
        }
    }
    return 1;
}

// ============================================================
// GetTarget (0x1400e4960) — slot 10
// Returns the game world's road build target entity.
// Uses hidden return pointer (member function ABI: RCX=this, RDX=retbuf).
// NOT HOOKED — RIP-relative prologue (bug class 2).
// ============================================================
Profession** BuildRoad::GetTarget(Profession** retbuf) {
    // The profession list's +0x38 profession (GameUpdateLoop::m_professionList)
    *retbuf = GameUpdateLoop::_singleton->m_professionList->field_0x38;
    return retbuf;
}

// ============================================================
// GetPriority (0x1400295a0) — slot 11
// Returns true (1) if field_48 is non-null, false (0) otherwise.
// NOT HOOKED — only 10 bytes total, too small.
// ============================================================
int BuildRoad::GetPriority() {
    return (field_48 != nullptr) ? 1 : 0;
}

// ============================================================
// Execute (0x1400e4190) — slot 4
// Road construction work logic. Determines what the citizen
// needs to do next: build directly, haul resources, or assign
// to a workplace.
// Returns: 1 = retry/not ready, 2 = action pushed
// ============================================================

// Helper: the citizen's position data: CharacterComponent (+0x278) -> ModelComponent
// (+0x68). The transform is raw storage (H5): its float reads are LEFT.
static uintptr_t GetPositionData(CitizenComponent** entityIBP) {
    return reinterpret_cast<uintptr_t>((*entityIBP)->character->modelComponent);
}

// Helper: convert position data to clamped map cell index
static int PosToCellIndex(int* mapGrid, uintptr_t posData) {
    int mapSize = *mapGrid;
    int halfMap = mapSize >> 1;
    float posX = *(float*)(posData + 0x158);
    float posY = *(float*)(posData + 0x15c);
    int cellX = (int)(posX + (float)halfMap);
    int cellY = (int)(posY + (float)halfMap);
    if (cellX < 0) cellX = 0;
    if (cellX >= mapSize) cellX = mapSize - 1;
    if (cellY < 0) cellY = 0;
    if (cellY >= mapSize) cellY = mapSize - 1;
    return mapSize * cellY + cellX;
}

// Helper: convert cell index to world-space Vector3
static Math::Vector3 CellToWorldVec3(int* mapGrid, int cellIndex) {
    int mapSize = *mapGrid;
    float halfOffset = (float)(mapSize >> 1) - 0.5f;
    float cellX = (float)(cellIndex % mapSize) - halfOffset;
    float cellY = (float)(cellIndex / mapSize) - halfOffset;
    return Math::Vector3(cellX, cellY, 0.0f);
}

// Shared epilogue: finalize action on citizen entity
static int FinalizeAction(BuildRoad* self, CitizenComponent** entityIBP, void* actionIBP, int priority) {
    CitizenComponent* citizenEntity = *entityIBP;

    Profession* actionPtr = *self->GetTarget(static_cast<Profession**>(actionIBP));   // LEFT: the return buffer is the caller's reused scratch slot (FinalizeAction takes it as void*)

    citizenEntity->field_0x30 = priority;
    citizenEntity->statusFlags |= 4;
    citizenEntity->field_0xB0 = 0;

    if (actionPtr == citizenEntity->profession && priority >= 0) {
        citizenEntity->happinessFlags |= 0x20000;
    }

    self->field_48 = *entityIBP;
    return 2;
}

// Road-wave branch counters.
extern "C" {
long long g_brExecStructOnCell = 0;   // cell+0x08 holds an entity (pile / resource)
long long g_brExecRawMatWait   = 0;   // blocked by a raw-material pile it asked to be cleared
long long g_brExecNatResWait   = 0;   // blocked by a natural resource (tree/stone)
long long g_brExecClearPath    = 0;   // the plain "walk there and build it" branch
}

int BuildRoad::Execute(CitizenComponent** entityIBP, System::Time* /*context*/) {
    if (field_48 != nullptr) {
        return 1;
    }

    // (GameUpdateLoop m_game / m_citizenList / +0x118 are still void*: LEFT)
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    int* mapGrid = (int*)gameWorld->m_game;

    uintptr_t posData = GetPositionData(entityIBP);
    int mapSize = *mapGrid;
    uint32_t heightU32 = *(uint32_t*)(posData + 0x160);
    int cellIndex = PosToCellIndex(mapGrid, posData);

    int targetCell = field_58;

    uint64_t passResult = WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, (uint32_t)cellIndex, targetCell);
    if ((char)passResult == '\0') {
        return 1;
    }

    uintptr_t structArrayBase = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
    ComponentEntity* structEntity = *(ComponentEntity**)(structArrayBase + 8 + (int64_t)targetCell * 0x30);

    if (structEntity != nullptr) {
        ++g_brExecStructOnCell;
        RawMaterialComponent* rawMatComp = structEntity->FindComponent<RawMaterialComponent>();

        if (rawMatComp != nullptr) {
            WorkComponent* workComp = structEntity->FindComponent<WorkComponent>();

            // WorkComponent+0x28 = the scheduler work id for the pile blocking this
            // cell, NOT a worker count; it is what gets handed to the scheduler below.
            int workId = workComp->workId;
            CitizenComponent* citizenRef = *entityIBP;

            uintptr_t assignMgr = (uintptr_t)gameWorld->m_tutorial;
            uint64_t assignResult = WorkGrid_TryAssignWorkplace_f8620(assignMgr, &citizenRef, workId, '\0');
            if ((char)assignResult != '\0') {
                return 2;
            }
            return 1;
        }

        NaturalResourceComponent* natResComp = structEntity->FindComponent<NaturalResourceComponent>();

        if (natResComp == nullptr) {
            return 1;
        }
        ++g_brExecNatResWait;

        natResComp = structEntity->FindComponent<NaturalResourceComponent>();

        uint32_t bsFlags = natResComp->_flags;
        if ((bsFlags >> 20) & 1) {
            return 1;
        }

        posData = GetPositionData(entityIBP);
        mapSize = *mapGrid;
        heightU32 = *(uint32_t*)(posData + 0x160);
        cellIndex = PosToCellIndex(mapGrid, posData);

        int adjacentCell = WorkGrid_FindCell_9ccf0(mapGrid, targetCell, cellIndex, 0xe07);
        if (adjacentCell < 0) {
            return 1;
        }

        passResult = WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, (uint32_t)adjacentCell, cellIndex);
        if ((char)passResult == '\0') {
            return 1;
        }

        Math::Vector3 worldPos = CellToWorldVec3(mapGrid, adjacentCell);

        float terrainPos[4] = {};
        Reimpl_MapGetTerrainPos(mapGrid, terrainPos, &worldPos.x, '\0', nullptr);

        NaturalResourceComponent* targetRef = natResComp;
        CitizenComponent* citizenRef = *entityIBP;

        WorkAct_PerformWorkCtor_c1eb0(
            &citizenRef, field_20, &targetRef, terrainPos, 0, 0);

        int priority = field_20;
        return FinalizeAction(this, entityIBP, &targetRef, priority);
    }

    if ((int64_t)field_50 == 0) {
        ++g_brExecClearPath;
        int64_t actionRef = 0;   // a null InternalBasePtr

        float velocity[3] = { 0.0f, 0.0f, 0.0f };
        Math::Vector3 worldPos = CellToWorldVec3(mapGrid, targetCell);

        float terrainPos[4] = {};
        Reimpl_MapGetTerrainPos(mapGrid, terrainPos, &worldPos.x, '\0', nullptr);

        CitizenComponent* citizenRef = *entityIBP;

        WorkAct_PerformWorkCtor_bffd0(
            &citizenRef, field_20, reinterpret_cast<uint32_t*>(terrainPos),
            (uint64_t)(uint32_t)targetCell,
            (uint32_t)field_5c, 0xff, 1, 0, 0,
            velocity, &actionRef);

        int priority = field_20;
        return FinalizeAction(this, entityIBP, &actionRef, priority);

    } else {
        // The citizen's entity (+0x10) storage, through the guarded finder
        StorageComponent* storageComp = nullptr;
        fn_FindComponent_Storage((*entityIBP)->_entity, &storageComp);

        int available = StorageComponent_QueryAvailable(storageComp, (int64_t)field_50);

        if (available > 0) {
            int64_t actionRef = 0;   // a null InternalBasePtr

            float velocity[3] = { 0.0f, 0.0f, 0.0f };
            Math::Vector3 worldPos = CellToWorldVec3(mapGrid, targetCell);

            float terrainPos[4] = {};
            Reimpl_MapGetTerrainPos(mapGrid, terrainPos, &worldPos.x, '\0', nullptr);

            CitizenComponent* citizenRef = *entityIBP;

            WorkAct_PerformWorkCtor_bffd0(
                &citizenRef, field_20, reinterpret_cast<uint32_t*>(terrainPos),
                (uint64_t)(uint32_t)targetCell,
                (uint32_t)field_5c, 0xff, 1,
                (int64_t)field_50, 0, velocity, &actionRef);

            int priority = field_20;
            return FinalizeAction(this, entityIBP, &actionRef, priority);

        } else {
            mapGrid = (int*)gameWorld->m_game;
            void** citizenList = (void**)gameWorld->m_citizenList;

            posData = GetPositionData(entityIBP);
            mapSize = *mapGrid;
            heightU32 = *(uint32_t*)(posData + 0x160);
            cellIndex = PosToCellIndex(mapGrid, posData);

            float velocity[3] = { 0.0f, 0.0f, 0.0f };
            uint64_t foundStorageIBP = 0;

            uint64_t foundEntity = 0;
            Pf_PickBestDropTarget_8d590(
                reinterpret_cast<int64_t**>(citizenList), reinterpret_cast<int64_t*>(&foundEntity),
                mapSize * ((cellIndex / mapSize)) + (cellIndex % mapSize),
                (int64_t)field_50, 1, '\0', reinterpret_cast<uint32_t*>(velocity), &foundStorageIBP);

            // 0x140006330 — scans the RawMaterialDescription list for field_50's
            // entry. This is NOT the entity component finder (0x1400223c0) used above;
            // both were called fn_FindComponent_RawMaterialC before the rename.
            WorkAcc_FindComponent_6330((int64_t)field_50, &foundStorageIBP);

            if (foundEntity == 0) {
                return 1;
            }

            int remainingCapacity = WorkLeaf_FreeStorageCapacity_116540(reinterpret_cast<intptr_t>(*entityIBP));

            // (0x140006330 returns a RawMaterialDescription whose +0x48 is the weight)
            if (remainingCapacity < *(int32_t*)(foundStorageIBP + 0x48)) {
                CitizenAI_State_LongTermPlan(reinterpret_cast<int64_t>(*entityIBP));
                return 2;
            }

            foundStorageIBP = foundEntity;

            Math::Vector3 worldPos = CellToWorldVec3(mapGrid, targetCell);

            float terrainPos[4] = {};
            Reimpl_MapGetTerrainPos(mapGrid, terrainPos, &worldPos.x, '\0', nullptr);

            CitizenComponent* citizenRef = *entityIBP;

            WorkAct_PerformWorkCtor_bffd0(
                &citizenRef, field_20, reinterpret_cast<uint32_t*>(terrainPos),
                (uint64_t)(uint32_t)targetCell,
                (uint32_t)field_5c, 0xff, 1,
                (int64_t)field_50, 1, velocity, reinterpret_cast<int64_t*>(&foundStorageIBP));

            int priority = field_20;
            return FinalizeAction(this, entityIBP, &foundStorageIBP, priority);
        }
    }
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_BuildRoad_Vtable` (slot 1 answered null, slot 2 a
// lazily cached id) registered at 0x140161020, and REGISTER_TYPE over
// BuildRoad_Factory / BuildRoad_CreateInPlace. Nothing outside this file reads
// 0x140161020 back (sweep by hex and by BuildRoadAddr::Vtable), so the table's
// registration goes with it.

// create 0x140029560: MemAlloc(0x70, 8), then the ctor 0x1400e4ad0.
void* BuildRoad::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(BuildRoad), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) BuildRoad());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* BuildRoad::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) BuildRoad());
}


#include "Object/factory_registration.h"

DEFINE_EXE_TYPE(BuildRoad, "Work")
