#pragma once
// BuildRoad function declarations — clean game logic layer.
// These functions are called from engine/reimpl/buildroad.cpp hook wrappers.
// They must NOT depend on hooks.h — they are portable for standalone exe.

#include <cstdint>

struct BuildRoad;

// BuildRoad::Ctor — initialize all fields
void* BuildRoad_Ctor_Impl(BuildRoad* self);

// BuildRoad::ScalarDtor — set vtables, call ~Internal, optionally free
void* BuildRoad_ScalarDtor_Impl(BuildRoad* self, uint32_t flags);

// BuildRoad::Serialize — serialize all BuildRoad fields
void BuildRoad_Serialize_Impl(BuildRoad* self, void* stream);

// BuildRoad::Execute — road construction work logic.
// Returns: 1 = not done / retry, 2 = action pushed / done
// Uses int return type (NOT bool) per bug class 7.
int BuildRoad_Execute(BuildRoad* self, void* entityIBP);

// BuildRoad::OnComplete — clear target entity ref, return (param4 == 1)
int BuildRoad_OnComplete_Impl(BuildRoad* self, void* param2, void* param3, int param4);

// BuildRoad::OnCancel — clear target entity ref
// Returns int per bug class 7.
int BuildRoad_OnCancel_Impl(BuildRoad* self);

// BuildRoad::Cleanup — remove pending work from target's task list
// Returns int per bug class 7.
int BuildRoad_Cleanup_Impl(BuildRoad* self);

// BuildRoad::Validate — check if assigned task still valid
int BuildRoad_Validate_Impl(BuildRoad* self, void* param2);

// BuildRoad::GetTarget — returns the road build target entity from gameWorld.
// Uses hidden return pointer ABI (member function): RCX=self, RDX=retbuf.
// NOT HOOKED — RIP-relative prologue (bug class 2), standalone exe only.
void* BuildRoad_GetTarget_Impl(BuildRoad* self, void* retbuf);

// BuildRoad::GetPriority — returns true (1) if field_48 is non-null.
// NOT HOOKED — only 10 bytes total, too small.
int BuildRoad_GetPriority_Impl(BuildRoad* self);

// Resolution helper
bool BuildRoad_ResolveFuncs();
