#pragma once
// Function declarations for deeply reimplemented MarketComponent methods.
// Called from engine/reimpl/marketcomponent.cpp wrappers.

#include "MarketComponent.h"
#include <cstdint>

// ============================================================
// MarketComponent — ResourceProduceComponent subclass (0x50 bytes)
// Vtable: 0x140159c40
// Only one non-trivial override: Shutdown
// ============================================================

// Shutdown (vtable slot [3], 0x14004ead0): Calls parent
// ResourceProduceComponent_Shutdown, then creates/destroys an empty
// Serializer block (version 0).
void MarketComponent_Shutdown(MarketComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void MarketComponent_InitFuncs();
