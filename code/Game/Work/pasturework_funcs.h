#pragma once
// PastureWork — ENDGAME B.3.8: the ctors, the dtor and the slot bodies are members of
// the class (Work/PastureWork.h). What is left: the file's helper resolver and the
// slot-4 door standalone/reg_work_tail_serialize.cpp registers at 0x1400f2370.

#include "Work/PastureWork.h"
#include <cstdint>

int PastureWork_Slot4_Impl(PastureWork* self, void* entityIBP, uint64_t param3, uint64_t param4);

bool PastureWork_ResolveFuncs();
