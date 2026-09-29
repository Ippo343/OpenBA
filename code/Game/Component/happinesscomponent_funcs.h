#pragma once
// Function declarations for deeply reimplemented HappinessComponent methods.
// Called from engine/reimpl/happinesscomponent.cpp wrappers.

#include "HappinessComponent.h"
#include <cstdint>

// ============================================================
// HappinessComponent -- citizen happiness tracking
// Vtable: 0x1401595d0
// Size: 0x50 bytes
// ============================================================


// The controller state table (retail 0x14004ac10) and its handlers are
// HappinessComponent members (HappinessComponent.h): states 2 and 13 "built" enter
// 0x14004a9f0 / per-frame 0x14004a9a0, state 3 "demolish" enter 0x14004a940.
