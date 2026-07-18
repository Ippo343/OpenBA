#pragma once
// Function declarations for deeply reimplemented CropFieldComponent methods.
// Called from engine/reimpl/cropfieldcomponent.cpp wrappers.

#include "CropFieldComponent.h"
#include <cstdint>

// ============================================================
// CropFieldComponent — crop field entity management
// Vtable: 0x140162ec0
// Size: 0x68 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14011ae70): Complex entity ref serialization.
// Component_Serialize + empty serializer + second serializer with entity ref
// validation, cropFlags as 2 bools, InternalBasePtr array at +0x50,
// and counted entity list at +0x58/+0x60.
void CropFieldComponent_Shutdown(CropFieldComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void CropFieldComponent_InitFuncs();
