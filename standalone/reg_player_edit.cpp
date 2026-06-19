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
#ifdef STANDALONE_EXE
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
// DEFERRED (bodies NOT written / NOT registered — unregistered exe sub-callee in
// the closure; a half-null port is silent corruption, strictly worse than the
// safe no-op skip):
//   0x140066800 EditTool_helper_6800 — tail-calls 0x1400666b0, whose closure pulls
//       in TradeWork_impl_2e30 + TradeWork_impl_c690 (both UNREG).
//   0x1400fcc90 Entity_impl_cc90 — exe sub-callee 0x1400fcb40 (Entity_impl_cb40)
//       whose closure is FUN_140004740 + thunk_helper_d250_sub_7080 + Entity_impl_c4a0
//       (all UNREG).
//   0x140100380 Entity_impl_0380 — exe sub-callees 0x1400fec70 + 0x1400fe960 (UNREG),
//       plus Volume::Box::GetCorners / Math::Transform::operator* bounding-box math
//       (no fn_ exports yet).
//   0x1401005a0 FUN_1401005a0 (camera mouse ray) — no exe sub-callees, but its body
//       needs Volume::Frustum::GetScreenDirection + Math::Transform3::operator* +
//       Math::Transform3::Rotate exports that have no fn_ pointer yet (intricate
//       48-byte Transform3 / 112-byte Frustum stack-struct math; dormant on autostart).
//
// LEVEL-1 CALLER BUG (do NOT register — reported for the lead to fix the literal):
//   edittool.cpp resolves "MapHitTest" at 0x14010a530, which is MID-FUNCTION
//   (entry 0x14010a2b0 CitizenAI_State_Wander). The TRUE callee is 0x1400aa530
//   (QuarryComponent_impl_a530), confirmed by the CALL at 0x1400fd12a in the original
//   EditTool OnKey/Draw (0x1400fd030). pathtool.cpp already uses 0x1400aa530 correctly.
// ----------------------------------------------------------------------------

void RegisterPlayerEditBatch() {
    RegisterExeAddr(0x1400a0280, reinterpret_cast<uintptr_t>(&PlayerEdit_EntityAtCell_a0280));
    RegisterExeAddr(0x140101730, reinterpret_cast<uintptr_t>(&PlayerEdit_AvgTerrainHeight_101730));
    RegisterExeAddr(0x1400dbf90, reinterpret_cast<uintptr_t>(&PlayerEdit_FencedAnchorCalc_dbf90));
}
#endif
