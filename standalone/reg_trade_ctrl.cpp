// standalone/reg_trade_ctrl.cpp
// TradeComponent controller wave, layer 1 — registers the state-machine bodies ported
// in code/Game/Component/tradecomponent_states.cpp.
//
// The retail table is built by TradeDescription_Setup -> 0x14005c650; our equivalent is
// TradeCtrl_PopulateTable(), which is NOT registered at 0x14005c650. c650 appends into a
// shared exe .data staging table (DAT_1401d0de0 / count DAT_1401d1d60) that standalone
// does not map, so registering our table builder at that address would hand any other
// caller of c650 an object with a different layout. The table is installed per component
// instance from TradeComponent_InstallController instead.
//
// Only the four ported handlers and their three leaves are registered. The six handlers
// that are still blocked (aa60, b160, a800, a6d0, a850, b240) are deliberately absent:
// an unregistered address reports "NOT IMPLEMENTED" if anything reaches for it, which is
// the honest state. See the header of tradecomponent_states.cpp for what blocks each.
//
// Registration is not evidence any of these run — the whole controller is behind
// BANISHED_LIVE_TRADE_CTRL. See reference_ported_registered_but_unwired.
#include "resolve.h"
#include <cstdint>

extern "C" char  TradeCtrl_State2_Enter(void* self, void* sm);              // 0x14005abd0
extern "C" char  TradeCtrl_State12_Enter(void* self, void* sm);             // 0x14005aa50
extern "C" char  TradeCtrl_State13_Tick(void* self, void* sm, void* dt);    // 0x14005a780
extern "C" char  TradeCtrl_Event0B_MerchantLost(void* self, void* sm);      // 0x14005a9c0
extern "C" void  TradeComponent_ArmMerchantTimer(void* self);               // 0x14005b450
extern "C" int   TradeComponent_PickMerchantTier(void* self);               // 0x14005b4f0
extern "C" void* TradeWork_CtorFromEntity(void* mem, void* entityIBP);      // 0x1400f6ea0

void RegisterTradeCtrlBatch() {
    RegisterExeAddr(0x14005abd0, reinterpret_cast<uintptr_t>(&TradeCtrl_State2_Enter));
    RegisterExeAddr(0x14005aa50, reinterpret_cast<uintptr_t>(&TradeCtrl_State12_Enter));
    RegisterExeAddr(0x14005a780, reinterpret_cast<uintptr_t>(&TradeCtrl_State13_Tick));
    RegisterExeAddr(0x14005a9c0, reinterpret_cast<uintptr_t>(&TradeCtrl_Event0B_MerchantLost));
    RegisterExeAddr(0x14005b450, reinterpret_cast<uintptr_t>(&TradeComponent_ArmMerchantTimer));
    RegisterExeAddr(0x14005b4f0, reinterpret_cast<uintptr_t>(&TradeComponent_PickMerchantTier));
    RegisterExeAddr(0x1400f6ea0, reinterpret_cast<uintptr_t>(&TradeWork_CtorFromEntity));
}
