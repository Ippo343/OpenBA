#pragma once
// Function declarations for deeply reimplemented HealerComponent methods.
// Called from engine/reimpl/healercomponent.cpp wrappers.

#include "HealerComponent.h"
#include <cstdint>

// ============================================================
// HealerComponent -- healer building registration
// Vtable: 0x140159730
// Size: 0x48 bytes
// ============================================================



// Controller state-machine enters (retail table 0x14004b020):
//   state 2 "built"    0x14004ae30 -- append the component IBP to townMgr+0x70
//   state 3 "demolish" 0x14004adf0 -- erase it again
// (HealerComponent::StateBuiltEnter / StateDemolishEnter, HealerComponent.h)
