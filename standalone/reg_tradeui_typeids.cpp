// standalone/reg_tradeui_typeids.cpp
// Registers the ONE .rdata type-id slot the whole TradeUI panel turns on:
//
//     DWORD_1401c4b80  =  Object::BaseTypeInfo("TradeComponent")
//
// ── Why this file exists ─────────────────────────────────────────────────────
// `fn_FindComponent_Building` (engine_funcs.h) resolves this slot through
// `g_TypeId_TradeComponent_FC`, which engine_funcs.cpp:2265 fills from
// `ResolveAddr(Module::Exe, 0x1401c4b80)`. That address was never registered, so the
// resolve returned 0, the global stayed NULL, and the finder has been returning
// "found nothing" on EVERY call for the life of the project.
//
// That is what crashed the panel the moment the TradeUI gate opened (2026-07-31):
// `tradeui.cpp:1057` read `*(int*)(buildingPtr[0] + 0x28)` on a null result —
// `ACCESS_VIOLATION, READ of 0x28`, matching the fault address exactly.
//
// ⚠ The crash location is a red herring and line 1057 is not special. There are TEN
// unguarded derefs of this finder's result in tradeui.cpp alone (105, 132, 344, 630,
// 1010, 1479, 1636, 1887, 1938, ...) and 25 call sites across the tree. Null-guarding
// the one that happened to crash would simply have moved the crash to the next one.
// The finder itself never faults — it succeeds at finding nothing — which is why the
// stack blamed the caller and why this read as an ordering/lifecycle bug for a while.
// It is neither: it is an unregistered address.
//
// Corroborated three ways, none of them the engine_funcs.h comment:
//   * no RegisterExeAddr(0x1401c4b80, ...) exists anywhere in standalone/
//   * standalone/tool_harness.cpp:1163 and standalone/trade_probe.cpp:148 BOTH already
//     state the slot is unregistered and deliberately match on something else instead
//   * standalone/factory_registrations.csv:64 independently gives DWORD_1401c4b80 as
//     "TradeComponent" (base "ControllerComponent"), confirming the type identity
//
// ── Why it is GATED and not just switched on ─────────────────────────────────
// Filling a type-id global wakes every dormant reader at once, and this one reaches
// well outside trade. code/Game/Component/animalcomponent.cpp:629 uses the same
// finder as its "is this tile's occupant a building?" habitat test:
//
//     fn_FindComponent_Building(occupant, &buildingIBP);
//     blocked = (buildingIBP == 0);
//
// With the global null that is DEGENERATE — every occupied tile evaluates as blocked,
// unconditionally. Registering the slot therefore changes deer pathing on the default
// path. That belongs to whoever owns the animal wave, not to a trade fix, so this is
// opt-in and the default run stays byte-for-byte identical: gate off means no
// registration, ResolveAddr keeps returning 0, and every reader keeps the null-guarded
// skip it has always taken.
//
// ── The post-Startup fill is load-bearing ────────────────────────────────────
// The storage is filled from FillTypeIdGlobals_tradeui(), called after
// Engine::Startup, NOT here at registration time. Object::Factory::Startup only
// assigns the real CRC ids during startup; filling early stores 0xFFFFFFFF, which
// would present as a completely different bug (a finder that matches nothing, but for
// a second reason). Same rule and same reason as reg_citizenai_typeids.cpp.
#include "resolve.h"
#include "Object/factory_registration.h"      // Object::GetTypeIdByName
#include "Component/component_factory_base.h" // ComponentFactory::LiveEnabled
#include <cstdint>
#include <cstdio>

// Backing storage for the .rdata slot. Stays 0 unless the gate is on AND the
// post-Startup fill has run.
static uint32_t g_tid_TradeComponent = 0;     // 0x1401c4b80

// ── The other three exe-side type-ids the panel path needs ───────────────────
// Added 2026-07-31 after the element-table batch (reg_element_typeids.cpp) let
// TradeUI_Serialize finish its element-wiring phase and reach Phase 5. The next
// fault was the SAME class of bug one function further on: `TradeUI_RefreshDisplay`
// faulted at reimpl-x64.exe+0xDB3BE = `mov (%rax),%edx` on
// `*g_TypeInfo_TradeDescription`, RAX=0 — engine_funcs.cpp:3516 resolves that
// global straight from ResolveAddr(Module::Exe, 0x1401c4bb8), which nothing had
// ever registered.
//
// These three are swept together for the same reason the element table was: each
// one left out is one more crash, one run apart. Unlike the element slots these are
// exe-registered types (their REGISTER_TYPE lines are in code/Game/Component/
// tradedescription.cpp:215, valuedescription.cpp:112, and the ComponentDescription
// entry at factory_registrations.csv:245), so they use the single-deref
// storage + GetTypeIdByName pattern above rather than a Runtime BaseTypeInfo export.
//
// ComponentDescription reaches beyond trade, but it is unregistered everywhere and
// the trade path is what needs it; it sits under this gate until something else
// claims it. address_registry.cpp:1724 mentions the address in a comment only —
// that is a cross-reference, not a registration.
static uint32_t g_tid_TradeDescription = 0;      // 0x1401c4bb8
static uint32_t g_tid_ValueDescription = 0;      // 0x1401c4bf0
static uint32_t g_tid_ComponentDescription = 0;  // 0x1401c7318

extern "C" void FillTypeIdGlobals_tradeui() {
    // No-op in effect unless the storage was registered above (gate on); writing it
    // when unregistered is harmless because nothing reads this static directly.
    g_tid_TradeComponent = ::Object::GetTypeIdByName("TradeComponent");
    g_tid_TradeDescription = ::Object::GetTypeIdByName("TradeDescription");
    g_tid_ValueDescription = ::Object::GetTypeIdByName("ValueDescription");
    g_tid_ComponentDescription = ::Object::GetTypeIdByName("ComponentDescription");

    // Report the OUTCOME. A 0 here means the type never registered exe-side and the
    // reader will silently match nothing rather than fault — the failure mode that
    // made 0x1401c4b80 invisible for the life of the project.
    fprintf(stderr, "[TradeTid] TradeComponent=%u TradeDescription=%u "
            "ValueDescription=%u ComponentDescription=%u\n",
            g_tid_TradeComponent, g_tid_TradeDescription,
            g_tid_ValueDescription, g_tid_ComponentDescription);
    fflush(stderr);
}

void RegisterTradeUITypeIds() {
    // BANISHED_LIVE_TRADE_TIDS=1 to enable. Deliberately its own flag rather than a
    // reuse of any BANISHED_LIVE_TRADE* name the harness sets, so a standard
    // `make sa-run` cannot flip it on by accident — the mistake reg_citizenai_typeids
    // calls out for BANISHED_LIVE_CITIZENAI.
    if (::ComponentFactory::LiveEnabled("BANISHED_LIVE_TRADE_TIDS")) {
        RegisterExeAddr(0x1401c4b80, reinterpret_cast<uintptr_t>(&g_tid_TradeComponent));
        RegisterExeAddr(0x1401c4bb8, reinterpret_cast<uintptr_t>(&g_tid_TradeDescription));
        RegisterExeAddr(0x1401c4bf0, reinterpret_cast<uintptr_t>(&g_tid_ValueDescription));
        RegisterExeAddr(0x1401c7318, reinterpret_cast<uintptr_t>(&g_tid_ComponentDescription));
    }
}
