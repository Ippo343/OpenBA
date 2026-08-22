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
// LAYER 2 (2026-08-22) added the merchant-arrival half: aa60 (spawn) and b160 (teardown),
// plus the route-builder chain they were blocked on — 0x1400456e0 and its leaves 0x14000eb20
// (FindComponent(BoatComponent)), 0x140123880 (named attach point), 0x1400a7310 (nearest
// route point) and 0x14009ffc0 (one water A* leg), all in boatcomponent_route.cpp.
//
// LAYER 3 (2026-08-22) added the DOCKED state: a800 (enter) and a6d0 (update/leaving),
// plus the two leaves they were blocked on — 0x14005b580 (the merchant manifest
// generator) and 0x140059ad0 (auto-trade), both in tradecomponent_dock.cpp.
//
// Still absent, and still deliberately so — an unregistered address reports "NOT
// IMPLEMENTED" if anything reaches for it, which is the honest state:
//   0x14005a850                dock event-log line — needs the EventLog string chain
//   0x14005b240                dismiss event — retail inlines all of StateMachine::Tick
// See the header of tradecomponent_states.cpp for the full table.
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
extern "C" char  TradeCtrl_State2_Tick(void* self, void* sm, void* dt);     // 0x14005aa60
extern "C" char  TradeCtrl_Teardown(void* self);                            // 0x14005b160
extern "C" void* BoatRoute_FindBoatComponent(int64_t entity, void* outIBP);  // 0x14000eb20
extern "C" bool  BoatRoute_FindAttachPoint(void* comp, const void* name,
                                           float* outPos, float* outYaw);    // 0x140123880
extern "C" int   BoatRoute_NearestPointIndex(void* arr, const float* pt);     // 0x1400a7310
extern "C" int   BoatRoute_FindWaterPath(int64_t terrain, void* u2, unsigned from,
                                         uint64_t to, void* u5, int64_t out); // 0x14009ffc0
extern "C" void  BoatComponent_SetDestination(int64_t comp, int64_t* destIBP);// 0x1400456e0
// Layer 3 — the docked state.
extern "C" void  TradeComponent_BuildMerchantManifest(void* self);           // 0x14005b580
extern "C" void  TradeComponent_AutoTrade(void* self);                       // 0x140059ad0
extern "C" char  TradeCtrl_State13_Enter(void* self, void* sm);              // 0x14005a800
extern "C" char  TradeCtrl_State13_Update(void* self, void* sm);             // 0x14005a6d0

void RegisterTradeCtrlBatch() {
    RegisterExeAddr(0x14005abd0, reinterpret_cast<uintptr_t>(&TradeCtrl_State2_Enter));
    RegisterExeAddr(0x14005aa50, reinterpret_cast<uintptr_t>(&TradeCtrl_State12_Enter));
    RegisterExeAddr(0x14005a780, reinterpret_cast<uintptr_t>(&TradeCtrl_State13_Tick));
    RegisterExeAddr(0x14005a9c0, reinterpret_cast<uintptr_t>(&TradeCtrl_Event0B_MerchantLost));
    RegisterExeAddr(0x14005b450, reinterpret_cast<uintptr_t>(&TradeComponent_ArmMerchantTimer));
    RegisterExeAddr(0x14005b4f0, reinterpret_cast<uintptr_t>(&TradeComponent_PickMerchantTier));
    RegisterExeAddr(0x1400f6ea0, reinterpret_cast<uintptr_t>(&TradeWork_CtorFromEntity));
    // Layer 2 — merchant arrival.
    RegisterExeAddr(0x14005aa60, reinterpret_cast<uintptr_t>(&TradeCtrl_State2_Tick));
    RegisterExeAddr(0x14005b160, reinterpret_cast<uintptr_t>(&TradeCtrl_Teardown));
    RegisterExeAddr(0x14000eb20, reinterpret_cast<uintptr_t>(&BoatRoute_FindBoatComponent));
    RegisterExeAddr(0x140123880, reinterpret_cast<uintptr_t>(&BoatRoute_FindAttachPoint));
    RegisterExeAddr(0x1400a7310, reinterpret_cast<uintptr_t>(&BoatRoute_NearestPointIndex));
    RegisterExeAddr(0x14009ffc0, reinterpret_cast<uintptr_t>(&BoatRoute_FindWaterPath));
    RegisterExeAddr(0x1400456e0, reinterpret_cast<uintptr_t>(&BoatComponent_SetDestination));
    // Layer 3 — the docked state. b580 and 9ad0 have no caller in the image other than
    // a800/a6d0 (checked: their only non-data xrefs are 0x14005a809, 0x14005a831 and
    // 0x14005a6e8), so registering them buys nothing at runtime. They are registered
    // anyway so that the registry, not a source grep, is the authority on what is done.
    RegisterExeAddr(0x14005b580, reinterpret_cast<uintptr_t>(&TradeComponent_BuildMerchantManifest));
    RegisterExeAddr(0x140059ad0, reinterpret_cast<uintptr_t>(&TradeComponent_AutoTrade));
    RegisterExeAddr(0x14005a800, reinterpret_cast<uintptr_t>(&TradeCtrl_State13_Enter));
    RegisterExeAddr(0x14005a6d0, reinterpret_cast<uintptr_t>(&TradeCtrl_State13_Update));
}
