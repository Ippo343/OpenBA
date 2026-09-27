// standalone/reg_quarry_live.cpp
// Address-registry wave: QuarryComponent — the quarry-pit terrain deformation.
//
// Why this wave exists: QuarryComponent was spawned by COMPONENT_INERT_FACTORY, so every
// instance carried the shared 48-slot stub vtable. Its Shutdown body had been ported by
// an earlier wave and was never reachable, and the three functions that actually dig the
// pit (0x1400549c0, 0x1400550f0, 0x140054b80) had no port at all. decomp/application.md
// listed the component as "0x28 bytes (smallest) ... Quarry stone extraction" with no
// note that anything was missing. Symptom: a working quarry produces stone forever and
// the ground under it never moves, because that pit IS the terrain heightmap carve these
// functions apply.
//
// Template/Quarry.rsc is the only template declaring a `quarry` description, so this wave
// is quarry-scoped:
//     QuarryDescription quarry {
//         PickingMesh _floorMesh = ".../QuarryFloor.rsc";   // desc +0x20
//         float _maxDepth     = -7.0;                       // desc +0x2c
//         float _initialDepth = -1.4;                       // desc +0x28
//     }
// and its ConsumeProduce block routes each finished production cycle back into the
// component: `ObjectType _messageType = QuarryComponent; int _messageId = 1;`.
//
// Closure — every callee these bodies need was already registered by earlier waves:
//   0x14000b630 FindComponent<Decal>   (reg_destroy_sm)
//   0x14000b6f0 FindConsumeProduce     (reg_action_child_ctors2)
//   0x140003f60 FindComponent<Model>   (reg_action_cnr)
//   0x140004680 FindComponent<Map>     (reg_component_nature)
//   0x14009f3e0 MapGetTerrainPos       (mapquery)
//   0x1400aaa20 cell-quad ray          (reg_pf_road)   0x1400aa810 ray/tri (reg_pf_geom)
//   0x1400ab150 region recalc          (reg_component_gap_D)
//   0x1400a8c90 fine-cell dirty        (reg_pf_road)   0x1400a6fe0 pf cmd2 (reg_pf_astar)
// Runtime-side the carve needs the same three .rdata dispatch slots the tunnel wave
// already filled: 0x140151418 Transform copy-ctor, 0x140151470 Transform::operator*,
// 0x140151478 Picking::PickingMesh::Vertices.
//
// QuarryComponent is a real class (ENDGAME B.3.8): slots 12 (0x1400550f0) and 15
// (0x140054b80) are compiler-vtable virtuals, and nothing read their addresses back,
// so their rows are gone. The ApplyDepth helper keeps its door.

#include "resolve.h"
#include "address_registry.h"
#include <cstdint>

extern "C" void QuarryComponent_ApplyDepth(void* self, float depth);   // 0x1400549c0

void RegisterQuarryLive() {
    RegisterExeAddr(0x1400549c0, reinterpret_cast<uintptr_t>(&QuarryComponent_ApplyDepth));
}
