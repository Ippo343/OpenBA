// standalone/reg_player_edit.cpp
// Player registration wave — batch "edit" (edittool.cpp + fencedtool.cpp helpers).
//
// Registers the exe addresses of shared Player sub-helpers (resolved from
// code/Game/Player/edittool.cpp and fencedtool.cpp call sites) to their
// reimplementations in code/Game/Player/tool_base_edit.cpp. The integrator wires
// RegisterPlayerEditBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address), is a faithful 1:1 port, and has its
// ENTIRE exe sub-callee closure satisfied (registered or Runtime-export). See the
// per-address notes in tool_base_edit.cpp.
#include "resolve.h"
#include <cstdint>

// EntityAtCell — void* fn(uintptr_t entityList, void* retIBP, int cellIndex)
extern void* PlayerEdit_EntityAtCell_a0280(uintptr_t, void*, int);          // 0x1400a0280

// CharacterComponent_impl_1730 (rect-average terrain height) —
//   float fn(void* gameWorld, int p2, int p3, int p4, int p5)
// Leaf dep registered bottom-up so 0x1400dbf90's closure is satisfied. Its only
// exe sub-callee is 0x14009f3e0 (terrain height query), already registered.
extern float PlayerEdit_AvgTerrainHeight_101730(void*, int, int, int, int);  // 0x140101730

// FencedTool anchor/rotation rectangle calc —
//   uint64_t fn(uintptr_t self, uint32_t* p2,p3,p4,p5, float* p6, uint32_t* p7)
extern uint64_t PlayerEdit_FencedAnchorCalc_dbf90(uintptr_t, uint32_t*, uint32_t*,
                                                  uint32_t*, uint32_t*, float*, uint32_t*); // 0x1400dbf90

// ----------------------------------------------------------------------------
// Status of the addresses this wave originally DEFERRED (all since reconciled):
//   0x140066800 EditTool_helper_6800 — REGISTERED by reg_player_trade.cpp (r2c trade;
//       the "2e30/c690" blockers were typos for 102e30/2c690, both registered there).
//   0x1400fcc90 Entity_impl_cc90 — DELIBERATELY UNREGISTERED, permanently: it doubles
//       as the EditTool per-frame deactivate vtable slot, so a registered body would
//       wipe a just-set selection outline every frame. Canonical rationale in
//       reg_player_render.cpp (the closure that blocked it here has since been ported
//       in tool_base_render.cpp — closure is no longer the reason).
//   0x140100380 Entity_impl_0380 — REGISTERED by reg_player_screenrect.cpp (r2d) with
//       its full closure (fec70/098210/fe960; value-return Math via ResolveExport).
//   0x1401005a0 GetMouseRay — REGISTERED by reg_player_ray.cpp (r2; resolves its
//       Frustum/Transform3 exports locally).
//
// LEVEL-1 CALLER BUG — FIXED: edittool.cpp's "MapHitTest" literal 0x14010a530
//   (mid-function in CitizenAI_State_Wander) was corrected to the true callee
//   0x1400aa530 (QuarryComponent_impl_a530); see edittool.cpp:145.
// ----------------------------------------------------------------------------

// FencedTool::CalcPath (fenced-area drag commit) — fencedtool_calcpath.cpp.
extern "C" uint64_t FencedTool_CalcPath_Port(void* self, int64_t startCell, int64_t endCell);

void RegisterPlayerEditBatch() {
    RegisterExeAddr(0x1400a0280, reinterpret_cast<uintptr_t>(&PlayerEdit_EntityAtCell_a0280));
    RegisterExeAddr(0x140101730, reinterpret_cast<uintptr_t>(&PlayerEdit_AvgTerrainHeight_101730));
    RegisterExeAddr(0x1400dbf90, reinterpret_cast<uintptr_t>(&PlayerEdit_FencedAnchorCalc_dbf90));
    RegisterExeAddr(0x1400dae00, reinterpret_cast<uintptr_t>(&FencedTool_CalcPath_Port));
}
