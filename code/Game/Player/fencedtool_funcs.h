#pragma once
// Function declarations for deeply reimplemented FencedTool methods.
// Called from engine/reimpl/fencedtool.cpp wrappers.

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

// Startup (vtable +0x20, Slot4): Initializes fenced tool components and mesh.
// Finds CreateFencedDescription, AreaDescription, FenceDescription, SizeDescription,
// resolves RotationSetting, allocates MeshInstance for fence preview.
// Original: 0x1400dcd50
void FencedTool_Startup_Impl(FencedTool* self);

// Draw (vtable +0x40, Slot8): Renders fence placement preview.
// Two phases: edit-mode tiles + preview-mode tiles with terrain validation.
// Original: 0x1400dc440
void FencedTool_Draw_Impl(FencedTool* self, void* context, char param3);

// CalcPath (vtable +0x68, Slot13): Creates fence entities along the drag rectangle.
// Builds TWO entities per placement: the main area entity (pasture/crop) and
// the fence entity (posts + side fences). Sets cell coordinates, rotation,
// transforms, and registers both with the entity list.
//
// Extremely complex (~150 lines decompiled): requires entity copy-ctor,
// component lookups, Math::Transform ops, Graphics::Geometry::SetTransform,
// linked list management for fence segments (FencedTool_Slot13_sub_bc80,
// FencedTool_Slot13_sub_b8c0), and entity registration (FUN_1400c48a0).
//
// Delegated to original exe function via resolved pointer — full reimplementation
// blocked on typing ~15 unresolved helper functions.
// Original: 0x1400dae00
// Signature: (self, startCell, endCell) -> uint64_t (low byte: 1=success, 0=failure)
uint64_t FencedTool_CalcPath_Impl(FencedTool* self, uint64_t startCell, uint64_t endCell);
