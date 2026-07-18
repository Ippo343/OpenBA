#pragma once
// Function declarations for deeply reimplemented DiseaseComponent methods.
// Called from engine/reimpl/diseasecomponent.cpp wrappers.

#include "DiseaseComponent.h"
#include <cstdint>

// ============================================================
// DiseaseComponent -- disease simulation for citizens
// Vtable: 0x140159290
// Size: 0x58 bytes
// ============================================================

// Update (vtable slot [9], 0x140048b90): Disease dispatch thunk.
// If field30 (duration remaining) > 0, runs active disease logic.
// Otherwise, runs infection tick / seasonal roll logic.
void DiseaseComponent_Update(DiseaseComponent* self, int64_t* deltaTime);

// Init: resolve all internal function pointers (called once at startup)
void DiseaseComponent_InitFuncs();
