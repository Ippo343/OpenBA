#pragma once
// Function declarations for deeply reimplemented FencedTool methods.
// Called from engine/reimpl/fencedtool.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

#include "FencedTool.h"
#include <cstdint>

// Dtor (vtable +0x00): Scalar deleting destructor.
// Resets vtable chain (FencedTool -> DragTool -> Tool), calls Internal::~Internal,
// conditionally frees memory if flags & 1.
// Original: 0x140026930
void* FencedTool_Dtor(FencedTool* self, uint32_t flags);

// Shutdown2 (vtable +0x28, Slot5): Cleanup — removes scene object and destroys mesh instance.
// If sceneObjectId >= 0, removes from Graphics::Scene. Then calls mesh dtor if allocated.
// Original: 0x1400dccc0
void FencedTool_Shutdown2(FencedTool* self);

// OnKey (vtable +0x30, Slot6): Input handler wrapping DragTool::OnKey.
// Calls base DragTool::OnKey, then handles rotation via KeyMap Pressed(0, 0x13)
// and Pressed(0, 0x14). Rotates RotationSetting (+0x50) value +/- 1, wrapping 0..3.
// Returns 1 always.
// Original: 0x1400dcbe0
uint64_t FencedTool_OnKey(FencedTool* self, char key);

// Shutdown (vtable +0x48, Slot9): Removes scene object, resets ID to -1,
// then calls base DragTool::Shutdown.
// Original: 0x1400dcd10
void FencedTool_Shutdown(FencedTool* self);
