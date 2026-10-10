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
void TradeMerchantOrders_Destroy(TradeMerchantOrder* base, int count);  // 0x14000ef10  // B9: stays free -- measured: 0x14000ef10 is Array<TradeMerchantOrder>'s element destroy (RCX the array, unread: ~Array 0x14000e730 MOV RBX,RCX / CALL; RDX the first element, R8D the count), the inlined ~TradeMerchantOrder over its goods ~Array with NO null test (0x14000ef4c CALL MemFree unconditional) then ~String; spelled as the spec row's rtcoll::TDestroy<TradeMerchantOrder> over the implicit dtor, B11 DIFF (tradecomponent.o): the tree's ~Array adds the J1 `if (_data)` test and the goods spec's element-destroy dispatch (test + indirect call) -- the explicit loop stays
template <> void Collection::Array<TradeBuyOrder>::SetSize(int count);         // 0x14000e8d0 (the member, specialized: tradecomponent.cpp)

// The AVL map ComponentDescription -> int (TradeResourceNode): the stock tree and
// 0x140059ad0's purchase tally (tradecomponent_serialize.cpp).
TradeResourceNode* TradeResourceTree_NewNode(ComponentDescription* key, int32_t value,  // B9: stays free -- no retail body: retail inlines `new Node(key, value, parent)` at every insert, placement-new null test included (0x140059ede MemAlloc(0x30, 8) / 0x140059ee7 TEST RAX,RAX, and 0x140059f53 / 0x140059f5c; the 0x14000ebe0 load insert); the spelling retail has is a TradeResourceNode constructor, which the tree's aggregate node lacks (and the dock's out-of-line call per insert would become inline construction there)
                                             TradeResourceNode* parent);   // MemAlloc(0x30, 8)
