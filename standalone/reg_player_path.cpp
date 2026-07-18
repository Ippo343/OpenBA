// standalone/reg_player_path.cpp
// Player registration wave (batch "path") — registration of the shared exe
// sub-helpers reimplemented in code/Game/Player/tool_base_path.cpp.
//
// The integrator wires RegisterPlayerPathBatch() into RegisterCodeFunctions()
// in standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address) and is a faithful 1:1 port whose
// ENTIRE exe sub-callee closure is satisfied (registered here bottom-up, already
// registered elsewhere, or a Runtime export / registered dispatch slot).
//
// VERDICTS (P4 = pathtool + removaltool, 7 assigned gap addrs):
//   0x1400aa530  DEFER   — QuarryComponent RaycastTerrain. Closure IS satisfied
//                          (Vector2::Normalize slot 0x1401526b0 registered,
//                          QuarryComponent_helper_aa20 0x1400aaa20 registered),
//                          but the float-march body uses heavy pointer-aliased
//                          stack groups; a faithful port is high-risk and the
//                          path (PathTool::OnKey raycast) is not exercised on
//                          autostart. Defer to a dedicated, debugger-verified pass.
//   0x1400c08c0  LEVEL-1 — NOT a function entry. Entry == 0x1400c0690
//                          (FUN_1400c0690); 0x1400c08c0 is mid-body. The
//                          pathtool.cpp call site (s_fn_SubmitDebugDraw) is also
//                          dead (resolved but never invoked — Draw uses
//                          fn_TileRender). Caller addr should be corrected to
//                          0x1400c0690 (or the resolve dropped); do not register.
//   0x1400dd710  DEFER   — PathTool_helper_d710 (SetSelectedEntity). Closure
//                          pulls in thunk_helper_d250, PathTool_helper_d0f0, and
//                          PathTool_helper_d770 — the last is a ~520-line AVL
//                          tree builder with ~12 unregistered exe sub-callees
//                          (FUN_140002e10, Entity_impl_7fb0, d770_sub_e5d0,
//                          d770_sub_6ba0, FUN_14009cfb0, FUN_1400021d0,
//                          FUN_140014c30/c70/da0, Entity_FindComponent_*).
//                          Not closure-safe for this wave.
//   0x1401035e0  DEFER   — RemovalTool_Slot13 (CalcPath). ~770-line function with
//                          ~15 unregistered sub-fns (ComponentEntity copy/register,
//                          road BST, crop tracker, TradeWork). Already documented
//                          as deferred in removaltool.cpp. Not closure-safe.
//   0x140104670  REGISTER — RemovalTool_sub_4670 (building-removal eligibility).
//   0x140104770  REGISTER — RemovalTool_sub_4770 (road/path-removal eligibility).
//   0x1401048a0  REGISTER — RemovalTool_sub_48a0 (crop/field-area removability).
//
// Bottom-up sub-deps registered here so the three sub_4xxx closures are complete:
//   0x140005780  FUN_140005780  FindComponent leaf  (DestroyComponent)
//   0x140003ea0  FUN_140003ea0  FindComponent leaf  (MapComponent variant)
//   0x14011e710  FUN_14011e710  is-removable-child (calls 140003ea0)
//   0x14009f5d0  CharacterComponent_impl_f5d0 single-cell terrain test
//                (calls FUN_14009f3e0 — already registered)
//
// Closure of the three REGISTERed sub_4xxx:
//   sub_4670 -> {CheckPtr slot, 140005780*, 140003ea0*}                          OK
//   sub_4770 -> {CheckPtr slot, 140005780*, 14011e710* -> 140003ea0*}            OK
//   sub_48a0 -> {fn_IsDerivedFrom, CheckPtr slot, FindComp 1400052c0 (reg),
//                14009f5d0* -> 14009f3e0 (reg) + fabsf, fn_MemFreeStatic}        OK
//   (* = registered in this batch, bottom-up)
#include "resolve.h"
#include <cstdint>

// FindComponent leaves — void* fn(uintptr_t entity, void* retIBP)
extern void* PlayerPath_FindComponent_5780(uintptr_t, void*);   // 0x140005780
extern void* PlayerPath_FindComponent_3ea0(uintptr_t, void*);   // 0x140003ea0

// is-removable-child — uint64_t fn(uintptr_t)
extern uint64_t PlayerPath_Sub_11e710(uintptr_t);               // 0x14011e710

// single-cell terrain test — bool fn(int*,int,int,uint,float,float,float,char,char,int64_t)
extern bool PlayerPath_CharComp_f5d0(int*, int, int, uint32_t, float, float, float,
                                     char, char, int64_t);       // 0x14009f5d0

// RemovalTool per-tile checks
extern uint64_t PlayerPath_Sub_4670(uintptr_t, void*);          // 0x140104670
extern uint64_t PlayerPath_Sub_4770(uintptr_t, void*);          // 0x140104770
extern uint64_t PlayerPath_Sub_48a0(uintptr_t, void*, int, int);// 0x1401048a0

void RegisterPlayerPathBatch() {
    // bottom-up: leaves first
    RegisterExeAddr(0x140005780, reinterpret_cast<uintptr_t>(&PlayerPath_FindComponent_5780));
    RegisterExeAddr(0x140003ea0, reinterpret_cast<uintptr_t>(&PlayerPath_FindComponent_3ea0));
    RegisterExeAddr(0x14011e710, reinterpret_cast<uintptr_t>(&PlayerPath_Sub_11e710));
    RegisterExeAddr(0x14009f5d0, reinterpret_cast<uintptr_t>(&PlayerPath_CharComp_f5d0));
    // the three assigned RemovalTool sub-checks
    RegisterExeAddr(0x140104670, reinterpret_cast<uintptr_t>(&PlayerPath_Sub_4670));
    RegisterExeAddr(0x140104770, reinterpret_cast<uintptr_t>(&PlayerPath_Sub_4770));
    RegisterExeAddr(0x1401048a0, reinterpret_cast<uintptr_t>(&PlayerPath_Sub_48a0));
}
