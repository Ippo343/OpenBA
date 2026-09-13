// Component registration wave — SpawnComponent live bring-up (Phase 1).
//
// SpawnComponent's Update (0x140055ad0) and OnCreate/Startup (0x140055e90) read two
// type-id .rdata globals that, in the original exe, are .data filled by RegisterType:
//   * DAT_1401c4918 = SpawnDescription  type id (resourceData = self->_unk18)
//   * DAT_1401c77e8 = GrowthDescription type id (the per-entry spawn-interval template)
// In standalone WE provide the storage (registered at AddressRegistryInit) and FILL
// it after Engine::Startup via Object::GetTypeIdByName (Object::Factory::Startup
// assigns the real CRC ids only then — see reg_component_nature.cpp for the same
// pattern). Without this, SpawnComponent_Update early-returns (both globals null).
//
// NOTE: every other address SpawnComponent needs is ALREADY registered elsewhere and
// is intentionally NOT re-registered here:
//   * 0x140001bb0 array resizer      -> address_registry.cpp (Reimpl_Array_Resize_Stride08)
//   * 0x1400052c0 GetLinkedObject    -> reg_work_base.cpp (WorkBase_FindComponent_52c0)
//   * 0x140055780 DoSpawn            -> reg_component_economy.cpp (SpawnComponent_DoSpawn)
//   * the full DoSpawn closure (0x140004680/9f5d0/120c30/c3720/c460/126540/120db0/c48a0)
//     -> reg_component_nature / reg_player_path / reg_work_leaf / reg_action_cnr / address_registry
//
// The integrator wires RegisterComponentBatch_spawn() into address_registry.cpp and
// FillComponentTypeIdGlobals_spawn() into game_main.cpp (after the Engine::Startup probe).
#include "resolve.h"
#include "Object/factory_registration.h"     // Object::GetTypeIdByName
#include <cstdint>

// Backing storage for the SpawnComponent type-id .rdata globals (filled after
// InvokeAllRegistrations/Engine::Startup — see FillComponentTypeIdGlobals_spawn).
static uint32_t g_tid_SpawnDescription = 0;   // 0x1401c4918
static uint32_t g_tid_GrowthDescription = 0;  // 0x1401c77e8

extern "C" void FillComponentTypeIdGlobals_spawn() {
    // No-op unless the storage was registered (live mode); harmless otherwise.
    g_tid_SpawnDescription  = ::Object::GetTypeIdByName("SpawnDescription");
    g_tid_GrowthDescription = ::Object::GetTypeIdByName("GrowthDescription");
}

void RegisterComponentBatch_spawn() {
    // These two type-id globals are also read by other (currently-null-guarded)
    // consumers (TileFree 0x140120c30, ForesterWork). Published unconditionally
    // since 2026-09-13 (the SpawnComponent live gate graduated).
    RegisterExeAddr(0x1401c4918, reinterpret_cast<uintptr_t>(&g_tid_SpawnDescription));
    RegisterExeAddr(0x1401c77e8, reinterpret_cast<uintptr_t>(&g_tid_GrowthDescription));
}
