#pragma once
// TradeUI C bodies (defined in tradeui.cpp). Split out of ui_panels.h (B.4 R0-EXE).
//
// B.4 DLG-e: the bodies are TradeUI members now (Dialog/TradeUI.h). What is left
// here are the B4 FORWARD spellings callers outside the block still name.

#include "Dialog/TradeUI.h"
#include <cstdint>

// B4 FORWARD: standalone/trade_buy_harness.cpp
void TradeUI_UpdateBuyTotals(TradeUI* self);
void TradeUI_UpdateSellTotals(TradeUI* self);
void TradeUI_RefreshSellTable(TradeUI* self);
void TradeUI_RefreshBuyTable(TradeUI* self);
