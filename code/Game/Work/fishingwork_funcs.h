#pragma once
// FishingWork function declarations — clean game logic layer.
// These functions are called from engine/reimpl/fishingwork.cpp hook wrappers.

#include <cstdint>

struct FishingWork;

// Serialize (vtable +0x18, 0x1400ea910):
// Complex serialization: Work base + IBP array with IsDerivedFrom + int array + retryCount.
void FishingWork_Serialize_Impl(FishingWork* self, void* stream);

// Slot4/Execute (vtable +0x20, 0x1400e9f20):
// Fishing dock work assignment — finds an available fishing spot, assigns a citizen
// to fish there, or retries with animation if no spot found.
// Returns: 1 = not ready/retry, 2 = action pushed
int FishingWork_Slot4_Impl(FishingWork* self, void* citizenIBP, void* time);

// Slot6 (vtable +0x30, 0x1400e9eb0):
// Clears citizen IBP slot and optionally marks spot state as done.
// param4 encoding: bits[3:0]=citizen slot, bit15=has spot, bits[7:4]=spot index.
// Returns: 0
int FishingWork_Slot6_Impl(FishingWork* self, void* param2, void* param3, uint32_t param4);

// Slot7 (vtable +0x38, 0x1400e9e80):
// Delegates to Slot6 (vtable +0x30) with copied parameters.
// Returns int per bug class 7.
int FishingWork_Slot7_Impl(FishingWork* self, void* param2, void* param3);

// Slot8/CancelAll (vtable +0x40, 0x1400ea830):
// Cancels all active fishing work. Iterates citizen slots and cancels
// their pending state machine actions.
// Returns int per bug class 7.
int FishingWork_Slot8_Impl(FishingWork* self);

// Slot9/Validate (vtable +0x48, 0x1400e9c30):
// Validates all fishing work assignments. Iterates citizen slots, checks each
// via Work::CanStartWork. Clears invalid citizens, marks orphaned spots for reset.
// Returns: 1 = all valid, 0 = some invalid
int FishingWork_Slot9_Impl(FishingWork* self, void* param2);

// Slot10/GetTarget (vtable +0x50, 0x1400ea8b0):
// Returns the building entity's position target IBP.
// Hidden return ptr pattern: returns param2.
void* FishingWork_Slot10_Impl(FishingWork* self, void* param2);

// Resolution helper — resolves all FishingWork-specific function pointers.
bool FishingWork_ResolveFuncs();
