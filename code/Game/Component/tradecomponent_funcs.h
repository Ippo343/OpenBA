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

// Serialize (vtable slot [3], 0x14005bd50; Ghidra mislabels it "Shutdown"):
// nested serialization of the ComponentRef table, an AVL tree of
// ComponentRef->int, the trade-name array, the item table, a Time, two
// bools, and a validated InternalBasePtr. Ported in full in
// tradecomponent_serialize.cpp (TradeComponent_Serialize); NOT wired into
// the live vtable yet — see that file's header (same rationale as
// animalwave_serialize.cpp: no save/load driver exists yet).
extern "C" void TradeComponent_Serialize(TradeComponent* self, void* stream);

// Draw (vtable slot [6], 0x14005c1d0): Adds owning entity to world trade
// pending list, then calls TradeSetup to initialize trade state.
void TradeComponent_Draw(TradeComponent* self);

// OnCreate (vtable slot [7], 0x14005c190): Registers owning entity with
// the world trade entity list at worldMgr+0x30.
void TradeComponent_OnCreate(TradeComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void TradeComponent_InitFuncs();
