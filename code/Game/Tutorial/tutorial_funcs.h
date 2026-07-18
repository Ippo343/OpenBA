#pragma once
// Function declarations for deeply reimplemented Tutorial methods.
// Called from engine/reimpl/tutorial.cpp wrappers.

#include "Tutorial.h"
#include <cstdint>

// ============================================================
// Tutorial — tutorial definition container
// Vtable: 0x140160740 (9 slots)
// Extends Object::External (0x20 bytes base)
// Size: 0x40 bytes
// ============================================================

// Slot0 (vtable +0x00, 0x140026f50): Scalar deleting destructor.
// Sets vtable, frees steps array, calls External::~External, optionally frees self.
void* Tutorial_Dtor(void* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x140026e40): GetTypeInfo — returns DAT_1401c6408.
// Original is 8 bytes, unhookable.

// Slot2 (vtable +0x10, 0x140026e30): GetTypeId — returns TypeInfo_Tutorial.
// Original is 7 bytes, unhookable.

// Slot3 (vtable +0x18, 0x1400deb80): GetDescription — returns constant 2.
// Original is 6 bytes, unhookable.
inline int Tutorial_GetDescription(void* /*self*/) { return 2; }

// Slot4 (vtable +0x20, 0x14003a912): Serialize thunk — unhookable.

// Slot5 (vtable +0x28, 0x1400dead0): Serialize.
// Serializes step count, step external references, setup reference, and current step index.
void Tutorial_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30, 0x140026f00): Cleanup.
// Calls Dtor(self, 0) to clean up, re-initializes base + fields.
void* Tutorial_Cleanup(void* self);

// Slot7 (vtable +0x38, 0x140001640): Base dispatch — 8 bytes, unhookable.

// Slot8 (vtable +0x40, 0x140026eb0): Ctor.
// Calls External::External(self) to init base, sets vtable, zeroes fields.
void* Tutorial_Ctor(void* self);
