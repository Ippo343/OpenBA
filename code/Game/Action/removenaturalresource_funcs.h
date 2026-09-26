#pragma once
// RemoveNaturalResource function declarations — clean game logic layer.
// These functions are called from engine/reimpl/removenaturalresource.cpp hook wrappers.

#include <cstdint>

class RemoveNaturalResource;

// RemoveNaturalResource::BaseDtor — returns TypeInfo (slot 2)
uint32_t RemoveNaturalResource_BaseDtor_Impl();

// RemoveNaturalResource::Serialize — serialize status, entity refs, harvest flag
void RemoveNaturalResource_Serialize_Impl(RemoveNaturalResource* self, void* stream);

// RemoveNaturalResource::AbstractImpl — checks resource ownership, sets harvest flags, removes resource
// Returns int per bug class 7.
int RemoveNaturalResource_AbstractImpl_Impl(RemoveNaturalResource* self, void* actionStack);

// RemoveNaturalResource::CheckEntity — clears "in use" bit on model component at +0x28
void RemoveNaturalResource_CheckEntity_Impl(RemoveNaturalResource* self);

// RemoveNaturalResource::GetTarget — returns true if param matches entity at +0x28 or parent at +0x20
bool RemoveNaturalResource_GetTarget_Impl(RemoveNaturalResource* self, void* param);

// Resolution helper
bool RemoveNaturalResource_ResolveFuncs();
