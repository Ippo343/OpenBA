// code/Game/Player/roadtool_calcpath.cpp
// RoadTool COMMIT path — the half of road placement that actually builds road.
//
// DragTool_OnKey dispatches vtable slot 13 (+0x68) on drag-release to commit whatever
// the drag previewed. RoadTool's slot 13 was a Tool_Ret0 stub, so a road drag always
// reported "invalid" (error sound, nothing placed) even though slot 14 (CalcCost) had
// already priced a valid path. This file ports slot 13 and the three exe helpers it
// needs; every deeper leaf was already ported + registered by earlier waves.
//
// Ported here (all verified against the Application-x64.exe disassembly):
//   0x140104d30  RoadTool::CalcPath            — slot 13, validate + place the run
//   0x14009c960  RoadTool_PlaceRoadSegment     — one cell: BuildRoad work + queue + BST
//   0x1400acd90  RoadTool_DecalStore           — record the road's decal by road type
//   0x140026990  RoadTool_FindMapDescription   — find MapDescription on the descriptor
//
// Leaves (every one is called by name since B3R; see the list below):
//   0x14009ce00 road A* (keystone_a_zone)      0x1400af370 road-mesh rebuild (pf_road)
//   0x1400e4b50 BuildRoad 8-arg ctor           0x1400f72e0 Work base ctor (reg_work_base)
//   0x1400f88a0 work-scheduler insert          0x14001a4f0 AVL insert (reg_pf_avl)
//   0x14009c700 road-cell removal handler      0x140001bb0/0x1400021d0 array resizers
//
// Map-grid layout used below (mapGrid = gameWorld+0xf8):
//   +0x00 gridSize · +0x80 terrain cell array (stride 0x30) · +0xe8 pending-road BST root
// Terrain cell (stride 0x30):
//   +0x00 occupying entity ptr · +0x28 road-adjacency count · +0x29 terrain type byte
//   +0x2a road type (0xff = none) · +0x2b road state (0x80 = queued)

#include "Player/roadtool_funcs.h"
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Action/exe_bodies.h"   // RoadTool_sub_ce00 (0x14009ce00), called by name
#include "Player/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Component/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Work/buildroad_funcs.h"
#include "system/MemoryAllocator.h"
#include "object/TypeInfo.h"
#include "Object/factory_registration.h"
#include "Component/DecalDescription.h"
#include "Component/MapDescription.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

namespace {

// ---- leaves, called by name ---------------------------------------------------
//   0x1400e4b50 BuildRoad_CtorParams_Impl (Work/buildroad_funcs.h)
//   0x1400f88a0 CitizenAI_WorkScheduler_Insert (Component/exe_bodies.h)
//   0x14001a4f0 Pf_AvlInsertA_a4f0 (Action/exe_bodies.h)
//   0x14009c700 RemovalCalc_helper_c700 (Player/exe_bodies.h) -- road-cell removal
//   0x140001bb0 / 0x1400021d0 Reimpl_Array_Resize_Stride08 / _Stride04
//               (Game/exe_bodies.h)
// (0x1400af370 was resolved here and never called: the dead reader is gone.)
//   0x14009ce00 RoadTool_sub_ce00 (Action/exe_bodies.h) -- the road A*; returns the
//               path length (B3R Leftovers)

uint32_t s_tid_DecalDescription = 0;
uint32_t s_tid_MapDescription   = 0;
bool     s_resolved             = false;

void EnsureCalcPathFuncs() {
    if (s_resolved) return;
    // DAT_1401c75b8 / DAT_1401c7a18 are raw .rdata constants -> NULL in standalone, so
    // take the names from the live factory tree. Both verified against their RegisterType
    // DATA writers: 0x14014ee10 DecalDescription, 0x14014f390 MapDescription.
    if (!s_tid_DecalDescription)
        s_tid_DecalDescription = DecalDescription::TypeId();
    if (!s_tid_MapDescription)
        s_tid_MapDescription = MapDescription::TypeId();

    // Latch only once the type-ids are in hand — the factory tree is not populated until
    // after Engine::Startup, and latching early would leave them zero for the session.
    s_resolved = (s_tid_DecalDescription != 0 && s_tid_MapDescription != 0);
}


// Shared component scan: walk the entity's component array (+0x28 ptr / +0x30 count),
// match GetTypeId (vtable +0x10) against `typeId`, confirm with IsDerivedFrom. This is
// the exact shape both 0x140026990 and CalcPath's inline DecalDescription loop use —
// including the `break` on first type-id match (retail does NOT keep scanning).
void* FindComponentByTypeId(uintptr_t entity, uint32_t typeId) {
    if (!entity || !typeId) return nullptr;
    int    count = *(int*)(entity + 0x30);
    void** comps = *(void***)(entity + 0x28);
    if (!comps) return nullptr;
    for (int i = 0; i < count; ++i) {
        void* comp = comps[i];
        if (!comp) continue;
        if (reinterpret_cast<Object::Base*>(comp)->Type() == typeId) {  // B.3.8 bridge
            return Object::IsDerivedFromNullSafe(comp, typeId) ? comp : nullptr;
        }
    }
    return nullptr;
}

}  // namespace

// ============================================================================
// 0x140026990 — RoadTool_helper_6990: find the MapDescription on the descriptor.
// Retail writes the result through param_2 and returns param_2.
// ============================================================================
extern "C" void** RoadTool_FindMapDescription_6990(void* entity, void** out) {
    EnsureCalcPathFuncs();
    if (out) *out = FindComponentByTypeId((uintptr_t)entity, s_tid_MapDescription);
    return out;
}

// ============================================================================
// 0x1400acd90 — record the decal pointer for a road type in the decal manager
// (gameWorld+0x110). Slot array of pointers at +0x210/+0x218 (count) with a parallel
// int array at +0x220/+0x228; both grow to `index+1` and the new int slots are zeroed.
// ============================================================================
extern "C" void RoadTool_DecalStore_acd90(int64_t mgr, int index, void* decal) {
    EnsureCalcPathFuncs();
    if (!mgr) return;

    int oldCount = *(int*)(mgr + 0x218);
    int needed   = index + 1;
    if (oldCount < needed) {
        Reimpl_Array_Resize_Stride08((void*)(mgr + 0x210), needed);
        Reimpl_Array_Resize_Stride04((void*)(mgr + 0x220), needed);
        // Zero the freshly grown tail of the int array. Retail bounds this by the int
        // array's own capacity field (+0x228), NOT by `needed`.
        int intCount = *(int*)(mgr + 0x228);
        int64_t intBase = *(int64_t*)(mgr + 0x220);
        if (intBase) {
            for (int i = oldCount; i < intCount; ++i)
                *(uint32_t*)(intBase + (int64_t)i * 4) = 0;
        }
    }
    int64_t ptrBase = *(int64_t*)(mgr + 0x210);
    if (ptrBase) *(void**)(ptrBase + (int64_t)index * 8) = decal;
}

// ============================================================================
// 0x14009c960 — place ONE road cell.
// Queues a BuildRoad work item for the cell (citizens build it), records it in the
// pending-road BST, and stamps the cell's road type + queued state. The leading
// `roadType < existing` test is what makes re-drawing the same or a lower-tier road
// over an existing one a no-op (0xff = "no road" sorts above every real type).
// ============================================================================
extern "C" void RoadTool_PlaceRoadSegment_9c960(uint32_t* mapGrid, uint32_t cellIdx,
                                                uint8_t roadType, uint64_t /*endCell*/,
                                                uint32_t mapDescVal, uint64_t roadDescExtra) {
    EnsureCalcPathFuncs();
    if (!mapGrid) return;

    const int64_t cellOff = (int64_t)(int)cellIdx * 0x30;
    int64_t terrainBase = *(int64_t*)((uintptr_t)mapGrid + 0x80);
    if (!terrainBase) return;
    int64_t cell = terrainBase + cellOff;

    // Only upgrade: bail unless the new road type outranks what is already there.
    if (!(roadType < *(uint8_t*)(cell + 0x2a))) return;

    int64_t* bstRoot = (int64_t*)((uintptr_t)mapGrid + 0xe8);

    // If this cell already has a pending road entry, clear it first (road BST node:
    // key int at +0x00, left child +0x10, right child +0x18).
    {
        int* node = *(int**)bstRoot;
        while (node) {
            int key = *node;
            if ((int)cellIdx < key)      node = *(int**)((uintptr_t)node + 0x10);
            else if ((int)cellIdx > key) node = *(int**)((uintptr_t)node + 0x18);
            else { RemovalCalc_helper_c700(mapGrid, (int)cellIdx); break; }
        }
    }

    // Bump the cell's road-adjacency counter, but only for a cell that is plain ground
    // (terrain bits 0/1 clear), currently roadless (0x2a == 0xff), and not already
    // pending in the BST.
    {
        int64_t tb = *(int64_t*)((uintptr_t)mapGrid + 0x80);
        uint32_t terrainFlag = 1u << (*(uint8_t*)(tb + 0x29 + cellOff) & 0x1f);
        if ((terrainFlag & 2) == 0 && (terrainFlag & 1) == 0 &&
            *(int8_t*)(tb + 0x2a + cellOff) == -1) {
            bool pending = false;
            int* node = *(int**)bstRoot;
            while (node) {
                int key = *node;
                if ((int)cellIdx < key)      node = *(int**)((uintptr_t)node + 0x10);
                else if ((int)cellIdx > key) node = *(int**)((uintptr_t)node + 0x18);
                else { pending = true; break; }
            }
            if (!pending) *(int8_t*)(cell + 0x28) = (int8_t)(*(int8_t*)(cell + 0x28) + 1);
        }
    }

    // Build the BuildRoad work item (0x70 bytes) and hand it to the work scheduler.
    uint64_t ownerIbp = 0;   // retail seeds a null InternalBasePtr and reuses it below
    void* work = nullptr;
    void* mem = System::MemoryAllocator::MemAlloc(0x70, 8);
    if (mem) {
        new (&ownerIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        work = BuildRoad_CtorParams_Impl(static_cast<BuildRoad*>(mem), &ownerIbp, cellIdx,
                                      mapDescVal, roadDescExtra,
                                      *(uint8_t*)(cell + 0x2a),   // existing road type
                                      *(uint8_t*)(cell + 0x2b),   // existing road state
                                      *(uint8_t*)(cell + 0x29));  // terrain type
    }

    if (!work) {
        return;
    }

    uint64_t workIbp = 0;
    new (&workIbp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge

    // Post to the work scheduler at gameWorld+0x118. code 0x2000 = the road category;
    // retail passes fromExisting='\0' and prio=-1 (the prio is dead on this path).
    if (GameUpdateLoop::_singleton != nullptr) {
        void* sched = *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0x118);
        if (sched) CitizenAI_WorkScheduler_Insert(static_cast<int*>(sched), &workIbp, 0x2000, '\0', -1);
    }

    // Record the cell in the pending-road BST. Retail inserts the ORIGINAL null IBP
    // here (not the work IBP) — the tree tracks which cells have a road queued.
    {
        uint32_t key = cellIdx;
        Pf_AvlInsertA_a4f0(bstRoot, &ownerIbp, &key);
    }

    *(uint8_t*)(cell + 0x2a) = roadType;
    *(uint8_t*)(cell + 0x2b) = 0x80;   // queued-for-construction marker
}

// ============================================================================
// 0x140104d30 — RoadTool::CalcPath (vtable slot 13, +0x68).
// Returns true when the road run was committed. Retail's failure paths all return a
// value whose low byte is 0, which DragTool_OnKey reads as false (error sound).
// ============================================================================
bool RoadTool::CalcPath(int startCell, int endCell) {
    EnsureCalcPathFuncs();
    if ((int)startCell < 0 || (int)endCell < 0) return false;

    // Sort so sc <= ec.
    uint32_t sc = startCell, ec = endCell;
    if ((int)ec < (int)sc) { sc = endCell; ec = startCell; }

    if (GameUpdateLoop::_singleton == nullptr) return false;
    uintptr_t gameWorld = (uintptr_t)(GameUpdateLoop::_singleton);
    uint32_t* mapGrid = *(uint32_t**)(gameWorld + 0xf8);
    if (!mapGrid) return false;
    int64_t terrainBase = *(int64_t*)((uintptr_t)mapGrid + 0x80);
    if (!terrainBase) return false;

    // Both endpoints must be road-legal terrain and unoccupied.
    int64_t cellA = terrainBase + (int64_t)(int)sc * 0x30;
    int64_t cellB = terrainBase + (int64_t)(int)ec * 0x30;
    uint32_t flagA = 1u << (*(uint8_t*)(cellA + 0x29) & 0x1f);
    uint32_t flagB = 1u << (*(uint8_t*)(cellB + 0x29) & 0x1f);
    if ((flagA & 0x5e0) != 0 || (flagB & 0x5e0) != 0) return false;
    if (*(int64_t*)cellA != 0 || *(int64_t*)cellB != 0) return false;

    // Honour the descriptor's fixed-endpoint constraint (used by scripted/tutorial roads).
    uintptr_t controller = (uintptr_t)dialogPtr;
    if (controller && *(char*)(controller + 0x80) != '\0') {
        int gridSize = (int)*mapGrid;
        if (gridSize != 0) {
            int a = *(int*)(controller + 0x98);
            int b = *(int*)(controller + 0x9c);
            uint32_t ca = (uint32_t)(gridSize * (a / gridSize) + a % gridSize);
            uint32_t cb = (uint32_t)(gridSize * (b / gridSize) + b % gridSize);
            if ((ca != sc || cb != ec) && (ca != ec || cb != sc)) return false;
        }
    }

    // Run the road A* over the span (same call CalcCost makes, mask 0xa17).
    int pathBuf[2048];
    int pathLen = 0;
    pathLen = RoadTool_sub_ce00((int64_t)mapGrid, 0xa17, sc, (uint64_t)ec, nullptr,
                                (int64_t)pathBuf, modifierFlag, 1);
    if (pathLen == 0) {
        return false;
    }

    uintptr_t descEntity = (uintptr_t)descriptionPtr;
    uintptr_t roadDesc   = (uintptr_t)this->roadDesc;
    if (!descEntity || !roadDesc) return false;

    // The decal the finished road paints, and the MapDescription value each segment gets.
    void* decalComp = FindComponentByTypeId(descEntity, s_tid_DecalDescription);
    if (!decalComp) {
        return false;
    }
    void* decal = *(void**)(*(uintptr_t*)((uintptr_t)decalComp + 0x28));

    void* mapDesc = nullptr;
    RoadTool_FindMapDescription_6990((void*)descEntity, &mapDesc);
    if (!mapDesc) {
        return false;
    }
    uint32_t mapDescVal = *(uint32_t*)((uintptr_t)mapDesc + 0x20);

    const uint8_t roadType      = *(uint8_t*)(roadDesc + 0x20);
    const uint64_t roadDescExtra = *(uint64_t*)(roadDesc + 0x28);

    int64_t decalMgr = *(int64_t*)(gameWorld + 0x110);
    RoadTool_DecalStore_acd90(decalMgr, (int)roadType, decal);

    for (int i = 0; i < pathLen; ++i) {
        RoadTool_PlaceRoadSegment_9c960(mapGrid, (uint32_t)pathBuf[i], roadType,
                                        (uint64_t)ec, mapDescVal, roadDescExtra);
    }


    return true;
}
