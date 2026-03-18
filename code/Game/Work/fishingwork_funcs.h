#pragma once
// FishingWork function declarations — clean game logic layer.
// These functions are called from engine/reimpl/fishingwork.cpp hook wrappers.
// They must NOT depend on hooks.h — they are portable for standalone exe.

#include <cstdint>

struct FishingWork;

// Slot4/Execute (vtable +0x20, 0x1400e9f20):
// Fishing dock work assignment — finds an available fishing spot, assigns a citizen
// to fish there, or retries with animation if no spot found.
// Returns: 1 = not ready/retry, 2 = action pushed
int FishingWork_Slot4_Impl(FishingWork* self, void* citizenIBP, void* time);

// Slot9/Validate (vtable +0x48, 0x1400e9c30):
// Validates all fishing work assignments. Iterates citizen slots, checks each
// via Work::CanStartWork. Clears invalid citizens, marks orphaned spots for reset.
// Returns: 1 = all valid, 0 = some invalid
int FishingWork_Slot9_Impl(FishingWork* self, void* param2);

// Resolution helper — resolves all FishingWork-specific function pointers.
bool FishingWork_ResolveFuncs();
