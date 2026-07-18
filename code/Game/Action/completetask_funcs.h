#pragma once
// CompleteTask function declarations — clean game logic layer.
// These functions are called from engine/reimpl/completetask.cpp hook wrappers.

#include <cstdint>

struct CompleteTask;

// CompleteTask::ScalarDtor — set vtables, call ~Internal, optionally free
void* CompleteTask_ScalarDtor_Impl(CompleteTask* self, uint32_t flags);

// CompleteTask::Serialize — serialize all CompleteTask fields
void CompleteTask_Serialize_Impl(CompleteTask* self, void* stream);

// CompleteTask::AbstractImpl — resolve task index, call task completion + cleanup
// Returns int (NOT bool) per bug class 7.
int CompleteTask_AbstractImpl_Impl(CompleteTask* self, void* actionStack);

// CompleteTask::CheckEntity — resolve task index, call task reassignment
void CompleteTask_CheckEntity_Impl(CompleteTask* self);

// CompleteTask::GetTarget — returns true if param matches +0x28 or parent of +0x20
bool CompleteTask_GetTarget_Impl(CompleteTask* self, void* param);

// Resolution helper
bool CompleteTask_ResolveFuncs();
