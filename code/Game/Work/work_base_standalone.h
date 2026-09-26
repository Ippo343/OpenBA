// code/Game/Work/work_base_standalone.h
// Registration entry point for the Work base class's table (ENDGAME B.3.8).
//
// The factory-cascade helpers this header used to declare (Work_Alloc,
// Work_GenericCtor, Work_GenericScalarDtor, Work_Slot1/5/12, Work_SlotRet0,
// Work_Slot11_HasActiveTasks, Work_SerializeNop, Work_PureCall) are gone: every Work
// leaf is a real class with a compiler table (code/Game/Work/<Class>.h), and the base
// slots are Work's own members (Work/Work.h).
#pragma once
#include <cstdint>

extern "C" {

// Registers Work::Vtable() at the exe address 0x140161a80, which by-address readers
// still stamp (see work_base_standalone.cpp). Called from RegisterWorkBaseBatch()
// (standalone/reg_work_base.cpp).
void RegisterWorkBaseVtable(void);        // 0x140161a80  Work

}  // extern "C"
