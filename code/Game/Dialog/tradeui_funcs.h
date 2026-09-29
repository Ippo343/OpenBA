#pragma once
// TradeUI C bodies (defined in tradeui.cpp). Split out of ui_panels.h (B.4 R0-EXE).

#include "Dialog/TradeUI.h"
#include <cstdint>

// Sub-functions (called by event handlers and Startup):
void TradeUI_UpdateBuyTotals(TradeUI* self);
void TradeUI_UpdateSellTotals(TradeUI* self);
void TradeUI_OnBuySpinner(TradeUI* self, int idx);
void TradeUI_OnSellSpinner(TradeUI* self, int idx);
void TradeUI_OnComboPurchase(TradeUI* self);
void TradeUI_OnComboOrder(TradeUI* self);
void TradeUI_OnOrderAmountSpinner(TradeUI* self, int idx);
void TradeUI_OnOrderSpinner(TradeUI* self, int idx, int idx2);
void TradeUI_OnDismissButton(TradeUI* self);
void TradeUI_OnPurchaseButton(TradeUI* self, int idx);
void TradeUI_OnStorageSpinner(TradeUI* self, int idx, void* reserved);
void TradeUI_RefreshDisplay(TradeUI* self, uint64_t p2, uint64_t p3, int64_t p4);
void TradeUI_RefreshSellTable(TradeUI* self);
void TradeUI_RefreshBuyTable(TradeUI* self);
void TradeUI_RefreshOrderQuantityDisplay(TradeUI* self);
void TradeUI_RefreshSellSpinnerValues(TradeUI* self);
void TradeUI_RefreshOrderDisplay(TradeUI* self);
