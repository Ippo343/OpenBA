#pragma once
// MoveTo function declarations -- clean game logic layer.
// These functions are called from engine/reimpl/moveto.cpp hook wrappers.
// They must NOT depend on hooks.h -- they are portable for standalone exe.

#include <cstdint>

struct MoveTo;

// MoveTo::Serialize -- versioned serialization of all path/movement data
void MoveTo_Serialize_Impl(void* self, void* stream);

// MoveTo::AbstractImpl -- distance check, path setup via A*
// Returns int per bug class 7: 0=arrived, 1=moving, 2=no path
int MoveTo_AbstractImpl_Impl(void* self, void* actionStack);

// MoveTo::Execute -- A* path traversal, terrain speed, animation
// Returns int per bug class 7: 0=arrived, 1=moving, 2=no path
// DISABLED HOOK (bug class 3: MOV RAX,RSP) -- for standalone exe only
int MoveTo_Execute_Impl(void* self, void* actionStack, void* time);

// MoveTo::Enter -- bridge detection, road flag setup
void MoveTo_Enter_Impl(void* self);

// MoveTo::Exit -- reset character animation to idle
void MoveTo_Exit_Impl(void* self, void* actionStack);

// MoveTo::CheckEntity -- reset character animation (same as Exit body)
void MoveTo_CheckEntity_Impl(void* self);

// MoveTo::GetTarget -- compare entity->+0x10 with param
bool MoveTo_GetTarget_Impl(void* self, void* param);

// Resolution helper -- resolve all MoveTo-local function pointers
bool MoveTo_ResolveFuncs();
