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

template <> void TradeComponent::MerchantOrderArray::SetSize(int count);   // 0x14000e760 (the member, specialized: tradecomponent.cpp)
void TradeMerchantOrders_Destroy(TradeMerchantOrder* base, int count);  // 0x14000ef10  // B8: stays free -- passed as a function pointer at tradecomponent.cpp:134 (rtcoll::DestroyAs<TradeMerchantOrder, &TradeMerchantOrders_Destroy>, the spec row's destroy); retail 0x14000ef10 takes the first element in RCX
template <> void Collection::Array<TradeBuyOrder>::SetSize(int count);         // 0x14000e8d0 (the member, specialized: tradecomponent.cpp)

// The AVL map ComponentDescription -> int (TradeResourceNode): the stock tree and
// 0x140059ad0's purchase tally (tradecomponent_serialize.cpp).
TradeResourceNode* TradeResourceTree_NewNode(ComponentDescription* key, int32_t value,  // B8: stays free -- not a ComponentDescription body: the TradeResourceNode allocation (MemAlloc(0x30) + fields), inlined in retail (0x140059ad0 is the purchase-tally body, not this); TradeResourceNode is COMP-B's
                                             TradeResourceNode* parent);   // MemAlloc(0x30, 8)
void TradeResourceTree_Retrace(TradeResourceNode** root, TradeResourceNode* start);   // 0x14000e360 B8: stays free -- read, not moved: its RCX is a root SLOT, not one class's tree: the stock tree passes &stock.root (TradeStockTree), tradecomponent_dock.cpp's AvlInsert a bare local `TradeResourceNode* treeRoot` (retail inlines that retrace at 0x140059f90 / 0x14005a081); AvlTreeHead<Node> (Action/PathAvl.h) has no retrace and the body cannot move to a header
