#pragma once
// Function declarations for deeply reimplemented StorageComponent methods.
// Called from engine/reimpl/storagecomponent.cpp wrappers.

#include "StorageComponent.h"
#include <cstdint>

// ============================================================
// StorageComponent — storage slot management for buildings
// Vtable: 0x140164720
// Size: 0x40 bytes
// ============================================================

// QueryCount (0x14012cf10): Count total items in storage (O(n*m) perf hotspot).
// Iterates storage slots, for each slot iterates items checking type via IsDerivedFrom.
// Null-safe version — guards against missing RawMaterialDescription component.
int StorageComponent_QueryCount(void* storageComp);

// Helper_c560 (0x14012c560): Check if (param_1[0x3C] & RawMaterialDescription[0x5C]) != 0.
// Null-safe version — returns 0 if RawMaterialDescription component is missing.
uint32_t StorageComponent_Helper_c560(void* param_1, void* param_2);

// Init: resolve all internal function pointers (called once at startup)
