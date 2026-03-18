#pragma once
// Function declarations for deeply reimplemented FishingWork methods.
// Called from engine/reimpl/fishingwork.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "FishingWork.h"
#include <cstdint>

// Serialize (vtable +0x18, 0x1400ea910):
// Work_Serialize + Serializer(v0) + serialize IBP array at +0x48/+0x50,
// with IsDerivedFrom validation per element, then int array at +0x58/+0x60,
// then int at +0x68.
void FishingWork_Serialize_Impl(FishingWork* self, void* stream);

// OnComplete (vtable +0x30, slot 6, 0x1400e9eb0):
// Clears citizen ref at index (param4 & 0xf), optionally marks spot as done.
// Returns 0.
int FishingWork_OnComplete_Impl(FishingWork* self, void* param2, void* param3, uint32_t param4);

// OnCancel (vtable +0x38, slot 7, 0x1400e9e80):
// Delegates to OnComplete (slot 6) via vtable call.
// NOTE: Must forward all 4 params (bug class 6 — implicit R9 forwarding to slot6).
void FishingWork_OnCancel_Impl(FishingWork* self, void* param2, void* param3, uint32_t param4);

// Cleanup (vtable +0x40, slot 8, 0x1400ea830):
// For each citizen ref, walks entity -> StateMachineComponent -> task list cleanup.
void FishingWork_Cleanup_Impl(FishingWork* self);

// GetTarget (vtable +0x50, slot 10, 0x1400ea8b0):
// Returns workplace entity's position component via hidden return ptr.
// Uses MSVC member function ABI: RCX=this, RDX=outPtr.
void* FishingWork_GetTarget_Impl(FishingWork* self, void* outPtr);

// Resolve all local function pointers. Called once from Install.
bool FishingWork_ResolveFuncs();
