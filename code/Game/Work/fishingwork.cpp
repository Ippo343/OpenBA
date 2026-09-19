// code/Game/Work/fishingwork.cpp
// Clean C++ reimplementation of FishingWork Work class methods.
// Source: Game/Work/FishingWork.cpp (from profile build debug strings)
//
// FishingWork — fishing dock work action. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> FishingWork
// Vtable: 0x140161320
//
// Implemented vtable slots:
//   +0x18: Serialize  (0x1400ea910) — array serialization with IsDerivedFrom
//   +0x20: Slot4      (0x1400e9f20) — fishing work assignment
//   +0x30: Slot6      (0x1400e9eb0) — clear citizen slot + mark spot done
//   +0x38: Slot7      (0x1400e9e80) — delegates to Slot6 via vtable +0x30
//   +0x40: Slot8      (0x1400ea830) — cancel all: clear citizens + cancel actions
//   +0x48: Slot9      (0x1400e9c30) — validate work assignments
//   +0x50: Slot10     (0x1400ea8b0) — get target entity position IBP
//
// Decompiled from Application-x64.exe using Ghidra.

#include "FishingWork.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include "redzone_alloc.h"   // RedzoneAlloc_PrintBacktrace (wild-write diagnostic)
#include "Object/factory_registration.h"   // GetTypeIdByName (drop-spot occupancy dump)
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Local function pointers resolved at init time
// ============================================================

// Entity_FindComponent_MeshInstance (0x140002f90) — finds MeshInstanceComponent on entity
static void* (*s_fn_FindComponent_MeshInstance)(uintptr_t entity, void* retIBP) = nullptr;

// Entity_FindComponent_Position (0x140002ed0) — finds PositionComponent on entity
static void* (*s_fn_FindComponent_Position)(uintptr_t entity, void* retIBP) = nullptr;

// Entity_impl_2e20 (0x140122e20) — get position for a spot index
// (entity, outVec3, mode, spotIndex, outHeading)
static void (*s_fn_Entity_GetSpotPosition)(void* entity, float* outVec3, int mode, int index, float* heading) = nullptr;

// FUN_14012c3f0 — MeshInstance visibility toggle
// (meshInstance, bitIndex, visible)
static void (*s_fn_MeshInstance_SetVisibility)(void* meshInstance, int bitIndex, int visible) = nullptr;

// FUN_1400a00f0 — Map_CheckPassable (already in engine_funcs as fn_Map_CheckPassable)

// FUN_1400beae0 — Fishing action constructor (builds action stack for fishing at spot)
// (citizenIBP, priority, entityIBP, position, heading, resourceType, uvScale, mapCell, spotInfo)
static void (*s_fn_FishingAction_Ctor)(void* citizenIBP, int priority, void* entityIBP,
    float* position, float heading, uint64_t resourceType, float uvScale,
    int mapCell, uint32_t spotInfo) = nullptr;

// PlayAnimation_Action_Ctor (0x1400c2490) — creates a play-animation action
// (citizenIBP, priority, citizenIndex)
static void (*s_fn_PlayAnimation_Action_Ctor)(void* citizenIBP, int priority, uint32_t citizenIndex) = nullptr;

// FUN_1400f8620 — TryAssignToFishingSpot (work assignment helper)
// (assignMgr, citizenIBP, spotRadius, doAssign) -> bool
static uint64_t (*s_fn_TryAssignToSpot)(uintptr_t assignMgr, void* citizenIBP, int spotRadius, char doAssign) = nullptr;

// StorageComponent_QueryCount (0x14012cf10) — already inline in engine_funcs

// CitizenAI_State_LongTermPlan (0x140115e60) — already in engine_funcs as fn_CitizenAI_LongTermPlan

// FUN_1400f7100 — Work::CanStartWork (already in engine_funcs as fn_Work_CanStartWork)

// System::MemoryAllocator::MemAlloc/MemFree — use fn_MemAlloc/fn_MemFreeStatic from engine_funcs

// Work::Serialize base (0x1400f7400)
static void (*s_fn_Work_Serialize)(void* self, void* stream) = nullptr;

// 0x1400032e0 (Array<Ptr8>::SetSize) and 0x1400021d0 (Array<int32>::SetSize) are
// exe-side Collection::Array<T> monomorphisations -- ENDGAME B.3.2 step 2 converted
// their two call sites below to GameColl::AsArray<T> member calls, so their
// resolved-pointer stand-ins are gone.

// Object::InternalBasePtr::Serialize (dispatch slot 0x140152aa0)
static void (*s_fn_IBP_Serialize)(void* ibp, void* serializer) = nullptr;

// IO::Serializer::Serialize for int array (dispatch slot 0x140151278)
// (serializer, count, int_array_ptr, name)
static void (*s_fn_Serialize_IntArray)(void* serializer, int count, int* array, const char* name) = nullptr;

// FUN_140002e10 — find PositionComponent on entity (for Slot10/GetTarget)
static void* (*s_fn_FindComp_PositionEntity)(uintptr_t entity, void* outIBP) = nullptr;

// FUN_1400c5400 — cancel pending actions on state machine action list
static void (*s_fn_CancelPendingActions)(void* smActionList) = nullptr;

// TypeId pointers for component lookups
static uint32_t* s_typeId_1401c7660 = nullptr;  // FishingComponent (searched on entity.componentList)
static uint32_t* s_typeId_1401c7698 = nullptr;  // FishingDescription (IsDerivedFrom check)
static uint32_t* s_typeId_1401c4870 = nullptr;  // RadiusComponent
static uint32_t* s_typeId_1401c7e78 = nullptr;  // StorageComponent_FC
static uint32_t* s_typeId_1401c8070 = nullptr;  // StorageLocationComponent / WorkplaceComponent
static uint32_t* s_typeId_1401c7158 = nullptr;  // Citizen type (IsDerivedFrom check in Serialize)

static bool s_fishingwork_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool FishingWork_ResolveFuncs() {
    if (s_fishingwork_funcs_resolved) return true;

    // Entity_FindComponent_MeshInstance (0x140002f90)
    s_fn_FindComponent_MeshInstance = reinterpret_cast<void*(*)(uintptr_t, void*)>(
        ResolveAddr(Module::Exe, 0x140002f90));

    // Entity_FindComponent_Position (0x140002ed0)
    s_fn_FindComponent_Position = reinterpret_cast<void*(*)(uintptr_t, void*)>(
        ResolveAddr(Module::Exe, 0x140002ed0));

    // Entity_impl_2e20 (0x140122e20) — spot position getter
    s_fn_Entity_GetSpotPosition = reinterpret_cast<void(*)(void*, float*, int, int, float*)>(
        ResolveAddr(Module::Exe, 0x140122e20));

    // FUN_14012c3f0 — MeshInstance visibility toggle
    s_fn_MeshInstance_SetVisibility = reinterpret_cast<void(*)(void*, int, int)>(
        ResolveAddr(Module::Exe, 0x14012c3f0));

    // FUN_1400beae0 — Fishing action constructor
    s_fn_FishingAction_Ctor = reinterpret_cast<void(*)(void*, int, void*, float*, float, uint64_t, float, int, uint32_t)>(
        ResolveAddr(Module::Exe, 0x1400beae0));

    // PlayAnimation_Action_Ctor (0x1400c2490)
    s_fn_PlayAnimation_Action_Ctor = reinterpret_cast<void(*)(void*, int, uint32_t)>(
        ResolveAddr(Module::Exe, 0x1400c2490));

    // FUN_1400f8620 — TryAssignToSpot
    s_fn_TryAssignToSpot = reinterpret_cast<uint64_t(*)(uintptr_t, void*, int, char)>(
        ResolveAddr(Module::Exe, 0x1400f8620));

    // Work::Serialize base (0x1400f7400)
    s_fn_Work_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    // InternalBasePtr::Serialize via dispatch slot 0x140152aa0
    {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152aa0);
        if (slot) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(slot);
    }

    // Serialize int array via dispatch slot 0x140151278
    {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140151278);
        if (slot) s_fn_Serialize_IntArray = *reinterpret_cast<void(**)(void*, int, int*, const char*)>(slot);
    }

    // FUN_140002e10 — find PositionComponent on entity
    s_fn_FindComp_PositionEntity = reinterpret_cast<void*(*)(uintptr_t, void*)>(
        ResolveAddr(Module::Exe, 0x140002e10));

    // FUN_1400c5400 — cancel pending actions
    s_fn_CancelPendingActions = reinterpret_cast<void(*)(void*)>(
        ResolveAddr(Module::Exe, 0x1400c5400));

    // Type IDs
    s_typeId_1401c7660 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7660));
    s_typeId_1401c7698 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7698));
    s_typeId_1401c4870 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4870));
    s_typeId_1401c7e78 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e78));
    s_typeId_1401c8070 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c8070));
    s_typeId_1401c7158 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));

    bool ok = s_fn_FindComponent_MeshInstance && s_fn_FindComponent_Position &&
              s_fn_Entity_GetSpotPosition && s_fn_MeshInstance_SetVisibility &&
              s_fn_FishingAction_Ctor && s_fn_PlayAnimation_Action_Ctor &&
              s_fn_TryAssignToSpot &&
              s_fn_Work_Serialize &&
              s_fn_IBP_Serialize && s_fn_Serialize_IntArray &&
              s_fn_FindComp_PositionEntity && s_fn_CancelPendingActions &&
              s_typeId_1401c7660 && s_typeId_1401c7698 &&
              s_typeId_1401c4870 && s_typeId_1401c7e78 && s_typeId_1401c8070 &&
              s_typeId_1401c7158 &&
              fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && fn_MemFreeStatic && fn_MemAlloc &&
              fn_Map_CheckPassable && fn_Work_CanStartWork &&
              fn_CitizenAI_LongTermPlan && fn_Random_RandTol &&
              g_pGameWorldSingleton && g_pMemAllocSingleton && g_fUVScale &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor && g_fn_Serialize_Int;

#ifdef _DEBUG
    printf("[FishingWork] ResolveFuncs: FindMesh=%p FindPos=%p GetSpotPos=%p SetVis=%p\n",
           (void*)s_fn_FindComponent_MeshInstance, (void*)s_fn_FindComponent_Position,
           (void*)s_fn_Entity_GetSpotPosition, (void*)s_fn_MeshInstance_SetVisibility);
    printf("[FishingWork]   FishAction=%p PlayAnim=%p TryAssign=%p\n",
           (void*)s_fn_FishingAction_Ctor, (void*)s_fn_PlayAnimation_Action_Ctor,
           (void*)s_fn_TryAssignToSpot);
    printf("[FishingWork]   TypeIds: 7660=%p 7698=%p 4870=%p 7e78=%p 8070=%p 7158=%p\n",
           (void*)s_typeId_1401c7660, (void*)s_typeId_1401c7698,
           (void*)s_typeId_1401c4870, (void*)s_typeId_1401c7e78, (void*)s_typeId_1401c8070,
           (void*)s_typeId_1401c7158);
    printf("[FishingWork]   WorkSer=%p IBPSer=%p IntArrSer=%p\n",
           (void*)s_fn_Work_Serialize, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_Serialize_IntArray);
    printf("[FishingWork]   FindPosEnt=%p CancelPending=%p\n",
           (void*)s_fn_FindComp_PositionEntity, (void*)s_fn_CancelPendingActions);
    if (!ok) printf("[FishingWork] ERROR: some function pointers not resolved!\n");
#endif

    s_fishingwork_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Helper: Find a specific component by type ID on an entity's component list
// Searches componentList at entity+0x30 (array ptr) / entity+0x38 (count)
// Returns raw pointer to component, or nullptr if not found.
// ============================================================
static void* FindComponentOnEntity(uintptr_t entity, uint32_t typeId) {
    int count = *(int*)(entity + 0x38);
    uintptr_t arrayPtr = *(uintptr_t*)(entity + 0x30);

    for (int i = 0; i < count; i++) {
        uintptr_t* ibpSlot = (uintptr_t*)(arrayPtr + (uintptr_t)i * 8);
        uintptr_t comp = *ibpSlot;
        uintptr_t vtable = *(uintptr_t*)comp;
        typedef uint32_t (*GetTypeId_t)();
        uint32_t compTypeId = ((GetTypeId_t)(*(uintptr_t*)(vtable + 0x10)))();
        if (compTypeId == typeId) {
            ibpSlot = (uintptr_t*)(arrayPtr + (uintptr_t)i * 8);
            void* result = (void*)*ibpSlot;
            if (fn_IsDerivedFrom(result, typeId)) {
                return result;
            }
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// Helper: Find component on a sub-object (entity+0x28 description).
// Sub-objects use +0x28 (array ptr) / +0x30 (count) — different from entities.
// ============================================================
static void* FindComponentOnSubObject(uintptr_t subObj, uint32_t typeId) {
    int count = *(int*)(subObj + 0x30);
    uintptr_t arrayPtr = *(uintptr_t*)(subObj + 0x28);

    for (int i = 0; i < count; i++) {
        uintptr_t comp = *(uintptr_t*)(arrayPtr + (uintptr_t)i * 8);
        uintptr_t vtable = *(uintptr_t*)comp;
        typedef uint32_t (*GetTypeId_t)();
        uint32_t compTypeId = ((GetTypeId_t)(*(uintptr_t*)(vtable + 0x10)))();
        if (compTypeId == typeId) {
            void* c = *(void**)(arrayPtr + (uintptr_t)i * 8);
            if (fn_IsDerivedFrom(c, typeId)) {
                return c;
            }
            return nullptr;
        }
    }
    return nullptr;
}


// ============================================================
// Helper: Clamp coordinate to map bounds
// ============================================================
static inline int ClampToMap(int val, int mapSize) {
    if (val < 0) return 0;
    if (val >= mapSize) return mapSize - 1;
    return val;
}

// ============================================================
// Slot4/Execute (0x1400e9f20)
// Main fishing work assignment logic. Called per citizen to assign
// them to a fishing spot at the fishing dock.
//
// Returns: 1 = not ready/retry, 2 = action pushed
// ============================================================
int FishingWork_Slot4_Impl(FishingWork* self, void* citizenIBP, void* /*time*/) {
    uintptr_t selfAddr = (uintptr_t)self;
    int64_t* selfQ = (int64_t*)self;  // for 8-byte indexed access matching Ghidra

    // Get game world
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    // CheckPtr on building entity IBP (at self+0x10)
    uintptr_t buildingEntity = *(uintptr_t*)(selfAddr + 0x10);

    // Find FishingComponent (DAT_1401c7660) on building entity
    void* fishingComp = FindComponentOnEntity(buildingEntity, *s_typeId_1401c7660);

    // Wrap in IBP
    uintptr_t fishingIBP = 0;
    new (&fishingIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(fishingComp));   // B.3.3 bridge

    // Get fishing description via IsDerivedFrom check on component+0x18
    void* compBase = *(void**)(fishingIBP + 0x18);
    void* fishingDesc = nullptr;
    if (fn_IsDerivedFrom(compBase, *s_typeId_1401c7698)) {
        fishingDesc = compBase;
    }

    // Check if fishing season is valid
    // Get terrain type from map grid for the spot index from fishingDesc+0x28
    int spotMapIdx = *(int*)((uintptr_t)fishingDesc + 0x28);
    uintptr_t mapEntries = *(uintptr_t*)(*(uintptr_t*)(gameWorld + 0xf0) + 0x170);
    int64_t offset = (int64_t)spotMapIdx * 0x20;
    // atCapacity = (limit <= current) — true when resource count is at/above limit
    bool atCapacity = *(int*)(mapEntries + offset + 8) <= *(int*)(mapEntries + offset);

    // Toggle mesh instance visibility based on capacity (visGroup 2)
    uintptr_t meshIBPBuf = 0;
    s_fn_FindComponent_MeshInstance(*(uintptr_t*)(selfAddr + 0x10), &meshIBPBuf);
    s_fn_MeshInstance_SetVisibility(*(void**)&meshIBPBuf, 2, atCapacity ? 1 : 0);

    if (atCapacity) {
        return 1;
    }

    // Find position component on building entity
    uintptr_t posIBP = 0;
    s_fn_FindComponent_Position(*(uintptr_t*)(selfAddr + 0x10), &posIBP);

    int citizenCount = (int)selfQ[10];  // +0x50

    if (citizenCount < 1) {
        return 1;
    }

    // Find first empty citizen slot
    uintptr_t citizenRefs = (uintptr_t)selfQ[9];  // +0x48
    int emptySlot = -1;
    for (int i = 0; i < citizenCount; i++) {
        if (*(uintptr_t*)(citizenRefs + (uintptr_t)i * 8) == 0) {
            emptySlot = i;
            break;
        }
    }

    if (emptySlot == -1) {
        return 1;
    }

    // Find first available fishing spot (state == 0)
    // Also reset spots that were marked as state 2 if the map cell is now empty
    int spotCount = (int)selfQ[0xc];    // +0x60
    uintptr_t spotStates = (uintptr_t)selfQ[0xb];  // +0x58
    int* mapGridPtr = *(int**)(gameWorld + 0xf8);
    int mapSize = *mapGridPtr;
    int freeSpot = -1;
    int freeSpotMapCell = -1;

    if (spotCount > 0) {
        for (int i = 0; i < spotCount; i++) {
            // Get spot position (mode=2)
            float spotPos[3];
            s_fn_Entity_GetSpotPosition((void*)posIBP, spotPos, 2, i, nullptr);

            int half = mapSize >> 1;
            int cx = ClampToMap((int)(spotPos[0] + (float)half), mapSize);
            int cy = ClampToMap((int)(spotPos[1] + (float)half), mapSize);
            int cellIdx = mapSize * cy + cx;

            // If spot state is 2 (waiting for reset) and map cell is empty, reset to 0
            int* spotState = (int*)(spotStates + (uintptr_t)i * 4);
            if (*spotState == 2) {
                uintptr_t mapEntryBase = *(uintptr_t*)((uintptr_t)mapGridPtr + 0x80);
                const uintptr_t occupant =
                    *(uintptr_t*)(mapEntryBase + 8 + (uintptr_t)cellIdx * 0x30);
                if (occupant == 0) {
                    *spotState = 0;
                }
            }

            // Pick first free spot
            if (freeSpot < 0 && *spotState == 0) {
                freeSpot = i;
                freeSpotMapCell = cellIdx;
            }
        }



        if (freeSpot >= 0) {
            // Get citizen position (mode=0) for pathfinding
            float citizenPos[3];
            float citizenHeading;
            s_fn_Entity_GetSpotPosition((void*)posIBP, citizenPos, 0, emptySlot, &citizenHeading);

            // Get citizen's home position for pathfinding source
            uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;
            uintptr_t smComp = *(uintptr_t*)(citizenEntity + 0x278);
            uintptr_t citizenPosData = *(uintptr_t*)(smComp + 0x68);

            int cMapSize = *mapGridPtr;
            float cPosX = *(float*)(citizenPosData + 0x158);
            float cPosY = *(float*)(citizenPosData + 0x15c);
            int cHalf = cMapSize >> 1;
            int ccx = ClampToMap((int)(cPosX + (float)cHalf), cMapSize);
            int ccy = ClampToMap((int)(cPosY + (float)cHalf), cMapSize);
            int citizenCell = cMapSize * ccy + ccx;

            // Get spot cell
            float spotPos[3];
            s_fn_Entity_GetSpotPosition((void*)posIBP, spotPos, 0, emptySlot, &citizenHeading);

            int spHalf = cMapSize >> 1;
            int scx = ClampToMap((int)(spotPos[0] + (float)spHalf), cMapSize);
            int scy = ClampToMap((int)(spotPos[1] + (float)spHalf), cMapSize);
            int spotCell = cMapSize * scy + scx;

            // Check passability. This is NOT an A*: 0x1400a00f0 is the region-equivalence
            // test (both cells' region word at cell+0x26 non-zero and EQUAL). A dock on
            // the far bank of a river is therefore unreachable no matter how good its
            // fishable-tile score, and this branch used to return silently — a working
            // dock and a stranded one logged exactly the same nothing. Count it and print
            // the two region ids, which name the cause outright.
            uint64_t passResult = fn_Map_CheckPassable(mapGridPtr, 0xe1f, (uint32_t)citizenCell, spotCell);
            if ((char)passResult == '\0') {
                return 1;
            }

            // Find RadiusComponent on building's description sub-object (+0x28)
            // Sub-objects use +0x28/+0x30 layout, NOT entity +0x30/+0x38
            uintptr_t entityForRadius = *(uintptr_t*)(*(uintptr_t*)(selfAddr + 0x10) + 0x28);
            void* radiusComp = FindComponentOnSubObject(entityForRadius, *s_typeId_1401c4870);

            int radius = *(int*)((uintptr_t)radiusComp + 0x28);
            int radiusSq = radius * radius;

            // Citizen's grid position for random offset
            int citizenGridX = (int)selfQ[5] / cMapSize;  // +0x28
            int citizenGridY = (int)selfQ[5] % cMapSize;

            // Get random source from game world
            void* randPtr = *(void**)(gameWorld + 0xd8);

            // Try up to 10 random fishing positions within radius.
            // Original uses nested do-while to guarantee each attempt has a
            // valid in-radius, in-bounds position before checking terrain.
            int attemptCount = 0;
            do {
                int rx, ry, targetX, targetY;
                // Inner do-while: generate valid position within radius + bounds
                do {
                    do {
                        rx = fn_Random_RandTol(randPtr, radius);
                        ry = fn_Random_RandTol(randPtr, radius);
                        targetX = citizenGridY + rx;
                        targetY = citizenGridX + ry;
                    } while (radiusSq < ry * ry + rx * rx);
                } while (targetY < 0 || targetY >= cMapSize);
                // Note: X bounds not checked matches original (negative X fails terrain check)

                // Clamp X for safety when accessing grid data
                if (targetX < 0 || targetX >= cMapSize) {
                    attemptCount++;
                    continue;
                }


                // Check map cell for valid water terrain
                int64_t mapCellIdx = (int64_t)(cMapSize * targetY + targetX);
                uintptr_t mapData = *(uintptr_t*)((uintptr_t)mapGridPtr + 0x80);
                uint8_t terrainType = *(uint8_t*)(mapData + 0x24 + mapCellIdx * 0x30);
                uint8_t terrainBit = *(uint8_t*)(mapData + 0x29 + mapCellIdx * 0x30);

                // Check fishing description allows this terrain and terrain is nonzero
                uint32_t fishingTerrainMask = *(uint32_t*)((uintptr_t)fishingDesc + 0x2c);
                if (((fishingTerrainMask >> (terrainBit & 0x1f)) & 1) == 0) {
                    attemptCount++;
                    continue;
                }
                if (terrainType == 0) {
                    attemptCount++;
                    continue;
                }

                // Valid spot found — create fishing action

                // Build action parameters
                uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);
                uintptr_t citizenRef = *(uintptr_t*)citizenIBP;
                float uvScale = (*g_fUVScale / (float)terrainType) * *(float*)(fishingIBP + 0x54);
                uint32_t spotInfo = ((freeSpot | 0x800) << 4) | emptySlot;

                s_fn_FishingAction_Ctor(
                    &citizenRef, (int)selfQ[4], &entityRef,
                    citizenPos, citizenHeading,
                    *(uint64_t*)((uintptr_t)fishingDesc + 0x20),
                    uvScale, freeSpotMapCell, spotInfo);

                // Mark spot as in-use (state = 1)
                *(int*)(spotStates + (uintptr_t)freeSpot * 4) = 1;

                // Finalize: push action, set citizen slot
                int priority = (int)selfQ[4];  // +0x20
                uintptr_t ce = *(uintptr_t*)citizenIBP;

                uintptr_t vt = *(uintptr_t*)self;
                typedef uint64_t* (*VtSlot50_t)(FishingWork*, void*);
                uint64_t* res = (*(VtSlot50_t*)(vt + 0x50))(self, &entityRef);
                uint64_t actionVal = *res;

                *(uint32_t*)(ce + 0x54) |= 4;
                *(int32_t*)(ce + 0x30) = priority;
                *(int32_t*)(ce + 0xb0) = 0;

                if (actionVal == *(uint64_t*)(ce + 0x1f0) && priority >= 0) {
                    *(uint32_t*)(ce + 0x88) |= 0x20000;
                }

                // Store citizen ref in slot
                *(uintptr_t*)(selfQ[9] + (int64_t)emptySlot * 8) = *(uintptr_t*)citizenIBP;
                // Reset retry counter
                *(int*)(selfAddr + 0x68) = 0;
                return 2;
            } while (attemptCount < 10);

            // All 10 random attempts failed — retry with animation if under limit
            if ((int)selfQ[0xd] < 0xb) {  // retry count < 11
                uintptr_t citizenRef2 = *(uintptr_t*)citizenIBP;
                s_fn_PlayAnimation_Action_Ctor(&citizenRef2, (int)selfQ[4], emptySlot);

                int priority = (int)selfQ[4];
                uintptr_t ce = *(uintptr_t*)citizenIBP;

                uintptr_t vt = *(uintptr_t*)self;
                typedef uint64_t* (*VtSlot50_t)(FishingWork*, void*);
                uint64_t* res = (*(VtSlot50_t*)(vt + 0x50))(self, &citizenRef2);
                uint64_t actionVal = *res;

                *(uint32_t*)(ce + 0x54) |= 4;
                *(int32_t*)(ce + 0x30) = priority;
                *(int32_t*)(ce + 0xb0) = 0;

                if (actionVal == *(uint64_t*)(ce + 0x1f0) && priority >= 0) {
                    *(uint32_t*)(ce + 0x88) |= 0x20000;
                }

                *(uintptr_t*)(selfQ[9] + (int64_t)emptySlot * 8) = *(uintptr_t*)citizenIBP;
                // Increment retry counter
                *(int*)(selfAddr + 0x68) = (int)selfQ[0xd] + 1;
                return 2;
            }

            // Retries exhausted — check if storage has items, go to long-term plan
            uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;
            uintptr_t homeEntity = *(uintptr_t*)(citizenEnt + 0x10);

            // Find StorageComponent on home entity
            void* storageComp = FindComponentOnEntity(homeEntity, *s_typeId_1401c7e78);
            uintptr_t storageIBP = 0;
            new (&storageIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(storageComp));   // B.3.3 bridge

            int storageCount = fn_StorageComponent_QueryCount((void*)storageIBP);
            if (storageCount < 1) {
                return 1;
            }

            // Has storage items — replan citizen
            fn_CitizenAI_LongTermPlan(*(void**)citizenIBP);
            return 2;
        }
    }

    // No free spot found — try workplace assignment fallback
    // Iterate spots and check if any has an entity that can be assigned
    if (spotCount > 0) {
        for (int i = 0; i < spotCount; i++) {
            float spotPos[3];
            s_fn_Entity_GetSpotPosition((void*)posIBP, spotPos, 2, i, nullptr);

            int half = *mapGridPtr >> 1;
            int cx = ClampToMap((int)(spotPos[0] + (float)half), *mapGridPtr);
            int cy = ClampToMap((int)(spotPos[1] + (float)half), *mapGridPtr);
            int cellIdx = *mapGridPtr * cy + cx;

            // Check if there's a structure at this cell
            uintptr_t mapData = *(uintptr_t*)((uintptr_t)mapGridPtr + 0x80);
            uintptr_t cellEntity = *(uintptr_t*)(mapData + 8 + (uintptr_t)cellIdx * 0x30);
            if (cellEntity == 0) continue;

            // ⚠ KNOWN DIVERGENCE FROM RETAIL — self-assignment guard.
            //
            // This fallback asks "is there some OTHER workplace standing on my attach
            // cells that this citizen could be assigned to instead?", looks up that
            // workplace's WorkComponent id and hands it to FUN_1400f8620, which dispatches
            // that work's Execute (vtable +0x20). In retail cell+8 on a dock's spot cells
            // holds a NATURAL RESOURCE (it is the resource slot — HerbalistWork reads the
            // same field to find herbs), so the work found is never this dock's own.
            //
            // In our standalone, cell+8 at the dock's own footprint holds the DOCK ENTITY,
            // so the lookup returns THIS FishingWork and f8620 re-enters this very
            // function with the same arguments — unbounded recursion and a STACK_OVERFLOW
            // (measured: FishingWork_Slot4 -> f8620 -> FishingWork_Slot4 ... same self,
            // same cellId=2). f8620's own re-entrancy guard cannot help: it stamps
            // work+0x2c only AFTER Execute returns, so the stamp is never reached.
            //
            // Skipping our own entity is therefore restoring retail's behaviour, not
            // papering over it — retail simply never encounters itself here.
            //
            // UPSTREAM CAUSE FOUND 2026-08-15 and fixed: the dock's own footprint claim
            // (dc20) was taking cell+8 on its '_' walkway cells, which are exactly where
            // its four mode-2 drop spots sit. BANISHED_LIVE_WALKWAY_DROPCELLS (default on)
            // leaves those cells unowned, so this branch should now be unreachable. It
            // stays as a tripwire: if it ever fires again the claim is back, and the
            // dock's 4-catch cap is back with it.
            if (cellEntity == *(uintptr_t*)((uintptr_t)self + 0x10)) {
                static bool s_warned = false;
                if (!s_warned) {
                    s_warned = true;
                    std::fprintf(stderr, "[FishWork] WARNING: the dock itself is at cell+8 of "
                                 "its own drop cell %d — the footprint claim is back "
                                 "(BANISHED_LIVE_WALKWAY_DROPCELLS off, or a new writer). "
                                 "The dock will cap at spotCount catches.\n", cellIdx);
                    std::fflush(stderr);
                }
                continue;
            }

            // Check for WorkplaceComponent (DAT_1401c8070) on cell entity
            uintptr_t cellEnt = *(uintptr_t*)&cellEntity;
            int compCount = *(int*)(cellEnt + 0x38);
            uintptr_t compArray = *(uintptr_t*)(cellEnt + 0x30);
            void* workplaceComp = nullptr;

            for (int j = 0; j < compCount; j++) {
                uintptr_t* ibpSlot = (uintptr_t*)(compArray + (uintptr_t)j * 8);
                uintptr_t comp = *ibpSlot;
                uintptr_t vt = *(uintptr_t*)comp;
                typedef uint32_t (*GetTypeId_t)();
                uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(vt + 0x10)))();
                if (tid == *s_typeId_1401c8070) {
                    ibpSlot = (uintptr_t*)(compArray + (uintptr_t)j * 8);
                    void* c = (void*)*ibpSlot;
                    if (fn_IsDerivedFrom(c, *s_typeId_1401c8070)) {
                        workplaceComp = c;
                    }
                    break;
                }
            }

            uintptr_t workplaceIBP = 0;
            new (&workplaceIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(workplaceComp));   // B.3.3 bridge

            // ── GUARD FOR ONE OF OUR OWN DEVIATIONS (READ of 0x28) ───────────────────
            // The `*(int*)(workplaceIBP + 0x28)` below is FAITHFUL. Verified against
            // retail 0x1400ea775..0x1400ea793: the not-found arm does
            // `xor edx,edx; Ctor(&ibp, edx); CheckPtr; mov rax,[ibp]; mov r8d,[rax+0x28]`
            // and the found arm `cmovne`s the component into the same rdx before joining
            // at 0x1400ea777, so BOTH arms reach the same unguarded deref. Retail is
            // relying on an invariant, and we know exactly which one — and exactly which
            // of our deviations breaks it. Measured end to end, seed 999:
            //
            //  1. Retail's footprint claim (dc20) writes the owner entity into cell+8 for
            //     '#' AND '_' cells alike — `CMP CL,0x2e ; SETNZ BL`, only '.' is free.
            //     BANISHED_LIVE_WALKWAY_DROPCELLS (default ON, keystone_a_map.cpp:411)
            //     deliberately treats '_' as '.' so the dock's drop cells read cell+8 == 0.
            //     That flag is what lifted the dock's 4-catch cap.
            //  2. SpawnComponent's DoSpawn gate TileFree (0x140120c30) tests cell+8 —
            //     `mov 0x8(%rax,%rcx,8),%rax ; test ; je reject`. An unclaimed drop cell
            //     therefore reads as free ground. The other gate, CanPlace (0x14009f5d0),
            //     rejects on the ZONE raster cell+0, which dc20 never writes for a
            //     building, so nothing else stops it.
            //  3. So a TREE spawns on the dock's pier after the dock is built. Confirmed
            //     by two measurements in one run: the footprint held ZERO standing
            //     natural resources at placement (startTown::residue silent), and a tree
            //     stood on drop cell (88,124) later.
            //  4. A tree carries MapComponent/ModelComponent/NaturalResourceComponent/
            //     GrowthComponent/SpawnComponent and NO WorkComponent, so the lookup
            //     below returns null and retail's deref faults.
            //
            // In retail step 1 never happens, cell+8 holds the DOCK, and the dock HAS a
            // WorkComponent — so retail's unguarded deref is safe by construction. This
            // guard therefore compensates for OUR deviation, which is the one case the
            // project's "don't fix a faithful crash with a null check" rule allows. It
            // MUST be removed when the '_' claim is restored, which is blocked on finding
            // retail's step that frees a claimed drop cell for a drop (still unfound).
            //
            // The dump names the occupant so a NEW upstream writer can't hide behind this.
            if (!workplaceComp) {
                static int s_n = 0;
                if (++s_n <= 8) {
                    const uint32_t want = *s_typeId_1401c8070;
                    const char* wantName = ::Object::GetTypeNameById(want);
                    std::fprintf(stderr,
                        "[FishWork] +0x28 NULL: spot[%d] cell=%d tile=(%d,%d) "
                        "cellEntity=%p dock=%p wanted tid=0x%x (%s) — entity carries "
                        "%d component(s):\n", i, cellIdx, cx, cy, (void*)cellEnt,
                        (void*)*(uintptr_t*)((uintptr_t)self + 0x10), want,
                        wantName ? wantName : "UNREGISTERED", compCount);
                    for (int k = 0; k < compCount; ++k) {
                        uintptr_t* s2 = (uintptr_t*)(compArray + (uintptr_t)k * 8);
                        uintptr_t c2 = *s2;
                        if (!c2) { std::fprintf(stderr, "[FishWork]     [%d] <null>\n", k); continue; }
                        uintptr_t v2 = *(uintptr_t*)c2;
                        typedef uint32_t (*GetTypeId_t)();
                        uint32_t t2 = ((GetTypeId_t)(*(uintptr_t*)(v2 + 0x10)))();
                        const char* n2 = ::Object::GetTypeNameById(t2);
                        std::fprintf(stderr, "[FishWork]     [%d] tid=0x%x %s\n",
                                     k, t2, n2 ? n2 : "(unregistered)");
                    }
                    std::fflush(stderr);
                }
                continue;   // skip this spot — see the block above; remove with the '_' claim
            }

            // Try assignment via FUN_1400f8620
            uintptr_t assignMgr = *(uintptr_t*)(gameWorld + 0x118);
            void* citizenRef = *(void**)citizenIBP;
            int spotRadius = *(int*)(workplaceIBP + 0x28);
            uint64_t assignResult = s_fn_TryAssignToSpot(assignMgr, &citizenRef, spotRadius, '\x01');

            if ((char)assignResult != '\0') {
                return 2;
            }
        }
    }

    return 1;
}

// ============================================================
// Slot9/Validate (0x1400e9c30)
// Validates all fishing work assignments. Iterates citizen slots,
// checks each via Work::CanStartWork. Invalid citizens are cleared,
// orphaned spots are marked for reset (state 1 -> 2).
//
// Returns: 1 = all valid (or no citizens), 0 = some invalid
// ============================================================
int FishingWork_Slot9_Impl(FishingWork* self, void* param2) {
    int64_t* selfQ = (int64_t*)self;

    int citizenCount = (int)selfQ[10];  // +0x50
    if (citizenCount <= 0) {
        return 1;
    }

    int* spotMapping = nullptr;     // dynamic array: citizen index -> spot index from task
    int mappingCount = 0;
    int mappingCapacity = 0;
    bool allValid = true;
    int iterIndex = 0;
    uintptr_t citizenRefs = (uintptr_t)selfQ[9];  // +0x48

    for (int i = 0; i < citizenCount; i++) {
        uintptr_t citizenRef = *(uintptr_t*)(citizenRefs + (uintptr_t)i * 8);
        if (citizenRef == 0) continue;

        // Validate this citizen's task
        uintptr_t citizenIBP = citizenRef;
        uintptr_t outTask = 0;
        char valid = (char)fn_Work_CanStartWork(self, param2, &citizenIBP, &outTask);

        if (valid == '\0') {
            // Invalid — clear citizen slot
            uintptr_t temp = 0;
            new (&temp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            *(uintptr_t*)(citizenRefs + (uintptr_t)i * 8) = temp;
            allValid = false;
        } else {
            // Valid — record the spot index from task data
            int taskSpotInfo = *(int*)(outTask + 0x34);

            int newCount = mappingCount + 1;
            if (newCount != mappingCount) {
                // Grow array if needed
                if (mappingCapacity < newCount) {
                    int newCap = newCount;
                    if (mappingCount != 0) {
                        newCap = newCount * 3;
                    }
                    int* newArr = (int*)fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 4, 4);

                    // Copy existing data
                    int copyStart = (spotMapping == newArr) ? mappingCount : 0;
                    for (int j = copyStart; j < newCount; j++) {
                        if (newArr) {
                            if (j < mappingCount && spotMapping) {
                                newArr[j] = spotMapping[j];
                            } else {
                                // New entry: extract spot index from taskSpotInfo
                                newArr[j] = (taskSpotInfo >> 4) & 0xf;
                            }
                        }
                    }

                    int* oldArr = spotMapping;
                    spotMapping = newArr;
                    mappingCapacity = newCap;

                    if (oldArr != newArr && oldArr != nullptr) {
                        fn_MemFreeStatic(oldArr);
                    }
                } else {
                    // Room in existing array
                    for (int j = mappingCount; j < newCount; j++) {
                        if (spotMapping) {
                            spotMapping[j] = (taskSpotInfo >> 4) & 0xf;
                        }
                    }
                }
                mappingCount = newCount;
            }
        }
        iterIndex++;
    }

    // If any citizen was invalid, mark orphaned spots as needing reset
    if (!allValid) {
        int spotCount = (int)selfQ[0xc];  // +0x60
        uintptr_t spotStates = (uintptr_t)selfQ[0xb];  // +0x58

        for (int s = 0; s < spotCount; s++) {
            // Check if any valid citizen maps to this spot
            bool spotOccupied = false;
            for (int m = 0; m < mappingCount; m++) {
                if (spotMapping[m] == s) {
                    spotOccupied = true;
                    break;
                }
            }

            if (!spotOccupied) {
                // If spot was in-use (state 1), mark for reset (state 2)
                int* state = (int*)(spotStates + (uintptr_t)s * 4);
                if (*state == 1) {
                    *state = 2;
                }
            }
        }
    }

    fn_MemFreeStatic(spotMapping);
    return allValid ? 1 : 0;
}

// ============================================================
// Serialize (0x1400ea910)
// Serializes FishingWork fields:
//   1. Work base serialize
//   2. IO::Serializer(version=0)
//   3. Serialize citizenCount (+0x50), resize IBP array if loading
//   4. Loop: IBP::Serialize each citizen ref, validate with IsDerivedFrom
//   5. Serialize spotCount (+0x60), resize int array if loading
//   6. Serialize spotStates int array (+0x58)
//   7. Serialize retryCount (+0x68)
// ============================================================
void FishingWork_Serialize_Impl(FishingWork* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    // 1. Call Work::Serialize base
    s_fn_Work_Serialize(self, stream);

    // 2. Create IO::Serializer (version=0)
    alignas(8) uint8_t serializer[24] = {0};
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // 3. Serialize citizenCount at +0x50
    int citizenCount = *(int*)(selfAddr + 0x50);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&citizenCount), nullptr);   // B.3.4 bridge

    // If loading (serializer mode == 0 at *(int*)(serializer[0] + 8)):
    // resize the IBP array at +0x48 to the deserialized count
    // B.3.2 bridge: FUN_1400032e0 == Collection::Array<Ptr8>::SetSize
    // (ImmigrationComp_DynArrayResize in immigrationcomponent.cpp).
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        GameColl::AsArray<GameColl::Ptr8>((void*)(selfAddr + 0x48)).SetSize(citizenCount);
    }

    // 4. Loop over each citizen IBP, serialize and validate
    uintptr_t ibpArray = *(uintptr_t*)(selfAddr + 0x48);
    int64_t remaining = (int64_t)citizenCount;
    if (remaining > 0) {
        uintptr_t ibpPtr = ibpArray;
        do {
            // Serialize the InternalBasePtr
            s_fn_IBP_Serialize((void*)ibpPtr, serializer);

            // Validate: if the ptr is non-null and not derived from expected type, null it
            uintptr_t ptrVal = *(uintptr_t*)ibpPtr;
            if (ptrVal != 0) {
                if (!fn_IsDerivedFrom((void*)ptrVal, *s_typeId_1401c7158)) {
                    *(uintptr_t*)ibpPtr = 0;
                }
            }

            ibpPtr += 8;
            remaining--;
        } while (remaining != 0);
    }

    // 5. Serialize spotCount at +0x60
    int spotCount = *(int*)(selfAddr + 0x60);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&spotCount), nullptr);   // B.3.4 bridge

    // If loading, resize the int array at +0x58
    // B.3.2 bridge: FUN_1400021d0 == Collection::Array<int32>::SetSize (retail
    // FishingWork_Shutdown calls it at 1400ea9ef). I32NoInit — a grown tail is left as
    // the allocation found it, like retail. Safe: the resize runs only when loading and
    // the bulk int read below writes all spotCount spot states from the stream.
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        GameColl::AsArray<GameColl::I32NoInit>((void*)(selfAddr + 0x58)).SetSize(spotCount);
    }

    // 6. Serialize the spotStates int array
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(spotCount, reinterpret_cast<int*>(*(int**)(selfAddr + 0x58)), nullptr);   // B.3.4 bridge

    // 7. Serialize retryCount at +0x68
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>((selfAddr + 0x68)), nullptr);   // B.3.4 bridge

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Slot6 (0x1400e9eb0)
// Called when a citizen completes fishing work at a spot.
// Clears the citizen IBP slot and marks the spot state as 2 (done)
// if the high bit flag is set in param_4.
//
// param_4 encoding: bits[3:0] = citizen slot index,
//                   bit 15 = has spot info,
//                   bits[7:4] = spot index
// Returns: 0 (via cleared upper bytes)
// ============================================================
int FishingWork_Slot6_Impl(FishingWork* self, void* param2, void* param3, uint32_t param4) {
    (void)param2;
    (void)param3;
    uintptr_t selfAddr = (uintptr_t)self;

    // Clear citizen IBP slot: citizenRefs[param4 & 0xf] = nullptr IBP
    uintptr_t nullIBP = 0;
    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    uint32_t citizenSlot = param4 & 0xf;
    *(uintptr_t*)(*(uintptr_t*)(selfAddr + 0x48) + (uintptr_t)citizenSlot * 8) = nullIBP;

    // If bit 15 is set, mark the spot state as 2 (done/needs reset)
    if ((param4 >> 15) & 1) {
        uint32_t spotIdx = (param4 >> 4) & 0xf;
        uintptr_t spotStates = *(uintptr_t*)(selfAddr + 0x58);
        // TEARDOWN_WILD_WRITE.md: same unbounded retail store (0x1400e9eb0) as
        // ConsumeProduce::OnComplete; report an index past the spot count first.
        if ((int)spotIdx >= *(int*)(selfAddr + 0x60) || (int)citizenSlot >= *(int*)(selfAddr + 0x50)) {
            std::fprintf(stderr,
                "[WILDWRITE] FishingWork::Slot6 OUT OF RANGE: work=%p entity=%p workId=%d "
                "param4=0x%x -> spotIdx=%u (count %d, cap %d, array %p) citizenSlot=%u (count %d)\n",
                (void*)self, *(void**)(selfAddr + 0x10), *(int*)(selfAddr + 0x20), param4,
                spotIdx, *(int*)(selfAddr + 0x60), *(int*)(selfAddr + 0x64), (void*)spotStates,
                citizenSlot, *(int*)(selfAddr + 0x50));
            RedzoneAlloc_PrintBacktrace("FishingWork::Slot6");
        }
        *(int32_t*)(spotStates + (uintptr_t)spotIdx * 4) = 2;
    }

    return 0;
}

// ============================================================
// Slot7 (0x1400e9e80)
// Delegates to Slot6 (vtable +0x30) with same parameters.
// The decompiled code copies param2 and param3 then calls
// vtable[+0x30](self, &copy2, &copy3).
// ============================================================
//
// THE TEARDOWN WILD WRITE (docs/runtime_port/TEARDOWN_WILD_WRITE.md, closed 2026-09-13).
// Retail 0x1400e9e80 is 11 instructions and never touches R9: the caller
// (CompleteTask::CheckEntity -> 0x1400f8800 TaskReassign) loads R9 = the action's
// packed slot word, and Slot6 reads the SAME R9 — the 4th argument is forwarded by
// register passthrough, which Ghidra renders as a 3-parameter function. This port
// copied the decompile: three parameters, three arguments, and Slot6 read whatever
// GCC had left in R9 (measured: 0xc9309598, the low half of a code address), so
// `spotStates[(R9 >> 4) & 0xf] = 2` and `citizenRefs[R9 & 0xf] = 0` landed up to
// 60 bytes past two 4-entry arrays on every cancelled fishing action. The 4-byte
// `2` that hit a neighbouring boundary tag was the crash in EntityList::Shutdown.
// See [[feedback_ghidra_dropped_args]]: an indirect call's register args are not
// in the decompile; read the disassembly for every slot that forwards.
int FishingWork_Slot7_Impl(FishingWork* self, void* param2, void* param3, uint32_t param4) {
    // Copy the pointed-to values
    uintptr_t copy2 = *(uintptr_t*)param2;
    uintptr_t copy3 = *(uintptr_t*)param3;

    // Call vtable slot +0x30 (Slot6) with the packed slot word forwarded.
    uintptr_t vt = *(uintptr_t*)self;
    typedef int (*VtSlot30_t)(FishingWork*, void*, void*, uint32_t);
    (*(VtSlot30_t*)(vt + 0x30))(self, &copy2, &copy3, param4);
    return 0;
}

// ============================================================
// Slot8/CancelAll (0x1400ea830)
// Cancels all active fishing work:
//   - Iterates citizen slots, for each non-null citizen:
//     calls CancelPendingActions on their state machine action list
//   - Then clears the citizen IBP slot
// ============================================================
int FishingWork_Slot8_Impl(FishingWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    int citizenCount = *(int*)(selfAddr + 0x50);
    if (citizenCount <= 0) return 0;

    int i = 0;
    int64_t offset = 0;
    do {
        uintptr_t* citizenSlot = (uintptr_t*)(*(uintptr_t*)(selfAddr + 0x48) + offset);
        if (*citizenSlot != 0) {
            // CheckPtr on citizen IBP
            uintptr_t citizenEntity = *citizenSlot;

            // Get state machine component at citizen+0x278
            uintptr_t smComp = *(uintptr_t*)(citizenEntity + 0x278);

            // Cancel pending actions on smComp+0x48 (action list)
            s_fn_CancelPendingActions((void*)(smComp + 0x48));
        }

        i++;
        offset += 8;
    } while (i < *(int*)(selfAddr + 0x50));
    return 0;
}

// ============================================================
// Slot10/GetTarget (0x1400ea8b0)
// Returns the target entity's position IBP (entity+0x48) via
// the building entity reference at self+0x10.
// Uses FUN_140002e10 to find PositionComponent on building entity.
// Returns param2 (hidden return pointer pattern).
// ============================================================
void* FishingWork_Slot10_Impl(FishingWork* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;

    // CheckPtr on building entity IBP (at self+0x10)
    uintptr_t buildingEntity = *(uintptr_t*)(selfAddr + 0x10);

    // Find PositionComponent via FUN_140002e10
    uintptr_t posIBP[1] = {0};
    s_fn_FindComp_PositionEntity(buildingEntity, posIBP);

    // CheckPtr on the returned IBP

    // Read position entity's field at +0x48 and store in output
    uintptr_t posEntity = *(uintptr_t*)posIBP;
    *(uintptr_t*)param2 = *(uintptr_t*)(posEntity + 0x48);

    return param2;
}

// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x140161320 (13 slots), size 0x70, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
//
// FLAGS (this file lacks several canonical functions):
//   - slot0  ScalarDtor : NO FishingWork_ScalarDtor_Impl defined -> /*FLAG*/ nullptr
//   - slot3  Serialize  : FishingWork_Serialize_Impl
//   - slot4  Execute    : named FishingWork_Slot4_Impl (3-arg: self, citizenIBP, time)
//   - slot6  OnComplete : named FishingWork_Slot6_Impl
//   - slot7  OnCancel   : named FishingWork_Slot7_Impl
//   - slot8  Cleanup    : named FishingWork_Slot8_Impl
//   - slot9  Validate   : named FishingWork_Slot9_Impl
//   - slot10 GetTarget  : named FishingWork_Slot10_Impl
//   - slot11 GetPriority: NO FishingWork_GetPriority_Impl defined -> /*FLAG*/ nullptr
//   - Ctor              : NO FishingWork_Ctor_Impl defined (only exe addr
//                         FishingWork_Ctor 0x1400eabe0) -> factory cannot
//                         construct -> /*FLAG*/ returns null until a Ctor_Impl
//                         (or in-place exe-ctor call) is written.
//   - ResolveFuncs      : FishingWork_ResolveFuncs() (does NOT resolve a vtable).
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t FishingWork_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("FishingWork");
    return c;
}

static void* g_FishingWork_Vtable[13] = {
    (void*)&Work_GenericScalarDtor,                   // +0x00 ScalarDtor (NO FishingWork_ScalarDtor_Impl)
    (void*)&Work_Slot1,                 // +0x08 slot1 (null)
    (void*)&FishingWork_GetTypeId,      // +0x10 GetTypeId
    (void*)&FishingWork_Serialize_Impl, // +0x18 Serialize
    (void*)&FishingWork_Slot4_Impl,     // +0x20 Execute   (FishingWork_Slot4_Impl)
    (void*)&Work_Slot5,                 // +0x28 slot5 (returns 0)
    (void*)&FishingWork_Slot6_Impl,     // +0x30 OnComplete(FishingWork_Slot6_Impl)
    (void*)&FishingWork_Slot7_Impl,     // +0x38 OnCancel  (FishingWork_Slot7_Impl)
    (void*)&FishingWork_Slot8_Impl,     // +0x40 Cleanup   (FishingWork_Slot8_Impl)
    (void*)&FishingWork_Slot9_Impl,     // +0x48 Validate  (FishingWork_Slot9_Impl)
    (void*)&FishingWork_Slot10_Impl,    // +0x50 GetTarget (FishingWork_Slot10_Impl)
    (void*)&Work_Slot11_HasActiveTasks,  // +0x58 slot11 (exe 0x140029860: self->+0x50 > 0 = "someone is working this"; was pinned to 0 by Work_SlotRet0)
    (void*)&Work_Slot12,                // +0x60 slot12 (void nop)
};

extern "C" void RegisterFishingWorkVtable() {
    RegisterExeAddr(0x140161320, (uintptr_t)g_FishingWork_Vtable);
}
extern "C" void* FishingWork_CreateInPlace(void* mem) {
    FishingWork_ResolveFuncs();
    return Work_GenericCtor(mem, 0x70, g_FishingWork_Vtable);
}
extern "C" void* FishingWork_Factory(void) {
    FishingWork_ResolveFuncs();
    void* p = Work_Alloc(0x70);
    return p ? Work_GenericCtor(p, 0x70, g_FishingWork_Vtable) : nullptr;
}
REGISTER_TYPE(FishingWork, "Work", &FishingWork_Factory, &FishingWork_CreateInPlace);
