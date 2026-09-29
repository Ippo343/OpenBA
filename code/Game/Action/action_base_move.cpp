// code/Game/Action/action_base_move.cpp
// Shared MoveTo / PerformWork / PlayAnimation base-class / ECS exe sub-helpers.
//
// The per-class Action .cpp files (moveto.cpp, performwork.cpp, playanimation.cpp,
// huntanimal.cpp) call these by name through Action/exe_bodies.h (B3R); the
// registration (RegisterActionMoveBatch in standalone/reg_action_move.cpp) stays for
// readers other blocks still have.
//
// Wave 3 (Action) — MoveTo / PerformWork / PlayAnimation sub-batch. Each function is
// a 1:1 port of the decompiled exe helper; do not "improve" behavior.
//
#include "Action/exe_bodies.h"

#include <new>
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/Map.h"
#include "Game/PathfinderThread.h"
#include "Action/PathSearch.h"
#include "Game/exe_bodies.h"       // Reimpl_MapGetTerrainPos
#include "Game/exe_constants.h"   // kRdata_140172114
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Object/factory_registration.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "math/Math.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Work/Work.h"         // B.3.x W0: Work::GetTarget by name
#include "Component/CharacterComponent.h"   // ENDGAME B.3.8 CB3: CharacterComponent::TypeId()
#include "Component/CharacterDescription.h"

// ============================================================
// Locally-resolved dispatch-slot / math helpers
// ============================================================
// ENDGAME B.3.1 step 2: the Math::Vector2 / Math::Vector3 helpers below used to be
// read from the exe's .rdata dispatch slots — 0x1401526b0 Math::Vector2::Normalize,
// 0x1401512c0 Math::Vector3::Normalize, 0x1401512d8 Math::Vector3::Heading. Those
// slots were the DLL boundary's shape; the calls are now direct members on the real
// Math:: classes (runtime/types/math/Math.h), same bodies, same operand order.

// ============================================================
// g_pGameWorldSingleton accessor (DAT_1401c3dd8 — declared in engine_funcs.h)
// The original reads `g_pGameWorldSingleton` directly as a byte* base (it is the
// GameWorld instance pointer, not a pointer-to-pointer). In our headers it is typed
// `void** g_pGameWorldSingleton` and dereferenced once to obtain the instance.
// ============================================================
static inline uintptr_t GameWorld() {
    return reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton);
}

// ============================================================
// FUN_1400012d0 — Entity_FindComponent_Family
// ============================================================
// Member-style FindComponent over the entity's component list. RCX=entity,
// RDX=retIBP, returns retIBP. Scans the component ptr-array at entity+0x30 (count
// at +0x38), calls each component's GetTypeId (vtable +0x10), and on the first type
// match (DAT_1401c7078) runs IsDerivedFrom before storing through the out IBP.
//
// 1:1 port — type-ID cell at DAT_1401c7078 is CharacterComponent's (Table 1).
void* ActMv_FindComponentFamily_12d0(uintptr_t entity, void* retIBP) {
    void* found = nullptr;
    uint32_t wantId = CharacterComponent::TypeId();
    int n = *reinterpret_cast<int*>(entity + 0x38);
    if (n > 0 && wantId != 0 && wantId != 0xFFFFFFFFu) {
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n; i++) {
            void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
            void* comp = *slot;
            uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)
            if (t == wantId) {
                void* c = *slot;
                found = ::Object::IsDerivedFromNullSafe(c, wantId) ? c : nullptr;
                break;
            }
        }
    }
    new (retIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
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
//   0x1401529f0 External::GetPath(retbuf)    -> ::Object::External::GetPath (B.3.3)
//   0x1401529d8 ExternalFactory _singleton   -> g_pExternalFactorySingleton
//   0x140151230 ExternalFactory::FindOrLoad  -> ::Object::ExternalFactory::FindOrLoad (B.3.3)
//   0x140152a98 IsDerivedFrom                -> Object::IsDerivedFromNullSafe (B3R)
//   0x140151d40 Filename::Filename()         -> System::Filename ctor (B.3.1)
//   0x140151218 IsReferenceModeEnabled       -> ::Object::ExternalFactory member (B.3.3)
//   0x140151220 GetReferenceId               -> ::Object::ExternalFactory member (B.3.3)
//   0x140151228 GetPathForReferenceId        -> ::Object::ExternalFactory member (B.3.3)
//   0x1401529e0 Filename::operator=          -> System::Filename::operator= (B.3.1)
//   0x1401529e8 Filename::~Filename          -> System::Filename dtor, RAII (B.3.1)
//   0x140152a50 Serializer::Serialize(uint*) -> local resolve
//   0x140152a58 Serializer::Serialize(bool*) -> local resolve
//   0x140151238 String::Serialize            -> System::String::Serialize (B.3.1)
//   0x1401c70b0 type-ID                       -> local resolve (NOT in engine_funcs.h)
void ActMv_ExternalPtrSerialize_22970(void* extPtrSlot, void* serializer) {
    uintptr_t* pSlot = reinterpret_cast<uintptr_t*>(extPtrSlot);
    uint32_t typeId = CharacterDescription::TypeId();

    // GetEncoding -> encoding->vtable[+0x10](encoding) == 3 ?
    int encType = -1;
    void* enc = const_cast<IO::Encoding*>(&reinterpret_cast<IO::Serializer*>(serializer)->GetEncoding());   // B.3.4 bridge
    if (enc)
        encType = (*reinterpret_cast<int(**)(void*)>(*reinterpret_cast<uintptr_t*>(enc) + 0x10))(enc);

    if (encType == 3) {
        // Read-only text path: drop the External; re-resolve by its current path.
        if (*pSlot == 0) return;
        System::Filename fnStorage;  // Filename retbuf (local_res18 in the original)
        fnStorage = reinterpret_cast<const ::Object::External*>(*pSlot)->GetPath();  // B.3.3 bridge
        if (::Object::ExternalFactory::_singleton) {
            ::Object::ExternalFactory* efactory =
                ::Object::ExternalFactory::_singleton;  // B.3.3 bridge
            void* found = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(fnStorage)));
            if (found) ::Object::IsDerivedFromNullSafe(found, typeId);
        }
        return;   // ~Filename releases fnStorage (was the manual fn_Filename_Dtor)
    }

    // Filename local (default-constructed) used to carry the path through the stream.
    System::Filename fnLocal;

    void* factory = ::Object::ExternalFactory::_singleton;
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge
    bool refMode = (factory)
                       ? efactory->IsReferenceModeEnabled() : false;

    uint32_t refId = 0xffffffff;
    bool didFind = false;  // whether we fall through to the FindOrLoad write-back block

    if (refMode) {
        if (*pSlot == 0) {
            refId = 0xffffffff;
        } else if (factory) {
            refId = efactory->GetReferenceId(
                reinterpret_cast<const ::Object::External*>(*pSlot));  // B.3.3 bridge
        }
        // Stream-dirty flag = *(serializer+0)->[+8]. (RAX=[RBX]; [RAX+8])
        int streamFlag = static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection());   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        if (*pSlot != 0 && streamFlag == 0 && refId == 0xffffffff) {
            // Special case: present but no ref id and writing -> just serialize the (-1) id.
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned int*>(&refId), nullptr);   // B.3.4 bridge
            // (no write-back)
        } else {
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned int*>(&refId), nullptr);   // B.3.4 bridge
            if ((int)refId != -1) {
                streamFlag = static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection());   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
                if (streamFlag == 0 && factory) {
                    System::Filename pathBuf = efactory->GetPathForReferenceId(refId);
                    fnLocal = pathBuf;
                }   // ~Filename releases pathBuf
                didFind = true;
            }
        }
    } else {
        // Reference mode off: bool "present" flag then the path String.
        uint8_t present = (*pSlot != 0) ? 1 : 0;
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(&present), nullptr);   // B.3.4 bridge
        if (present != 0) {
            if (*pSlot != 0) {
                System::Filename pathBuf =
                    reinterpret_cast<const ::Object::External*>(*pSlot)->GetPath();  // B.3.3 bridge
                fnLocal = pathBuf;
            }   // ~Filename releases pathBuf
            // Retail serializes the Filename through String::Serialize (the two
            // classes are layout-identical and it casts between them).
            fnLocal.AsString().Serialize(serializer);
            didFind = true;
        }
    }

    if (didFind) {
        int streamFlag = static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection());   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        if (streamFlag == 0) {
            void* found = nullptr;
            if (::Object::ExternalFactory::_singleton)
                found = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(fnLocal)));
            uintptr_t result = 0;
            if (found && ::Object::IsDerivedFromNullSafe(found, typeId))
                result = reinterpret_cast<uintptr_t>(found);
            *pSlot = result;
        }
    }
    // ~Filename releases fnLocal (was the manual fn_Filename_Dtor).
}

// ============================================================
// FUN_14010a060 — PerformWork_helper_a060 (SetFacingDirection)
// ============================================================
// Sets a component's facing from a Vector3 direction. Clears a byte at +0x28,
// normalizes (x,y,0) into +0x2c..+0x34, stores Heading(dir) at +0x3c.
// Caller signature: void fn(void* comp, void* vector3)
void ActMv_SetFacingDirection_a060(void* comp, void* vector3) {
    uintptr_t self = reinterpret_cast<uintptr_t>(comp);
    const Math::Vector3& dir = *reinterpret_cast<const Math::Vector3*>(vector3);  // B.3.1 bridge

    *reinterpret_cast<uint8_t*>(self + 0x28) = 0;
    Math::Vector3 tmp(dir.x, dir.y, 0.0f);
    Math::Vector3 n = tmp.Normalize();
    *reinterpret_cast<float*>(self + 0x2c) = n.x;
    *reinterpret_cast<float*>(self + 0x30) = n.y;
    *reinterpret_cast<float*>(self + 0x34) = n.z;
    *reinterpret_cast<float*>(self + 0x3c) = dir.Heading();
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
//   CharacterComponent_impl_f3e0 (0x14009f3e0, Reimpl_MapGetTerrainPos) = grid->world
//   conversion. DAT_140172114 = 0.5f (kRdata_140172114).
// Caller signature: void fn(void* self)
void ActMv_UpdateFacing_c8d20(void* selfp) {
    uintptr_t self = reinterpret_cast<uintptr_t>(selfp);
    int* grid = reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(GameWorld() + 0xf8));
    float halfCell = kRdata_140172114;   // 0x140172114

    // Resolve the citizen movement sub-object: self+0x28 -> +0x68.
    uintptr_t citizen = *reinterpret_cast<uintptr_t*>(self + 0x28);
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
    Reimpl_MapGetTerrainPos(grid, outWorld, inGrid, 0, nullptr);   // 0x14009f3e0

    // dir2 = (worldX - curX, worldY - curY), normalized.
    Math::Vector2 dir2in(outWorld[0] - curX, outWorld[1] - curY);
    Math::Vector2 dir2 = dir2in.Normalize();

    // Re-check the IBP, store normalized 3D facing into the movement sub-object.
    uintptr_t citizen2 = *reinterpret_cast<uintptr_t*>(self + 0x28);
    *reinterpret_cast<uint8_t*>(citizen2 + 0x28) = 0;

    // local_58/54/50 = (dir2.x, dir2.y, 0) from the Vector2::Normalize result.
    Math::Vector3 dir3(dir2.x, dir2.y, 0.0f);
    Math::Vector3 n = dir3.Normalize();
    *reinterpret_cast<float*>(citizen2 + 0x2c) = n.x;
    *reinterpret_cast<float*>(citizen2 + 0x30) = n.y;
    *reinterpret_cast<float*>(citizen2 + 0x34) = n.z;

    Math::Vector3 head3(dir2.x, dir2.y, 0.0f);
    *reinterpret_cast<float*>(citizen2 + 0x3c) = head3.Heading();
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
void ActMv_SetAnimation_a1f0(void* selfp, uint32_t animId, float p3, float p4) {
    uintptr_t self = reinterpret_cast<uintptr_t>(selfp);
    uintptr_t sub = *reinterpret_cast<uintptr_t*>(self + 0x78);
    uintptr_t animObj = *reinterpret_cast<uintptr_t*>(self + 0x68);
    // 5th arg (p3) is a float passed on the stack in the original (MOVSS [RSP+0x20]).
    ActMvDep_AnimHelper_50f0(animObj, animId, 0.0f, *reinterpret_cast<float*>(sub + 0x2c), p3);   // 0x1401250f0
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
uint8_t ActMv_CitizenBreakCheck_116650(void* entityp) {
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
        // InternalBasePtr ctor (&local_res10, 0): value stays null.
        new (&valueIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        valueObj = reinterpret_cast<uintptr_t>(valueIBP);
    }

    if (valueObj == 0)
        return 0;

    uintptr_t v = reinterpret_cast<uintptr_t>(valueIBP);

    // value->vtable[+0x50](value, &out) -> record ptr in 'out'.
    uintptr_t outRec = 0;
    // The value is the scheduler's work IBP (gw+0x118 id map, filled by
    // CitizenAI_impl_88a0 0x1400f88a0 with the posted Work leaves).
    reinterpret_cast<Work*>(v)->GetTarget(&outRec);   // B.3.x bridge (slot 10, retslot)

    // Original copies 8 bytes (*(undefined8*)(rec+0x54)) — a9290 reads BOTH floats
    // (pair[0] = value, pair[1] = weight). Passing a single float would feed it stack
    // garbage for pair[1]. Copy the full pair.
    float duePair[2];
    duePair[0] = *reinterpret_cast<float*>(outRec + 0x54);
    duePair[1] = *reinterpret_cast<float*>(outRec + 0x58);
    void* rng = *reinterpret_cast<void**>(gw + 0xd8);
    bool ok = ActMvDep_TimeCheck_a9290(reinterpret_cast<void*>(self + 0x200), duePair, rng);   // 0x1400a9290
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
//
// B.4 ACT-a: Map::SetMovementCosts. Retail reads RCX (the Map), RDX (the pathfinder) and
// R8D (the profile) only (0x14009e55a / 0x14009e55d / 0x14009e560); the tree's fourth
// argument was never read.
void Map::SetMovementCosts(PathfinderThread* pathfinder, int profile) {
    PathfinderThread* pf = pathfinder;
    uint8_t roadFlag;

    if (profile == 0) {
        pf->SetStepCost(0, 3);
        pf->SetStepCost(1, 3);
        pf->SetStepCost(2, 4);
        pf->SetStepCost(3, 8);
        pf->SetStepCost(4, 0x10);
        pf->SetStepCost(5, 0x10);
        pf->SetStepCost(6, 0x10);
        pf->SetStepCost(7, 0x10);
        pf->SetStepCost(8, 0x10);
        pf->SetStepCost(9, 3);
        pf->SetStepCost(10, 4);
        roadFlag = 1;
    } else if (profile == 1) {
        pf->SetStepCost(0, 0x10);
        pf->SetStepCost(1, 0x10);
        pf->SetStepCost(2, 4);
        pf->SetStepCost(3, 4);
        pf->SetStepCost(4, 0x10);
        pf->SetStepCost(5, 0x10);
        pf->SetStepCost(6, 0x10);
        pf->SetStepCost(7, 4);
        pf->SetStepCost(8, 4);
        pf->SetStepCost(9, 4);
        pf->SetStepCost(10, 4);
        roadFlag = 1;
    } else if (profile == 2) {
        for (int i = 0; i <= 10; i++)
            pf->SetStepCost(i, 1);
        roadFlag = 0;
    } else if (profile == 3) {
        pf->SetStepCost(0, 10);
        pf->SetStepCost(1, 10);
        pf->SetStepCost(2, 10);
        pf->SetStepCost(3, 10);
        pf->SetStepCost(4, 10);
        pf->SetStepCost(5, 10);
        pf->SetStepCost(6, 10);
        pf->SetStepCost(7, 5);
        pf->SetStepCost(8, 1);
        pf->SetStepCost(9, 5);
        pf->SetStepCost(10, 10);
        roadFlag = 1;
    } else {
        return;
    }

    this->pathfinder->SetSearchFlags(roadFlag, 0, 0);   // 0x1400a68b0 (DL, R8B = 0, R9B = 0)
}

// B4 FORWARD: 1 outside caller (Action/moveto.cpp)
void ActMv_SetTerrainCosts_e550(void* mapGrid, void* pathfinder, int profile, void*) {
    reinterpret_cast<Map*>(mapGrid)->SetMovementCosts(reinterpret_cast<PathfinderThread*>(pathfinder), profile);
}

// ============================================================
// FUN_1400a6d50 — MoveTo_helper_6d50 (CopyPathFromFinder)
// ============================================================
// PathfinderThread::TakePath: copies up to maxNodes path cells from the ticket's result
// slot into the caller's path array (retail re-reads the ticket and the slot table on
// every element), then frees the slot (token 0, ready 0, path emptied) and resets the
// ticket to {-1, 0}. Returns the number of cells copied (min(count, maxNodes)). No lock.
int PathfinderThread::TakePath(PathTicket* ticket, int32_t* path, int maxNodes) {
    int count = results.data[ticket->slot].path._size;
    if (maxNodes < count) count = maxNodes;

    if (count > 0) {
        for (int i = 0; i < count; i++)
            path[i] = results.data[ticket->slot].path._data[i];
    }

    results.data[ticket->slot].token = 0;
    results.data[ticket->slot].ready = 0;
    // 0x1400021d0 == Collection::Array<int32>::SetSize (1400a6df3); SetSize(0) never grows.
    results.data[ticket->slot].path.SetSize(0);

    ticket->slot = -1;                 // one qword store of {-1, 0} in retail (H7)
    ticket->token = 0;
    return count;
}

// B4 FORWARD: 2 outside callers (Action/keystone_a_zone.cpp, Action/moveto.cpp)
int ActMv_CopyPathFromFinder_a6d50(void* pathfinder, void* slotIdPtr, void* pathArray, int maxNodes) {
    return reinterpret_cast<PathfinderThread*>(pathfinder)->TakePath(reinterpret_cast<PathTicket*>(slotIdPtr),
                                                                reinterpret_cast<int32_t*>(pathArray), maxNodes);
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
//
// B.4 ACT-a: Map::RequestPath. The zone refresh 0x14009c480 is Map::FillZones; the two
// pathfinder calls are PathfinderThread::SetPassableTypes / ::RequestPath.
PathTicket* Map::RequestPath(PathTicket* out, uint64_t flags, uint32_t destCell, uint32_t srcCell) {
    FillZones();   // 0x14009c480 (RCX only)

    const MapCell* recSrc  = &cells[static_cast<int>(srcCell)];
    const MapCell* recDest = &cells[static_cast<int>(destCell)];

    uint32_t typeSrc  = 1u << (recSrc->terrainType & 0x1f);
    uint32_t typeDest = 1u << (recDest->terrainType & 0x1f);
    uint32_t mask = static_cast<uint32_t>(flags) | typeDest | typeSrc;

    bool reachable = (mask & typeDest) && (mask & typeSrc);
    int16_t rd = static_cast<int16_t>(recDest->zone);
    int16_t rs = static_cast<int16_t>(recSrc->zone);
    if (reachable && (mask & 0x1e0u) == 0) {
        if (rd == 0 || rs == 0 || rd != rs)
            reachable = false;
    }


    if (!reachable) {
        out->token = 0;
        out->slot = -1;
        return out;
    }

    if ((mask & 0x180u) != 0)
        mask |= 0x180u;

    // Retail leaves R8 / R9 holding the two cell records here; 0x1400a6b20 reads EDX only.
    pathfinder->SetPassableTypes(static_cast<int>(mask));   // 0x1400a6b20
    pathfinder->RequestPath(out, static_cast<int>(destCell), static_cast<int>(srcCell));   // 0x1400a6c10
    return out;
}

// B4 FORWARD: 2 outside callers (Action/moveto.cpp, Player/pathtool_setselected.cpp)
void* ActMv_PathfinderQuery_9cfb0(void* gridp, void* outResultp, uint64_t flags,
                                  uint32_t destCell, uint32_t srcCell) {
    return reinterpret_cast<Map*>(gridp)->RequestPath(reinterpret_cast<PathTicket*>(outResultp), flags, destCell, srcCell);
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
void* ActMv_PathSetup_c8c10(void* selfp, void* worldPosp, void* /*param3*/, void* /*param4*/) {
    uintptr_t self = reinterpret_cast<uintptr_t>(selfp);
    float* worldPos = reinterpret_cast<float*>(worldPosp);
    Map* grid = GameUpdateLoop::_singleton->m_game;
    int width = grid->size;
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
        grid->SetMovementCosts(grid->pathfinder, *reinterpret_cast<int*>(self + 0x470));   // 0x14009e550
        // Original writes the query result into a 2-int stack scratch (local_res8),
        // then copies the 8-byte result into self+0x20.
        PathTicket scratch = {0, 0};
        PathTicket* res = grid->RequestPath(&scratch, *reinterpret_cast<uint32_t*>(self + 0x5c), destCell, srcCell);   // 0x14009cfb0
        *reinterpret_cast<PathTicket*>(self + 0x20) = *res;
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
