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

// dragtool.cpp — the SHARED DragTool cancel-drag / "can-deactivate?" slot-9 body.
// Every DragTool subclass vtable holds this address at +0x48 (Bridge/Road/Removal/
// DragPlacement), and FencedTool's own slot 9 (0x1400dcd10) TAIL-JUMPS to it. It was
// registered nowhere, so any caller reaching it by address (engine_funcs'
// fn_DragTool_Shutdown) got NULL: mid-drag the drag was never cancelled and the
// CreateBar cost dialog was never closed before the tool was deleted.
extern "C" int DragTool_CancelDrag_Impl(void*);                // 0x1400fc640

// NOTE — the P1 defers below have all since been REGISTERED by later waves:
//   0x1400ff9e0 OpenToolDialog  — reg_player_toolui.cpp (r2c): registered with a
//       faithful standalone child-dialog vtable written directly by the reimpl; the
//       exe vtable 0x14015f5a0 stays DELIBERATELY unregistered forever (registering
//       it poisons the menu's BuildId child — see reg_player_toolui.cpp).
//   0x1400ffc60 UpdateToolUI    — reg_player_createbar.cpp (GUI Phase 3B).
//   0x140101270 ScreenToCell    — reg_player_ray.cpp (r2).
//
// LEVEL-1 caller bug — FIXED: dragplacementtool.cpp's mid-function literal 0x1400ff5d0
//   was corrected to the true callee CharacterComponent_impl_f5d0 @ 0x14009f5d0
//   (see dragplacementtool.cpp:146).

void RegisterPlayerDragBatch() {
    RegisterExeAddr(0x140005160, reinterpret_cast<uintptr_t>(&DragTool_FindDescription_5160));
    RegisterExeAddr(0x1400ff980, reinterpret_cast<uintptr_t>(&DragTool_CloseToolDialog_ff980));
    RegisterExeAddr(0x1400ffbf0, reinterpret_cast<uintptr_t>(&DragTool_PlayTickSound_ffbf0));
    RegisterExeAddr(0x1401000f0, reinterpret_cast<uintptr_t>(&DragTool_PlayPlaceSound_1000f0));
    RegisterExeAddr(0x140100140, reinterpret_cast<uintptr_t>(&DragTool_PlayErrorSound_100140));
    RegisterExeAddr(0x1401018d0, reinterpret_cast<uintptr_t>(&DragPlacementTool_ClearPathEntities_1018d0));
    RegisterExeAddr(0x1400fc640, reinterpret_cast<uintptr_t>(&DragTool_CancelDrag_Impl));
}
