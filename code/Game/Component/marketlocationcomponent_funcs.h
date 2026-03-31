#pragma once
// Function declarations for deeply reimplemented MarketLocationComponent methods.
// Called from engine/reimpl/marketlocationcomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "MarketLocationComponent.h"
#include <cstdint>

// ============================================================
// MarketLocationComponent — ControllerComponent subclass (0x48 bytes)
// Vtable: 0x140159dd0
// Two non-trivial overrides: Shutdown, OnCreate
// ============================================================

// Shutdown (vtable slot [3], 0x14004efa0): Component_Serialize + two empty
// Serializer blocks (version 0 each).
void MarketLocationComponent_Shutdown(MarketLocationComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x14004ef60): Registers owning entity with
// the game world's market location entity list at worldMgr+0x10.
void MarketLocationComponent_OnCreate(MarketLocationComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void MarketLocationComponent_InitFuncs();
