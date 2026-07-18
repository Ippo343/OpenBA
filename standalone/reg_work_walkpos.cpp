// standalone/reg_work_walkpos.cpp
// Registration for the Work shared-helper walkable-position cluster
// (bodies: code/Game/Work/work_base_walkpos.cpp).
//
//   0x140122be0  WorkImpl_WalkablePos_122be0           — single build-record array search
//   0x140122b00  WorkImpl_WalkablePosRecursive_122b00  — recursive search across children
//   0x1400f6ef0  WorkImpl_BuildStructureStep_6ef0      — build-progress / time-budget step
//
// Closure: all exe-code sub-callees (0x140003f60 ClearPathQueue, 0x1400a00f0 ValidateCell,
// 0x14009f810 MapPathCost) are registered by earlier waves; the Math::Transform::operator*
// dispatch slot 0x140151470 is registered in RegisterDispatchSlots() (address_registry.cpp).
//


#include "resolve.h"
#include <cstdint>

bool     WorkImpl_WalkablePos_122be0(uintptr_t param_1, uintptr_t param_2,
                                     float* param_3, float* param_4, int* param_5);
uint8_t  WorkImpl_WalkablePosRecursive_122b00(uintptr_t param_1, int param_2,
                                              float* param_3, float* param_4, int* param_5);
uint64_t WorkImpl_BuildStructureStep_6ef0(uintptr_t param_1, void* param_2IBP,
                                          uint32_t param_3, uint32_t param_4, void* param_5Time);

void RegisterWorkWalkposBatch() {
    RegisterExeAddr(0x140122be0, reinterpret_cast<uintptr_t>(&WorkImpl_WalkablePos_122be0));
    RegisterExeAddr(0x140122b00, reinterpret_cast<uintptr_t>(&WorkImpl_WalkablePosRecursive_122b00));
    RegisterExeAddr(0x1400f6ef0, reinterpret_cast<uintptr_t>(&WorkImpl_BuildStructureStep_6ef0));
}

