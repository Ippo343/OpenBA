#pragma once
// TradeComponent's array helpers shared by tradecomponent.cpp and
// tradecomponent_serialize.cpp.
//
// They are Collection::Array<T>::SetSize / destroy instances of the class's element
// types (TradeComponent.h). They stay the tree's bodies over the typed elements:
// Collection::Array<T>'s trait-derived spec would copy-construct and zero-fill
// differently from these retail bodies (0x14000e760 deep-copies the goods array and
// zeroes a whole new element; 0x14000e8d0 zeroes only +0x08 of a new buy order).

#include "TradeComponent.h"
#include <cstdint>

void TradeMerchantOrders_SetSize(TradeComponent& trade, int count);    // 0x14000e760
void TradeMerchantOrders_Destroy(TradeMerchantOrder* base, int count);  // 0x14000ef10
void TradeBuyOrders_SetSize(TradeComponent& trade, int count);          // 0x14000e8d0

// The AVL map ComponentDescription -> int (TradeResourceNode): the stock tree and
// 0x140059ad0's purchase tally (tradecomponent_serialize.cpp).
TradeResourceNode* TradeResourceTree_NewNode(ComponentDescription* key, int32_t value,
                                             TradeResourceNode* parent);   // MemAlloc(0x30, 8)
void TradeResourceTree_Retrace(TradeResourceNode** root, TradeResourceNode* start);   // 0x14000e360
