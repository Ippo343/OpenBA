#pragma once
// ShowTool function declarations -- clean game logic layer.
// These functions are called from engine/reimpl/showtool.cpp hook wrappers.
// They must NOT depend on hooks.h -- they are portable for standalone exe.

#include <cstdint>

struct ShowTool;

// ShowTool::Serialize -- serialize all fields
void ShowTool_Serialize_Impl(void* self, void* stream);

// ShowTool::AbstractImpl (_purecall) -- show/hide tool based on toolId
// Returns int per bug class 7. RDX = actionStack (bug class 6).
int ShowTool_AbstractImpl_Impl(void* self, void* actionStack);

// ShowTool::Exit -- 3 bytes (ret 0), unhookable but implemented for standalone exe
int ShowTool_Exit_Impl(void* self);

// ShowTool::CheckEntity -- hides tool on entity at +0x20
void ShowTool_CheckEntity_Impl(void* self);

// ShowTool::GetTarget -- returns true if entity's parent matches param
bool ShowTool_GetTarget_Impl(void* self, void* param);

// Resolution helper
bool ShowTool_ResolveFuncs();
