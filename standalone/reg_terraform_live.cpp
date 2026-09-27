// standalone/reg_terraform_live.cpp
// Address-registry wave: TerraformComponent — the terrain-deformation path.
//
// Why this wave exists: TerraformComponent was spawned by COMPONENT_INERT_FACTORY, so
// every instance carried the shared 48-slot stub vtable. Its Shutdown and OnRemove
// bodies had been ported since an earlier wave and were never reachable, and the three
// functions that actually deform the terrain (0x140056b50, 0x140057120, 0x140057310)
// had no port at all. notes/component_goals.md marked the component COMPLETE and
// decomp/components/audit.md said "OnCreate passthrough" — neither was true; nothing
// in the component ran. Symptom: a finished Tunnel has no recess at its mouth, because
// that recess IS the terrain heightmap carve these functions apply.
//
// Only Template/Tunnel.rsc and Template/TunnelStart.rsc declare a `terraform`
// description, so this wave is tunnel-scoped. Other terrain-modifying buildings flatten
// through ClearAreaDescription._flattenGround, an unrelated path.
//
// Closure — every callee these bodies need was already registered by earlier waves:
//   0x1400ab150 region recalc (reg_component_gap_D)      0x14001a4f0 (reg_pf_avl)
//   0x1400a8c90 fine-cell dirty (reg_pf_road)            0x1400a6fe0 (reg_pf_astar)
//   0x14000d030 Vector3 array resize (reg_component_nature)
//   0x140003f60 / 0x140004680 component finders          0x14009f3e0 terrain sample
//   0x1401c49f8 TerraformDescription type id (reg_economy_typeids, this wave)
// Runtime-side, Picking::PickingMesh::Vertices comes from the exe .rdata dispatch slot at
// 0x140151478. That slot had NO reader anywhere in the port, so it was never registered and
// ResolveAddr returned 0 — RegisterDispatchSlots() in address_registry.cpp now fills it.
//
// TerraformComponent and MultiObjectComponent are real classes (ENDGAME B.3.8): the
// Persist / OnRemove / slot-12 rows (0x140057500 / 0x140057350 / 0x140057310, and
// MultiObjectComponent's slot-12 forwarder 0x140125490) went with them -- nothing read
// those addresses back. The two carve helpers keep their doors.

#include "resolve.h"
#include "address_registry.h"
#include <cstdint>

class TerraformComponent;

extern void TerraformComponent_MeshPass(TerraformComponent* self, bool ceiling);   // 0x140056b50
extern void TerraformComponent_FootprintPass(TerraformComponent* self);            // 0x140057120

// Retail passes the ceiling/floor selector as a `char`, not a `bool`; keep the exe
// signature exact at the registry boundary so any retail-side caller lands correctly.
static void TerraformComponent_MeshPass_Thunk(void* self, char ceiling) {
    TerraformComponent_MeshPass(reinterpret_cast<TerraformComponent*>(self), ceiling != 0);
}

void RegisterTerraformLive() {
    RegisterExeAddr(0x140056b50, reinterpret_cast<uintptr_t>(&TerraformComponent_MeshPass_Thunk));
    RegisterExeAddr(0x140057120, reinterpret_cast<uintptr_t>(
        static_cast<void(*)(TerraformComponent*)>(&TerraformComponent_FootprintPass)));
}

