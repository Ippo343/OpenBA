#pragma once
// Declarations for Goal-module exe-helper reimplementations.
// See code/Game/Goal/goal_exe_helpers.cpp.

#include <cstdint>

// FUN_1400265b0 — StatGoal lazy ExternalPtr resolver. void fn(externalRef).
void StatGoal_ResolveExternalPtr(void* externalRef);

// FUN_14001b340 - ExternalPtr-triple array resize (stride 0x18). void fn(arr, count).
void Goal_RefArray_Resize(void* arr, uint32_t newCount);
