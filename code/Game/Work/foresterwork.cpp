// code/Game/Work/foresterwork.cpp
// Clean C++ reimplementation of ForesterWork::Slot4 — main work-assignment logic.
// Source: Game/Work/ForesterWork.cpp (from profile build debug strings)
//
// ForesterWork (0x60 bytes) — Forester resource gathering work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> ForesterWork
// Vtable: 0x1401613a0
//
// Slot4 (0x1400EAEC0) — 599 instructions, the largest single function across all Work classes.
// Finds nearby trees, checks terrain, assigns forester to cut/plant tasks.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/exe_bodies.h"
#include "Work/ForesterWork.h"
#include "Work/foresterwork_funcs.h"
#include "options.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Tutorial/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "math/Math.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: bodies called by name
#include "Player/exe_bodies.h"      // B3R: 0x14009f5d0
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include <cstring>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/ForesterComponent.h"             // ENDGAME B.3.9 D5
#include "Component/ForesterDescription.h"           // ENDGAME B.3.9 D5
#include "Component/RadiusDescription.h"             // ENDGAME B.3.9 D5
#include "Component/NaturalResourceDescription.h"    // ENDGAME B.3.9 D5

// ============================================================
// Sub-functions, called by name (B3R; the module exe_bodies.h headers)
// ============================================================
//   0x1400e3db0  WorkGrid_AutoPickup_e3db0
//   0x140003050  WorkBase_FindComponent_3050
//   0x140002f90  WorkBase_FindComponent_2f90
//   0x14012c3f0  CitizenComp_FUN_14012c3f0
//   0x14012e5c0  WorkComponent_CalcMaxWorkers
//   0x140132010  WorkImpl_Sqrtf_132010
//   0x14000c3c0  WorkLeaf_FindGrowthDescription_c3c0
//   0x140087fb0  Pf_EntityAnchorCell_7fb0
//   0x14009c480  Pf_ZoneRefresh_9c480
//   0x140008830  WorkBase_CalcDistance_8830
//   0x14009ccf0  WorkGrid_FindCell_9ccf0
//   0x1400a00f0  WorkBase_ValidateCell_a00f0
//   0x1400052c0  WorkBase_FindComponent_52c0
//   0x140006330  WorkAcc_FindComponent_6330
//   0x14009f5d0  PlayerPath_CharComp_f5d0
//   0x14009f3e0  Reimpl_MapGetTerrainPos
//   0x140120c30  WorkLeaf_FootprintOwnerCheck_120c30
//   0x1400be6e0  WorkAct_PlantWorkCtor_be6e0
//   0x1400c2490  WorkAct_PlayAnimationCtor_c2490
//   0x1400c1eb0  WorkAct_PerformWorkCtor_c1eb0
//   0x140021a30  WorkBST_RegisterWork_1a30
//   0x140115e60  CitizenAI_State_LongTermPlan
//   0x140006270  WorkBase_FindComponent_6270
//   0x14012cf10  CitizenComp_StorageComponent_QueryCount
//   0x1400296b0  WorkBST_BST_Serialize_296b0
//   0x1400f7100  WorkBase_ValidateTask_f7100
//   0x14001a6e0  WorkBST_BSTRemove_a6e0
//   0x140025520  WorkBST_BSTFindRemove_25520
//   0x1400c5400  WorkBase_TaskListCleanup_c5400
//   0x140002e10  AttendanceComponent_GetJobHostComponent
//   0x140014c30  WorkBase_TreeNodeFree_14c30
//   0x140172184  kRdata_140172184 (.rdata)
//   0x140172248  kRdata_140172248 (.rdata)
//   0x14017213c  kRdata_14017213c (.rdata)
//   0x140172114  kRdata_140172114 (.rdata)
//   0x14017214c  kRdata_14017214c (.rdata)
//   0x1401260f0  ActCNR_RemoveNatRes_1260f0 (the force-harvest harness only)
//
// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: ForesterWork_Ctor
// disassembles to `0x1400ebac9: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// The AutoPickupWork vtable it then overwrites with is 0x140160fa0 (the next LEA
// in the same ctor); only after that comes this class's own vtable.
// These were previously 0x1401612e0 / 0x140161340, neither of which is a vtable:
// both are INTERIOR addresses of the FireWork (0x1401612a0) and FishingWork
// (0x140161320) vtables, and Ghidra reports ZERO xrefs to either.

// A .rdata constant stored as a bit pattern, read as the float the cell held.
static inline float WorkRdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

// Harness entry (no retail address): the force-driver gates on this. B3R: the helpers
// are called by name, so the only thing left to test is the game world (STORAGE).
bool ForesterWork_ExecuteReady() {
    return GameUpdateLoop::_singleton != nullptr;
}

// extern "C" wrapper so the force harness can drive ForesterWork::Execute directly (the Work
// driver ABI: self + the worker citizen's IBP) without needing the ForesterWork type/mangling.
extern "C" int ForesterWork_ForceDrive(void* self, void* citizenIBP) {
    return reinterpret_cast<ForesterWork*>(self)->ForesterWork::Execute(citizenIBP, nullptr);   // B.3.8 bridge
}


// ============================================================
// Helper: Find component by type ID in entity's component list
// Iterates array at (entity+0x30), count at (entity+0x38).
// Each entry is an InternalBasePtr; vtable+0x10 = GetTypeId().
// Returns the component pointer, or nullptr if not found.
// ============================================================
static void* FindComponentInList(uintptr_t entity, uint32_t typeId) {
    int count = *(int*)(entity + 0x38);
    uintptr_t arr = *(uintptr_t*)(entity + 0x30);
    for (int i = 0; i < count; i++) {
        uintptr_t* ibpPtr = (uintptr_t*)(arr + (uintptr_t)i * 8);
        uintptr_t obj = *ibpPtr;
        // slot 2 Type() (the old no-argument typedef never passed `this`)
        if (reinterpret_cast<Object::Base*>(obj)->Type() == typeId) {   // B.3.8 bridge
            void* comp = (void*)*ibpPtr;
            if (Object::IsDerivedFromNullSafe(comp, typeId)) {
                return comp;
            }
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// Helper: Find component by type ID in sub-object's component list
// Uses the +0x28/+0x30 layout (different from entity main list).
// ============================================================
static void* FindSubComponentInList(uintptr_t subObj, uint32_t typeId) {
    uintptr_t listBase = *(uintptr_t*)(subObj + 0x28);
    int count = *(int*)(subObj + 0x30);
    for (int i = 0; i < count; i++) {
        uintptr_t obj = *(uintptr_t*)(listBase + (uintptr_t)i * 8);
        // slot 2 Type() (the old no-argument typedef never passed `this`)
        if (reinterpret_cast<Object::Base*>(obj)->Type() == typeId) {   // B.3.8 bridge
            void* comp = *(void**)(listBase + (uintptr_t)i * 8);
            if (Object::IsDerivedFromNullSafe(comp, typeId)) {
                return comp;
            }
            return nullptr;
        }
    }
    return nullptr;
}


// ============================================================
// Helper: Assign work and register with AI
// Common tail code for both plant and cut paths.
// Sets entity flags, stores priority, registers work with citizen AI.
// ============================================================
static void AssignWorkToEntity(ForesterWork* self, void* citizenIBP, int priority) {
    uintptr_t citizen = *(uintptr_t*)citizenIBP;

    // Call vtable slot +0x50 (GetWorkEntity) on self
    uintptr_t workEntityIBP[2] = {0};
    uintptr_t* result = static_cast<uintptr_t*>(self->GetTarget(workEntityIBP));
    uintptr_t workEntity = *result;

    // Set entity flags
    *(uint32_t*)(citizen + 0x54) |= 4;
    *(uint32_t*)(citizen + 0xb0) = 0;
    *(int32_t*)(citizen + 0x30) = priority;

    // If work entity matches citizen's home entity (+0x1f0) and priority >= 0,
    // set the "owned work" flag
    if (workEntity == *(uintptr_t*)(citizen + 0x1f0) && priority >= 0) {
        *(uint32_t*)(citizen + 0x88) |= 0x20000;
    }

    // Register work with citizen AI subsystem at self+0x48
    uintptr_t citizenAISub = (uintptr_t)self + 0x48;
    WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>((void*)citizenAISub), workEntityIBP, citizenIBP);
}


// ============================================================
// ForesterWork::Slot4 — Main work-assignment logic
// ============================================================
//
// Algorithm:
// 1. Check if citizen already has auto-pickup assignments
// 2. Find WorkplaceComponent, RadiusComponent, MaterialDescription
// 3. Check if material count has reached capacity (return 1 = done if so)
// 4. Calculate search radius from workplace radius
// 5. Random iteration over grid cells in radius
// 6. For each valid cell:
//    a. If empty cell and planting enabled: check terrain, create plant action
//    b. If occupied cell and cutting enabled: check resource, create cut action
// 7. If no work found after all attempts:
//    a. If enough failures, check storage and replan
//    b. Otherwise play idle animation and increment fail counter
//
int ForesterWork::Execute(void* entityIBP, void* /*context*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;  // for qword-aligned field access

    // Bring-up trace (first few ticks): confirms ForesterWork::Execute is actually being
    // ticked by the executor and reports the work-area entity + workplace cell it operates on.
    { static int s_tc = 0;
      if (s_tc < 6) { ++s_tc;
        fprintf(stderr, "[ForesterWork::Execute] tick#%d this=%p workArea+0x10=%p cell+0x28=%d\n",
                s_tc, (void*)this, (void*)selfQ[2], (int)selfQ[5]);
        fflush(stderr); } }

    // Save citizen ref for later use
    uintptr_t citizenRef = *(uintptr_t*)entityIBP;
    void* citizenIBP = entityIBP;

    // -------------------------------------------------------
    // Step 1: Check auto-pickup assignments
    // -------------------------------------------------------
    int checkResult = WorkGrid_AutoPickup_e3db0(selfAddr, &citizenRef);
    if (checkResult == 2) {
        return 2;
    }

    // -------------------------------------------------------
    // Step 2: Find components on the workplace entity
    // -------------------------------------------------------

    // Get material description from workplace entity (self+0x10)
    uintptr_t matDescIBP = 0;
    WorkBase_FindComponent_3050(selfQ[2], &matDescIBP);

    // Find ForesterComponent on workplace entity (DAT_1401c7740, ENDGAME B.3.9 D5:
    // by cell -- not the legacy variable name "WorkplaceComponent").
    uintptr_t entity = selfQ[2];
    void* workplaceComp = FindComponentInList(entity, ForesterComponent::TypeId());

    // Wrap in InternalBasePtr
    uintptr_t workplaceIBP = 0;
    new (&workplaceIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(workplaceComp));   // B.3.3 bridge

    // Get ForesterDescription from workplace description (+0x18) (DAT_1401c7778,
    // ENDGAME B.3.9 D5: by cell -- not the legacy variable name "RadiusComponent").
    void* workplaceDesc = *(void**)(workplaceIBP + 0x18);
    void* radiusComp = nullptr;
    if (Object::IsDerivedFromNullSafe(workplaceDesc, ForesterDescription::TypeId())) {
        radiusComp = workplaceDesc;
    }

    // Find RadiusDescription on workplace's sub-object (+0x28) (DAT_1401c4870,
    // ENDGAME B.3.9 D5: by cell -- not the legacy variable name "MaterialComponent").
    uintptr_t entitySub = *(uintptr_t*)(selfQ[2] + 0x28);
    void* materialComp = FindSubComponentInList(entitySub, RadiusDescription::TypeId());

    // -------------------------------------------------------
    // Step 3: Check capacity — is material count at limit?
    // -------------------------------------------------------
    int materialIdx = *(int*)((uintptr_t)radiusComp + 0x28);
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
    uintptr_t resourceTable = *(uintptr_t*)(resourceTracker + 0x170);
    uintptr_t resEntry = resourceTable + (uintptr_t)materialIdx * 0x20;
    bool atCapacity = *(int*)(resEntry + 8) <= *(int*)(resEntry);

    // Set mesh visibility based on capacity (visGroup 2)
    uintptr_t meshIBP[2] = {0};
    WorkBase_FindComponent_2f90(selfQ[2], meshIBP);
    CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>((void*)(*(uintptr_t*)meshIBP)), 2, atCapacity ? 1 : 0);

    // Get tree count (self+0x50 as int from qword at offset 10)
    int treeCount = (int)selfQ[10]; // self->_field50

    // If material description count is 0, calculate it
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = WorkComponent_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }

    // If treeCount exceeds max workers, we're done (return 1)
    if (treeCount > *(int*)(matDescIBP + 0x30)) {
        return 1;
    }

    // -------------------------------------------------------
    // Step 4: Calculate search parameters
    // -------------------------------------------------------
    void* rng = *(void**)(gameWorld + 0xd8);
    int* mapGrid = *(int**)(gameWorld + 0xf8);

    int workplaceRadius = *(int*)((uintptr_t)materialComp + 0x28);
    int radiusSq = workplaceRadius * workplaceRadius;
    float searchArea = WorkImpl_Sqrtf_132010((float)radiusSq * (kRdata_140172184));
    int mapSize = *mapGrid;
    int maxAttempts = (int)searchArea;

    // Workplace grid cell position
    int workplaceCell = (int)selfQ[5]; // self->_field28
    int cellRow = workplaceCell / mapSize;
    int cellCol = workplaceCell % mapSize;

    // Get NaturalResourceDescription from the material description's ref
    uintptr_t matDescCompRef = *(uintptr_t*)((uintptr_t)radiusComp + 0x20);
    void* natResDesc = nullptr;
    WorkLeaf_FindGrowthDescription_c3c0(matDescCompRef, &natResDesc);

    // Get citizen's current position from citizen entity
    uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;

    // Navigate: citizen -> +0x278 -> CheckPtr -> deref -> +0x68 -> CheckPtr -> deref
    uintptr_t aiComp = *(uintptr_t*)(citizenEntity + 0x278);
    uintptr_t posComp = *(uintptr_t*)(aiComp + 0x68);

    int mapSz = *mapGrid;
    (void)*(float*)(posComp + 0x160);  // citizenYaw at +0x160, unused in this path
    int citizenCellX = (int)(*(float*)(posComp + 0x158) + (float)(mapSz >> 1));
    int citizenCellY = (int)(*(float*)(posComp + 0x15c) + (float)(mapSz >> 1));

    // Clamp citizen cell to map bounds
    if (citizenCellX < 0) citizenCellX = 0;
    if (citizenCellX >= mapSz) citizenCellX = mapSz - 1;
    if (citizenCellY < 0) citizenCellY = 0;
    if (citizenCellY >= mapSz) citizenCellY = mapSz - 1;

    uint32_t citizenGridCell = mapSz * citizenCellY + citizenCellX;

    // Get entity's grid cell for distance calculations
    uintptr_t entityIBPBuf[2] = { selfQ[2], 0 };
    uint32_t entityGridCell = Pf_EntityAnchorCell_7fb0(entityIBPBuf, nullptr);

    // -------------------------------------------------------
    // Step 5: Random search for tree spots
    // -------------------------------------------------------
    float fMaxHeight = WorkRdataF(kRdata_140172248);
    float fMinGrowthVal = WorkRdataF(kRdata_14017213c);
    // Note: XMM7 = g_fUVScale (DAT_14017214c = 1.0f) used for growth comparison.
    float fGrowthThreshold = kRdata_14017214c;   // DAT_14017214c

    int entityCellSigned = (int)entityGridCell;

    if (maxAttempts > 0) {
        for (int attempt = 0; attempt < maxAttempts; attempt++) {
            int randX, randY;
            int testCellX, testCellY;

            // Generate random position within radius (rejection sampling)
            do {
                do {
                    randX = reinterpret_cast<System::Random*>(rng)->RandTol(workplaceRadius);   // B.3.4b bridge
                    randY = reinterpret_cast<System::Random*>(rng)->RandTol(workplaceRadius);   // B.3.4b bridge
                    testCellX = cellCol + randX;
                    testCellY = cellRow + randY;
                } while (randY * randY + randX * randX > radiusSq);
            } while (testCellX < 0 || testCellX >= mapSize ||
                     testCellY < 0 || testCellY >= mapSize);

            uint32_t testCell = (uint32_t)(*mapGrid) * testCellY + testCellX;
            uintptr_t gridData = *(uintptr_t*)(mapGrid + 0x20); // piVar3[0x20] = mapGrid+0x80
            uintptr_t cellData = gridData + (uintptr_t)testCell * 0x30;

            // Check terrain type bitmask — skip "forest-incompatible" terrain types
            // bit 5..8 (0x1e0 mask) indicates forest/grass terrain
            uint8_t terrainByte = *(uint8_t*)(cellData + 0x29);
            uint32_t terrainBit = 1u << (terrainByte & 0x1f);
            if ((terrainBit & 0x1e0u) != 0) {
                // Terrain already has resources (trees), skip
                goto next_attempt;
            }

            {
                // Map invalidation for connectivity
                // Retail 0x1400eb303: `MOV RCX,R15; CALL 0x14009c480` -- only RCX is set,
                // and the body has no other parameter.
                Pf_ZoneRefresh_9c480(mapGrid);

                // Read fresh grid data after invalidation
                uintptr_t gridData2 = *(uintptr_t*)(mapGrid + 0x20);
                uintptr_t entityCellData = gridData2 + (uintptr_t)entityCellSigned * 0x30;
                uintptr_t testCellData2 = gridData2 + (uintptr_t)(int)testCell * 0x30;

                uint32_t entityTerrainBit = 1u << (*(uint8_t*)(entityCellData + 0x29) & 0x1f);
                uint32_t testTerrainBit = 1u << (*(uint8_t*)(testCellData2 + 0x29) & 0x1f);
                uint32_t combinedBits = testTerrainBit | entityTerrainBit;
                uint32_t fullMask = combinedBits | 0xe1f;

                // Both cells must be passable
                if ((fullMask & testTerrainBit) == 0) goto next_attempt;
                if ((fullMask & entityTerrainBit) == 0) goto next_attempt;

                // Either both are "forest terrain" (0x1e0), or they share the same zone
                if ((combinedBits & 0x1e0) == 0) {
                    int16_t zone1 = *(int16_t*)(testCellData2 + 0x26);
                    if (zone1 == 0) goto next_attempt;
                    int16_t zone2 = *(int16_t*)(entityCellData + 0x26);
                    if (zone2 == 0) goto next_attempt;
                    if (zone1 != zone2) goto next_attempt;
                }

                // Cell must be unoccupied (no entity at cell)
                if (*(uintptr_t*)(testCellData2) != 0) goto next_attempt;

                // Check distance is within 2x workplace radius
                int dist = WorkBase_CalcDistance_8830(mapGrid, entityGridCell, testCell);
                if (dist > workplaceRadius * 2) goto next_attempt;

                // Get resource entity at this cell
                uintptr_t resourceEntity = *(uintptr_t*)(gridData2 + (uintptr_t)(int)testCell * 0x30 + 8);

                // -------------------------------------------------------
                // Branch: Check workplace flags for cut vs plant
                // -------------------------------------------------------
                // PORT-BUG FIX: the original (0x1400eaec0) enters PLANT only when the
                // ForesterComponent's plant-enabled flag (+0x50) is NON-zero and the cell
                // is empty:  else-branch of `if (cmp[0x50]==0 || resource!=0)` ==
                // `(+0x50 != 0 && resource==0)`. The prior port had `+0x50 == 0`, which
                // (with the inert lodge's flag defaulting to 0) ALWAYS entered the plant
                // path -> CheckResourceDensity(natResDesc=null)+0x5c null-deref. CUT is
                // unchanged: `+0x51 != 0 && resource!=0`. After this fix the if/else
                // matches the original's nested branches exactly for all four cases.
                bool plantEnabled = (*(uint8_t*)(workplaceIBP + 0x50) != 0);  // plant-trees enabled
                bool cutEnabled = (*(uint8_t*)(workplaceIBP + 0x51) != 0);    // cut-trees enabled

                if (plantEnabled && resourceEntity == 0) {
                    // PLANT PATH: empty cell, check if we can plant here
                    bool canPath = static_cast<uint8_t>(WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, testCell, citizenGridCell));   // the door read AL
                    if (canPath) {
                        // Check terrain suitability for planting
                        bool terrainOk = PlayerPath_CharComp_f5d0(
                            mapGrid, testCellX, testCellY, 4,
                            fMinGrowthVal, fMaxHeight, 0.0f, 0, 0, 0);
                        if (terrainOk) {
                            // Check natural resource density limits
                            bool densityOk = static_cast<uint8_t>(WorkLeaf_FootprintOwnerCheck_120c30(
                                reinterpret_cast<intptr_t>(natResDesc), testCellX, testCellY));   // the door read AL
                            if (densityOk) {
                                // Reset fail counter
                                *(uint32_t*)(selfAddr + 0x58) = 0;

                                // Calculate world position for target cell
                                float halfOffset = (float)(*mapGrid >> 1) - kRdata_140172114;
                                Math::Vector3 targetPos(
                                    (float)(testCell % *mapGrid) - halfOffset,
                                    (float)(testCell / *mapGrid) - halfOffset,
                                    0.0f);

                                // Get terrain height at position
                                float pathPos[3];
                                Reimpl_MapGetTerrainPos(mapGrid, pathPos, &targetPos.x, 0, nullptr);

                                // Get material description ref for plant action
                                uintptr_t matDescRef = *(uintptr_t*)((uintptr_t)radiusComp + 0x20);

                                // Create plant action chain
                                uintptr_t entityRef = *(uintptr_t*)citizenIBP;
                                uintptr_t plantBuf[2] = { entityRef, 0 };
                                uintptr_t resourceVal = 0;
                                uintptr_t resCountBuf[2] = { 0, 0 };
                                WorkAct_PlantWorkCtor_be6e0(
                                    plantBuf,                          // entityIBP
                                    (int)selfQ[4],                     // priority (workplaceId)
                                    reinterpret_cast<void*>(matDescRef),                        // matDescRef
                                    0,                                 // param4
                                    pathPos,                           // pathPos
                                    testCell,                          // cellIdx
                                    0,                                 // param7
                                    reinterpret_cast<int64_t*>(&resourceVal),                      // resourcePtr
                                    (uint32_t*)resCountBuf,            // resCountPtr
                                    0,                                 // param10
                                    0);                                // param11

                                // Assign work and register with AI
                                AssignWorkToEntity(this, citizenIBP, (int)selfQ[4]);
                                return 2;
                            }
                        }
                    }
                }
                else if (cutEnabled && resourceEntity != 0) {
                    // CUT PATH: cell has a resource, check if we can cut it

                    // Find NaturalResourceComponent on the resource entity
                    uintptr_t natResCompIBP[4] = {0};
                    WorkBase_FindComponent_52c0(resourceEntity, natResCompIBP);

                    if (*(uintptr_t*)natResCompIBP != 0) {
                        // Check capacity — can we still add more resources?
                        int matIdx2 = *(int*)((uintptr_t)radiusComp + 0x28);
                        uintptr_t resEntry2 = resourceTable + (uintptr_t)matIdx2 * 0x20;
                        if (*(int*)(resEntry2) < *(int*)(resEntry2 + 8)) {
                            // Find NaturalResourceDescription on the resource
                            uintptr_t natResCompIBP2[4] = {0};
                            WorkBase_FindComponent_52c0(resourceEntity, natResCompIBP2);

                            void* natResDescObj = *(void**)(*(uintptr_t*)natResCompIBP2 + 0x18);
                            void* natResDescFound = nullptr;
                            if (Object::IsDerivedFromNullSafe(natResDescObj, NaturalResourceDescription::TypeId())) {
                                natResDescFound = natResDescObj;
                            }

                            // Find RawMaterialComponent
                            uintptr_t rawMatPtr = 0;
                            WorkAcc_FindComponent_6330(
                                *(uintptr_t*)((uintptr_t)natResDescFound + 0x20), &rawMatPtr);

                            // Check resource flags:
                            // bit 5 of +0x5c = "being harvested"
                            // bit 13 of +0x5c = "reserved"
                            uint32_t resFlags = *(uint32_t*)(rawMatPtr + 0x5c);
                            if ((resFlags >> 5) & 1) goto next_attempt;
                            if ((resFlags >> 13) & 1) goto next_attempt;

                            // Check growth is sufficient (>= g_fUVScale threshold)
                            float growth = *(float*)(*(uintptr_t*)natResCompIBP2 + 0x28);
                            if (growth < fGrowthThreshold) goto next_attempt;

                            // Check bit 20 of +0x20 = "flagged for removal"
                            uint32_t flags20 = *(uint32_t*)(*(uintptr_t*)natResCompIBP2 + 0x20);
                            if ((flags20 >> 20) & 1) goto next_attempt;

                            // Find nearest passable cell for pathfinding
                            int nearestCell = WorkGrid_FindCell_9ccf0(
                                mapGrid, testCell, citizenGridCell, 0xe07);
                            if (nearestCell < 0) goto next_attempt;

                            // Check path to the nearest passable cell
                            bool canPath2 = static_cast<uint8_t>(WorkBase_ValidateCell_a00f0(
                                mapGrid, 0xe1f, nearestCell, citizenGridCell));   // the door read AL
                            if (!canPath2) goto next_attempt;

                            // Reset fail counter
                            *(uint32_t*)(selfAddr + 0x58) = 0;

                            // Calculate world position for nearest cell
                            float halfOffset2 = (float)(*mapGrid >> 1) - kRdata_140172114;
                            Math::Vector3 cutPos(
                                (float)(nearestCell % *mapGrid) - halfOffset2,
                                (float)(nearestCell / *mapGrid) - halfOffset2,
                                0.0f);

                            // Get terrain height
                            float cutPathPos[3];
                            Reimpl_MapGetTerrainPos(mapGrid, cutPathPos, &cutPos.x, 0, nullptr);

                            // ── PHASE 5 PRODUCE VERIFICATION (env BANISHED_FORCE_HARVEST, default OFF) ──
                            // The organic delivery of a citizen to the tree is gated on the pathfinder
                            // MoveTo/arrival keystone (project_citizen_pathfinding_zero_dest) — citizens
                            // currently drift via walk-anim root-motion and never arrive. To verify the
                            // Phase-5 PRODUCTION chain (RemoveNaturalResource::Main → FUN_1401260f0 →
                            // RegisterRawMaterial) end-to-end independent of that movement blocker, this
                            // block invokes the real produce helper directly on the tree the real forester
                            // scan just found + validated. It mirrors RemoveNaturalResource::Main's flag
                            // setup (natResComp+0x2d harvestable, +0x2c mature) then calls 0x1401260f0.
                            if (Opt().forceHarvest) {
                                static int s_harvestCount = 0;
                                if (s_harvestCount < 6) {
                                    ++s_harvestCount;
                                    uintptr_t nrc = *(uintptr_t*)natResCompIBP2;
                                    *(uint8_t*)(nrc + 0x2d) = 1;   // harvestable
                                    *(uint8_t*)(nrc + 0x2c) = 1;   // mature
                                    uintptr_t czEnt = *(uintptr_t*)citizenIBP;
                                    uintptr_t residence = czEnt ? *(uintptr_t*)(czEnt + 0x248) : 0;
                                    uintptr_t workIBPbuf[2] = { residence, 0 };
                                    fprintf(stderr, "[FORCE-HARVEST] #%d invoking produce on tree natResComp=%p "
                                            "(mirrors RemoveNaturalResource::Main; bypasses MoveTo arrival)\n",
                                            s_harvestCount, (void*)nrc);
                                    fflush(stderr);
                                    ActCNR_RemoveNatRes_1260f0(reinterpret_cast<int64_t>((void*)nrc), workIBPbuf, 1);
                                    // With BANISHED_PRODUCE_SPAWN_ENTITY the force-produce just
                                    // REMOVED the tree (dirty-list -> freed next EntityList tick).
                                    // Do NOT also queue the real cut sequence on it: its pending
                                    // AnimateNaturalResource/PerformWork children would execute
                                    // against the freed entity (retail order runs all tree-touching
                                    // actions BEFORE RemoveNaturalResource, so this ordering only
                                    // exists under the harness). Scan on for a different tree.
                                    goto next_attempt;
                                }
                            }

                            // Create PerformWork action (cut tree)
                            // Get target entity IBP
                            uintptr_t targetRef = *(uintptr_t*)natResCompIBP2;
                            uintptr_t cutEntityRef = *(uintptr_t*)citizenIBP;
                            uintptr_t cutBuf[2] = { cutEntityRef, 0 };
                            uintptr_t cutTargetBuf[2] = { targetRef, 0 };
                            WorkAct_PerformWorkCtor_c1eb0(
                                cutBuf,                            // entityIBP
                                (int)selfQ[4],                     // priority (workplaceId)
                                cutTargetBuf,                      // targetIBP
                                cutPathPos,                        // position
                                0,                                 // flag
                                1);                                // param6

                            // Assign work and register with AI
                            AssignWorkToEntity(this, citizenIBP, (int)selfQ[4]);
                            return 2;
                        }
                    }
                }
            }

        next_attempt:
            ;  // Continue to next random attempt
        }
    }

    // -------------------------------------------------------
    // Step 7: No work found — handle failure
    // -------------------------------------------------------
    int failCount = (int)selfQ[0xb]; // self->_field58

    if (failCount > maxAttempts) {
        // Too many failures — check if citizen's storage has items, then replan
        uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

        uintptr_t storageIBP[2] = {0};
        WorkBase_FindComponent_6270(*(uintptr_t*)(citizenEnt + 0x10), storageIBP);

        int storageCount = CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>((void*)*(uintptr_t*)storageIBP));
        if (storageCount < 1) {
            return 0;
        }

        // Citizen has items in storage — replan long term
        CitizenAI_State_LongTermPlan(reinterpret_cast<int64_t>(*(void**)citizenIBP));
    }
    else {
        // Play idle animation and increment fail counter
        uintptr_t idleEntityRef = *(uintptr_t*)citizenIBP;
        uintptr_t idleBuf[2] = { idleEntityRef, 0 };
        WorkAct_PlayAnimationCtor_c2490(idleBuf, (int)selfQ[4], 0);

        // Assign work (idle animation) and register with AI
        int priority = (int)selfQ[4];
        uintptr_t citizenEnt2 = *(uintptr_t*)citizenIBP;

        // Call vtable slot +0x50 (GetWorkEntity)
        uintptr_t workEntIBP[2] = {0};
        uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(workEntIBP));
        uintptr_t workEnt = *wResult;

        *(uint32_t*)(citizenEnt2 + 0x54) |= 4;
        *(int32_t*)(citizenEnt2 + 0x30) = priority;
        *(uint32_t*)(citizenEnt2 + 0xb0) = 0;

        if (workEnt == *(uintptr_t*)(citizenEnt2 + 0x1f0) && priority >= 0) {
            *(uint32_t*)(citizenEnt2 + 0x88) |= 0x20000;
        }

        uintptr_t citizenAISub = (uintptr_t)this + 0x48;
        WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>((void*)citizenAISub), workEntIBP, citizenIBP);

        // Increment fail counter
        *(int32_t*)(selfAddr + 0x58) = failCount + 1;
    }

    return 2;
}


// ============================================================
// ForesterWork::DtorBody (0x1400EBA00)
// Sets vtables down ForesterWork -> AutoPickupWork -> Work chain,
// frees BST nodes at +0x48, zeros count at +0x50, then calls
// Object::Internal::~Internal via tail-call.
// ============================================================
// ForesterWork dtor (retail slot 0 0x14002A370 inlines it). The stamps and the `~Internal` call are the compiler's chain.
ForesterWork::~ForesterWork() {
    uintptr_t selfAddr = (uintptr_t)this;

    // Free BST nodes at +0x48
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        // Clean up left subtree
        WorkBase_TreeNodeFree_14c30((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x10));
        // Clean up right subtree
        WorkBase_TreeNodeFree_14c30((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        // Free root node
        System::MemoryAllocator::MemFree(bstRoot);
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

}



// ============================================================
// ForesterWork::Serialize (0x1400EB970)
// Work::Serialize + version block + BST serialization + int field
// ============================================================
void ForesterWork::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create first serializer (version block, version=0)
    alignas(8) uint8_t serializer1[24] = {0};
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    // Create second serializer (version=0) for BST + int
    alignas(8) uint8_t serializer2[24] = {0};
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize BST at self+0x48
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), serializer2);

    // Serialize int field at self+0x58
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>((selfAddr + 0x58)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// ForesterWork::Slot6 (0x1400EAEA0) — NOT HOOKABLE
// 20-byte thunk: sub rsp,0x28; add rcx,0x48; call CitizenAI_impl_5520;
//                xor al,al; add rsp,0x28; ret
// Removes entity from BST at self+0x48, returns false.
// Standalone exe only.
// ============================================================
int ForesterWork::OnComplete(void* param2, void* /*arg2*/, int /*arg3*/) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return 0;
}


// ============================================================
// ForesterWork::Slot7 (0x1400EAE90) — 9-byte thunk, too small to hook.
// Thunk: add rcx,0x48; jmp CitizenAI_impl_5520
// Calls BSTFindRemove on the BST at self+0x48.
// Standalone exe only.
// ============================================================
int ForesterWork::OnCancel(void* param2, void* /*arg2*/, int /*arg3*/) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return 0;
}


// ============================================================
// ForesterWork::Slot8 / CancelAll (0x1400EB8A0)
// Loops through tree at +0x48 while count (+0x50) > 0.
// For each leftmost node, cancels the work item via engine call.
// ============================================================
int ForesterWork::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        // Find leftmost node in BST (following +0x10 = left child)
        void* node = *(void**)(selfAddr + 0x48);
        void* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            node = *(void**)((uintptr_t)node + 0x10);
        }

        // Cancel the work item: get citizen entity from node key
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        // Navigate to citizen AI subsystem and cancel
        WorkBase_TaskListCleanup_c5400((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// ForesterWork::Slot9 / RemoveInvalid (0x1400EAC60) — 164 instructions
// Iterates BST at +0x48, validates each entry via FUN_1400f7100.
// Collects invalid ones in a dynamic array, removes them from tree.
// Returns true if all valid (removeCount == 0), false otherwise.
// ============================================================
int ForesterWork::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    void** bstRootPtr = (void**)((uintptr_t)this + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root != nullptr) {
        // Find leftmost node (minimum)
        void* node = root;
        while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
            node = *(void**)((uintptr_t)node + 0x10);
        }

        // In-order traversal
        while (node != nullptr) {
            uintptr_t entityIBP = *(uintptr_t*)node;

            // Validate this entity
            char stillValid = WorkBase_ValidateTask_f7100(reinterpret_cast<void*>(selfAddr), nullptr, (void*)&entityIBP, nullptr);

            if (stillValid == '\0') {
                // Add to removal list -- grow array if needed
                uint32_t newCount = removeCount + 1;
                if (newCount != removeCount) {
                    uintptr_t* newArr = removeArr;
                    if ((int)removeCapacity < (int)newCount) {
                        uint32_t newCap = newCount;
                        if (removeCount != 0) {
                            newCap = newCount * 3;
                        }
                        removeCapacity = newCap;
                        newArr = (uintptr_t*)System::MemoryAllocator::MemAlloc((int64_t)(int)newCap * 8, 8);
                    }

                    // Copy existing entries if buffer changed
                    int copyStart = (removeArr == newArr) ? (int)removeCount : 0;
                    for (int ci = copyStart; ci < (int)newCount; ci++) {
                        uintptr_t* dst = &newArr[ci];
                        *dst = 0;
                        if (ci < (int)removeCount) {
                            *dst = removeArr[ci];
                        } else {
                            *dst = entityIBP;
                        }
                    }

                    if (removeArr != newArr) {
                        System::MemoryAllocator::MemFree(removeArr);
                    }
                    removeArr = newArr;
                    removeCount = newCount;
                }
            }

            // Get next node in tree (in-order traversal)
            void* right = *(void**)((uintptr_t)node + 0x18);
            if (right != nullptr) {
                // Go right, then leftmost
                node = right;
                while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
                    node = *(void**)((uintptr_t)node + 0x10);
                }
            } else {
                // Go up until we come from a left child
                void* parent = *(void**)((uintptr_t)node + 0x8);
                while (parent != nullptr) {
                    if (*(void**)((uintptr_t)parent + 0x10) == node) {
                        // node is left child of parent -- parent is next
                        break;
                    }
                    // node is right child, keep going up
                    node = parent;
                    parent = *(void**)((uintptr_t)node + 0x8);
                }
                node = parent;
            }
        }
    }

    // Remove invalid entries from tree
    if ((int)removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            uintptr_t searchKey = removeArr[i];
            void* treeNode = *bstRootPtr;

            while (treeNode != nullptr) {
                uintptr_t nodeKey = *(uintptr_t*)treeNode;
                if (searchKey < nodeKey) {
                    treeNode = *(void**)((uintptr_t)treeNode + 0x10);
                } else {
                    if (searchKey <= nodeKey) {
                        // Found -- remove this node
                        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(bstRootPtr), treeNode);
                        break;
                    }
                    treeNode = *(void**)((uintptr_t)treeNode + 0x18);
                }
            }
        }
    }

    System::MemoryAllocator::MemFree(removeArr);
    return (removeCount == 0) ? 1 : 0;
}


// ============================================================
// ForesterWork::Slot10 / GetWorkEntity (0x1400EB910)
// Looks up entity from this->_field10, gets sub-object via
// FUN_140002e10, returns entity +0x48 through output pointer.
// Hidden return ptr ABI: RCX=self, RDX=retbuf.
// ============================================================
void* ForesterWork::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    // 0x140002e10 returns its out IBP (RDX) in RAX; the ported body is declared void,
    // so the returned pointer is the out buffer itself.
    AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(entityRef), subIBP);
    void* result = subIBP;


    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}



// [11] 0x140029860 (shared by 13 tables): `XOR EAX,EAX; CMP [RCX+0x50],EAX; SETG AL`
// — "is anyone assigned". The old table pointed at Work_Slot11_HasActiveTasks.
int ForesterWork::GetPriority() {
    return static_cast<int32_t>(_field50) > 0 ? 1 : 0;
}

// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The three stamps are the compiler's.
ForesterWork::ForesterWork() : AutoPickupWork(), _field48(nullptr), _field50(0), _field58(0) {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_ForesterWork_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over ForesterWork_Factory / _CreateInPlace (both
// Work_GenericCtor), plus three vtable-pointer statics for the dtor's stamps.

// create 0x14002a310: MemAlloc(sizeof, 8), then the ctor.
void* ForesterWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ForesterWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ForesterWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* ForesterWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ForesterWork());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

// The factory the forester force harness (forester_force_harness.cpp) names.
extern "C" void* ForesterWork_Factory(void) { return ForesterWork::CreateNewObject(); }
DEFINE_EXE_TYPE(ForesterWork, "AutoPickupWork")
