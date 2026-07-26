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
//   0x140104d30  RoadTool_CalcPath_Impl        — slot 13, validate + place the run
//   0x14009c960  RoadTool_PlaceRoadSegment     — one cell: BuildRoad work + queue + BST
//   0x1400acd90  RoadTool_DecalStore           — record the road's decal by road type
//   0x140026990  RoadTool_FindMapDescription   — find MapDescription on the descriptor
//
// Leaves reached through ResolveAddr (already registered elsewhere):
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
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// ---- leaf function pointers -------------------------------------------------
typedef int      (*fn_RoadAStar_t)(int64_t mapGrid, uint32_t mask, uint32_t startCell,
                                   uint64_t endCell, uint64_t unused, int64_t outPath,
                                   uint8_t modifier, uint8_t dirFlag);
typedef void*    (*fn_BuildRoadCtor_t)(void* self, uint64_t* ownerIbp, uint32_t cellIdx,
                                       uint32_t mapDescVal, uint64_t roadDescExtra,
                                       uint32_t roadTypeExisting, uint32_t roadState,
                                       uint32_t terrainType);
typedef int      (*fn_SchedInsert_t)(void* sched, void* pWorkIbp, uint32_t code,
                                     char fromExisting, int prio);
typedef uint64_t*(*fn_AvlInsert_t)(int64_t* root, uint64_t* ibp, uint32_t* key);
typedef uint64_t (*fn_RoadCellRemove_t)(uint32_t* mapGrid, int cellIdx);
typedef void     (*fn_RoadMeshRebuild_t)(int64_t decalMgr, uint32_t* mapGrid);
typedef void     (*fn_ArrayResize8_t)(int64_t* arr, int newCount);
typedef void     (*fn_ArrayResize4_t)(int64_t* arr, int newCount);

fn_RoadAStar_t       s_fn_RoadAStar      = nullptr;
fn_BuildRoadCtor_t   s_fn_BuildRoadCtor  = nullptr;
fn_SchedInsert_t     s_fn_SchedInsert    = nullptr;
fn_AvlInsert_t       s_fn_AvlInsert      = nullptr;
fn_RoadCellRemove_t  s_fn_RoadCellRemove = nullptr;
fn_RoadMeshRebuild_t s_fn_RoadMeshRebuild= nullptr;
fn_ArrayResize8_t    s_fn_ArrayResize8   = nullptr;
fn_ArrayResize4_t    s_fn_ArrayResize4   = nullptr;

uint32_t s_tid_DecalDescription = 0;
uint32_t s_tid_MapDescription   = 0;
bool     s_resolved             = false;

void EnsureCalcPathFuncs() {
    if (s_resolved) return;
    s_fn_RoadAStar       = (fn_RoadAStar_t)      ResolveAddr(Module::Exe, 0x14009ce00);
    s_fn_BuildRoadCtor   = (fn_BuildRoadCtor_t)  ResolveAddr(Module::Exe, 0x1400e4b50);
    s_fn_SchedInsert     = (fn_SchedInsert_t)    ResolveAddr(Module::Exe, 0x1400f88a0);
    s_fn_AvlInsert       = (fn_AvlInsert_t)      ResolveAddr(Module::Exe, 0x14001a4f0);
    s_fn_RoadCellRemove  = (fn_RoadCellRemove_t) ResolveAddr(Module::Exe, 0x14009c700);
    s_fn_RoadMeshRebuild = (fn_RoadMeshRebuild_t)ResolveAddr(Module::Exe, 0x1400af370);
    s_fn_ArrayResize8    = (fn_ArrayResize8_t)   ResolveAddr(Module::Exe, 0x140001bb0);
    s_fn_ArrayResize4    = (fn_ArrayResize4_t)   ResolveAddr(Module::Exe, 0x1400021d0);

    // DAT_1401c75b8 / DAT_1401c7a18 are raw .rdata constants -> NULL in standalone, so
    // take the names from the live factory tree. Both verified against their RegisterType
    // DATA writers: 0x14014ee10 DecalDescription, 0x14014f390 MapDescription.
    if (!s_tid_DecalDescription)
        s_tid_DecalDescription = ::Object::ResolveTypeIdByNameLazy("DecalDescription");
    if (!s_tid_MapDescription)
        s_tid_MapDescription = ::Object::ResolveTypeIdByNameLazy("MapDescription");

    // Latch only once the type-ids are in hand — the factory tree is not populated until
    // after Engine::Startup, and latching early would leave them zero for the session.
    s_resolved = (s_tid_DecalDescription != 0 && s_tid_MapDescription != 0);
}

bool RoadTraceOn() {
    static int t = -1;
    if (t < 0) { const char* e = std::getenv("BANISHED_ROAD_TRACE"); t = (e && *e && *e != '0'); }
    return t != 0;
}

// Shared component scan: walk the entity's component array (+0x28 ptr / +0x30 count),
// match GetTypeId (vtable +0x10) against `typeId`, confirm with IsDerivedFrom. This is
// the exact shape both 0x140026990 and CalcPath's inline DecalDescription loop use —
// including the `break` on first type-id match (retail does NOT keep scanning).
void* FindComponentByTypeId(uintptr_t entity, uint32_t typeId) {
    if (!entity || !typeId || !fn_IsDerivedFrom) return nullptr;
    int    count = *(int*)(entity + 0x30);
    void** comps = *(void***)(entity + 0x28);
    if (!comps) return nullptr;
    for (int i = 0; i < count; ++i) {
        void* comp = comps[i];
        if (!comp) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        GetTypeId_t getTypeId = *(GetTypeId_t*)(*(uintptr_t*)comp + 0x10);
        if (getTypeId(comp) == typeId) {
            return fn_IsDerivedFrom(comp, typeId) ? comp : nullptr;
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
        if (s_fn_ArrayResize8) s_fn_ArrayResize8((int64_t*)(mgr + 0x210), needed);
        if (s_fn_ArrayResize4) s_fn_ArrayResize4((int64_t*)(mgr + 0x220), needed);
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
            else { if (s_fn_RoadCellRemove) s_fn_RoadCellRemove(mapGrid, (int)cellIdx); break; }
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
    void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x70, 8) : nullptr;
    if (mem) {
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ownerIbp, nullptr);
        if (s_fn_BuildRoadCtor)
            work = s_fn_BuildRoadCtor(mem, &ownerIbp, cellIdx, mapDescVal, roadDescExtra,
                                      *(uint8_t*)(cell + 0x2a),   // existing road type
                                      *(uint8_t*)(cell + 0x2b),   // existing road state
                                      *(uint8_t*)(cell + 0x29));  // terrain type
    }

    if (!work) {
        // Standalone safety net: retail's ctor cannot fail, so it posts unconditionally.
        // Here a missing 0x1400e4b50 registration would post a NULL work item and fault
        // inside the scheduler's AVL insert. Bail loudly instead of corrupting the queue.
        if (RoadTraceOn())
            fprintf(stderr, "[Road] segment %u: BuildRoad ctor failed (mem=%p ctor=%p)\n",
                    cellIdx, mem, (void*)s_fn_BuildRoadCtor);
        return;
    }

    uint64_t workIbp = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&workIbp, work);

    // Post to the work scheduler at gameWorld+0x118. code 0x2000 = the road category;
    // retail passes fromExisting='\0' and prio=-1 (the prio is dead on this path).
    if (s_fn_SchedInsert && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        void* sched = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x118);
        if (sched) s_fn_SchedInsert(sched, &workIbp, 0x2000, '\0', -1);
    }

    // Record the cell in the pending-road BST. Retail inserts the ORIGINAL null IBP
    // here (not the work IBP) — the tree tracks which cells have a road queued.
    if (s_fn_AvlInsert) {
        uint32_t key = cellIdx;
        s_fn_AvlInsert(bstRoot, &ownerIbp, &key);
    }

    *(uint8_t*)(cell + 0x2a) = roadType;
    *(uint8_t*)(cell + 0x2b) = 0x80;   // queued-for-construction marker
}

// ============================================================================
// 0x140104d30 — RoadTool::CalcPath (vtable slot 13, +0x68).
// Returns true when the road run was committed. Retail's failure paths all return a
// value whose low byte is 0, which DragTool_OnKey reads as false (error sound).
// ============================================================================
extern "C" bool RoadTool_CalcPath_Impl(RoadTool* self, uint32_t startCell, uint32_t endCell) {
    EnsureCalcPathFuncs();
    if (!self) return false;
    if ((int)startCell < 0 || (int)endCell < 0) return false;

    // Sort so sc <= ec.
    uint32_t sc = startCell, ec = endCell;
    if ((int)ec < (int)sc) { sc = endCell; ec = startCell; }

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return false;
    uintptr_t gameWorld = (uintptr_t)(*g_pGameWorldSingleton);
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
    uintptr_t controller = (uintptr_t)*(void**)((uintptr_t)self + 0x10);
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
    if (s_fn_RoadAStar) {
        pathLen = s_fn_RoadAStar((int64_t)mapGrid, 0xa17, sc, (uint64_t)ec, 0,
                                 (int64_t)pathBuf, self->modifierFlag, 1);
    }
    if (pathLen == 0) {
        if (RoadTraceOn())
            fprintf(stderr, "[Road] CalcPath: no path %u->%u\n", sc, ec);
        return false;
    }

    uintptr_t descEntity = (uintptr_t)*(void**)((uintptr_t)self + 0x20);
    uintptr_t roadDesc   = (uintptr_t)*(void**)((uintptr_t)self + 0x38);
    if (!descEntity || !roadDesc) return false;

    // The decal the finished road paints, and the MapDescription value each segment gets.
    void* decalComp = FindComponentByTypeId(descEntity, s_tid_DecalDescription);
    if (!decalComp) {
        if (RoadTraceOn())
            fprintf(stderr, "[Road] CalcPath: no DecalDescription on descriptor\n");
        return false;
    }
    void* decal = *(void**)(*(uintptr_t*)((uintptr_t)decalComp + 0x28));

    void* mapDesc = nullptr;
    RoadTool_FindMapDescription_6990((void*)descEntity, &mapDesc);
    if (!mapDesc) {
        if (RoadTraceOn())
            fprintf(stderr, "[Road] CalcPath: no MapDescription on descriptor\n");
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

    // Rebuild the road mesh so the new cells render.
    if (s_fn_RoadMeshRebuild) s_fn_RoadMeshRebuild(decalMgr, mapGrid);

    if (RoadTraceOn())
        fprintf(stderr, "[Road] CalcPath: placed %d cells (%u->%u) roadType=%u\n",
                pathLen, sc, ec, roadType);
    return true;
}
