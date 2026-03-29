#pragma once
// Function declarations for deeply reimplemented FencedTool methods.
// Called from engine/reimpl/fencedtool.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

#include "FencedTool.h"
#include <cstdint>

// Dtor (vtable +0x00, Slot0): Scalar deleting destructor.
void* FencedTool_Dtor(FencedTool* self, uint32_t flags);

// Shutdown2 (vtable +0x28, Slot5): Removes fence preview from scene, destroys mesh.
void FencedTool_Shutdown2(FencedTool* self);

// OnKey (vtable +0x30, Slot6): Wraps DragTool::OnKey + rotation handling.
uint64_t FencedTool_OnKey(FencedTool* self, char key);

// Serialize (vtable +0x38, Slot7): Manages fence preview mesh in the scene.
// Original: 0x1400dcad0
void FencedTool_Serialize_Impl(FencedTool* self, void* param2, char param3);

// Shutdown (vtable +0x48, Slot9): Removes scene object, calls base DragTool::Shutdown.
void FencedTool_Shutdown(FencedTool* self);

// CalcCost (vtable +0x70, Slot14): Validates rectangular fence region.
// Returns 0=valid, 1=too small, 2=too large, 3=blocked.
// Original: 0x1400db490
int FencedTool_CalcCost_Impl(FencedTool* self, int startCell, int endCell,
                              uint32_t* outWidth, int* outHeight, int* outDepth,
                              int* outFenceSegments);

// Draw (vtable +0x40, Slot8): Renders fence placement preview.
// Two phases: edit-mode tiles + preview-mode tiles with terrain validation.
// Original: 0x1400dc440
void FencedTool_Draw_Impl(FencedTool* self, void* context, char param3);
