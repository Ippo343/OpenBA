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
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "MoveTo.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <cmath>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- MoveTo-specific helpers)
// ============================================================

// Object::InternalBasePtr::CheckPtr -- via IAT at 0x140152ab0
static void (*s_fn_CheckPtr)(void* ibp) = nullptr;

// Object::InternalBasePtr::Serialize -- via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* ibp, void* serializer) = nullptr;

// IO::Serializer ctor/dtor/getversion -- via IAT
static void (*s_fn_Serializer_Ctor)(void* self, void* stream, int version) = nullptr;
static void (*s_fn_Serializer_Dtor)(void* self) = nullptr;
static int  (*s_fn_Serializer_GetVersion)(void* self) = nullptr;

// IO::Serializer::Serialize(bool*) via IAT at 0x140152a58
static void (*s_fn_Serialize_Bool)(void* ser, void* field, const char* name) = nullptr;

// IO::Serializer::Serialize(int*) via IAT at 0x140152ac8
static void (*s_fn_Serialize_Int)(void* ser, void* field, const char* name) = nullptr;

// IO::Serializer::Serialize(uint*) via IAT at 0x140152a50
static void (*s_fn_Serialize_UInt)(void* ser, void* field, const char* name) = nullptr;

// IO::Serializer::Serialize(float*) via IAT at 0x140152a60
static void (*s_fn_Serialize_Float)(void* ser, void* field, const char* name) = nullptr;

// IO::Serializer::Serialize(int count, int* array, name) via IAT at 0x140151278
// Array serialization: serializer, count, array_ptr, name
static void (*s_fn_Serialize_IntArray)(void* ser, int count, void* array, const char* name) = nullptr;

// Math::Vector3::Serialize(this, serializer) via IAT at 0x140152a68
static void (*s_fn_Vector3_Serialize)(void* vec3, void* serializer) = nullptr;

// FUN_140022970 -- ExternalPtr serialize (path description)
static void (*s_fn_ExternalPtr_Serialize)(void* extPtr, void* serializer) = nullptr;

// fn_IsDerivedFrom via IAT at 0x140152a98
static bool (*s_fn_IsDerivedFrom)(void* obj, uint32_t typeId) = nullptr;

// Math::Vector2::Dot(this, out) via IAT at 0x140152458
static void (*s_fn_Vector2_Dot)(void* vec2, void* outScalar) = nullptr;

// Math::Vector2::Normalize(this) via IAT at 0x1401526b0
// Value-return member ABI (verified from exe callers 0x1400cc70c): RCX=this, RDX=retbuf,
// returns RAX=retbuf. NOT in-place — must pass a separate retbuf and read from it.
static void* (*s_fn_Vector2_Normalize)(void* thisVec, void* retbuf) = nullptr;

// Math::Vector3::Vector3(this, x, y, z) via IAT at 0x140152a30
static float* (*s_fn_Vector3_Ctor_Floats)(float* self, float x, float y, float z) = nullptr;

// Math::Vector3::Vector3(this, Scalar*, Scalar*, Scalar*) via IAT at 0x140151268
static void* (*s_fn_Vector3_Ctor_Scalars)(void* self, void* sx, void* sy, void* sz) = nullptr;

// Math::Vector3::Normalize(this) via IAT at 0x1401512c0
// Vector3::Normalize() const returns Vector3 BY VALUE (12 bytes > 8): member value-return
// ABI RCX=this, RDX=hidden retptr. Caller passes a >=12-byte result buffer.
static void* (*s_fn_Vector3_Normalize)(void* vec3, void* retbuf) = nullptr;

// Math::Vector3::Heading(this) via IAT at 0x1401512d8
static float (*s_fn_Vector3_Heading)(void* vec3) = nullptr;

// Math::Vector3::Dot(this, other) via IAT at 0x140151298
static float* (*s_fn_Vector3_Dot)(void* a, void* b) = nullptr;

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

// TypeId at DAT_1401c7078 -- for entity type validation on +0x28
static uint32_t* s_typeId_1401c7078 = nullptr;

// TypeId at DAT_1401c4790 -- for version-0 compatibility serialize
static uint32_t* s_typeId_1401c4790 = nullptr;

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

    // CheckPtr via IAT at [0x140152ab0]
    uintptr_t iat;
    iat = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iat) s_fn_CheckPtr = *reinterpret_cast<void(**)(void*)>(iat);

    // IBP::Serialize via IAT at 0x140152aa0
    iat = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iat) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iat);

    // Serializer ctor/dtor via IAT
    iat = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iat) s_fn_Serializer_Ctor = *reinterpret_cast<void(**)(void*, void*, int)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iat) s_fn_Serializer_Dtor = *reinterpret_cast<void(**)(void*)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152a48);
    if (iat) s_fn_Serializer_GetVersion = *reinterpret_cast<int(**)(void*)>(iat);

    // Serialize overloads via IAT
    iat = ResolveAddr(Module::Exe, 0x140152a58);
    if (iat) s_fn_Serialize_Bool = *reinterpret_cast<void(**)(void*, void*, const char*)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iat) s_fn_Serialize_Int = *reinterpret_cast<void(**)(void*, void*, const char*)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152a50);
    if (iat) s_fn_Serialize_UInt = *reinterpret_cast<void(**)(void*, void*, const char*)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152a60);
    if (iat) s_fn_Serialize_Float = *reinterpret_cast<void(**)(void*, void*, const char*)>(iat);

    // Array serialize via IAT at 0x140151278
    iat = ResolveAddr(Module::Exe, 0x140151278);
    if (iat) s_fn_Serialize_IntArray = *reinterpret_cast<void(**)(void*, int, void*, const char*)>(iat);

    // Vector3::Serialize via IAT at 0x140152a68
    iat = ResolveAddr(Module::Exe, 0x140152a68);
    if (iat) s_fn_Vector3_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iat);

    // ExternalPtr serialize (FUN_140022970)
    s_fn_ExternalPtr_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x140022970));

    // IsDerivedFrom via IAT at 0x140152a98
    iat = ResolveAddr(Module::Exe, 0x140152a98);
    if (iat) s_fn_IsDerivedFrom = *reinterpret_cast<bool(**)(void*, uint32_t)>(iat);

    // Vector2::Dot via IAT at 0x140152458
    iat = ResolveAddr(Module::Exe, 0x140152458);
    if (iat) s_fn_Vector2_Dot = *reinterpret_cast<void(**)(void*, void*)>(iat);

    // Vector2::Normalize via IAT at 0x1401526b0
    iat = ResolveAddr(Module::Exe, 0x1401526b0);
    if (iat) s_fn_Vector2_Normalize = *reinterpret_cast<void*(**)(void*, void*)>(iat);

    // Vector3 constructors/methods via IAT
    iat = ResolveAddr(Module::Exe, 0x140152a30);
    if (iat) s_fn_Vector3_Ctor_Floats = *reinterpret_cast<float*(**)(float*, float, float, float)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140151268);
    if (iat) s_fn_Vector3_Ctor_Scalars = *reinterpret_cast<void*(**)(void*, void*, void*, void*)>(iat);
    iat = ResolveAddr(Module::Exe, 0x1401512c0);
    if (iat) s_fn_Vector3_Normalize = *reinterpret_cast<void*(**)(void*, void*)>(iat);
    iat = ResolveAddr(Module::Exe, 0x1401512d8);
    if (iat) s_fn_Vector3_Heading = *reinterpret_cast<float(**)(void*)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140151298);
    if (iat) s_fn_Vector3_Dot = *reinterpret_cast<float*(**)(void*, void*)>(iat);

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

    // Type IDs
    s_typeId_1401c7078 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7078));
    s_typeId_1401c4790 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4790));

    // Constants
    s_fMinDistSq = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401720d4));
    s_fHalfCell = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    bool ok = s_fn_CheckPtr && s_fn_IBP_Serialize &&
              s_fn_Serializer_Ctor && s_fn_Serializer_Dtor && s_fn_Serializer_GetVersion &&
              s_fn_Serialize_Bool && s_fn_Serialize_Int && s_fn_Serialize_UInt &&
              s_fn_Serialize_Float && s_fn_Serialize_IntArray &&
              s_fn_Vector3_Serialize && s_fn_ExternalPtr_Serialize &&
              s_fn_IsDerivedFrom &&
              s_fn_Vector2_Dot && s_fn_Vector2_Normalize &&
              s_fn_Vector3_Ctor_Floats && s_fn_Vector3_Ctor_Scalars &&
              s_fn_Vector3_Normalize && s_fn_Vector3_Heading && s_fn_Vector3_Dot &&
              s_fn_Sqrtf && s_fn_SetAnimation &&
              s_fn_BridgeQuery && s_fn_PathSetup && s_fn_UpdateFacing &&
              s_fn_SetTerrainCosts && s_fn_PathfinderQuery && s_fn_CopyPathFromFinder &&
              s_typeId_1401c7078 && s_typeId_1401c4790 &&
              s_fMinDistSq && s_fHalfCell &&
              g_pGameWorldSingleton && g_fUVScale;

#ifdef _DEBUG
    printf("[MoveTo] ResolveFuncs: CheckPtr=%p IBPSer=%p SerCtor=%p SerDtor=%p\n",
           (void*)s_fn_CheckPtr, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_Serializer_Ctor, (void*)s_fn_Serializer_Dtor);
    printf("[MoveTo]   GetVer=%p Bool=%p Int=%p UInt=%p Float=%p IntArr=%p\n",
           (void*)s_fn_Serializer_GetVersion, (void*)s_fn_Serialize_Bool,
           (void*)s_fn_Serialize_Int, (void*)s_fn_Serialize_UInt,
           (void*)s_fn_Serialize_Float, (void*)s_fn_Serialize_IntArray);
    printf("[MoveTo]   Vec3Ser=%p ExtPtrSer=%p IsDerived=%p\n",
           (void*)s_fn_Vector3_Serialize, (void*)s_fn_ExternalPtr_Serialize,
           (void*)s_fn_IsDerivedFrom);
    printf("[MoveTo]   V2Dot=%p V2Norm=%p V3CtorF=%p V3CtorS=%p V3Norm=%p V3Head=%p V3Dot=%p\n",
           (void*)s_fn_Vector2_Dot, (void*)s_fn_Vector2_Normalize,
           (void*)s_fn_Vector3_Ctor_Floats, (void*)s_fn_Vector3_Ctor_Scalars,
           (void*)s_fn_Vector3_Normalize, (void*)s_fn_Vector3_Heading,
           (void*)s_fn_Vector3_Dot);
    printf("[MoveTo]   Sqrt=%p SetAnim=%p BridgeQ=%p PathSetup=%p UpdFace=%p\n",
           (void*)s_fn_Sqrtf, (void*)s_fn_SetAnimation,
           (void*)s_fn_BridgeQuery, (void*)s_fn_PathSetup, (void*)s_fn_UpdateFacing);
    printf("[MoveTo]   SetCost=%p PFQuery=%p CopyPath=%p\n",
           (void*)s_fn_SetTerrainCosts, (void*)s_fn_PathfinderQuery,
           (void*)s_fn_CopyPathFromFinder);
    printf("[MoveTo]   typeId7078=%p typeId4790=%p minDist=%p halfCell=%p\n",
           (void*)s_typeId_1401c7078, (void*)s_typeId_1401c4790,
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
void MoveTo_Serialize_Impl(void* self, void* stream) {
    MoveTo_ResolveFuncs();
    uint8_t* s = (uint8_t*)self;
    uint8_t ser0[24];
    uint8_t ser1[24];

    // Version 0: serialize status byte at +0x18
    s_fn_Serializer_Ctor(ser0, stream, 0);
    s_fn_Serialize_Bool(ser0, s + 0x18, nullptr);
    s_fn_Serializer_Dtor(ser0);

    // Version 1: serialize all movement data
    s_fn_Serializer_Ctor(ser1, stream, 1);

    // Target entity IBP (+0x28)
    s_fn_IBP_Serialize(s + 0x28, ser1);
    if (*(void**)(s + 0x28) != nullptr) {
        bool valid = s_fn_IsDerivedFrom(*(void**)(s + 0x28), *s_typeId_1401c7078);
        if (!valid) {
            *(void**)(s + 0x28) = nullptr;
        }
    }

    // Path description ExternalPtr (+0x30)
    s_fn_ExternalPtr_Serialize(s + 0x30, ser1);

    // Path node count (+0x64) and current index (+0x60) as ints
    s_fn_Serialize_Int(ser1, s + 0x64, nullptr);
    s_fn_Serialize_Int(ser1, s + 0x60, nullptr);

    // Path node array (+0x68), count from +0x64
    s_fn_Serialize_IntArray(ser1, *(int32_t*)(s + 0x64), s + 0x68, nullptr);

    // Destination position (+0x38) as Vector3
    s_fn_Vector3_Serialize(s + 0x38, ser1);

    // Terrain flags (+0x5c) as uint
    s_fn_Serialize_UInt(ser1, s + 0x5c, nullptr);

    // Idle animation ID (+0x468) as int
    s_fn_Serialize_Int(ser1, s + 0x468, nullptr);

    // Walk animation ID (+0x46c) as int
    s_fn_Serialize_Int(ser1, s + 0x46c, nullptr);

    // Version check for bridge flag
    int version = s_fn_Serializer_GetVersion(ser1);
    if (version == 0) {
        // Version 0 compatibility: deserialize and discard a legacy IBP field
        void* tempIBP = nullptr;
        s_fn_IBP_Serialize(&tempIBP, ser1);
        if (tempIBP != nullptr) {
            s_fn_IsDerivedFrom(tempIBP, *s_typeId_1401c4790);
        }
    } else {
        // Version > 0: serialize bridge flag (+0x470)
        version = s_fn_Serializer_GetVersion(ser1);
        if (version > 0) {
            s_fn_Serialize_Int(ser1, s + 0x470, nullptr);
        }
    }

    // Road flag (+0x474) as bool
    s_fn_Serialize_Bool(ser1, s + 0x474, nullptr);

    // Speed multiplier (+0x478) as float
    s_fn_Serialize_Float(ser1, s + 0x478, nullptr);

    s_fn_Serializer_Dtor(ser1);
}

// ============================================================
// MoveTo::AbstractImpl (0x1400c8ab0)
// Checks distance to destination. If close enough (< 0.05), returns 0 (arrived).
// Otherwise sets up A* path and returns 1 (moving) or 2 (no path).
// ============================================================
int MoveTo_AbstractImpl_Impl(void* self, void* actionStack) {
    MoveTo_ResolveFuncs();   // lazy-resolve: action may be built by Pf_MoveToCtor_9170 (pf_ctors), which never calls our ctor's ResolveFuncs
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;

    // Get target entity's character component position
    s_fn_CheckPtr(s + 0x28);
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);
    s_fn_CheckPtr((void*)(targetEntity + 0x68));
    uintptr_t charComp = *(uintptr_t*)(targetEntity + 0x68);

    // Current character position
    float curX = *(float*)(charComp + 0x158);
    float curY = *(float*)(charComp + 0x15c);
    float curZ = *(float*)(charComp + 0x160);

    // Destination position from MoveTo fields
    float destX = *(float*)(s + 0x38);
    float destY = *(float*)(s + 0x3c);

    // Compute 2D distance squared: (curX - destX)^2 + (curY - destY)^2
    float dx = curX - destX;
    float dy = curY - destY;
    float vec2[2] = { dx, dy };
    float dotResult;
    s_fn_Vector2_Dot(vec2, &dotResult);
    float dist = s_fn_Sqrtf(dotResult);

    // If already close enough, return 0 (arrived)
    if (dist < *s_fMinDistSq) {
        return 0;
    }

    // Get current position again (re-read since CheckPtr may update)
    s_fn_CheckPtr(s + 0x28);
    targetEntity = *(uintptr_t*)(s + 0x28);
    s_fn_CheckPtr((void*)(targetEntity + 0x68));
    charComp = *(uintptr_t*)(targetEntity + 0x68);

    float worldPos[3];
    worldPos[0] = *(float*)(charComp + 0x158);
    worldPos[1] = *(float*)(charComp + 0x15c);
    worldPos[2] = *(float*)(charComp + 0x160);

    // Set up A* path from current position to destination
    // param3 reuses the vec2 pointer, param4 comes from actionStack (R9 in original)
    s_fn_PathSetup(self, worldPos, vec2, actionStack);

    // Check path setup result
    int32_t pathVersion = *(int32_t*)(s + 0x24);
    int32_t pathSlotId = *(int32_t*)(s + 0x20);
    int32_t pathIndex = *(int32_t*)(s + 0x60);

    // Path-query diagnostic (BANISHED_PF_TRACE, default off): after PathSetup, log
    // whether the A* produced a path slot. return-2 here = "no path possible" — the
    // pathfinder found no route from cur->dest (the star/terrain-ignore symptom).
    {
        static int s_pf = -1, s_pfN = 0;
        if (s_pf < 0) { const char* e = std::getenv("BANISHED_PF_TRACE"); s_pf = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (s_pf == 1 && s_pfN < 60) { ++s_pfN;
            fprintf(stderr,
                "[PFQuery] self=%p cur=(%.2f,%.2f) dest=(%.2f,%.2f) dist=%.2f -> ver=%d slot=%d idx=%d %s\n",
                self, curX, curY, destX, destY, dist, pathVersion, pathSlotId, pathIndex,
                ((pathVersion == 0 || pathSlotId < 0) && pathIndex < 0) ? "NO-PATH(ret2)"
                    : (pathIndex < 0 ? "WAIT(ret1)" : "READY"));
            fflush(stderr);
        }
    }

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
    s_fn_UpdateFacing(self);

    // Set walk animation on the target entity
    s_fn_CheckPtr(s + 0x28);
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
int MoveTo_Execute_Impl(void* self, void* actionStack, void* time) {
    MoveTo_ResolveFuncs();
    (void)time;
    uint8_t* s = (uint8_t*)self;

    int* mapGrid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);

    // Get character's current world position
    s_fn_CheckPtr(s + 0x28);
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);
    s_fn_CheckPtr((void*)(targetEntity + 0x68));
    uintptr_t charComp = *(uintptr_t*)(targetEntity + 0x68);

    float curX = *(float*)(charComp + 0x158);
    float curY = *(float*)(charComp + 0x15c);
    float curZ = *(float*)(charComp + 0x160);

    int32_t pathIndex = *(int32_t*)(s + 0x60);

    // ---- CASE 1: No active path segment (pathIndex == -1) ----
    if (pathIndex == -1) {
        int32_t pathVersion = *(int32_t*)(s + 0x24);

        if (pathVersion != 0) {
            // Have a cached path version -- try to retrieve from pathfinder
            int32_t slotId = *(int32_t*)(s + 0x20);
            if (slotId >= 0) {
                // Access pathfinder data structure
                uintptr_t pathfinder = *(uintptr_t*)((uintptr_t)mapGrid + 0x68);

                // Execute slot-resolution diagnostic (BANISHED_PF_TRACE).
                static int s_ex = -1, s_exN = 0;
                if (s_ex < 0) { const char* e = std::getenv("BANISHED_PF_TRACE"); s_ex = (e && e[0] && e[0] != '0') ? 1 : 0; }
                #define PF_EXLOG(msg) do { if (s_ex == 1 && s_exN < 80) { ++s_exN; \
                    fprintf(stderr, "[PFExec] self=%p slot=%d ver=%d slotCnt=%d %s\n", self, slotId, pathVersion, \
                            *(int32_t*)(pathfinder + 0x168), msg); fflush(stderr); } } while(0)

                // Check if slot is still valid
                if (slotId >= *(int32_t*)(pathfinder + 0x168)) {
                    PF_EXLOG("slot-out-of-range->WAIT");
                    return 1;  // Slot out of range, wait
                }

                // Verify path version matches
                uintptr_t slotBase = *(uintptr_t*)(pathfinder + 0x160);
                int32_t storedVersion = *(int32_t*)(slotBase + (int64_t)slotId * 0x18 + 4);
                if (storedVersion != pathVersion) {
                    PF_EXLOG("version-mismatch->WAIT");
                    return 1;  // Version mismatch, wait for re-query
                }

                // Check if path is ready (slot active flag)
                if (*(char*)(slotBase + (int64_t)slotId * 0x18) == '\0') {
                    PF_EXLOG("slot-inactive->WAIT");
                    return 1;  // Path not yet computed
                }

                // Copy path nodes from pathfinder into our local array
                int nodeCount = s_fn_CopyPathFromFinder(
                    (void*)pathfinder, s + 0x20, s + 0x68, 0x100);
                *(int32_t*)(s + 0x64) = nodeCount;

                PF_EXLOG(nodeCount == 0 ? "READY-but-EMPTY(0 nodes)->ret2" : "READY-with-PATH");
                if (nodeCount == 0) {
                    return 2;  // Path found but empty -- no valid route
                }

                // Start traversal at index 1 (skip start cell)
                *(int32_t*)(s + 0x60) = 1;

                // Update facing direction
                s_fn_UpdateFacing(self);

                // Set walk animation
                s_fn_CheckPtr(s + 0x28);
                targetEntity = *(uintptr_t*)(s + 0x28);
                uint32_t walkAnimId = *(uint32_t*)(s + 0x468);
                s_fn_SetAnimation((void*)targetEntity, walkAnimId, *g_fUVScale, *g_fUVScale);
                return 1;
            }
        }

        // No cached path or invalid slot -- call AbstractImpl (vtable slot +0x20)
        uintptr_t vtable = *(uintptr_t*)s;
        typedef int (*AbstractImpl_t)(void* self, void* actionStack);
        AbstractImpl_t abstractImpl = *(AbstractImpl_t*)(vtable + 0x20);
        int result = abstractImpl(self, actionStack);
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
        float diffVec[3];
        s_fn_Vector3_Ctor_Floats(diffVec, dx2, dy2, dz);

        float dotVec[2] = { diffVec[0], diffVec[1] };
        float dotOut;
        s_fn_Vector2_Dot(dotVec, &dotOut);
        float distToGoal = s_fn_Sqrtf(dotOut);

        if (distToGoal < *s_fMinDistSq) {
            // Close enough -- arrived
            return 0;
        }

        // Not close enough yet -- compute direction and set facing
        float revDx = destX - curX;
        float revDy = destY - curY;
        float revDz = *(float*)(s + 0x40) - curZ;
        float revVec[3];
        s_fn_Vector3_Ctor_Floats(revVec, revDx, revDy, revDz);

        // Normalize 2D direction (value-return ABI: result written to retbuf, not in place)
        float dir2d[2] = { revVec[0], revVec[1] };
        float dir2dN[4] = { dir2d[0], dir2d[1], 0.0f, 0.0f };
        if (s_fn_Vector2_Normalize) s_fn_Vector2_Normalize(dir2d, dir2dN);

        // Build 3D facing vector and set on character
        float facing3d[3] = { dir2dN[0], dir2dN[1], 0.0f };
        s_fn_CheckPtr(s + 0x28);
        targetEntity = *(uintptr_t*)(s + 0x28);
        *(uint8_t*)(targetEntity + 0x28) = 0;  // Clear animation lock flag

        // Build normalized facing vector from 2D components
        float scalarX = facing3d[0];
        float scalarY = facing3d[1];
        float scalarZ = 0.0f;
        float tempVec[3];
        void* normResult = s_fn_Vector3_Ctor_Scalars(tempVec, &scalarX, &scalarY, &scalarZ);
        float normalized[4] = {0,0,0,0};
        s_fn_Vector3_Normalize(normResult, normalized);

        // Write facing direction to character (+0x2c, +0x30, +0x34)
        *(uint32_t*)(targetEntity + 0x2c) = *(uint32_t*)&normalized[0];
        *(uint32_t*)(targetEntity + 0x30) = *(uint32_t*)&normalized[1];
        *(uint32_t*)(targetEntity + 0x34) = *(uint32_t*)&normalized[2];

        // Compute heading angle from facing direction
        float heading = s_fn_Vector3_Heading(facing3d);
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
        s_fn_CheckPtr(s + 0x28);
        targetEntity = *(uintptr_t*)(s + 0x28);
        uint32_t idleAnimId = *(uint32_t*)(s + 0x46c);
        s_fn_SetAnimation((void*)targetEntity, idleAnimId, *g_fUVScale, *g_fUVScale);
        *(int32_t*)(s + 0x60) = -1;
        return 1;
    }

    // Look up terrain type at current target cell for speed calculation
    // The current target cell is pathNodes[curCell] -- but we use destCell (the cell
    // we're heading toward) which is pathNodes[pathIndex-1] or similar
    uintptr_t cellArrayBase = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
    uint8_t terrainType = *(uint8_t*)(cellArrayBase + (int64_t)destCell * 0x30 + 0x29);

    float speed;
    bool onRoad = *(uint8_t*)(s + 0x474) != 0;

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
    s_fn_CheckPtr(s + 0x28);
    targetEntity = *(uintptr_t*)(s + 0x28);
    *(float*)(targetEntity + 0x38) = speed * *(float*)(s + 0x478);

    // Check if we've reached the current waypoint cell
    int32_t waypointCell = *(int32_t*)(s + 0x68 + (int64_t)pathIndex * 4);
    if (destCell != waypointCell) {
        // Haven't reached the current waypoint yet, keep moving
        return 1;
    }

    // Reached the waypoint -- advance to next
    *(int32_t*)(s + 0x60) = pathIndex + 1;

    // Update facing direction for next segment
    s_fn_UpdateFacing(self);

    return 1;
}

// ============================================================
// MoveTo::Enter (0x1400c8970)
// Bridge detection: queries nearby entities for bridges. If a bridge
// is found facing the same direction, sets the road flag (+0x474).
// ============================================================
void MoveTo_Enter_Impl(void* self) {
    MoveTo_ResolveFuncs();
    uint8_t* s = (uint8_t*)self;

    // Get target entity's character component position
    s_fn_CheckPtr(s + 0x28);
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);
    s_fn_CheckPtr((void*)(targetEntity + 0x68));
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
        s_fn_CheckPtr(s + 0x28);
        if (*(uintptr_t*)(s + 0x28) < bridgeResult) {
            // Compare facing directions via dot product
            s_fn_CheckPtr(s + 0x28);
            targetEntity = *(uintptr_t*)(s + 0x28);

            // Our entity's facing direction (+0x2c..+0x34)
            float myFacing[3];
            myFacing[0] = *(float*)(targetEntity + 0x2c);
            myFacing[1] = *(float*)(targetEntity + 0x30);
            myFacing[2] = *(float*)(targetEntity + 0x34);

            // Bridge entity's facing direction
            s_fn_CheckPtr(&bridgeResult);
            float bridgeFacing[3];
            bridgeFacing[0] = *(float*)(bridgeResult + 0x2c);
            bridgeFacing[1] = *(float*)(bridgeResult + 0x30);
            bridgeFacing[2] = *(float*)(bridgeResult + 0x34);

            float* dotResult = s_fn_Vector3_Dot(bridgeFacing, myFacing);
            if (*dotResult > 0.0f) {
                // Facing same direction as bridge -- set road flag
                *(uint8_t*)(s + 0x474) = 1;
                return;
            }
        }
    }

    // No bridge or wrong direction -- clear road flag
    *(uint8_t*)(s + 0x474) = 0;
}

// ============================================================
// MoveTo::Exit (0x1400c8520)
// Reset character animation to idle (walk anim at +0x46c).
// ============================================================
void MoveTo_Exit_Impl(void* self, void* actionStack) {
    MoveTo_ResolveFuncs();
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;

    s_fn_CheckPtr(s + 0x28);
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);
    uint32_t idleAnimId = *(uint32_t*)(s + 0x46c);
    s_fn_SetAnimation((void*)targetEntity, idleAnimId, *g_fUVScale, *g_fUVScale);
}

// ============================================================
// MoveTo::CheckEntity (0x1400c84d0)
// Same body as Exit -- reset character animation to idle.
// ============================================================
void MoveTo_CheckEntity_Impl(void* self) {
    MoveTo_ResolveFuncs();
    uint8_t* s = (uint8_t*)self;

    s_fn_CheckPtr(s + 0x28);
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);
    uint32_t idleAnimId = *(uint32_t*)(s + 0x46c);
    s_fn_SetAnimation((void*)targetEntity, idleAnimId, *g_fUVScale, *g_fUVScale);
}

// ============================================================
// MoveTo::GetTarget (0x1400c8480)
// Returns true if the given param (InternalBasePtr) matches
// the entity at target->+0x10.
// ============================================================
bool MoveTo_GetTarget_Impl(void* self, void* param) {
    MoveTo_ResolveFuncs();
    uint8_t* s = (uint8_t*)self;

    // CheckPtr on target entity
    s_fn_CheckPtr(s + 0x28);
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x28);

    // Get entity reference at target->+0x10
    uintptr_t entityRef = *(uintptr_t*)(targetEntity + 0x10);

    // CheckPtr on param
    s_fn_CheckPtr(param);

    return entityRef == *(uintptr_t*)param;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015fe40 (10 slots), size 0x480, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#ifdef STANDALONE_EXE
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t MoveTo_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("MoveTo");
    return c;
}

static void* g_MoveTo_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&MoveTo_GetTypeId,           // +0x10 GetTypeId
    (void*)&MoveTo_Serialize_Impl,      // +0x18 Serialize
    (void*)&MoveTo_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&MoveTo_Execute_Impl,   // +0x28 Execute
    (void*)&MoveTo_Enter_Impl,  // +0x30 Enter
    (void*)&MoveTo_Exit_Impl,   // +0x38 Exit
    (void*)&MoveTo_CheckEntity_Impl,  // +0x40 CheckEntity
    (void*)&MoveTo_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterMoveToVtable() {
    RegisterExeAddr(0x14015fe40, (uintptr_t)g_MoveTo_Vtable);
}
extern "C" void* MoveTo_CreateInPlace(void* mem) {
    MoveTo_ResolveFuncs();
    return Action_InitInPlace(mem, 0x480, g_MoveTo_Vtable);
}
extern "C" void* MoveTo_Factory(void) {
    MoveTo_ResolveFuncs();
    return Action_Alloc(0x480, g_MoveTo_Vtable);
}
REGISTER_TYPE(MoveTo, "Action", &MoveTo_Factory, &MoveTo_CreateInPlace);
#endif // STANDALONE_EXE
