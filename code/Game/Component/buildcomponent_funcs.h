#pragma once
// Function declarations for deeply reimplemented BuildComponent methods.
// Called from engine/reimpl/buildcomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

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

// Shutdown (vtable slot [3], 0x140108D80): Serializes BuildComponent fields.
// Component_Serialize base call, then version 0 empty block, then version 1
// block with buildState, buildProgress, buildFlag, two InternalBasePtr arrays
// with IsDerivedFrom validation.
void BuildComponent_Shutdown(BuildComponent* self, void* stream);

// Draw (vtable slot [6], 0x140108D40): Complex worker assignment logic.
// Checks game flags, iterates multi-object children, finds BuildComponents
// and StorageComponents, manages worker assignments and draw calls.
// NOTE: This hook is DISABLED in hooks.cpp due to RIP-relative prologue.
// Included here for standalone exe completeness.
void BuildComponent_Draw(BuildComponent* self);

// OnCreate (vtable slot [7], 0x140108CB0): Removes component from removal queue.
void BuildComponent_OnCreate(BuildComponent* self);
