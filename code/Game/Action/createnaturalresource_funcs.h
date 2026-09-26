#pragma once
// CreateNaturalResource function declarations -- clean game logic layer.
// Called from engine/reimpl/createnaturalresource.cpp hook wrappers.

#include <cstdint>

class CreateNaturalResource;

// CreateNaturalResource::Serialize -- serialize status, typeRef, slot, entity
void CreateNaturalResource_Serialize_Impl(CreateNaturalResource* self, void* stream);

// CreateNaturalResource::AbstractImpl -- create natural resource entity in world
// Returns int (not bool) per bug class 7.
int CreateNaturalResource_AbstractImpl_Impl(CreateNaturalResource* self, void* actionStack);

// CreateNaturalResource::GetTarget -- returns true if param matches entity at +0x28
int CreateNaturalResource_GetTarget_Impl(CreateNaturalResource* self, void* param);

// Resolution helper
bool CreateNaturalResource_ResolveFuncs();
