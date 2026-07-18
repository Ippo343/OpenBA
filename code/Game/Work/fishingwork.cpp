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
// This file must NOT depend on hooks.h — it is portable for standalone exe.

#include "FishingWork.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

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

// FUN_1400032e0 — IBP array resize (ptr at +0x48, count/cap at +0x50/+0x54)
static void (*s_fn_IBPArray_Resize)(void* arrayField, int newCount) = nullptr;

// FUN_1400021d0 — int32 array resize (ptr at +0x58, count/cap at +0x60/+0x64)
static void (*s_fn_IntArray_Resize)(void* arrayField, int newCount) = nullptr;

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

    // FUN_1400032e0 — IBP array resize
    s_fn_IBPArray_Resize = reinterpret_cast<void(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x1400032e0));

    // FUN_1400021d0 — int array resize
    s_fn_IntArray_Resize = reinterpret_cast<void(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x1400021d0));

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
              s_fn_Work_Serialize && s_fn_IBPArray_Resize && s_fn_IntArray_Resize &&
              s_fn_IBP_Serialize && s_fn_Serialize_IntArray &&
              s_fn_FindComp_PositionEntity && s_fn_CancelPendingActions &&
              s_typeId_1401c7660 && s_typeId_1401c7698 &&
              s_typeId_1401c4870 && s_typeId_1401c7e78 && s_typeId_1401c8070 &&
              s_typeId_1401c7158 &&
              fn_InternalBasePtr_Ctor && fn_InternalBasePtr_CheckPtr &&
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
    printf("[FishingWork]   WorkSer=%p IBPResize=%p IntResize=%p IBPSer=%p IntArrSer=%p\n",
           (void*)s_fn_Work_Serialize, (void*)s_fn_IBPArray_Resize,
           (void*)s_fn_IntArray_Resize, (void*)s_fn_IBP_Serialize,
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
        fn_InternalBasePtr_CheckPtr(ibpSlot);
        uintptr_t comp = *ibpSlot;
        uintptr_t vtable = *(uintptr_t*)comp;
        typedef uint32_t (*GetTypeId_t)();
        uint32_t compTypeId = ((GetTypeId_t)(*(uintptr_t*)(vtable + 0x10)))();
        if (compTypeId == typeId) {
            ibpSlot = (uintptr_t*)(arrayPtr + (uintptr_t)i * 8);
            fn_InternalBasePtr_CheckPtr(ibpSlot);
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
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t buildingEntity = *(uintptr_t*)(selfAddr + 0x10);

    // Find FishingComponent (DAT_1401c7660) on building entity
    void* fishingComp = FindComponentOnEntity(buildingEntity, *s_typeId_1401c7660);

    // Wrap in IBP
    uintptr_t fishingIBP = 0;
    fn_InternalBasePtr_Ctor(&fishingIBP, fishingComp);

    // Get fishing description via IsDerivedFrom check on component+0x18
    fn_InternalBasePtr_CheckPtr(&fishingIBP);
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
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t meshIBPBuf = 0;
    s_fn_FindComponent_MeshInstance(*(uintptr_t*)(selfAddr + 0x10), &meshIBPBuf);
    fn_InternalBasePtr_CheckPtr(&meshIBPBuf);
    s_fn_MeshInstance_SetVisibility(*(void**)&meshIBPBuf, 2, atCapacity ? 1 : 0);

    if (atCapacity) {
        return 1;
    }

    // Find position component on building entity
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
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
            fn_InternalBasePtr_CheckPtr(&posIBP);
            s_fn_Entity_GetSpotPosition((void*)posIBP, spotPos, 2, i, nullptr);

            int half = mapSize >> 1;
            int cx = ClampToMap((int)(spotPos[0] + (float)half), mapSize);
            int cy = ClampToMap((int)(spotPos[1] + (float)half), mapSize);
            int cellIdx = mapSize * cy + cx;

            // If spot state is 2 (waiting for reset) and map cell is empty, reset to 0
            int* spotState = (int*)(spotStates + (uintptr_t)i * 4);
            if (*spotState == 2) {
                uintptr_t mapEntryBase = *(uintptr_t*)((uintptr_t)mapGridPtr + 0x80);
                if (*(uintptr_t*)(mapEntryBase + 8 + (uintptr_t)cellIdx * 0x30) == 0) {
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
            fn_InternalBasePtr_CheckPtr(&posIBP);
            s_fn_Entity_GetSpotPosition((void*)posIBP, citizenPos, 0, emptySlot, &citizenHeading);

            // Get citizen's home position for pathfinding source
            fn_InternalBasePtr_CheckPtr(citizenIBP);
            uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;
            fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
            uintptr_t smComp = *(uintptr_t*)(citizenEntity + 0x278);
            fn_InternalBasePtr_CheckPtr((void*)(smComp + 0x68));
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
            fn_InternalBasePtr_CheckPtr(&posIBP);
            s_fn_Entity_GetSpotPosition((void*)posIBP, spotPos, 0, emptySlot, &citizenHeading);

            int spHalf = cMapSize >> 1;
            int scx = ClampToMap((int)(spotPos[0] + (float)spHalf), cMapSize);
            int scy = ClampToMap((int)(spotPos[1] + (float)spHalf), cMapSize);
            int spotCell = cMapSize * scy + scx;

            // Check passability
            uint64_t passResult = fn_Map_CheckPassable(mapGridPtr, 0xe1f, (uint32_t)citizenCell, spotCell);
            if ((char)passResult == '\0') {
                return 1;
            }

            // Find RadiusComponent on building's description sub-object (+0x28)
            // Sub-objects use +0x28/+0x30 layout, NOT entity +0x30/+0x38
            fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
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
                fn_InternalBasePtr_CheckPtr(&fishingIBP);

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
                fn_InternalBasePtr_CheckPtr(citizenIBP);
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
                fn_InternalBasePtr_CheckPtr(citizenIBP);
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
            fn_InternalBasePtr_CheckPtr(citizenIBP);
            uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;
            fn_InternalBasePtr_CheckPtr((void*)(citizenEnt + 0x10));
            uintptr_t homeEntity = *(uintptr_t*)(citizenEnt + 0x10);

            // Find StorageComponent on home entity
            void* storageComp = FindComponentOnEntity(homeEntity, *s_typeId_1401c7e78);
            uintptr_t storageIBP = 0;
            fn_InternalBasePtr_Ctor(&storageIBP, storageComp);

            fn_InternalBasePtr_CheckPtr(&storageIBP);
            int storageCount = fn_StorageComponent_QueryCount((void*)storageIBP);
            if (storageCount < 1) {
                return 1;
            }

            // Has storage items — replan citizen
            fn_InternalBasePtr_CheckPtr(citizenIBP);
            fn_CitizenAI_LongTermPlan(*(void**)citizenIBP);
            return 2;
        }
    }

    // No free spot found — try workplace assignment fallback
    // Iterate spots and check if any has an entity that can be assigned
    if (spotCount > 0) {
        for (int i = 0; i < spotCount; i++) {
            float spotPos[3];
            fn_InternalBasePtr_CheckPtr(&posIBP);
            s_fn_Entity_GetSpotPosition((void*)posIBP, spotPos, 2, i, nullptr);

            int half = *mapGridPtr >> 1;
            int cx = ClampToMap((int)(spotPos[0] + (float)half), *mapGridPtr);
            int cy = ClampToMap((int)(spotPos[1] + (float)half), *mapGridPtr);
            int cellIdx = *mapGridPtr * cy + cx;

            // Check if there's a structure at this cell
            uintptr_t mapData = *(uintptr_t*)((uintptr_t)mapGridPtr + 0x80);
            uintptr_t cellEntity = *(uintptr_t*)(mapData + 8 + (uintptr_t)cellIdx * 0x30);
            if (cellEntity == 0) continue;

            // Check for WorkplaceComponent (DAT_1401c8070) on cell entity
            fn_InternalBasePtr_CheckPtr(&cellEntity);
            uintptr_t cellEnt = *(uintptr_t*)&cellEntity;
            int compCount = *(int*)(cellEnt + 0x38);
            uintptr_t compArray = *(uintptr_t*)(cellEnt + 0x30);
            void* workplaceComp = nullptr;

            for (int j = 0; j < compCount; j++) {
                uintptr_t* ibpSlot = (uintptr_t*)(compArray + (uintptr_t)j * 8);
                fn_InternalBasePtr_CheckPtr(ibpSlot);
                uintptr_t comp = *ibpSlot;
                uintptr_t vt = *(uintptr_t*)comp;
                typedef uint32_t (*GetTypeId_t)();
                uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(vt + 0x10)))();
                if (tid == *s_typeId_1401c8070) {
                    ibpSlot = (uintptr_t*)(compArray + (uintptr_t)j * 8);
                    fn_InternalBasePtr_CheckPtr(ibpSlot);
                    void* c = (void*)*ibpSlot;
                    if (fn_IsDerivedFrom(c, *s_typeId_1401c8070)) {
                        workplaceComp = c;
                    }
                    break;
                }
            }

            uintptr_t workplaceIBP = 0;
            fn_InternalBasePtr_Ctor(&workplaceIBP, workplaceComp);
            fn_InternalBasePtr_CheckPtr(&workplaceIBP);

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
            fn_InternalBasePtr_Ctor(&temp, nullptr);
            *(uintptr_t*)(citizenRefs + (uintptr_t)i * 8) = temp;
            allValid = false;
        } else {
            // Valid — record the spot index from task data
            fn_InternalBasePtr_CheckPtr(&outTask);
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
    uint8_t serializer[24] = {0};
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // 3. Serialize citizenCount at +0x50
    int citizenCount = *(int*)(selfAddr + 0x50);
    g_fn_Serialize_Int(serializer, &citizenCount, nullptr);

    // If loading (serializer mode == 0 at *(int*)(serializer[0] + 8)):
    // resize the IBP array at +0x48 to the deserialized count
    uintptr_t serPtr = *(uintptr_t*)serializer;
    if (*(int*)(serPtr + 8) == 0) {
        s_fn_IBPArray_Resize((void*)(selfAddr + 0x48), citizenCount);
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
    g_fn_Serialize_Int(serializer, &spotCount, nullptr);

    // If loading, resize the int array at +0x58
    serPtr = *(uintptr_t*)serializer;
    if (*(int*)(serPtr + 8) == 0) {
        s_fn_IntArray_Resize((void*)(selfAddr + 0x58), spotCount);
    }

    // 6. Serialize the spotStates int array
    s_fn_Serialize_IntArray(serializer, spotCount,
                            *(int**)(selfAddr + 0x58), nullptr);

    // 7. Serialize retryCount at +0x68
    g_fn_Serialize_Int(serializer, (int*)(selfAddr + 0x68), nullptr);

    // Destroy serializer
    g_fn_Serializer_Dtor(serializer);
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
    uintptr_t selfAddr = (uintptr_t)self;

    // Clear citizen IBP slot: citizenRefs[param4 & 0xf] = nullptr IBP
    uintptr_t nullIBP = 0;
    fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
    uint32_t citizenSlot = param4 & 0xf;
    *(uintptr_t*)(*(uintptr_t*)(selfAddr + 0x48) + (uintptr_t)citizenSlot * 8) = nullIBP;

    // If bit 15 is set, mark the spot state as 2 (done/needs reset)
    if ((param4 >> 15) & 1) {
        uint32_t spotIdx = (param4 >> 4) & 0xf;
        uintptr_t spotStates = *(uintptr_t*)(selfAddr + 0x58);
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
int FishingWork_Slot7_Impl(FishingWork* self, void* param2, void* param3) {
    // Copy the pointed-to values
    uintptr_t copy2 = *(uintptr_t*)param2;
    uintptr_t copy3 = *(uintptr_t*)param3;

    // Call vtable slot +0x30 (Slot6)
    uintptr_t vt = *(uintptr_t*)self;
    typedef int (*VtSlot30_t)(FishingWork*, void*, void*);
    (*(VtSlot30_t*)(vt + 0x30))(self, &copy2, &copy3);
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
            fn_InternalBasePtr_CheckPtr(citizenSlot);
            uintptr_t citizenEntity = *citizenSlot;

            // Get state machine component at citizen+0x278
            fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
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
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t buildingEntity = *(uintptr_t*)(selfAddr + 0x10);

    // Find PositionComponent via FUN_140002e10
    uintptr_t posIBP[1] = {0};
    s_fn_FindComp_PositionEntity(buildingEntity, posIBP);

    // CheckPtr on the returned IBP
    fn_InternalBasePtr_CheckPtr(posIBP);

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
    (void*)&Work_SlotRet0,                   // +0x58 GetPriority (NO FishingWork_GetPriority_Impl)
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
