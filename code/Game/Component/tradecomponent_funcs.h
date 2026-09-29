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

// Init: resolve all internal function pointers (called once at startup)
