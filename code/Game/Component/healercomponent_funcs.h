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

// Doors onto HealerComponent::Persist / ::OnRemove, kept for their by-address
// rows (B.3.8).
// Shutdown (vtable slot [3], 0x14004aeb0): Component_Serialize + two empty
// serializer blocks (component section + controller section).
void HealerComponent_Shutdown(HealerComponent* self, void* stream);

// OnRemove (vtable slot [7], 0x14004ae70): UNregisters the COMPONENT from the
// TownManager healer list at *(GameWorld+0xF0)+0x70. Ghidra labels the slot
// OnCreate; the body is an erase. The matching REGISTER is the state-2 enter.
void HealerComponent_OnRemove(HealerComponent* self);

// Controller state-machine enters (retail table 0x14004b020):
//   state 2 "built"    0x14004ae30 -- append the component IBP to townMgr+0x70
//   state 3 "demolish" 0x14004adf0 -- erase it again
extern "C" char HealerComponent_EnterBuilt(void* comp, void* sm);
extern "C" char HealerComponent_EnterDemolish(void* comp, void* sm);
