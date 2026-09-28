#pragma once
// Function declarations for deeply reimplemented BuildComponent methods.
// Called from engine/reimpl/buildcomponent.cpp wrappers.

#include "BuildComponent.h"
#include <cstdint>

// ============================================================
// BuildComponent — construction management component
// Vtable: 0x140162260 (12 slots)
// Size: 0x78 bytes
// ============================================================

// Serialize (vtable slot [4], 0x140108D60): 3-byte RET 0 no-op.
// Original is literally "xor eax,eax; ret" — does nothing.
inline void BuildComponent_Serialize(BuildComponent* /*self*/, void* /*stream*/) {}

// OnUpdate2 (vtable slot [11], 0x140108D70): 3-byte RET 0 no-op.
inline void BuildComponent_OnUpdate2(BuildComponent* /*self*/) {}


