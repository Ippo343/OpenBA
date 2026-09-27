// standalone/reg_pf_ctors.cpp
// Registration for the Action-ctor + entity/storage leaf batch
// (code/Game/Action/action_pf_ctors.cpp). The integrator wires
// RegisterPfCtorsBatch() into RegisterCodeFunctions() in address_registry.cpp.
//
// Every address is a real function ENTRY (Ghidra Entry == address) and a faithful 1:1
// port. Closure: all exe sub-callees these reach are already registered elsewhere —
//   0x1400032e0 (component_people), 0x1400c3470 (action_hunt), 0x1400012d0 (action_move),
//   0x1400c5400 / 0x140006270 (work_base), 0x14012d150 (action_rawmat) — plus the
//   Object::Internal::Internal / CheckPtr / Time::Time / IsDerivedFrom engine helpers.
#include "resolve.h"
#include <cstdint>

extern void  Pf_ActStackWalkClear_4f10(void*, void*);                       // 0x1400c4f10
extern void  Pf_ActStackPush_54e0(int64_t*, void*);                         // 0x1400c54e0
extern void* Pf_CompleteTaskCopyCtor_5da0(void*, uint32_t, uint64_t*, uint64_t*, uint32_t); // 0x1400c5da0
extern void* Pf_CreateRawMaterialCopyCtor_71a0(void*, uint64_t, float, uint64_t*, uint64_t*,
                                               uint64_t, uint32_t, uint32_t, uint8_t,
                                               uint64_t*, uint32_t);          // 0x1400c71a0 (arg3 float/XMM2)
extern void* Pf_MoveToCtor_9170(void*, void*, uint32_t, int, float*, int, int, float); // 0x1400c9170
extern void* Pf_PerformWorkCtor7_9720(void*, void*, float, uint32_t, int64_t, uint8_t, int); // 0x1400c9720
extern void* Pf_SendComponentMessageCopyCtor_b1d0(void*, uint64_t*, uint32_t*, uint32_t,
                                                  uint64_t*, uint32_t);       // 0x1400cb1d0
extern void  Pf_SetFlagRefreshUpdate_107260(int64_t*);                       // 0x140107260
extern int   Pf_RecursiveChildSum_123070(int64_t, int);                      // 0x140123070
extern int   Pf_ResourceLimitFlagTest_c560(int64_t, int64_t);                // 0x14012c560
extern void  Pf_StorageDropOff_116760(int64_t, int64_t, int);                // 0x140116760
extern int   Pf_StorageCountDelta_1167c0(int64_t, int64_t);                  // 0x1401167c0

void RegisterPfCtorsBatch() {
    RegisterExeAddr(0x1400c54e0, reinterpret_cast<uintptr_t>(&Pf_ActStackPush_54e0));
    RegisterExeAddr(0x1400c5da0, reinterpret_cast<uintptr_t>(&Pf_CompleteTaskCopyCtor_5da0));
    RegisterExeAddr(0x1400c71a0, reinterpret_cast<uintptr_t>(&Pf_CreateRawMaterialCopyCtor_71a0));
    RegisterExeAddr(0x1400c9170, reinterpret_cast<uintptr_t>(&Pf_MoveToCtor_9170));
    RegisterExeAddr(0x1400c9720, reinterpret_cast<uintptr_t>(&Pf_PerformWorkCtor7_9720));
    RegisterExeAddr(0x1400cb1d0, reinterpret_cast<uintptr_t>(&Pf_SendComponentMessageCopyCtor_b1d0));
    RegisterExeAddr(0x140107260, reinterpret_cast<uintptr_t>(&Pf_SetFlagRefreshUpdate_107260));
    RegisterExeAddr(0x140123070, reinterpret_cast<uintptr_t>(&Pf_RecursiveChildSum_123070));
    RegisterExeAddr(0x14012c560, reinterpret_cast<uintptr_t>(&Pf_ResourceLimitFlagTest_c560));
    RegisterExeAddr(0x140116760, reinterpret_cast<uintptr_t>(&Pf_StorageDropOff_116760));
    RegisterExeAddr(0x1401167c0, reinterpret_cast<uintptr_t>(&Pf_StorageCountDelta_1167c0));
}
