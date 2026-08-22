// standalone/reg_market_typeid.cpp
// Storage + fill for the two MarketComponent / MarketDescription type-id .rdata globals.
//
//   0x1401c44f0  MarketComponent    (writer MarketComponent_RegisterType 0x14014b0c0,
//                standalone/factory_registrations.csv:34:
//                `14014b0c0,MarketComponent_RegisterType,&DAT_1401c44f0,
//                 "MarketComponent","ResourceProduceComponent",...`)
//   0x1401c4528  MarketDescription  (writer MarketDescription_Register 0x14014b100,
//                factory_registrations.csv:35)
//
// Both slots were registered NOWHERE — they existed only as CSV rows — so
// ResolveAddr(Exe, ...) returned 0 for each. This is the exact failure mode documented
// in reg_orchard_typeid.cpp and reference_null_typeid_global_fakes_ordering_bug: an
// unregistered type-id slot is not a benign default, it is a silent PERMANENT miss.
//
// Why it matters here: MarketWork's vtable slot 4 (retail 0x1400eff80, the market
// hauling planner) scans the market entity for its MarketComponent by comparing each
// component's GetTypeId against *DAT_1401c44f0, then narrows the description with
// IsDerivedFrom against *DAT_1401c4528. With both reading 0 the scan matches nothing,
// marketDesc stays null, and retail's UNGUARDED `fill >= marketDesc+0x20` compare is a
// null deref. Stages 2 and 4 of that port read further type-ids from the same
// neighbourhood and would hit the identical wall.
//
// ⚠ Registering the slot is only half the job — guard the VALUE, not the pointer. A
// registered slot still holding 0 is indistinguishable from an unregistered one at every
// call site (reference_registered_but_filled_with_zero), which is why the fill below
// reports the outcome in BOTH arms rather than staying silent on success.
//
// Two-phase, matching reg_orchard_typeid.cpp / reg_tracker_typeid.cpp exactly:
//   RegisterMarketTypeIds()     — from AddressRegistryInit, BEFORE EngineFuncsInit, so
//                                 any engine_funcs global points at our storage.
//   FillTypeIdGlobals_market()  — from game_main.cpp AFTER Object::Factory::Startup,
//                                 the only point at which the real CRC ids exist.
//                                 Filling earlier stores the 0xFFFFFFFF sentinel.
//
// NOT env-gated, for the same reason as the orchard and tracker slots.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>

static uint32_t g_tid_MarketComponent   = 0;  // 0x1401c44f0
static uint32_t g_tid_MarketDescription = 0;  // 0x1401c4528

// 0x1401c7ee8 StorageLocationComponent (writer StorageLocationComponent_RegisterType
// 0x14014fb90, factory_registrations.csv:299). Also registered nowhere. MarketWork slot 4
// stage 5 reads it to find the winning building's adjacent drop cell, and the already-ported
// 0x14008d590 reads the same constant at 0x14008d764.
// ⚠ code/Game/Action/action_pf_roots_route.cpp:48 calls this slot "route/path comp (?)" —
// that guess is WRONG; the CSV writer names the class. Left that comment alone rather than
// edit an unrelated file, but do not trust it.
static uint32_t g_tid_StorageLocationComponent = 0;  // 0x1401c7ee8

extern "C" void FillTypeIdGlobals_market() {
    g_tid_MarketComponent   = ::Object::GetTypeIdByName("MarketComponent");
    g_tid_MarketDescription = ::Object::GetTypeIdByName("MarketDescription");
    g_tid_StorageLocationComponent = ::Object::GetTypeIdByName("StorageLocationComponent");

    if (g_tid_MarketComponent == 0)
        std::fprintf(stderr, "[MarketTypeId] ERROR: GetTypeIdByName(\"MarketComponent\") "
                             "returned 0 — slot 0x1401c44f0 is live but EMPTY; MarketWork "
                             "slot 4 will find no MarketComponent on any market\n");
    else
        std::fprintf(stderr, "[MarketTypeId] 0x1401c44f0 = %u (0x%08X)\n",
                     g_tid_MarketComponent, g_tid_MarketComponent);

    if (g_tid_MarketDescription == 0)
        std::fprintf(stderr, "[MarketTypeId] ERROR: GetTypeIdByName(\"MarketDescription\") "
                             "returned 0 — slot 0x1401c4528 is live but EMPTY; the "
                             "IsDerivedFrom narrowing in MarketWork slot 4 will never match\n");
    else
        std::fprintf(stderr, "[MarketTypeId] 0x1401c4528 = %u (0x%08X)\n",
                     g_tid_MarketDescription, g_tid_MarketDescription);

    if (g_tid_StorageLocationComponent == 0)
        std::fprintf(stderr, "[MarketTypeId] ERROR: GetTypeIdByName(\"StorageLocationComponent\") "
                             "returned 0 — slot 0x1401c7ee8 is live but EMPTY; MarketWork slot 4 "
                             "stage 5 will not find a drop cell\n");
    else
        std::fprintf(stderr, "[MarketTypeId] 0x1401c7ee8 = %u (0x%08X)\n",
                     g_tid_StorageLocationComponent, g_tid_StorageLocationComponent);

    std::fflush(stderr);
}

void RegisterMarketTypeIds() {
    RegisterExeAddr(0x1401c44f0, reinterpret_cast<uintptr_t>(&g_tid_MarketComponent));
    RegisterExeAddr(0x1401c4528, reinterpret_cast<uintptr_t>(&g_tid_MarketDescription));
    RegisterExeAddr(0x1401c7ee8, reinterpret_cast<uintptr_t>(&g_tid_StorageLocationComponent));
}
