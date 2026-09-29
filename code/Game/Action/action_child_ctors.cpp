// code/Game/Action/action_child_ctors.cpp
// FUN_1400c29a0 (Entity_impl_29a0): the heading from an entity toward a world target,
// used by the Work action-queue builders. No class yet (B.4 inventory A: unattributed).
//
// B.4 R0-EXE: the four Action ctors that lived here (ShowTool 0x140042890,
// RemoveNaturalResource 0x1400cadf0, AnimateNaturalResource 0x1400c57d0,
// CreateNaturalResource 0x1400c6a20) and their by-address doors moved, text unchanged,
// to their classes' files (showtool.cpp, removenaturalresource.cpp,
// animatenaturalresource.cpp, createnaturalresource.cpp).
#include "Action/exe_bodies.h"
// Closure (bodies NOT duplicated; called by name, B3R):
//   Entity_ClearPathQueue        0x140003f60 ActCNR_FindComponent_3f60 (action_base_cnr.cpp)
//   FUN_140004680                0x140004680 FloorComponent_FindResData (floorcomponent.cpp)
//   CharacterComponent_impl_f3e0 0x14009f3e0 Reimpl_MapGetTerrainPos (mapquery.cpp / cell->world)
//   Math::Vector3::Normalize     slot 0x1401512c0 / ?Normalize@Vector3@Math@@QEBA?AV12@XZ
//   Math::Vector3::Heading       slot 0x1401512d8 / ?Heading@Vector3@Math@@QEBAMXZ
//
// Registration: RegisterActionChildCtorsBatch() in standalone/reg_action_child_ctors.cpp.

#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Component/exe_bodies.h"   // FloorComponent_FindResData
#include "Game/exe_bodies.h"        // Reimpl_MapGetTerrainPos
#include "math/Math.h"
#include <cstdint>
#include <new>

// Math::Vector3::Normalize (slot 0x1401512c0) and Heading (slot 0x1401512d8) are direct
// members of the real class (B.3.1).

// ============================================================
// FUN_1400c29a0 — Entity_impl_29a0
// Compute the heading (radians, XMM0) from a citizen toward a world target.
//   param_1 = float* worldPos (the target);  param_2 = InternalBasePtr* ({inner ptr}).
// If the entity has a live path node (Entity_ClearPathQueue(*p2) -> node, *node != 0),
// use the node's stored world pos (node+0x158..+0x160). Otherwise derive a world pos
// from its cell node (FUN_140004680 -> node) via the grid math + CharacterComponent
// cell->world sampler. Then v = pos - worldPos; Normalize(v); return Heading(v).
// ============================================================
float ActChild_Entity29a0_c29a0(float* worldPos, void* param_2) {
    float pos[3] = {0.0f, 0.0f, 0.0f};

    uint8_t retbuf[8] = {0};  // local_res8 — IBP retval for the getters
    int64_t inner = *reinterpret_cast<int64_t*>(param_2);
    void* pathIbp = ActCNR_FindComponent_3f60(static_cast<uintptr_t>(inner), retbuf);   // 0x140003f60

    if (pathIbp && *reinterpret_cast<int64_t*>(pathIbp) != 0) {
        // --- path node present: read its world pos directly ---
        int64_t inner2 = *reinterpret_cast<int64_t*>(param_2);
        void* ibp2 = ActCNR_FindComponent_3f60(static_cast<uintptr_t>(inner2), retbuf);   // 0x140003f60
        int64_t node = ibp2 ? *reinterpret_cast<int64_t*>(ibp2) : 0;
        pos[0] = *reinterpret_cast<float*>(node + 0x158);
        pos[1] = *reinterpret_cast<float*>(node + 0x15c);
        pos[2] = *reinterpret_cast<float*>(node + 0x160);
    } else {
        // --- no path node: derive a world pos from the entity's cell node ---
        int64_t inner2 = *reinterpret_cast<int64_t*>(param_2);
        void* cellIbp = FloorComponent_FindResData(reinterpret_cast<void*>(inner2), retbuf);   // 0x140004680
        int64_t node = cellIbp ? *reinterpret_cast<int64_t*>(cellIbp) : 0;

        void* gw = GameUpdateLoop::_singleton;
        int32_t* mapPtr = gw ? *reinterpret_cast<int32_t**>(reinterpret_cast<uintptr_t>(gw) + 0xf8) : nullptr;
        int32_t width = mapPtr ? *mapPtr : 1;

        int32_t s28 = *reinterpret_cast<int16_t*>(node + 0x28);
        int32_t s2a = *reinterpret_cast<int16_t*>(node + 0x2a);
        int32_t s2c = *reinterpret_cast<int16_t*>(node + 0x2c);
        int32_t s2e = *reinterpret_cast<int16_t*>(node + 0x2e);

        int32_t cellIdx = (s2a + (s2e >> 1)) * width + (s2c >> 1) + s28;
        int32_t quot = (width != 0) ? cellIdx / width : 0;   // EAX after IDIV
        int32_t rem  = (width != 0) ? cellIdx % width : 0;    // EDX after IDIV
        float   center = static_cast<float>(width >> 1) - 0.5f;  // (width>>1) - DAT_140172114

        float inVec[3];
        inVec[0] = static_cast<float>(rem)  - center;  // local_28
        inVec[1] = static_cast<float>(quot) - center;  // local_24
        inVec[2] = 0.0f;                               // local_20

        float outVec[3] = {0.0f, 0.0f, 0.0f};
        Reimpl_MapGetTerrainPos(mapPtr, outVec, inVec, 0, nullptr);   // 0x14009f3e0
        pos[0] = outVec[0];
        pos[1] = outVec[1];
        pos[2] = outVec[2];
    }

    // --- v = pos - worldPos ; Normalize(v) ; return Heading(v) ---
    Math::Vector3 v(pos[0] - worldPos[0],
                    pos[1] - worldPos[1],
                    pos[2] - worldPos[2]);
    Math::Vector3 n = v.Normalize();
    return n.Heading();
}
