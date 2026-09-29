#pragma once
// Free helpers of ClearAreaComponent shared with other files.

#include "ClearAreaComponent.h"
#include <cstdint>

// B4 FORWARD: ClearAreaComponent::FinishClear (0x140118270) for the outside caller
// buildcomponent_state1.cpp (BuildComponent::State1Enter's construction-site reset).
void ClearArea_Finalize8270(void* comp);
