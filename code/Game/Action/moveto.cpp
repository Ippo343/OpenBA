// code/Game/Action/moveto.cpp
// Clean C++ reimplementation of MoveTo action methods.
// Source: Game/Action/MoveTo.cpp (from profile build debug strings)
//
// MoveTo -- 0x480 bytes (1152!). Full citizen pathfinding and movement.
// Inherits: Object::Internal -> Action (0x20) -> MoveTo
// Vtable at 0x14015fe40 (10 slots).
//
// Key fields:
//   +0x20: targetId / pathfinder slot index (int, -1 = invalid)
//   +0x24: path version (int, 0 = no path cached)
//   +0x28: target entity (InternalBasePtr)
//   +0x30: path data pointer (External ptr to entity description)
//   +0x38: destination X (float)
//   +0x3c: destination Y (float)
//   +0x40: destination Z (float)
//   +0x48: (unused/padding?)
//   +0x5c: terrain flags for pathfinding (uint)
//   +0x60: current path index (int, -1 = finished)
//   +0x64: path node count (int)
//   +0x68: path node array (int[0x100] -- cell indices)
//   +0x468: idle animation ID (int)
//   +0x46c: walk animation ID (int)
//   +0x470: bridge flag (int)
//   +0x474: road flag (bool/byte)
//   +0x478: speed multiplier (float, default 1.0)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "MoveTo.h"
#include "engine_funcs.h"
#include "perf_terrain.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "math/Math.h"
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Component/CharacterComponent.h"
#include "Component/PopulationComponent.h"

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- MoveTo-specific helpers)
// ============================================================

// ENDGAME B.3.1 step 2: Math::Vector3::Serialize was read from the IAT slot at
// 0x140152a68; it is now a direct member call on Math::Vector3.

// FUN_140022970 -- ExternalPtr serialize (path description)
static void (*s_fn_ExternalPtr_Serialize)(void* extPtr, void* serializer) = nullptr;

// fn_IsDerivedFrom via IAT at 0x140152a98
static bool (*s_fn_IsDerivedFrom)(void* obj, uint32_t typeId) = nullptr;

// ENDGAME B.3.1 step 2: the Math value-type helpers below used to be read from the
// exe's IAT dispatch slots, each with the DLL boundary's (this, hidden-retptr, args)
// shape. Runtime-x64.dll is gone, so they are now plain member calls on the real
// Math:: classes (runtime/types/math/Math.h), which return by value and let the
// compiler place the result. The slots they came from, for provenance:
//   0x140152458  ?Dot@Vector2@Math@@QEBA?AVScalar@2@AEBV12@@Z   -> Vector2::Dot
//   0x1401526b0  ?Normalize@Vector2@Math@@QEBA?AV12@XZ          -> Vector2::Normalize
//   0x140152a30  ??0Vector3@Math@@QEAA@MMM@Z                    -> Vector3(x,y,z)
//   0x140151268  ??0Vector3@Math@@QEAA@AEBVScalar@1@0 0@Z       -> Vector3(Scalar,Scalar,Scalar)
//   0x1401512c0  ?Normalize@Vector3@Math@@QEBA?AV12@XZ          -> Vector3::Normalize
//   0x1401512d8  ?Heading@Vector3@Math@@QEBAMXZ                 -> Vector3::Heading
//   0x140151298  ?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z   -> Vector3::Dot
//   0x140152a68  ?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z -> Vector3::Serialize
// The historical R8-was-uninitialised bug on Vector2::Dot (a 2-arg call left the
// "other" operand garbage and produced NaN distances) cannot recur: the operand is
// now a real function parameter.

// FUN_140132010 -- sqrtf implementation
static float (*s_fn_Sqrtf)(float val) = nullptr;

// AnimalComponent_impl_a1f0 -- set animation on entity
// (entity_ptr, animId, param3_float, param4_float)
static void (*s_fn_SetAnimation)(void* entity, uint32_t animId, float p3, float p4) = nullptr;

// FUN_140092260 -- bridge entity query near position
// (mapBridges, outResult, position, radius, flags, excludeEntity)
static void* (*s_fn_BridgeQuery)(void* mapBridges, void* outResult, void* position,
                                  void* radius, int flags, void* excludeEntity) = nullptr;

// MoveTo_helper_8c10 -- path setup (world pos to cell, A* query)
static void* (*s_fn_PathSetup)(void* self, void* worldPos, void* param3, void* param4) = nullptr;

// MoveTo_sub_8d20 -- update facing direction toward next waypoint
static void (*s_fn_UpdateFacing)(void* self) = nullptr;

// MoveTo_helper_e550 -- set pathfinder terrain costs based on bridge flag
static void (*s_fn_SetTerrainCosts)(void* mapGrid, void* pathfinder, int bridgeFlag, void* param4) = nullptr;

// FUN_14009cfb0 -- pathfinder query (start->end cell, with terrain flags)
static void* (*s_fn_PathfinderQuery)(void* mapGrid, void* outResult, uint64_t terrainFlags,
                                      uint32_t startCell, uint32_t endCell) = nullptr;

// MoveTo_helper_6d50 -- copy path from pathfinder result into path array
static int (*s_fn_CopyPathFromFinder)(void* pathfinder, void* slotIdPtr,
                                       void* pathArray, int maxNodes) = nullptr;

// fn_MapGetTerrainPos -- already in engine_funcs.h but we resolve locally too
// CharacterComponent_impl_f3e0 -- same function as fn_MapGetTerrainPos

// Constants
// DAT_1401720d4 = 0.05f (minimum distance threshold)
// DAT_140172114 = 0.5f (half-cell offset for cell-to-world conversion)
// DAT_14017214c = 1.0f (g_fUVScale, used for animation params)
static float* s_fMinDistSq = nullptr;   // 0x1401720d4 = 0.05f
static float* s_fHalfCell = nullptr;    // 0x140172114 = 0.5f

static bool s_moveto_funcs_resolved = false;
static bool s_moveto_funcs_attempted = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool MoveTo_ResolveFuncs() {
    // PERF (critical): cache after the FIRST attempt, not only on full success.
    // MoveTo_Execute lazy-calls this every frame (the action is built by the exe
    // action-ctor path, not our ctor). In standalone two pointers in the `ok` set —
    // the type-id globals 0x1401c7078 / 0x1401c4790, used ONLY by Serialize, never by
    // the per-frame Execute/AbstractImpl path — are "NOT IMPLEMENTED" and never
    // resolve, so `ok` stayed false and `s_moveto_funcs_resolved` never latched. That
    // made this whole ~30-lookup (ResolveAddr + ResolveExport) resolver re-run on EVERY
    // MoveTo_Execute call → ~2.8 ms per citizen per frame, which alone dragged the live
    // citizen sim to ~15 fps. Latching on "attempted" makes it run exactly once.
    if (s_moveto_funcs_attempted) return s_moveto_funcs_resolved;
    s_moveto_funcs_attempted = true;

    uintptr_t iat;

    // ExternalPtr serialize (FUN_140022970)
    s_fn_ExternalPtr_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x140022970));

    // IsDerivedFrom via IAT at 0x140152a98
    iat = ResolveAddr(Module::Exe, 0x140152a98);
    if (iat) s_fn_IsDerivedFrom = *reinterpret_cast<bool(**)(void*, uint32_t)>(iat);

    // (B.3.1: the Math::Vector2/Vector3 slots that used to be resolved here are
    //  direct member calls now — see the header comment above.)

    // sqrtf (FUN_140132010)
    s_fn_Sqrtf = reinterpret_cast<float(*)(float)>(
        ResolveAddr(Module::Exe, 0x140132010));

    // SetAnimation (AnimalComponent_impl_a1f0 at 0x14010a1f0)
    s_fn_SetAnimation = reinterpret_cast<void(*)(void*, uint32_t, float, float)>(
        ResolveAddr(Module::Exe, 0x14010a1f0));

    // BridgeQuery (FUN_140092260)
    s_fn_BridgeQuery = reinterpret_cast<void*(*)(void*, void*, void*, void*, int, void*)>(
        ResolveAddr(Module::Exe, 0x140092260));

    // PathSetup (MoveTo_helper_8c10 at 0x1400c8c10)
    s_fn_PathSetup = reinterpret_cast<void*(*)(void*, void*, void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400c8c10));

    // UpdateFacing (MoveTo_sub_8d20 at 0x1400c8d20)
    s_fn_UpdateFacing = reinterpret_cast<void(*)(void*)>(
        ResolveAddr(Module::Exe, 0x1400c8d20));

    // SetTerrainCosts (MoveTo_helper_e550 at 0x14009e550)
    s_fn_SetTerrainCosts = reinterpret_cast<void(*)(void*, void*, int, void*)>(
        ResolveAddr(Module::Exe, 0x14009e550));

    // PathfinderQuery (FUN_14009cfb0)
    s_fn_PathfinderQuery = reinterpret_cast<void*(*)(void*, void*, uint64_t, uint32_t, uint32_t)>(
        ResolveAddr(Module::Exe, 0x14009cfb0));

    // CopyPathFromFinder (MoveTo_helper_6d50 at 0x1400a6d50)
    s_fn_CopyPathFromFinder = reinterpret_cast<int(*)(void*, void*, void*, int)>(
        ResolveAddr(Module::Exe, 0x1400a6d50));

    // Constants
    s_fMinDistSq = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401720d4));
    s_fHalfCell = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    bool ok = s_fn_ExternalPtr_Serialize &&
              s_fn_IsDerivedFrom &&
              s_fn_Sqrtf && s_fn_SetAnimation &&
              s_fn_BridgeQuery && s_fn_PathSetup && s_fn_UpdateFacing &&
              s_fn_SetTerrainCosts && s_fn_PathfinderQuery && s_fn_CopyPathFromFinder &&
              s_fMinDistSq && s_fHalfCell &&
              g_pGameWorldSingleton && g_fUVScale;

#ifdef _DEBUG
    printf("[MoveTo] ResolveFuncs: ExtPtrSer=%p IsDerived=%p\n",
           (void*)s_fn_ExternalPtr_Serialize, (void*)s_fn_IsDerivedFrom);
    printf("[MoveTo]   Sqrt=%p SetAnim=%p BridgeQ=%p PathSetup=%p UpdFace=%p\n",
           (void*)s_fn_Sqrtf, (void*)s_fn_SetAnimation,
           (void*)s_fn_BridgeQuery, (void*)s_fn_PathSetup, (void*)s_fn_UpdateFacing);
    printf("[MoveTo]   SetCost=%p PFQuery=%p CopyPath=%p\n",
           (void*)s_fn_SetTerrainCosts, (void*)s_fn_PathfinderQuery,
           (void*)s_fn_CopyPathFromFinder);
    printf("[MoveTo]   minDist=%p halfCell=%p\n",
           (void*)s_fMinDistSq, (void*)s_fHalfCell);
    if (!ok) printf("[MoveTo] ERROR: some function pointers not resolved!\n");
#endif

    s_moveto_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Helper: clamp int to [0, mapSize-1]
// ============================================================
static inline int ClampCell(int val, int mapSize) {
    if (val < 0) return 0;
    if (val >= mapSize) return mapSize - 1;
    return val;
}

// ============================================================
// Helper: world position to clamped cell index
// ============================================================
static inline int WorldToCell(int mapSize, float worldX, float worldY) {
    float halfMap = (float)(mapSize >> 1);
    int cellX = ClampCell((int)(worldX + halfMap), mapSize);
    int cellY = ClampCell((int)(worldY + halfMap), mapSize);
    return mapSize * cellY + cellX;
}

// ============================================================
// MoveTo::Serialize (0x1400c8ec0)
// Versioned serialization. Version 0 = status, Version 1 = all path data.
// Fields: +0x18 status, +0x28 target IBP, +0x30 path desc ExternalPtr,
//   +0x64 pathNodeCount, +0x60 currentPathIndex, +0x68 pathNodes[count],
//   +0x38 destination Vec3, +0x5c terrainFlags, +0x468 idleAnimId,
//   +0x46c walkAnimId, +0x470 bridgeFlag (version>0), +0x474 roadFlag,
//   +0x478 speedMultiplier
// ============================================================
void MoveTo::Persist(void* stream) {
    MoveTo_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser0[24];
    alignas(8) uint8_t ser1[24];

    // Version 0: serialize status byte at +0x18
    ::new (static_cast<void*>(ser0)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser0)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser0)->~Serializer();   // B.3.4 bridge

    // Version 1: serialize all movement data
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    // Target entity IBP (+0x28)
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x28)->Serialize(*reinterpret_cast<IO::Serializer*>(ser1));   // B.3.4 bridge
    if (_field28 != nullptr) {
        bool valid = s_fn_IsDerivedFrom(_field28, CharacterComponent::TypeId());
        if (!valid) {
            _field28 = nullptr;
        }
    }

    // Path description ExternalPtr (+0x30)
    s_fn_ExternalPtr_Serialize(s + 0x30, ser1);

    // Path node count (+0x64) and current index (+0x60) as ints
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<int*>(s + 0x64), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<int*>(s + 0x60), nullptr);   // B.3.4 bridge

    // Path node array (+0x68), count from +0x64
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*(int32_t*)(s + 0x64), reinterpret_cast<int*>(s + 0x68), nullptr);   // B.3.4 bridge

    // Destination position (+0x38) as Vector3
    reinterpret_cast<Math::Vector3*>(s + 0x38)->Serialize(ser1);   // B.3.1 bridge

    // Terrain flags (+0x5c) as uint
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x5c), nullptr);   // B.3.4 bridge

    // Idle animation ID (+0x468) as int
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<int*>(s + 0x468), nullptr);   // B.3.4 bridge

    // Walk animation ID (+0x46c) as int
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<int*>(s + 0x46c), nullptr);   // B.3.4 bridge

    // Version check for bridge flag
    int version = reinterpret_cast<IO::Serializer*>(ser1)->GetVersion();   // B.3.4 bridge
    if (version == 0) {
        // Version 0 compatibility: deserialize and discard a legacy IBP field
        void* tempIBP = nullptr;
        reinterpret_cast<Object::InternalBasePtr*>(&tempIBP)->Serialize(*reinterpret_cast<IO::Serializer*>(ser1));   // B.3.4 bridge
        if (tempIBP != nullptr) {
            s_fn_IsDerivedFrom(tempIBP, PopulationComponent::TypeId());
        }
    } else {
        // Version > 0: serialize bridge flag (+0x470)
        version = reinterpret_cast<IO::Serializer*>(ser1)->GetVersion();   // B.3.4 bridge
        if (version > 0) {
            reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<int*>(s + 0x470), nullptr);   // B.3.4 bridge
        }
    }

    // Road flag (+0x474) as bool
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<bool*>(s + 0x474), nullptr);   // B.3.4 bridge

    // Speed multiplier (+0x478) as float
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(speedMultiplier, nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// MoveTo::AbstractImpl (0x1400c8ab0)
// Checks distance to destination. If close enough (< 0.05), returns 0 (arrived).
// Otherwise sets up A* path and returns 1 (moving) or 2 (no path).
// ============================================================
int  MoveTo::AbstractImpl(void* actionStack) {
    MoveTo_ResolveFuncs();   // lazy-resolve: action may be built by Pf_MoveToCtor_9170 (pf_ctors), which never calls our ctor's ResolveFuncs
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    // Get target entity's character component position
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);
    uintptr_t charComp = *(uintptr_t*)(targetEntity + 0x68);

    // Current character position
    float curX = *(float*)(charComp + 0x158);
    float curY = *(float*)(charComp + 0x15c);

    // Destination position from MoveTo fields
    float destX = *(float*)(s + 0x38);
    float destY = *(float*)(s + 0x3c);

    // Compute 2D distance squared: (curX - destX)^2 + (curY - destY)^2
    float dx = curX - destX;
    float dy = curY - destY;
    Math::Vector2 vec2(dx, dy);
    float dotResult = vec2.Dot(vec2).v;        // this-dot → dx²+dy²
    float dist = s_fn_Sqrtf(dotResult);

    // If already close enough, return 0 (arrived)
    if (dist < *s_fMinDistSq) {
        return 0;
    }

    // Get current position again (re-read since CheckPtr may update)
    targetEntity = *(uintptr_t*)(s + 0x28);
    charComp = *(uintptr_t*)(targetEntity + 0x68);

    float worldPos[3];
    worldPos[0] = *(float*)(charComp + 0x158);
    worldPos[1] = *(float*)(charComp + 0x15c);
    worldPos[2] = *(float*)(charComp + 0x160);

    // Set up A* path from current position to destination
    // param3 reuses the vec2 pointer, param4 comes from actionStack (R9 in original)
    s_fn_PathSetup(this, worldPos, &vec2, actionStack);

    // Check path setup result
    int32_t pathVersion = *(int32_t*)(s + 0x24);
    int32_t pathSlotId = targetId;
    int32_t pathIndex = _field60;


    if (pathVersion == 0 || pathSlotId < 0) {
        // No valid path cached
        if (pathIndex < 0) {
            return 2;  // No path possible
        }
    } else {
        // Path cached but current index not started
        if (pathIndex < 0) {
            return 1;  // Wait for path resolution
        }
    }

    // Path is ready -- update facing direction toward first waypoint
    s_fn_UpdateFacing(this);

    // Set walk animation on the target entity
    targetEntity = *(uintptr_t*)(s + 0x28);
    uint32_t idleAnimId = *(uint32_t*)(s + 0x468);
    s_fn_SetAnimation((void*)targetEntity, idleAnimId, *g_fUVScale, *g_fUVScale);

    return 1;
}

// ============================================================
// MoveTo::Execute (0x1400c8570)
// DISABLED HOOK (bug class 3: MOV RAX,RSP in prologue).
// Full A* path traversal along waypoints:
//   - If no current path index (+0x60 == -1): check for cached path, copy from
//     pathfinder, or call AbstractImpl (vtable +0x20) for re-query.
//   - If traversing: compute target cell from path array, check terrain type
//     for speed (road/bridge/normal), update character speed, advance index.
//   - If path exhausted: reset to idle animation.
// Returns: 0=arrived, 1=moving, 2=no path
// ============================================================

// PERF (2026-09-04): these were bit-identical inline copies of the trivial Runtime
// math exports the hot paths called through dispatch slots (a DLL call per 3-float
// store). ENDGAME B.3.1 step 2 (2026-09-13) retires the Math ones: Runtime-x64.dll is
// gone, and the classes in runtime/types/math/Math.h now carry those same bodies —
// Vector3::Dot still evaluates (y, x, z) in that order, so the float result is
// unchanged to the bit. The Math transcriptions removed here were
//   ??0Vector3@Math@@QEAA@MMM@Z (0x1800152d0)           -> Math::Vector3(x, y, z)
//   ??0Vector3@Math@@QEAA@AEBVScalar@1@@Z (0x180015360) -> Math::Vector3(Scalar) (unused)
//   ?Dot@Vector3@Math@@ (0x18001dff0)                   -> Math::Vector3::Dot   (unused here)
//   ?Dot@Vector2@Math@@ (0x18006c840)                   -> Math::Vector2::Dot
//   ?Max@Scalar@Math@@ (0x18005ff60)                    -> Math::Scalar::Max    (unused)
// InlBoxIsect stays: Volume::Box is not a Math value type and is not part of B.3.1.
//   ?Intersect@Box@Volume@@ (0x180012160)               -> InlBoxIsect(a,b)
static inline bool InlBoxIsect(const void* a, const void* b) {
    const float* p = static_cast<const float*>(a); const float* q = static_cast<const float*>(b);
    return p[0] <= q[3] && q[0] <= p[3] && p[1] <= q[4] && q[1] <= p[4] && p[2] <= q[5] && q[2] <= p[5];
}

int  MoveTo::Execute(void* actionStack, void* time) {
    MoveTo_ResolveFuncs();
    (void)time;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    int* mapGrid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);

    // Get character's current world position
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);
    uintptr_t charComp = *(uintptr_t*)(targetEntity + 0x68);

    float curX = *(float*)(charComp + 0x158);
    float curY = *(float*)(charComp + 0x15c);
    float curZ = *(float*)(charComp + 0x160);

    int32_t pathIndex = _field60;

    // ---- CASE 1: No active path segment (pathIndex == -1) ----
    if (pathIndex == -1) {
        int32_t pathVersion = *(int32_t*)(s + 0x24);

        if (pathVersion != 0) {
            // Have a cached path version -- try to retrieve from pathfinder
            int32_t slotId = targetId;
            if (slotId >= 0) {
                // Access pathfinder data structure
                uintptr_t pathfinder = *(uintptr_t*)((uintptr_t)mapGrid + 0x68);


                // Check if slot is still valid
                if (slotId >= *(int32_t*)(pathfinder + 0x168)) {
                    return 1;  // Slot out of range, wait
                }

                // Verify path version matches
                uintptr_t slotBase = *(uintptr_t*)(pathfinder + 0x160);
                int32_t storedVersion = *(int32_t*)(slotBase + (int64_t)slotId * 0x18 + 4);
                if (storedVersion != pathVersion) {
                    return 1;  // Version mismatch, wait for re-query
                }

                // Check if path is ready (slot active flag)
                if (*(char*)(slotBase + (int64_t)slotId * 0x18) == '\0') {
                    return 1;  // Path not yet computed
                }

                // Copy path nodes from pathfinder into our local array
                int nodeCount = s_fn_CopyPathFromFinder(
                    (void*)pathfinder, s + 0x20, s + 0x68, 0x100);
                *(int32_t*)(s + 0x64) = nodeCount;

                if (nodeCount == 0) {
                    return 2;  // Path found but empty -- no valid route
                }

                // Start traversal at index 1 (skip start cell)
                _field60 = 1;

                // Update facing direction
                s_fn_UpdateFacing(this);

                // Set walk animation
                targetEntity = *(uintptr_t*)(s + 0x28);
                uint32_t walkAnimId = *(uint32_t*)(s + 0x468);
                s_fn_SetAnimation((void*)targetEntity, walkAnimId, *g_fUVScale, *g_fUVScale);
                return 1;
            }
        }

        // No cached path or invalid slot -- call AbstractImpl (vtable slot +0x20,
        // a virtual dispatch in retail)
        int result = AbstractImpl(actionStack);
        if (result != 2) {
            return 1;
        }
        return 2;
    }

    // ---- CASE 2: Active path traversal (pathIndex >= 0) ----
    int mapSize = *mapGrid;

    // Compute destination cell from +0x38/+0x3c
    float destX = *(float*)(s + 0x38);
    float destY = *(float*)(s + 0x3c);
    int destCell = WorldToCell(mapSize, destX, destY);

    // Compute current position cell
    int curCell = WorldToCell(mapSize, curX, curY);

    // Check if we've reached the destination cell
    if (curCell == destCell) {
        // At destination -- compute precise distance
        float dz = curZ - *(float*)(s + 0x40);
        float dy2 = curY - destY;
        float dx2 = curX - destX;
        Math::Vector3 diffVec(dx2, dy2, dz);

        Math::Vector2 dotVec(diffVec.x, diffVec.y);
        float dotOut = dotVec.Dot(dotVec).v;   // this-dot → dx²+dy²
        float distToGoal = s_fn_Sqrtf(dotOut);

        if (distToGoal < *s_fMinDistSq) {
            // Close enough -- arrived
            return 0;
        }

        // Not close enough yet -- compute direction and set facing
        float revDx = destX - curX;
        float revDy = destY - curY;
        float revDz = *(float*)(s + 0x40) - curZ;
        Math::Vector3 revVec(revDx, revDy, revDz);

        // Normalize the 2D direction.
        Math::Vector2 dir2d(revVec.x, revVec.y);
        Math::Vector2 dir2dN = dir2d.Normalize();

        // Build 3D facing vector and set on character
        Math::Vector3 facing3d(dir2dN.x, dir2dN.y, 0.0f);
        targetEntity = *(uintptr_t*)(s + 0x28);
        *(uint8_t*)(targetEntity + 0x28) = 0;  // Clear animation lock flag

        // Build normalized facing vector from 2D components
        float scalarX = facing3d.x;
        float scalarY = facing3d.y;
        float scalarZ = 0.0f;
        Math::Vector3 tempVec(scalarX, scalarY, scalarZ);   // Vector3(Scalar,Scalar,Scalar) = 3 stores
        Math::Vector3 normalized = tempVec.Normalize();

        // Write facing direction to character (+0x2c, +0x30, +0x34)
        *(uint32_t*)(targetEntity + 0x2c) = *(uint32_t*)&normalized.x;
        *(uint32_t*)(targetEntity + 0x30) = *(uint32_t*)&normalized.y;
        *(uint32_t*)(targetEntity + 0x34) = *(uint32_t*)&normalized.z;

        // Compute heading angle from facing direction
        float heading = facing3d.Heading();
        *(float*)(targetEntity + 0x3c) = heading;

        return 1;
    }

    // Not at destination cell yet -- check if traversing path
    int32_t nodeCount = *(int32_t*)(s + 0x64);
    if (nodeCount < 2) {
        // Path too short, just keep moving
        return 1;
    }

    // Check if we've reached the end of the path nodes
    if (pathIndex >= nodeCount) {
        // Path exhausted -- set idle animation and reset path
        targetEntity = *(uintptr_t*)(s + 0x28);
        uint32_t idleAnimId = *(uint32_t*)(s + 0x46c);
        s_fn_SetAnimation((void*)targetEntity, idleAnimId, *g_fUVScale, *g_fUVScale);
        _field60 = -1;
        return 1;
    }

    // Look up terrain type at the citizen's CURRENT cell for the speed calculation.
    // Verified vs the real MoveTo::Execute (0x1400c8570): the terrain byte is read at
    // iVar11 = curCell (the cell the citizen currently occupies), NOT destCell. Using
    // destCell here samples the wrong tile's movement cost.
    // [PerfX] TERRAIN_SIDECAR: same byte, read through the 1-byte-per-cell mirror
    // (perf_terrain.h) instead of the 0x30-stride cell record -- raw read when off.
    uintptr_t cellArrayBase = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
    uint8_t terrainType = PerfX::TerrainGet((const uint8_t*)cellArrayBase,
                                            *(int32_t*)((uintptr_t)mapGrid + 0x88), curCell);

    float speed;
    bool onRoad = _field474 != 0;

    if (onRoad) {
        // On road -- use road speed from path data (+0x30)
        uintptr_t pathData = *(uintptr_t*)(s + 0x30);
        speed = *(float*)(pathData + 0x30);
    } else {
        // Check terrain type for road tiles
        // terrainType & ~0x08 in {1, 0} means road/path tile
        if (((terrainType - 1) & 0xFFFFFFF7) == 0) {
            // Road tile -- use road speed
            uintptr_t pathData = *(uintptr_t*)(s + 0x30);
            speed = *(float*)(pathData + 0x34);
        } else if (terrainType == 0) {
            // Normal terrain -- use normal speed
            uintptr_t pathData = *(uintptr_t*)(s + 0x30);
            speed = *(float*)(pathData + 0x38);
        } else {
            // Default speed (e.g., grass) -- use g_fUVScale (1.0)
            speed = *g_fUVScale;
        }
    }

    // Apply speed multiplier and set on character
    targetEntity = *(uintptr_t*)(s + 0x28);
    *(float*)(targetEntity + 0x38) = speed * speedMultiplier;

    // Check if the citizen's CURRENT cell has reached the current waypoint cell.
    // Verified vs the real MoveTo::Execute (0x1400c8570): the comparison is
    // iVar11 (curCell) != pathNodes[pathIndex], NOT destCell. The prior port compared
    // the FINAL destination cell against the waypoint, so the index only advanced when
    // pathIndex already pointed at the last node — intermediate waypoints never fired,
    // UpdateFacing was never re-called, and the citizen bee-lined in a fixed heading
    // past its target forever instead of steering along the A* path.
    int32_t waypointCell = *(int32_t*)(s + 0x68 + (int64_t)pathIndex * 4);
    if (curCell != waypointCell) {
        // Haven't reached the current waypoint yet, keep moving
        return 1;
    }

    // Reached the waypoint -- advance to next
    _field60 = pathIndex + 1;

    // Update facing direction for next segment
    s_fn_UpdateFacing(this);

    return 1;
}

// ============================================================
// MoveTo::Enter (0x1400c8970)
// Bridge detection: queries nearby entities for bridges. If a bridge
// is found facing the same direction, sets the road flag (+0x474).
// ============================================================
void MoveTo::Enter(void*, void*) {
    MoveTo_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    // Get target entity's character component position
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);
    uintptr_t charComp = *(uintptr_t*)(targetEntity + 0x68);

    // Read current position
    float position[3];
    position[0] = *(float*)(charComp + 0x158);
    position[1] = *(float*)(charComp + 0x15c);
    position[2] = *(float*)(charComp + 0x160);

    // Query for nearby bridge entities
    float radius = *s_fHalfCell;   // 0.5f search radius
    uintptr_t bridgeResult = 0;
    void* bridgeMgr = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x88);
    uintptr_t excludeEntity = *(uintptr_t*)(s + 0x28);
    uint8_t bridgeExclude[8];
    *(uintptr_t*)bridgeExclude = excludeEntity;

    s_fn_BridgeQuery(bridgeMgr, &bridgeResult, position, &radius, 0, bridgeExclude);

    if (bridgeResult != 0) {
        // Found a nearby entity -- check if it's closer (not the target entity)
        if (*(uintptr_t*)(s + 0x28) < bridgeResult) {
            // Compare facing directions via dot product
            targetEntity = *(uintptr_t*)(s + 0x28);

            // Our entity's facing direction (+0x2c..+0x34)
            Math::Vector3 myFacing(*(float*)(targetEntity + 0x2c),
                                   *(float*)(targetEntity + 0x30),
                                   *(float*)(targetEntity + 0x34));

            // Bridge entity's facing direction
            Math::Vector3 bridgeFacing(*(float*)(bridgeResult + 0x2c),
                                       *(float*)(bridgeResult + 0x30),
                                       *(float*)(bridgeResult + 0x34));

            // this = bridgeFacing, other = myFacing (the original's RCX / R8).
            float dotOut = bridgeFacing.Dot(myFacing).v;
            if (dotOut > 0.0f) {
                // Facing same direction as bridge -- set road flag
                _field474 = 1;
                return;
            }
        }
    }

    // No bridge or wrong direction -- clear road flag
    _field474 = 0;
}

// ============================================================
// MoveTo::Exit (0x1400c8520)
// Reset character animation to idle (walk anim at +0x46c).
// ============================================================
void MoveTo::Exit(void* actionStack) {
    MoveTo_ResolveFuncs();
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);
    uint32_t idleAnimId = *(uint32_t*)(s + 0x46c);
    s_fn_SetAnimation((void*)targetEntity, idleAnimId, *g_fUVScale, *g_fUVScale);
}

// ============================================================
// MoveTo::CheckEntity (0x1400c84d0)
// Same body as Exit -- reset character animation to idle.
// ============================================================
void MoveTo::CheckEntity() {
    MoveTo_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);
    uint32_t idleAnimId = *(uint32_t*)(s + 0x46c);
    s_fn_SetAnimation((void*)targetEntity, idleAnimId, *g_fUVScale, *g_fUVScale);
}

// ============================================================
// MoveTo::GetTarget (0x1400c8480)
// Returns true if the given param (InternalBasePtr) matches
// the entity at target->+0x10.
// ============================================================
bool MoveTo::GetTarget(void* param) {
    MoveTo_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);

    // Get entity reference at target->+0x10
    uintptr_t entityRef = *(uintptr_t*)(targetEntity + 0x10);

    return entityRef == *(uintptr_t*)param;
}

// ============================================================
// class MoveTo (MoveTo.h) — ENDGAME B.3.8 ACT.
// The hand-built g_MoveTo_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x1400228c0. Retail's default ctor (0x1400c90e0) stores +0x20 = -1, +0x60 = -1, +0x478 = 1.0f;
// the tree's factory zero-filled every field instead. B38 B8-1: the retail stores are kept; the
// fields retail leaves unwritten stay zero-filled (MemAlloc gives no guarantee).
MoveTo::MoveTo()
    : targetId(-1),
      _field24(0),
      _field28(nullptr),
      _field30(nullptr),
      _field38(nullptr),
      _field40(nullptr),
      _field48(nullptr),
      _field50(nullptr),
      _field58(nullptr),
      _field60(-1),
      _field64(0),
      _pathData{},
      _field474(0),
      _pad475{},
      speedMultiplier(1.0f),
      _pad47C{} {}

// [3] 0x1400c8ec0
// [4] 0x1400c8ab0
// [5] 0x1400c8570
// [6] 0x1400c8970
// [7] 0x1400c8520
// [8] 0x1400c84d0
// [9] 0x1400c8480

void* MoveTo::CreateNewObject() {
    MoveTo_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(MoveTo), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MoveTo());
}
void* MoveTo::CreateInPlaceObject(void* storage) {
    MoveTo_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MoveTo());
}


DEFINE_EXE_TYPE(MoveTo, "Action")
