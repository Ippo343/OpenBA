#pragma once
// Function declarations for deeply reimplemented DragTool methods.
// Called from engine/reimpl/dragtool.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

#include "Tool.h"
#include <cstdint>

// Dtor (vtable +0x00): Scalar deleting destructor.
// Resets vtable chain DragTool->Tool, calls Object::Internal::~Internal,
// conditionally frees memory.
// Currently delegates to trampoline (needs Object::Internal::~Internal fn ptr).
void* DragTool_Dtor(DragTool* self, uint32_t flags);

// OnKey (vtable +0x30, Slot6): Main input handler for click-and-drag tools.
// Handles the full drag lifecycle: idle -> drag start -> dragging -> commit/cancel.
// Returns 1 always (event consumed).
uint64_t DragTool_OnKey(DragTool* self, char key);
