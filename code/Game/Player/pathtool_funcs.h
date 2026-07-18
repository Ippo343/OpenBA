#pragma once
// Function declarations for deeply reimplemented PathTool methods.
// Called from engine/reimpl/pathtool.cpp wrappers.

#include "PathTool.h"
#include <cstdint>

// Dtor (vtable +0x00, Slot0): Scalar deleting destructor.
// Resets vtable to PathTool -> Tool, calls Object::Internal::~Internal,
// conditionally frees memory if flags & 1.
// Original: 0x140026b70
void* PathTool_Dtor(PathTool* self, uint32_t flags);

// OnKey (vtable +0x30, Slot6): Path creation input handler (multi-point, NOT drag-based).
// On key-down with Pressed(0,5): ray-picks map cell under cursor, resolves entity at cell,
// highlights/selects the entity for path placement by calling helper functions.
// Always returns 1 (event consumed).
// Original: 0x1400de6b0
uint64_t PathTool_OnKey(PathTool* self, char key);

// Draw (vtable +0x40, Slot8): Renders path preview overlay.
// Iterates path points (self+0x40/+0x48) and line segments (self+0x50/+0x58),
// builds per-cell render entries with position, color, and type info, then
// submits them to the debug draw system.
// Original: 0x1400dd3b0
void PathTool_Draw(PathTool* self, void* context);

// Serialize (vtable +0x50, Slot10): Save/load comparison.
// Calls CheckPtr on self->selectedEntity (+0x28), then compares *param_2
// to the selected entity value. Returns true if they match.
// Original: 0x1400de8d0
bool PathTool_Serialize(PathTool* self, uint64_t* param2);

// --- Unhookable slots (for standalone exe completeness) ---

// Slot4 (vtable +0x20): Empty function (RET). Original: 0x1400de940
// void PathTool_Slot4(PathTool* self);

// Slot5 (vtable +0x28): JMP to thunk_helper_d250 (cleanup entity highlighting).
// Original: 0x1400de930 -> 0x1400dd250

// Slot7 (vtable +0x38): Empty function (RET). Original: 0x1400dd700

// Slot9 (vtable +0x48): Returns false. Original: 0x1400de920

// Slot12 (vtable +0x60): Returns 4 (tool type ID). Original: 0x1400de910
