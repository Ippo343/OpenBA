#pragma once
// BuildStructure — the entry points of code/Game/Work/buildstructure.cpp that are not
// members of the class (Work/BuildStructure.h). ENDGAME B.3.8: the ctors, the dtor and
// the slot bodies are members now; the C entry points other files name are declared
// where they are used (extern "C").

#include "Work/BuildStructure.h"
#include <cstdint>

// Shared (vtable +0x28, 0x1400f7540): Always returns false (Work::Slot5).
