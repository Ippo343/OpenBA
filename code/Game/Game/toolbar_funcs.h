#pragma once
// Function declarations for deeply reimplemented Toolbar methods.
// Called from engine/reimpl/toolbar.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

#include "Toolbar.h"
#include <cstdint>

// Dtor (vtable +0x00): Destroys Toolbar strings, calls External base dtor,
// conditionally frees memory.
void* Toolbar_Dtor(Toolbar* self, uint32_t flags);

// Slot5 (vtable +0x28): Serializes Toolbar fields to/from stream.
void Toolbar_Serialize(Toolbar* self, void* stream);

// Slot6 (vtable +0x30): Cleanup -- destroys then re-constructs in place.
void Toolbar_Cleanup(Toolbar* self);

// Init: Initializes External base, sets vtable and default field values.
// Called by Cleanup and Factory. Mirrors Toolbar_Ctor (0x1400d2130).
void Toolbar_Init(Toolbar* self);

// Slot8 (vtable +0x40): Construct-from-raw-memory pattern (null check + init).
// NOT HOOKED (RIP-relative JNZ unsafe for trampoline) but included for standalone exe.
void* Toolbar_Slot8(Toolbar* self);
