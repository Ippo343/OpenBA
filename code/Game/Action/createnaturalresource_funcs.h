#pragma once
// CreateNaturalResource function declarations -- clean game logic layer.
// Called from engine/reimpl/createnaturalresource.cpp hook wrappers.
// Must NOT depend on hooks.h -- portable for standalone exe.

#include <cstdint>

struct CreateNaturalResource;

// CreateNaturalResource::ScalarDtor -- set vtables, call ~Internal, optionally free
void* CreateNaturalResource_ScalarDtor_Impl(CreateNaturalResource* self, uint32_t flags);

// CreateNaturalResource::Serialize -- serialize status, typeRef, slot, entity
void CreateNaturalResource_Serialize_Impl(CreateNaturalResource* self, void* stream);

// CreateNaturalResource::AbstractImpl -- create natural resource entity in world
// Returns int (not bool) per bug class 7.
int CreateNaturalResource_AbstractImpl_Impl(CreateNaturalResource* self, void* actionStack);

// CreateNaturalResource::GetTarget -- returns true if param matches entity at +0x28
int CreateNaturalResource_GetTarget_Impl(CreateNaturalResource* self, void* param);

// Resolution helper
bool CreateNaturalResource_ResolveFuncs();
