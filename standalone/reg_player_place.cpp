// standalone/reg_player_place.cpp
// Player wave (batch "place") — PlacementTool / RoadTool shared sub-helper
// registration.
//
// Registers the exe addresses of PlacementTool/RoadTool sub-helpers (resolved
// from code/Game/Player/placementtool.cpp + roadtool.cpp call sites) to their
// reimplementations in code/Game/Player/tool_base_place.cpp. The integrator
// wires RegisterPlayerPlaceBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address) and registered ONLY when its entire
// exe sub-callee closure is satisfied (registered exe code / registered dispatch
// slot / Runtime export). DEFERRED addresses are documented with the exact
// unregistered sub-callee.
#include "resolve.h"
#include <cstdint>
#include <unordered_map>

// PlacementTool sub_d960 (flip/mirror + advance mesh variant) — void fn(void* self)
extern void PlacementToolPlace_FlipVariant_d960(void*);  // 0x1400fd960

// ------------------------------------------------------------------------
// Self-contained dispatch-slot registration (mirrors the static
// RegisterDispatchSlot in address_registry.cpp, which we cannot call from here).
// code reads:  fn = *(fn_t*)ResolveAddr(Module::Exe, slot)
// engine_funcs.cpp's UpdateHierarchy init reads slot 0x140152888 with NO
// ResolveExport fallback, so without this the fn_ pointer stays null in
// standalone. RegisterPlayerPlaceBatch runs (via AddressRegistryInit) BEFORE
// EngineFuncsInit, so the slot is populated in time.
// ------------------------------------------------------------------------
static std::unordered_map<uintptr_t, uintptr_t>& PlacePlayerSlotStorage() {
    static std::unordered_map<uintptr_t, uintptr_t> slots;
    return slots;
}

static void RegisterPlacePlayerSlot(uintptr_t ghidra_addr, uintptr_t fn_ptr) {
    auto& slots = PlacePlayerSlotStorage();
    slots[ghidra_addr] = fn_ptr;
    RegisterExeAddr(ghidra_addr, reinterpret_cast<uintptr_t>(&slots[ghidra_addr]));
}

// ============================================================================
// DEFERRED — NOT registered (unregistered exe sub-callee in the closure):
//
//   0x140055430  HighlightAdd (radius display)
//                -> TradeWork_impl_0d80 @ 0x1400b0d80 (NOT registered;
//                   its own closure pulls in 0x14001fa80[reg], FUN_1400ace40
//                   @ 0x1400ace40 [NOT reg], MemFree[export]).
//
//   0x14009d580  TileValidityCheck (helper_d580)
//                -> CharacterComponent_impl_f5d0 @ 0x14009f5d0 (NOT registered;
//                   this is engine_funcs fn_TileValidityCheck, a shared
//                   Component/Work-wave gap addr, owned by another batch).
//
//   0x1400fd650  helper_d650 (placement validation + create)
//                -> PlacementTool_helper_d580 @ 0x14009d580 (deferred, needs f5d0),
//                   FUN_1401018d0 @ 0x1401018d0 (NOT reg),
//                   FUN_140003870 @ 0x140003870 (NOT reg).
//
//   0x1400fda00  sub_da00 (placement position calc)
//                -> FUN_140101270 @ 0x140101270 (ScreenToCell, NOT registered;
//                   its closure pulls in FUN_1401005a0 @ 0x1401005a0 [NOT reg]
//                   and QuarryComponent_impl_a530 @ 0x1400aa530 [NOT reg]).
//                   (0x14009f3e0 GridToWorld IS registered.)
//
//   0x140101380  sub_1380 (tile render + validate overlay)
//                -> CharacterComponent_impl_08c0 @ 0x1401008c0 (NOT reg,
//                   = engine_funcs fn_TileRender, shared cross-module gap addr)
//                   AND CharacterComponent_impl_f5d0 @ 0x14009f5d0 (NOT reg).
//
//   0x14009ce00  RoadTool sub_ce00 (road A* pathfind)
//                -> RoadTool_helper_6e30 @ 0x1400a6e30 (NOT registered; part of
//                   the pathfinder/road worker-thread family that is explicitly
//                   deferred across all prior waves — wiring it starts an unwired
//                   System::Thread / builds half-populated path+road records).
//                   (0x1400a6a30 + 0x1400a68b0 ARE registered in reg_pf_astar.)
// ============================================================================

void RegisterPlayerPlaceBatch() {
    // Dispatch slot needed by sub_d960's UpdateHierarchy (engine_funcs reads it).
    // Graphics::MeshInstance::UpdateHierarchy — pure Runtime export.
    RegisterPlacePlayerSlot(0x140152888,
        reinterpret_cast<uintptr_t>(
            ResolveExport("?UpdateHierarchy@MeshInstance@Graphics@@QEAAXXZ")));

    // sub_d960: closure = engine_funcs Scene::Add/Remove + MeshInstance::SetMesh/
    // UpdateHierarchy (all Runtime-export-backed; slots 0x140151360/370/498
    // already registered, 0x140152888 registered just above) + variant-index +
    // mesh-array indexing (no exe sub-callees).
    RegisterExeAddr(0x1400fd960,
        reinterpret_cast<uintptr_t>(&PlacementToolPlace_FlipVariant_d960));
}
