#pragma once
// PastureWork — ENDGAME B.3.8: the ctors, the dtor and the slot bodies are members of
// the class (Work/PastureWork.h). What is left: the file's helper resolver. (The
// slot-4 door at 0x1400f2370 went with its only reader, B.3.8 CB3.)

#include "Work/PastureWork.h"
#include <cstdint>

bool PastureWork_ResolveFuncs();
