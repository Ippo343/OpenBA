#pragma once
// Free helpers of ClearAreaComponent shared with other files.

#include "ClearAreaComponent.h"
#include <cstdint>

// ClearAreaComponent::FinishClear (0x140118270) for buildcomponent_state1.cpp
// (BuildComponent::State1Enter's construction-site reset); keeps the tree's null test.
void ClearArea_Finalize8270(ClearAreaComponent* comp);

// The two orchardcomponent_states.cpp reaches (its state 20 / 21 handlers).
class ClearArea; class ComponentEntity; class ComponentDescription;
extern "C" bool ClearAreaCtrl_IsDone(ClearAreaComponent* comp);   // keeps the tree's null test (true)
extern "C" ClearArea* ClearAreaCtrl_BuildWork(void* work, ComponentEntity* ownerEnt, ComponentDescription* desc,
                                              bool clearTrees, bool clearGround,
                                              bool clearBuildings);
