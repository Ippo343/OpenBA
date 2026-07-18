#pragma once
// AnimateNaturalResource function declarations — clean game logic layer.
// These functions are called from engine/reimpl/animatenaturalresource.cpp hook wrappers.

#include <cstdint>

struct AnimateNaturalResource;

// AnimateNaturalResource::ScalarDtor — set vtables, call ~Internal, optionally free
void* AnimateNaturalResource_ScalarDtor_Impl(AnimateNaturalResource* self, uint32_t flags);

// AnimateNaturalResource::Serialize — serialize status, entity ref, animIndex, waitForAnim
void AnimateNaturalResource_Serialize_Impl(AnimateNaturalResource* self, void* stream);

// AnimateNaturalResource::AbstractImpl — set animation on natural resource component
// Returns int (NOT bool) per bug class 7.
int AnimateNaturalResource_AbstractImpl_Impl(AnimateNaturalResource* self, void* actionStack);

// AnimateNaturalResource::Execute — check if model is still animating
// Returns int (NOT bool) per bug class 7.
int AnimateNaturalResource_Execute_Impl(AnimateNaturalResource* self, void* actionStack, void* time);

// AnimateNaturalResource::GetTarget — returns true if param matches entity at +0x20
bool AnimateNaturalResource_GetTarget_Impl(AnimateNaturalResource* self, void* param);

// AnimateNaturalResource::CheckEntity — 3 bytes, returns void (no-op)
// NOT HOOKED — too small. Standalone exe only.

// Resolution helper
bool AnimateNaturalResource_ResolveFuncs();
