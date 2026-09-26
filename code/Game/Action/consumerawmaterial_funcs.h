#pragma once
// ConsumeRawMaterial function declarations -- clean game logic layer.
// These functions are called from engine/reimpl/consumerawmaterial.cpp hook wrappers.

#include <cstdint>

class ConsumeRawMaterial;

// ConsumeRawMaterial::Serialize -- versioned serialization with entity ref + material list
void ConsumeRawMaterial_Serialize_Impl(ConsumeRawMaterial* self, void* stream);

// ConsumeRawMaterial::AbstractImpl -- consume materials or check availability
// Returns int per bug class 7. RDX = actionStack (bug class 6).
int ConsumeRawMaterial_AbstractImpl_Impl(ConsumeRawMaterial* self, void* actionStack);

// ConsumeRawMaterial::CheckEntity -- undo consumption when entity removed (consume mode only)
void ConsumeRawMaterial_CheckEntity_Impl(ConsumeRawMaterial* self);

// ConsumeRawMaterial::GetTarget -- simple compare with +0x20
bool ConsumeRawMaterial_GetTarget_Impl(ConsumeRawMaterial* self, void* param);

// Resolution helper
bool ConsumeRawMaterial_ResolveFuncs();
