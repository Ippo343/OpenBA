// standalone/reg_player_drag.cpp
// Player registration wave (P1 — drag) registration.
//
// Registers the exe addresses of closure-safe DragTool / DragPlacementTool sub-helpers
// (resolved from code/Game/Player/dragtool.cpp and dragplacementtool.cpp call sites)
// to their reimplementations in code/Game/Player/tool_base_drag.cpp. The integrator
// wires RegisterPlayerDragBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address), and its ENTIRE exe sub-callee closure
// is satisfied (registered exe code / engine_funcs exports / singleton globals).
// Each is a faithful 1:1 port.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// dragtool.cpp helpers (tool_base_drag.cpp)
extern void* DragTool_FindDescription_5160(uintptr_t, void*);  // 0x140005160 type 0x1401c7f58
extern void  DragTool_CloseToolDialog_ff980(uint64_t*);        // 0x1400ff980
extern void  DragTool_PlayTickSound_ffbf0(uintptr_t);          // 0x1400ffbf0
extern void  DragTool_PlayPlaceSound_1000f0(uint64_t);         // 0x1401000f0
extern void  DragTool_PlayErrorSound_100140(uint64_t);         // 0x140100140

// dragplacementtool.cpp helper (tool_base_drag.cpp)
extern void  DragPlacementTool_ClearPathEntities_1018d0(void*, int, int, int, int, int64_t); // 0x1401018d0

// NOTE — DEFERRED (NOT registered; bodies intentionally absent from tool_base_drag.cpp):
//   0x1400ff9e0 OpenToolDialog  — sets dialog vtable to g_ToolUI_ChildDialog_Vtable, which
//                                 resolves to exe vtable 0x14015f5a0 (UNregistered → 0 in
//                                 standalone → broken vtable). Register once 0x14015f5a0 lands.
//   0x1400ffc60 UpdateToolUI    — deep unregistered closure (e600/e700/e420/e360/ebb0/ec70/fe960).
//   0x140101270 ScreenToCell    — unregistered FUN_1401005a0 + QuarryComponent_impl_a530(->aa20).
//
// LEVEL-1 caller bug (report only; out of this batch's file scope):
//   0x1400ff5d0 in dragplacementtool.cpp is MID-FUNCTION. True callee is
//   CharacterComponent_impl_f5d0 @ 0x14009f5d0. Fix the ResolveAddr literal there.

void RegisterPlayerDragBatch() {
    RegisterExeAddr(0x140005160, reinterpret_cast<uintptr_t>(&DragTool_FindDescription_5160));
    RegisterExeAddr(0x1400ff980, reinterpret_cast<uintptr_t>(&DragTool_CloseToolDialog_ff980));
    RegisterExeAddr(0x1400ffbf0, reinterpret_cast<uintptr_t>(&DragTool_PlayTickSound_ffbf0));
    RegisterExeAddr(0x1401000f0, reinterpret_cast<uintptr_t>(&DragTool_PlayPlaceSound_1000f0));
    RegisterExeAddr(0x140100140, reinterpret_cast<uintptr_t>(&DragTool_PlayErrorSound_100140));
    RegisterExeAddr(0x1401018d0, reinterpret_cast<uintptr_t>(&DragPlacementTool_ClearPathEntities_1018d0));
}
#endif // STANDALONE_EXE
