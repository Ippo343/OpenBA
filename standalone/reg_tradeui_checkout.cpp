// standalone/reg_tradeui_checkout.cpp
// Wave D — registration for the TradeUI checkout ported in
// code/Game/Dialog/tradeui_checkout.cpp.
//
// Registering 0x140081420 is what makes engine_funcs' fn_TradeUI_OnTradeButton
// non-null, which lets TradeUI_OnRemove's buttonTrade branch dispatch the real
// handler instead of bailing out.
//
// NOTE: registration is NOT the same as enablement here. The handler is opt-in
// behind BANISHED_LIVE_TRADE_CHECKOUT and commits nothing while that is unset.
// "Registered" has repeatedly been mistaken for "runs" in this tree; it means
// only that the address resolves.
//
// 0x14005ace0 (the livestock spawn) WAS the one genuinely unported dependency
// that kept this whole wave opt-in. It is now ported in
// code/Game/Component/tradecomponent_livestock.cpp and registered below, so
// fn_Building_AddBuyToStorage resolves and a livestock purchase puts animals in
// the world instead of committing the order and warning on stderr.
//
// Wired via RegisterTradeUICheckoutBatch() from RegisterCodeFunctions() in
// standalone/address_registry.cpp.

#include "resolve.h"
#include <cstdint>

class TradeUI;

extern "C" void TradeUI_OnTradeButton(TradeUI* self, void* p2, void* p3, int64_t p4); // 0x140081420
extern "C" void TradeComponent_RemoveBuyOrder(void* tradeComp, void* resourceEntity,
                                              int qty);                                // 0x14005baa0
extern "C" void TradeDescription_ExtendMerchantStay(void* tradeComp);                   // 0x14005a490
extern "C" void TradeComponent_SpawnPurchasedLivestock(void* tradeComp, void* tmpl,
                                                       unsigned int qty);               // 0x14005ace0

void RegisterTradeUICheckoutBatch() {
    RegisterExeAddr(0x140081420, reinterpret_cast<uintptr_t>(&TradeUI_OnTradeButton));

    // The livestock arm of the checkout. Registering this is what makes
    // fn_Building_AddBuyToStorage non-null.
    RegisterExeAddr(0x14005ace0,
                    reinterpret_cast<uintptr_t>(&TradeComponent_SpawnPurchasedLivestock));

    // Both leaves are registered as well as ported: each is a real exe function
    // with callers outside the checkout (0x14005baa0 is the generic buy-order
    // retire, 0x14005a490 the merchant-stay bump), so leaving them unregistered
    // would hand those callers a null once they are wired.
    RegisterExeAddr(0x14005a490, reinterpret_cast<uintptr_t>(&TradeDescription_ExtendMerchantStay));
}
