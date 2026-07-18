// code/Game/Action/action_base_move.cpp
// Shared MoveTo / PerformWork / PlayAnimation base-class / ECS exe sub-helpers.
//
// These exe sub-helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from the
// per-class Action .cpp files (moveto.cpp, performwork.cpp, playanimation.cpp,
// huntanimal.cpp). They resolve to 0 (silently skipped) unless registered. This file provides
// faithful 1:1 reimplementations and a single registration point
// (RegisterActionMoveBatch in standalone/reg_action_move.cpp) so the shared call
// sites fire real logic in the standalone exe.
//
// Wave 3 (Action) — MoveTo / PerformWork / PlayAnimation sub-batch. Each function is
// a 1:1 port of the decompiled exe helper; do not "improve" behavior.
//

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// ============================================================
// Locally-resolved dispatch-slot / math helpers
// ============================================================
// The Math::Vector2 / Math::Vector3 helpers used below are read from the exe's
// .rdata dispatch slots (function-pointer tables), exactly like moveto.cpp does.
// They are NOT the slot address itself — the slot holds the runtime function
// pointer, so we deref one level after ResolveAddr.

// Math::Vector2::Normalize(this) — dispatch slot 0x1401526b0
// Value-return member ABI (RCX=this, RDX=retbuf, returns retbuf) — not in-place.
static void* (*s_fn_Vector2_Normalize)(void* thisVec, void* retbuf) = nullptr;
// Math::Vector3::Normalize(this) const -> Vector3 BY VALUE (12 bytes > 8) — member
// value-return ABI: RCX=this(input vec3), RDX=hidden retptr, returns retptr in RAX.
// dispatch slot 0x1401512c0. Caller MUST pass a >=12-byte result buffer.
static float* (*s_fn_Vector3_Normalize)(void* vec3, void* retbuf) = nullptr;
// Math::Vector3::Heading(this) — dispatch slot 0x1401512d8
static float (*s_fn_Vector3_Heading)(void* vec3) = nullptr;

// ============================================================
// g_pGameWorldSingleton accessor (DAT_1401c3dd8 — declared in engine_funcs.h)
// The original reads `g_pGameWorldSingleton` directly as a byte* base (it is the
// GameWorld instance pointer, not a pointer-to-pointer). In our headers it is typed
// `void** g_pGameWorldSingleton` and dereferenced once to obtain the instance.
// ============================================================
static inline uintptr_t GameWorld() {
    return reinterpret_cast<uintptr_t>(g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
}

static bool s_mvFuncsResolved = false;
static void EnsureMoveFuncs() {
    if (s_mvFuncsResolved) return;
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x1401526b0);
    if (slot) s_fn_Vector2_Normalize = *reinterpret_cast<void*(**)(void*, void*)>(slot);
    slot = ResolveAddr(Module::Exe, 0x1401512c0);
    if (slot) s_fn_Vector3_Normalize = *reinterpret_cast<float*(**)(void*, void*)>(slot);
    slot = ResolveAddr(Module::Exe, 0x1401512d8);
    if (slot) s_fn_Vector3_Heading = *reinterpret_cast<float(**)(void*)>(slot);
    s_mvFuncsResolved = true;
}

// ============================================================
// FUN_1400012d0 — Entity_FindComponent_Family
// ============================================================
// Member-style FindComponent over the entity's component list. RCX=entity,
// RDX=retIBP, returns retIBP. Scans the component ptr-array at entity+0x30 (count
// at +0x38), calls each component's GetTypeId (vtable +0x10), and on the first type
// match (DAT_1401c7078) runs IsDerivedFrom before storing through the out IBP.
//
// 1:1 port — type-ID global at DAT_1401c7078 resolved lazily.
static const uint32_t* s_pTid_1c7078 = nullptr;

void* ActMv_FindComponentFamily_12d0(uintptr_t entity, void* retIBP) {
    if (!s_pTid_1c7078)
        s_pTid_1c7078 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7078));

    void* found = nullptr;
    uint32_t wantId = s_pTid_1c7078 ? *s_pTid_1c7078 : 0;
    // Standalone: the shared CharacterComponent type-id global (DAT_1401c7078, written by
    // CharacterComponent_RegisterType) is left at the 0xFFFFFFFF sentinel because RegisterType
    // runs before the real CRC ids are assigned. Filling the global would un-dormant ~30 other
    // readers, so resolve the family type-id self-contained via Object::GetTypeIdByName (the
    // same real-CRC source the live factories' vtable+0x10 GetTypeId uses) — matches the brick
    // pattern. Without this the family scan never matches → null entity IBP → MoveTo ctor
    // null-deref when a citizen state (Leisure/Wander/...) builds a MoveTo action.
    if (wantId == 0 || wantId == 0xFFFFFFFFu)
        wantId = ::Object::GetTypeIdByName("CharacterComponent");
    int n = *reinterpret_cast<int*>(entity + 0x38);
    if (n > 0 && wantId != 0 && wantId != 0xFFFFFFFFu) {
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n; i++) {
            void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == wantId) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                void* c = *slot;
                found = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, wantId)) ? c : nullptr;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    return retIBP;
}

// ============================================================
// FUN_140022970 — ExternalPtr::Serialize (path description External slot)
// ============================================================
// Serializes an Object::External* stored at *param_1 through the IO::Serializer
// param_2. Three encoding paths mirror the original exactly:
//   encoding type == 3  (text/ref read-only): re-resolve the External by its path.
//   reference-mode on   : serialize a uint reference id, re-look-up by id on load.
//   reference-mode off  : serialize a bool "present" flag, then the path String.
// On load (stream dirty flag *(stream+8)==0) the External is re-resolved via
// ExternalFactory::FindOrLoad + IsDerivedFrom(DAT_1401c70b0) and written back to
// *param_1.   Caller signature: void fn(void* extPtrSlot, void* serializer)
//
// Dispatch-slot map (verified in disassembly):
//   0x1401529f8 Serializer::GetEncoding      -> fn_Serializer_GetEncoding
//   0x1401529f0 External::GetPath(retbuf)    -> fn_External_GetPath
//   0x1401529d8 ExternalFactory _singleton   -> g_pExternalFactorySingleton
//   0x140151230 ExternalFactory::FindOrLoad  -> fn_ExternalFactory_FindOrLoad
//   0x140152a98 IsDerivedFrom                -> fn_IsDerivedFrom
//   0x140151d40 Filename::Filename()         -> fn_Filename_Ctor
//   0x140151218 IsReferenceModeEnabled       -> fn_ExternalFactory_IsReferenceModeEnabled
//   0x140151220 GetReferenceId               -> fn_ExternalFactory_GetReferenceId
//   0x140151228 GetPathForReferenceId        -> fn_ExternalFactory_GetPathForReferenceId
//   0x1401529e0 Filename::operator=          -> fn_Filename_AssignOp
//   0x1401529e8 Filename::~Filename          -> fn_Filename_Dtor
//   0x140152a50 Serializer::Serialize(uint*) -> local resolve
//   0x140152a58 Serializer::Serialize(bool*) -> local resolve
//   0x140151238 String::Serialize            -> g_fn_String_Serialize
//   0x1401c70b0 type-ID                       -> local resolve (NOT in engine_funcs.h)
static void (*s_fn_SerUInt_a50)(void* ser, void* field, const char* name) = nullptr;
static void (*s_fn_SerBool_a58)(void* ser, void* field, const char* name) = nullptr;
static const uint32_t* s_pTid_1c70b0 = nullptr;
static bool s_extSerResolved = false;

static void EnsureExtSerFuncs() {
    if (s_extSerResolved) return;
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152a50);
    if (slot) s_fn_SerUInt_a50 = *reinterpret_cast<void(**)(void*, void*, const char*)>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_SerBool_a58 = *reinterpret_cast<void(**)(void*, void*, const char*)>(slot);
    s_pTid_1c70b0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70b0));
    s_extSerResolved = true;
}

void ActMv_ExternalPtrSerialize_22970(void* extPtrSlot, void* serializer) {
    EnsureExtSerFuncs();
    uintptr_t* pSlot = reinterpret_cast<uintptr_t*>(extPtrSlot);
    uint32_t typeId = s_pTid_1c70b0 ? *s_pTid_1c70b0 : 0;

    // GetEncoding -> encoding->vtable[+0x10](encoding) == 3 ?
    int encType = -1;
    if (fn_Serializer_GetEncoding) {
        void* enc = fn_Serializer_GetEncoding(serializer);
        if (enc)
            encType = (*reinterpret_cast<int(**)(void*)>(*reinterpret_cast<uintptr_t*>(enc) + 0x10))(enc);
    }

    if (encType == 3) {
        // Read-only text path: drop the External; re-resolve by its current path.
        if (*pSlot == 0) return;
        uintptr_t fnStorage = 0;  // Filename retbuf (local_res18 in the original)
        void* path = nullptr;
        if (fn_External_GetPath) path = fn_External_GetPath(reinterpret_cast<void*>(*pSlot), &fnStorage);
        if (g_pExternalFactorySingleton && *g_pExternalFactorySingleton && fn_ExternalFactory_FindOrLoad) {
            void* found = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, path);
            if (found && fn_IsDerivedFrom) fn_IsDerivedFrom(found, typeId);
        }
        if (fn_Filename_Dtor) fn_Filename_Dtor(&fnStorage);
        return;
    }

    // Filename local (default-constructed) used to carry the path through the stream.
    uintptr_t fnLocal = 0;
    if (fn_Filename_Ctor) fn_Filename_Ctor(&fnLocal);

    void* factory = (g_pExternalFactorySingleton && *g_pExternalFactorySingleton)
                        ? *g_pExternalFactorySingleton : nullptr;
    bool refMode = (factory && fn_ExternalFactory_IsReferenceModeEnabled)
                       ? fn_ExternalFactory_IsReferenceModeEnabled(factory) : false;

    uint32_t refId = 0xffffffff;
    bool didFind = false;  // whether we fall through to the FindOrLoad write-back block

    if (refMode) {
        if (*pSlot == 0) {
            refId = 0xffffffff;
        } else if (factory && fn_ExternalFactory_GetReferenceId) {
            refId = fn_ExternalFactory_GetReferenceId(factory, reinterpret_cast<void*>(*pSlot));
        }
        // Stream-dirty flag = *(serializer+0)->[+8]. (RAX=[RBX]; [RAX+8])
        int streamFlag = *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(serializer) + 8);
        if (*pSlot != 0 && streamFlag == 0 && refId == 0xffffffff) {
            // Special case: present but no ref id and writing -> just serialize the (-1) id.
            if (s_fn_SerUInt_a50) s_fn_SerUInt_a50(serializer, &refId, nullptr);
            // (no write-back)
        } else {
            if (s_fn_SerUInt_a50) s_fn_SerUInt_a50(serializer, &refId, nullptr);
            if ((int)refId != -1) {
                streamFlag = *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(serializer) + 8);
                if (streamFlag == 0 && factory && fn_ExternalFactory_GetPathForReferenceId) {
                    uintptr_t pathBuf = 0;
                    fn_ExternalFactory_GetPathForReferenceId(factory, &pathBuf, refId);
                    if (fn_Filename_AssignOp) fn_Filename_AssignOp(&fnLocal, &pathBuf);
                    if (fn_Filename_Dtor) fn_Filename_Dtor(&pathBuf);
                }
                didFind = true;
            }
        }
    } else {
        // Reference mode off: bool "present" flag then the path String.
        uint8_t present = (*pSlot != 0) ? 1 : 0;
        if (s_fn_SerBool_a58) s_fn_SerBool_a58(serializer, &present, nullptr);
        if (present != 0) {
            if (*pSlot != 0 && fn_External_GetPath) {
                uintptr_t pathBuf = 0;
                fn_External_GetPath(reinterpret_cast<void*>(*pSlot), &pathBuf);
                if (fn_Filename_AssignOp) fn_Filename_AssignOp(&fnLocal, &pathBuf);
                if (fn_Filename_Dtor) fn_Filename_Dtor(&pathBuf);
            }
            if (g_fn_String_Serialize) g_fn_String_Serialize(&fnLocal, serializer);
            didFind = true;
        }
    }

    if (didFind) {
        int streamFlag = *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(serializer) + 8);
        if (streamFlag == 0) {
            void* found = nullptr;
            if (g_pExternalFactorySingleton && *g_pExternalFactorySingleton && fn_ExternalFactory_FindOrLoad)
                found = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, &fnLocal);
            uintptr_t result = 0;
            if (found && fn_IsDerivedFrom && fn_IsDerivedFrom(found, typeId))
                result = reinterpret_cast<uintptr_t>(found);
            *pSlot = result;
        }
    }

    if (fn_Filename_Dtor) fn_Filename_Dtor(&fnLocal);
}

// ============================================================
// FUN_14010a060 — PerformWork_helper_a060 (SetFacingDirection)
// ============================================================
// Sets a component's facing from a Vector3 direction. Clears a byte at +0x28,
// normalizes (x,y,0) into +0x2c..+0x34, stores Heading(dir) at +0x3c.
// Caller signature: void fn(void* comp, void* vector3)
void ActMv_SetFacingDirection_a060(void* comp, void* vector3) {
    EnsureMoveFuncs();
    uintptr_t self = reinterpret_cast<uintptr_t>(comp);
    float* v = reinterpret_cast<float*>(vector3);

    *reinterpret_cast<uint8_t*>(self + 0x28) = 0;
    float tmp[3];
    tmp[0] = v[0];
    tmp[1] = v[1];
    tmp[2] = 0.0f;
    if (s_fn_Vector3_Normalize) {
        float n[4] = {0,0,0,0};
        s_fn_Vector3_Normalize(tmp, n);
        *reinterpret_cast<float*>(self + 0x2c) = n[0];
        *reinterpret_cast<float*>(self + 0x30) = n[1];
        *reinterpret_cast<float*>(self + 0x34) = n[2];
    }
    if (s_fn_Vector3_Heading)
        *reinterpret_cast<float*>(self + 0x3c) = s_fn_Vector3_Heading(vector3);
}

// ============================================================
// FUN_1400c8d20 — MoveTo_sub_8d20 (UpdateFacing toward next waypoint)
// ============================================================
// Computes the facing direction from the citizen's current world position toward
// the next path cell, normalizes it into the movement component (+0x2c..+0x34) and
// stores the Heading at +0x3c. Mirrors PerformWork_helper_a060 but derives the
// direction from grid geometry.
//   g_pGameWorldSingleton+0xf8 -> map grid (int*). grid[0] = width.
//   self+0x28 -> citizen IBP; that+0x68 -> movement sub-object; +0x158/+0x15c =
//     current world x/y; +0x160 carried through.
//   self+0x60 = current path index; self+0x68[idx] = target cell.
//   CharacterComponent_impl_f3e0 (0x14009f3e0) = grid->world conversion.
// Caller signature: void fn(void* self)
typedef float* (*fn_GridToWorld_f3e0_t)(int* grid, float* outWorld, float* inGrid, char flag, void* extra);
static fn_GridToWorld_f3e0_t s_fn_GridToWorld_f3e0 = nullptr;
static float* s_pHalfCell_172114 = nullptr;  // DAT_140172114 = 0.5f
static bool s_facingResolved = false;
static void EnsureFacingFuncs() {
    if (s_facingResolved) return;
    s_fn_GridToWorld_f3e0 = reinterpret_cast<fn_GridToWorld_f3e0_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_pHalfCell_172114 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_facingResolved = true;
}

void ActMv_UpdateFacing_c8d20(void* selfp) {
    EnsureMoveFuncs();
    EnsureFacingFuncs();
    uintptr_t self = reinterpret_cast<uintptr_t>(selfp);
    int* grid = reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(GameWorld() + 0xf8));
    float halfCell = s_pHalfCell_172114 ? *s_pHalfCell_172114 : 0.5f;

    // Resolve the citizen movement sub-object: self+0x28 -> +0x68.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x28));
    uintptr_t citizen = *reinterpret_cast<uintptr_t*>(self + 0x28);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(citizen + 0x68));
    uintptr_t move = *reinterpret_cast<uintptr_t*>(citizen + 0x68);

    float curX = *reinterpret_cast<float*>(move + 0x158);
    float curY = *reinterpret_cast<float*>(move + 0x15c);

    // Target cell index from the path array (self+0x68 + curIdx*4).
    int curIdx = *reinterpret_cast<int*>(self + 0x60);
    int targetCell = *reinterpret_cast<int*>(self + 0x68 + static_cast<int64_t>(curIdx) * 4);

    int gridW = *grid;
    float center = static_cast<float>(gridW >> 1) - halfCell;
    float inGrid[3];
    inGrid[0] = static_cast<float>(targetCell % gridW) - center;  // local_58
    inGrid[1] = static_cast<float>(targetCell / gridW) - center;  // local_54
    inGrid[2] = 0.0f;

    float outWorld[3] = {0.0f, 0.0f, 0.0f};
    if (s_fn_GridToWorld_f3e0)
        s_fn_GridToWorld_f3e0(grid, outWorld, inGrid, 0, nullptr);

    // dir2 = (worldX - curX, worldY - curY); Vector2::Normalize (value-return -> retbuf).
    float dir2in[2];
    dir2in[0] = outWorld[0] - curX;
    dir2in[1] = outWorld[1] - curY;
    float dir2[4] = { dir2in[0], dir2in[1], 0.0f, 0.0f };
    if (s_fn_Vector2_Normalize) s_fn_Vector2_Normalize(dir2in, dir2);

    // Re-check the IBP, store normalized 3D facing into the movement sub-object.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x28));
    uintptr_t citizen2 = *reinterpret_cast<uintptr_t*>(self + 0x28);
    *reinterpret_cast<uint8_t*>(citizen2 + 0x28) = 0;

    float dir3[3];
    dir3[0] = dir2[0];   // local_58 = local_68 (= local_res10 from Vector2::Normalize result lo)
    dir3[1] = dir2[1];   // local_54 = local_64
    dir3[2] = 0.0f;      // local_50 = 0
    if (s_fn_Vector3_Normalize) {
        float n[4] = {0,0,0,0};
        s_fn_Vector3_Normalize(dir3, n);
        *reinterpret_cast<float*>(citizen2 + 0x2c) = n[0];
        *reinterpret_cast<float*>(citizen2 + 0x30) = n[1];
        *reinterpret_cast<float*>(citizen2 + 0x34) = n[2];
    }
    if (s_fn_Vector3_Heading) {
        float head3[3] = {dir2[0], dir2[1], 0.0f};
        *reinterpret_cast<float*>(citizen2 + 0x3c) = s_fn_Vector3_Heading(head3);
    }
}

// ============================================================
// FUN_14010a1f0 — AnimalComponent_impl_a1f0 (SetAnimation)
// ============================================================
// Sets an animation on the family/model component and computes a speed scalar.
//   self+0x78 -> sub-object (lVar1) read for +0x2c.
//   self+0x68 -> animation sub-object IBP (checked).
//   AnimalComponent_helper_50f0 (0x1401250f0): (RCX=animObj, EDX=animId, XMM2=0.0f,
//     XMM3=*(lVar1+0x2c), stack5=p3 float).  Verified in disassembly: XMM6=p3, XMM7=p4.
//   self+0x38 = *(animObj+0x13c) * p4  (speed multiplier).
// Caller signature: void fn(void* self, uint32_t animId, float p3, float p4)
typedef void (*fn_AnimHelper_50f0_t)(uintptr_t animObj, uint32_t animId, float a, float b, float p3);
static fn_AnimHelper_50f0_t s_fn_AnimHelper_50f0 = nullptr;
static bool s_animResolved = false;

void ActMv_SetAnimation_a1f0(void* selfp, uint32_t animId, float p3, float p4) {
    if (!s_animResolved) {
        s_fn_AnimHelper_50f0 = reinterpret_cast<fn_AnimHelper_50f0_t>(ResolveAddr(Module::Exe, 0x1401250f0));
        s_animResolved = true;
    }
    uintptr_t self = reinterpret_cast<uintptr_t>(selfp);
    uintptr_t sub = *reinterpret_cast<uintptr_t*>(self + 0x78);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x68));
    uintptr_t animObj = *reinterpret_cast<uintptr_t*>(self + 0x68);
    if (s_fn_AnimHelper_50f0) {
        // 5th arg (p3) is a float passed on the stack in the original (MOVSS [RSP+0x20]).
        s_fn_AnimHelper_50f0(animObj, animId, 0.0f, *reinterpret_cast<float*>(sub + 0x2c), p3);
    }
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x68));
    uintptr_t animObj2 = *reinterpret_cast<uintptr_t*>(self + 0x68);
    *reinterpret_cast<float*>(self + 0x38) = *reinterpret_cast<float*>(animObj2 + 0x13c) * p4;
}

// ============================================================
// FUN_140116650 — PerformWork_helper_6650 (citizen break / task-due check)
// ============================================================
// Returns nonzero when the citizen needs a break / has a pending task.
//   gw+0xd0 bit 8: if set, returns 0 (paused/disabled state).
//   self+0x1f8 != 0: a task already pending -> returns 1.
//   self+0x30 (task id) >=0 : walk the AVL tree at gw+0x118->+0x30 keyed on int at
//     node+0 (left=node+6 ints i.e. +0x18, right=node+8 ints i.e. +0x20), value IBP at
//     node+2 ints i.e. +0x08. On the matching node, call value->vtable[+0x50](value,&out)
//     to fetch a record; read float at out+0x54; FUN_1400a9290(self+0x200, &thatFloat,
//     gw+0xd8 Random) decides. On success stash out at self+0x1f8 and return it (nonzero).
// Caller signature: uint8_t fn(void* entity)
typedef bool (*fn_TimeCheck_a9290_t)(void* time, float* val, void* rng);
static fn_TimeCheck_a9290_t s_fn_TimeCheck_a9290 = nullptr;
static bool s_breakResolved = false;

uint8_t ActMv_CitizenBreakCheck_116650(void* entityp) {
    if (!s_breakResolved) {
        s_fn_TimeCheck_a9290 = reinterpret_cast<fn_TimeCheck_a9290_t>(ResolveAddr(Module::Exe, 0x1400a9290));
        s_breakResolved = true;
    }
    uintptr_t self = reinterpret_cast<uintptr_t>(entityp);
    uintptr_t gw = GameWorld();

    uint32_t flags = *reinterpret_cast<uint32_t*>(gw + 0xd0);
    if (((flags >> 8) & 1) != 0)
        return 0;

    if (*reinterpret_cast<uintptr_t*>(self + 0x1f8) != 0)
        return 1;

    int taskId = *reinterpret_cast<int*>(self + 0x30);
    if (taskId < 0)
        return 0;

    // AVL tree search at gw+0x118 -> +0x30 (root node).
    int* node = *reinterpret_cast<int**>(*reinterpret_cast<uintptr_t*>(gw + 0x118) + 0x30);
    void* valueIBP = nullptr;       // local_res10 (longlong*)
    uintptr_t valueObj = 0;
    bool haveValue = false;
    while (node != nullptr) {
        if (taskId < *node) {
            node = *reinterpret_cast<int**>(node + 6);      // +0x18
        } else if (taskId > *node) {
            node = *reinterpret_cast<int**>(node + 8);      // +0x20
        } else {
            valueObj = *reinterpret_cast<uintptr_t*>(node + 2);  // +0x08
            valueIBP = reinterpret_cast<void*>(valueObj);
            haveValue = true;
            break;
        }
    }
    if (!haveValue) {
        // (*fn_InternalBasePtr_Ctor)(&local_res10, 0): value stays null.
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&valueIBP, nullptr);
        valueObj = reinterpret_cast<uintptr_t>(valueIBP);
    }

    if (valueObj == 0)
        return 0;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&valueIBP);
    uintptr_t v = reinterpret_cast<uintptr_t>(valueIBP);

    // value->vtable[+0x50](value, &out) -> record ptr in 'out'.
    uintptr_t outRec = 0;
    (*reinterpret_cast<void(**)(void*, void*)>(*reinterpret_cast<uintptr_t*>(v) + 0x50))(
        reinterpret_cast<void*>(v), &outRec);

    // Original copies 8 bytes (*(undefined8*)(rec+0x54)) — a9290 reads BOTH floats
    // (pair[0] = value, pair[1] = weight). Passing a single float would feed it stack
    // garbage for pair[1]. Copy the full pair.
    float duePair[2];
    duePair[0] = *reinterpret_cast<float*>(outRec + 0x54);
    duePair[1] = *reinterpret_cast<float*>(outRec + 0x58);
    void* rng = *reinterpret_cast<void**>(gw + 0xd8);
    bool ok = false;
    if (s_fn_TimeCheck_a9290)
        ok = s_fn_TimeCheck_a9290(reinterpret_cast<void*>(self + 0x200), duePair, rng);
    if (ok) {
        *reinterpret_cast<uintptr_t*>(self + 0x1f8) = outRec;
        return 1;
    }
    return 0;
}

// ============================================================
// FUN_14009e550 — MoveTo_helper_e550 (SetTerrainCosts by movement profile)
// ============================================================
// Programs the pathfinder's per-terrain step costs for one of 4 movement profiles
// (param_3 = 0..3), via FUN_1400a6a30(pathfinder, slot, cost, param4). Then calls
// RoadTool_sub_ce00_sub_68b0 to set a road-preference flag.
//   param_1 = map grid (longlong); +0x68 -> some sub-object for the final call.
//   param_2 = pathfinder (Thread*).
// Caller signature: void fn(void* mapGrid, void* pathfinder, int profile, void* param4)
typedef void (*fn_SetCost_a6a30_t)(void* pathfinder, int slot, int cost, void* p4);
typedef void (*fn_RoadFlag_a68b0_t)(void* sub, uint8_t flag, int a, int b);
static fn_SetCost_a6a30_t  s_fn_SetCost_a6a30 = nullptr;
static fn_RoadFlag_a68b0_t s_fn_RoadFlag_a68b0 = nullptr;
static bool s_terrainResolved = false;

void ActMv_SetTerrainCosts_e550(void* mapGrid, void* pathfinder, int profile, void* param4) {
    if (!s_terrainResolved) {
        s_fn_SetCost_a6a30  = reinterpret_cast<fn_SetCost_a6a30_t>(ResolveAddr(Module::Exe, 0x1400a6a30));
        s_fn_RoadFlag_a68b0 = reinterpret_cast<fn_RoadFlag_a68b0_t>(ResolveAddr(Module::Exe, 0x1400a68b0));
        s_terrainResolved = true;
    }
    if (!s_fn_SetCost_a6a30) return;
    fn_SetCost_a6a30_t set = s_fn_SetCost_a6a30;
    uint8_t roadFlag;

    if (profile == 0) {
        set(pathfinder, 0, 3, param4);
        set(pathfinder, 1, 3, param4);
        set(pathfinder, 2, 4, param4);
        set(pathfinder, 3, 8, param4);
        set(pathfinder, 4, 0x10, param4);
        set(pathfinder, 5, 0x10, param4);
        set(pathfinder, 6, 0x10, param4);
        set(pathfinder, 7, 0x10, param4);
        set(pathfinder, 8, 0x10, param4);
        set(pathfinder, 9, 3, param4);
        set(pathfinder, 10, 4, param4);
        roadFlag = 1;
    } else if (profile == 1) {
        set(pathfinder, 0, 0x10, param4);
        set(pathfinder, 1, 0x10, param4);
        set(pathfinder, 2, 4, param4);
        set(pathfinder, 3, 4, param4);
        set(pathfinder, 4, 0x10, param4);
        set(pathfinder, 5, 0x10, param4);
        set(pathfinder, 6, 0x10, param4);
        set(pathfinder, 7, 4, param4);
        set(pathfinder, 8, 4, param4);
        set(pathfinder, 9, 4, param4);
        set(pathfinder, 10, 4, param4);
        roadFlag = 1;
    } else if (profile == 2) {
        for (int i = 0; i <= 10; i++)
            set(pathfinder, i, 1, param4);
        roadFlag = 0;
    } else if (profile == 3) {
        set(pathfinder, 0, 10, param4);
        set(pathfinder, 1, 10, param4);
        set(pathfinder, 2, 10, param4);
        set(pathfinder, 3, 10, param4);
        set(pathfinder, 4, 10, param4);
        set(pathfinder, 5, 10, param4);
        set(pathfinder, 6, 10, param4);
        set(pathfinder, 7, 5, param4);
        set(pathfinder, 8, 1, param4);
        set(pathfinder, 9, 5, param4);
        set(pathfinder, 10, 10, param4);
        roadFlag = 1;
    } else {
        return;
    }

    if (s_fn_RoadFlag_a68b0) {
        uintptr_t sub = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(mapGrid) + 0x68);
        s_fn_RoadFlag_a68b0(reinterpret_cast<void*>(sub), roadFlag, 0, 0);
    }
}

// ============================================================
// FUN_1400a6d50 — MoveTo_helper_6d50 (CopyPathFromFinder)
// ============================================================
// Copies up to maxNodes path cells from the pathfinder result slot into the caller's
// path array, then resets that result slot.
//   param_1+0x160 -> result-slot array base; slot stride 0x18, slot = *param_2 (slotId).
//     slot+0x10 = node count, slot+0x08 = source cell array, slot+0x04 reset to 0,
//     slot+0x00 byte reset to 0, slot+0x08(+0) IBP reset via FUN_1400021d0(slot+8, 0).
//   Returns the number of cells actually copied (min(count, maxNodes)).
//   param_2 (int*) is reset: [0]=-1, [1]=0.
// Caller signature: int fn(void* pathfinder, void* slotIdPtr, void* pathArray, int maxNodes)
typedef void (*fn_ResetSlot_21d0_t)(void* slotIBP, int zero);
static fn_ResetSlot_21d0_t s_fn_ResetSlot_21d0 = nullptr;
static bool s_copyResolved = false;

int ActMv_CopyPathFromFinder_a6d50(void* pathfinder, void* slotIdPtr, void* pathArray, int maxNodes) {
    if (!s_copyResolved) {
        s_fn_ResetSlot_21d0 = reinterpret_cast<fn_ResetSlot_21d0_t>(ResolveAddr(Module::Exe, 0x1400021d0));
        s_copyResolved = true;
    }
    uintptr_t self = reinterpret_cast<uintptr_t>(pathfinder);
    int* slotId = reinterpret_cast<int*>(slotIdPtr);
    uintptr_t base = *reinterpret_cast<uintptr_t*>(self + 0x160);
    int64_t off = static_cast<int64_t>(*slotId) * 0x18;

    int count = *reinterpret_cast<int*>(base + 0x10 + off);
    if (maxNodes < count) count = maxNodes;

    if (count > 0) {
        const int* src = *reinterpret_cast<const int**>(base + 8 + off);
        int* dst = reinterpret_cast<int*>(pathArray);
        for (int i = 0; i < count; i++)
            dst[i] = src[i];
    }

    *reinterpret_cast<int*>(base + 4 + off) = 0;
    *reinterpret_cast<uint8_t*>(base + off) = 0;
    if (s_fn_ResetSlot_21d0) s_fn_ResetSlot_21d0(reinterpret_cast<void*>(base + 8 + off), 0);

    slotId[0] = -1;
    slotId[1] = 0;
    return count;
}

// ============================================================
// FUN_14009cfb0 — PathfinderQuery (cell-to-cell reachability + path-cost build)
// ============================================================
// Refreshes connectivity (FUN_14009c480), then validates that the two cells share a
// compatible terrain pass-type / road overlay; if so it programs the path query via
// FUN_1400a6b20 + TradeComponent_impl_6c10 (the actual A* setup) and returns the
// out-result buffer. On failure it writes the "no path" sentinel (out[0]=-1,out[1]=0).
//   param_1 = map grid (int*). cell records: array at grid+0x20 (int-units => *(grid+0x20)
//     read as longlong), stride 0x30, pass byte +0x29, region short +0x26.
//   grid+0x1a (int-units) => *(grid+0x1a) read as Thread* = pathfinder owner.
//   param_3 (uint64) = terrain pass flags. param_4 = destCell, param_5 = srcCell.
// Caller signature: void* fn(void* grid, void* outResult, uint64_t flags, uint32_t dest, uint32_t src)
typedef void (*fn_ConnRefresh_9c480_t)(int* grid, int* out, uint64_t flags, uint32_t dest);
typedef void (*fn_PathSetupA_a6b20_t)(void* owner, uint32_t mask, uintptr_t recDest, uintptr_t recSrc);
typedef void (*fn_PathSetupB_a6c10_t)(void* owner, int* out, uint64_t dest, uint64_t src);
static fn_ConnRefresh_9c480_t s_fn_ConnRefresh_9c480 = nullptr;
static fn_PathSetupA_a6b20_t  s_fn_PathSetupA_a6b20 = nullptr;
static fn_PathSetupB_a6c10_t  s_fn_PathSetupB_a6c10 = nullptr;
static bool s_pfqResolved = false;

void* ActMv_PathfinderQuery_9cfb0(void* gridp, void* outResultp, uint64_t flags,
                                  uint32_t destCell, uint32_t srcCell) {
    if (!s_pfqResolved) {
        s_fn_ConnRefresh_9c480 = reinterpret_cast<fn_ConnRefresh_9c480_t>(ResolveAddr(Module::Exe, 0x14009c480));
        s_fn_PathSetupA_a6b20  = reinterpret_cast<fn_PathSetupA_a6b20_t>(ResolveAddr(Module::Exe, 0x1400a6b20));
        s_fn_PathSetupB_a6c10  = reinterpret_cast<fn_PathSetupB_a6c10_t>(ResolveAddr(Module::Exe, 0x1400a6c10));
        s_pfqResolved = true;
    }
    int* grid = reinterpret_cast<int*>(gridp);
    int* out = reinterpret_cast<int*>(outResultp);

    if (s_fn_ConnRefresh_9c480) s_fn_ConnRefresh_9c480(grid, out, flags, destCell);

    // Cell-record array base: *(grid + 0x20 int-units) == *(uintptr_t*)((char*)grid + 0x80).
    uintptr_t cellArr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80);
    uintptr_t recSrc  = cellArr + static_cast<uintptr_t>(static_cast<int>(srcCell)) * 0x30;
    uintptr_t recDest = cellArr + static_cast<uintptr_t>(static_cast<int>(destCell)) * 0x30;

    uint32_t typeSrc  = 1u << (*reinterpret_cast<const uint8_t*>(recSrc  + 0x29) & 0x1f);
    uint32_t typeDest = 1u << (*reinterpret_cast<const uint8_t*>(recDest + 0x29) & 0x1f);
    uint32_t mask = static_cast<uint32_t>(flags) | typeDest | typeSrc;

    bool reachable = (mask & typeDest) && (mask & typeSrc);
    int16_t rd = *reinterpret_cast<const int16_t*>(recDest + 0x26);
    int16_t rs = *reinterpret_cast<const int16_t*>(recSrc + 0x26);
    if (reachable && (mask & 0x1e0u) == 0) {
        if (rd == 0 || rs == 0 || rd != rs)
            reachable = false;
    }

    // Bug-A diagnostic (BANISHED_PF_TRACE, default off): the decisive per-query
    // evidence — raw src/dest terrain TYPE bytes (cell+0x29), region labels
    // (cell+0x26), the final walk-mask, and the reachable verdict. An empty A*
    // for a "reachable" pair means the walk-mask/region flood-fill disagree.
    {
        static int s_q = -1, s_qN = 0;
        if (s_q < 0) { const char* e = std::getenv("BANISHED_PF_TRACE"); s_q = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (s_q == 1 && s_qN < 80) { ++s_qN;
            uint8_t tSrc  = *reinterpret_cast<const uint8_t*>(recSrc  + 0x29);
            uint8_t tDest = *reinterpret_cast<const uint8_t*>(recDest + 0x29);
            std::fprintf(stderr,
                "[PFQuery9cfb0] src=%u(type=%u,rgn=%d) dest=%u(type=%u,rgn=%d) flags=0x%llx mask=0x%x -> %s\n",
                srcCell, tSrc, (int)rs, destCell, tDest, (int)rd,
                (unsigned long long)flags, mask, reachable ? "A*-RUN" : "NO-PATH(gate)");
            std::fflush(stderr);
        }
    }

    if (!reachable) {
        out[1] = 0;
        out[0] = -1;
        return out;
    }

    if ((mask & 0x180u) != 0)
        mask |= 0x180u;

    // owner = *(grid + 0x1a int-units) == *(void**)((char*)grid + 0x68).
    void* owner = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(grid) + 0x68);
    if (s_fn_PathSetupA_a6b20) s_fn_PathSetupA_a6b20(owner, mask, recDest, recSrc);
    if (s_fn_PathSetupB_a6c10) s_fn_PathSetupB_a6c10(owner, out, destCell, srcCell);
    return out;
}

// ============================================================
// FUN_1400c8c10 — MoveTo_helper_8c10 (PathSetup: world pos -> cells -> query)
// ============================================================
// Converts the target world position (param_2) and the citizen's current world
// position (self+0x38/+0x3c) to clamped grid cells, and either marks "arrived"
// (same cell) or programs the terrain costs + pathfinder query for the move.
//   gw+0xf8 -> map grid (int*); grid[0] = width; center = width>>1 (float).
//   self+0x60 = path index (reset -1), +0x64 = arrived flag, +0x68 = arrived cell,
//   self+0x470 = movement profile, self+0x5c = terrain flags, self+0x20 = result store.
// Caller signature: void* fn(void* self, void* worldPos, void* param3, void* param4)
//   (returns a truthy nonzero; original returns (1 in low byte) | high garbage.)
void* ActMv_PathSetup_c8c10(void* selfp, void* worldPosp, void* /*param3*/, void* param4) {
    uintptr_t self = reinterpret_cast<uintptr_t>(selfp);
    float* worldPos = reinterpret_cast<float*>(worldPosp);
    int* grid = reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(GameWorld() + 0xf8));
    int width = grid[0];
    float center = static_cast<float>(width >> 1);

    auto clamp = [&](float f) -> int {
        int v = static_cast<int>(f);
        if (v < 0) v = 0;
        if (v >= width) v = width - 1;
        return v;
    };

    int tx = clamp(center + worldPos[0]);
    int ty = clamp(worldPos[1] + center);
    uint32_t destCell = static_cast<uint32_t>(width * ty + tx);

    int cx = clamp(*reinterpret_cast<float*>(self + 0x38) + center);
    int cy = clamp(*reinterpret_cast<float*>(self + 0x3c) + center);
    uint32_t srcCell = static_cast<uint32_t>(width * cy + cx);

    *reinterpret_cast<int*>(self + 0x60) = -1;

    if (destCell == srcCell) {
        *reinterpret_cast<int*>(self + 0x64) = 1;
        *reinterpret_cast<uint32_t*>(self + 0x68) = destCell;
        *reinterpret_cast<int*>(self + 0x60) = 0;
    } else {
        // grid+0x1a int-units == (char*)grid + 0x68 = pathfinder owner (Thread*).
        void* owner = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(grid) + 0x68);
        ActMv_SetTerrainCosts_e550(reinterpret_cast<void*>(grid), owner,
                                   *reinterpret_cast<int*>(self + 0x470), param4);
        // Original writes the query result into a 2-int stack scratch (local_res8),
        // then copies the 8-byte result into self+0x20.
        int32_t scratch[2] = {0, 0};
        void* res = ActMv_PathfinderQuery_9cfb0(
            grid, scratch, *reinterpret_cast<uint32_t*>(self + 0x5c), destCell, srcCell);
        *reinterpret_cast<uint64_t*>(self + 0x20) = *reinterpret_cast<uint64_t*>(res);
    }
    return reinterpret_cast<void*>(static_cast<uintptr_t>(1));
}

// ============================================================
// FUN_140092260 — Bridge/spatial entity query near a position (quadtree walk)
//   *** NOW REIMPLEMENTED — code/Game/Action/action_pf_roots_misc.cpp ***
// ============================================================
// Formerly skipped because of its frame-exact ~9.5KB local quadtree-walker. The
// pathfinding wave ported the walker's builder (18dd0) and subdivider (18880) as
// proper registered functions operating on a passed walker pointer, so 92260 was
// reimplemented (Pf_SpatialProximityQuery_92260) with a same-size local buffer
// `alignas(16) uint8_t walker[0x2438]` and disasm-verified record offsets, calling
// the registered 18dd0/18880. Registered via RegisterPfRootsMiscBatch.
