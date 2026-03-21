#pragma once
// PerformWork function declarations -- clean game logic layer.
// These functions are called from engine/reimpl/performwork.cpp hook wrappers.
// They must NOT depend on hooks.h -- they are portable for standalone exe.

#include <cstdint>

struct PerformWork;

// PerformWork::Serialize -- serialize all PerformWork fields
void PerformWork_Serialize_Impl(PerformWork* self, void* stream);

// PerformWork::AbstractImpl -- set animation, facing direction, optionally decrement tool durability
// Returns 1 always (action stays active, Execute checks completion).
// Uses int return type (NOT bool) per bug class 7.
int PerformWork_AbstractImpl_Impl(PerformWork* self, void* actionStack);

// PerformWork::Execute -- accumulate elapsed time, check completion
// Returns: 0 = done, 1 = still working, 2 = break/pause needed
// Uses int return type (NOT bool) per bug class 7.
int PerformWork_Execute_Impl(PerformWork* self, void* actionStack, void* deltaTime);

// PerformWork::Exit -- reset character animation to idle
void PerformWork_Exit_Impl(PerformWork* self, void* actionStack);

// PerformWork::CheckEntity -- reset character animation (same as Exit)
void PerformWork_CheckEntity_Impl(PerformWork* self);

// PerformWork::GetTarget -- returns true if param matches work target's parent entity
// NOT HOOKED -- too small prologue for some configurations, standalone exe only.
bool PerformWork_GetTarget_Impl(PerformWork* self, void* param);

// PerformWork::ScalarDtor -- set vtables, cleanup, call ~Internal, optionally free
void* PerformWork_ScalarDtor_Impl(PerformWork* self, uint32_t flags);

// PerformWork::BaseDtor (slot 2) -- returns TypeInfo
uint32_t PerformWork_BaseDtor_Impl();

// Resolution helper
bool PerformWork_ResolveFuncs();
