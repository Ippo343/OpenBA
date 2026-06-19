// standalone/reg_action_complete.cpp
// Wave 3 (Action) — CompleteTask / ShowTool exe sub-helper registration (closure-safe).
//
// Registers the exe addresses of the action sub-helpers referenced from
// code/Game/Action/{completetask,showtool}.cpp to their reimplementations in
// code/Game/Action/action_base_complete.cpp. The integrator wires
// RegisterActionCompleteBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Safety: every address below is a real function ENTRY (Ghidra
// get_function_by_address Entry == address) and a faithful 1:1 port whose entire exe
// sub-callee closure is also satisfied:
//   - CompleteTask 8760/8800/8e30: InternalBasePtr CheckPtr/DeletePtr + IsDerivedFrom
//     come from engine_funcs (export-resolved); the two intrusive-BST removes 8e30 uses
//     (0x14001ca60 / 0x14001a6e0) were registered by the Work wave.
//   - ShowTool 1173b0/1176e0: graphics ops route through the registered SetMesh /
//     Scene::AddObject / Scene::RemoveObject slots + named Geometry/GraphicsMesh exports;
//     any unregistered cosmetic slot (Vector4 ctor 0x140152970, Time ctor 0x140152968)
//     is null-skipped (no tool mesh, no corruption).
//
// DEFERRED (NOT registered here): DropOffRawMaterial 0x1400c7570 / 0x1400c7620 — their
// closure pulls in unregistered code sub-callees (FUN_1400c78c0 spawn, MoveTo ctor
// 0x1400c9170, task-list append 0x1400c54e0). Draft in notes/wave3_drafts/.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// CompleteTask task-manager BST helpers (action_base_complete.cpp)
extern bool ActCt_TryCompleteTask_8760(void*, void*, int, void*, uint32_t);   // 0x1400f8760
extern void ActCt_TaskReassign_8800(void*, void*, int, void*, uint32_t);      // 0x1400f8800
extern void ActCt_TaskCleanup_8e30(void*, int);                               // 0x1400f8e30

// ShowTool tool-mesh helpers (action_base_complete.cpp)
extern void ActCt_ShowToolOnEntity_1173b0(void*, int);                        // 0x1401173b0
extern void ActCt_HideTool_1176e0(void*);                                     // 0x1401176e0

void RegisterActionCompleteBatch() {
    RegisterExeAddr(0x1400f8760, reinterpret_cast<uintptr_t>(&ActCt_TryCompleteTask_8760));
    RegisterExeAddr(0x1400f8800, reinterpret_cast<uintptr_t>(&ActCt_TaskReassign_8800));
    RegisterExeAddr(0x1400f8e30, reinterpret_cast<uintptr_t>(&ActCt_TaskCleanup_8e30));
    RegisterExeAddr(0x1401173b0, reinterpret_cast<uintptr_t>(&ActCt_ShowToolOnEntity_1173b0));
    RegisterExeAddr(0x1401176e0, reinterpret_cast<uintptr_t>(&ActCt_HideTool_1176e0));
}
#endif
