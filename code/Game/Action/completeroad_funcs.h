#pragma once
// CompleteRoad function declarations -- clean game logic layer.
// These functions are called from engine/reimpl/completeroad.cpp hook wrappers.

#include <cstdint>

struct CompleteRoad;

// CompleteRoad::Serialize -- serialize all fields
void CompleteRoad_Serialize_Impl(void* self, void* stream);

// CompleteRoad::AbstractImpl (_purecall) -- complete road tile via terrain system
// Returns int per bug class 7. RDX = actionStack (bug class 6).
int CompleteRoad_AbstractImpl_Impl(void* self, void* actionStack);

// Resolution helper
bool CompleteRoad_ResolveFuncs();
