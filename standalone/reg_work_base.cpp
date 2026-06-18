// standalone/reg_work_base.cpp
// Wave 2 (Work) — shared base-helper sub-batch registration.
//
// Registers the exe addresses of high-reuse Work base/ECS helpers (resolved from
// many code/Game/Work/*.cpp call sites) to their reimplementations in
// code/Game/Work/work_base_reimpl.cpp. The integrator wires RegisterWorkBaseBatch()
// into RegisterCodeFunctions() in standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address) and is a faithful 1:1 port.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// FindComponent family (work_base_reimpl.cpp) — void* fn(uintptr_t entity, void* retIBP)
extern void* WorkBase_FindComponent_2ed0(uintptr_t, void*);  // 0x140002ed0 type 0x1401c7900
extern void* WorkBase_FindComponent_2f90(uintptr_t, void*);  // 0x140002f90 type 0x1401c7e08
extern void* WorkBase_FindComponent_3050(uintptr_t, void*);  // 0x140003050 type 0x1401c8070
extern void* WorkBase_FindComponent_52c0(uintptr_t, void*);  // 0x1400052c0 type 0x1401c7b30
extern void* WorkBase_FindComponent_6270(uintptr_t, void*);  // 0x140006270 type 0x1401c7e78

// Recursive tree free (work_base_reimpl.cpp) — void fn(void* root, void* node)
extern void WorkBase_TreeNodeFree_14c30(void*, void*);       // 0x140014c30 children +0x10/+0x18
extern void WorkBase_TreeNodeFree_e320(void*, void*);        // 0x14000e320 children +0x18/+0x20

// Work base lifecycle (work_base_reimpl.cpp)
extern void    WorkBase_TaskListCleanup_c5400(void*);                       // 0x1400c5400
extern void    WorkBase_Serialize_f7400(void*, void*);                      // 0x1400f7400
extern uint8_t WorkBase_ValidateTask_f7100(void*, void*, void*, void**);    // 0x1400f7100

void RegisterWorkBaseBatch() {
    RegisterExeAddr(0x140002ed0, reinterpret_cast<uintptr_t>(&WorkBase_FindComponent_2ed0));
    RegisterExeAddr(0x140002f90, reinterpret_cast<uintptr_t>(&WorkBase_FindComponent_2f90));
    RegisterExeAddr(0x140003050, reinterpret_cast<uintptr_t>(&WorkBase_FindComponent_3050));
    RegisterExeAddr(0x1400052c0, reinterpret_cast<uintptr_t>(&WorkBase_FindComponent_52c0));
    RegisterExeAddr(0x140006270, reinterpret_cast<uintptr_t>(&WorkBase_FindComponent_6270));
    RegisterExeAddr(0x140014c30, reinterpret_cast<uintptr_t>(&WorkBase_TreeNodeFree_14c30));
    RegisterExeAddr(0x14000e320, reinterpret_cast<uintptr_t>(&WorkBase_TreeNodeFree_e320));
    RegisterExeAddr(0x1400c5400, reinterpret_cast<uintptr_t>(&WorkBase_TaskListCleanup_c5400));
    RegisterExeAddr(0x1400f7400, reinterpret_cast<uintptr_t>(&WorkBase_Serialize_f7400));
    RegisterExeAddr(0x1400f7100, reinterpret_cast<uintptr_t>(&WorkBase_ValidateTask_f7100));
}
#endif
