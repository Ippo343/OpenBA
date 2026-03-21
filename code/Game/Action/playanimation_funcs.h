#pragma once
// PlayAnimation function declarations -- clean game logic layer.
// These functions are called from engine/reimpl/playanimation.cpp hook wrappers.
// They must NOT depend on hooks.h -- they are portable for standalone exe.

#include <cstdint>

struct PlayAnimation;

// PlayAnimation::Serialize -- serialize all fields
void PlayAnimation_Serialize_Impl(void* self, void* stream);

// PlayAnimation::AbstractImpl (_purecall) -- find family component, set animation
// Returns int per bug class 7. RDX = actionStack (bug class 6).
int PlayAnimation_AbstractImpl_Impl(void* self, void* actionStack);

// PlayAnimation::Execute -- accumulate elapsed time, return while animating
// Returns int per bug class 7. RDX = actionStack, R8 = time (bug class 6).
int PlayAnimation_Execute_Impl(void* self, void* actionStack, void* time_ptr);

// PlayAnimation::CheckEntity -- 3 bytes (ret 0), unhookable but implemented for standalone exe
int PlayAnimation_CheckEntity_Impl(void* self);

// PlayAnimation::GetTarget -- returns true if param matches entity at +0x20
bool PlayAnimation_GetTarget_Impl(void* self, void* param);

// Resolution helper
bool PlayAnimation_ResolveFuncs();
