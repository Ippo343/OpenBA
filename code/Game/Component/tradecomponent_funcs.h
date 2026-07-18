#pragma once
// Function declarations for deeply reimplemented TradeComponent methods.
// Called from engine/reimpl/tradecomponent.cpp wrappers.

#include "TradeComponent.h"
#include <cstdint>

// ============================================================
// TradeComponent — largest ControllerComponent variant (0xa8 bytes)
// Vtable: 0x14015acd0
// Three non-trivial overrides: Shutdown, Draw, OnCreate
// ============================================================

// Shutdown (vtable slot [3], 0x14005bd50): Complex nested serialization
// of multiple arrays (storage items, trade goods, offers), string
// serialization, entity refs, Time, and version migration.
// Kept delegated to the original exe function due to extreme complexity (174 instructions).
// Declared here for API completeness.
// void TradeComponent_Shutdown(TradeComponent* self, void* stream);

// Draw (vtable slot [6], 0x14005c1d0): Adds owning entity to world trade
// pending list, then calls TradeSetup to initialize trade state.
void TradeComponent_Draw(TradeComponent* self);

// OnCreate (vtable slot [7], 0x14005c190): Registers owning entity with
// the world trade entity list at worldMgr+0x30.
void TradeComponent_OnCreate(TradeComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void TradeComponent_InitFuncs();
