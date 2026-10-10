#pragma once
// Free helpers of ClearAreaComponent shared with other files.

#include "ClearAreaComponent.h"
#include <cstdint>


// The two orchardcomponent_states.cpp reaches (its state 20 / 21 handlers).
class ClearArea; class ComponentEntity; class ComponentDescription;
extern "C" ClearArea* ClearAreaCtrl_BuildWork(void* work, ComponentEntity* ownerEnt, ComponentDescription* desc,
                                              bool clearTrees, bool clearGround,
                                              bool clearBuildings);
