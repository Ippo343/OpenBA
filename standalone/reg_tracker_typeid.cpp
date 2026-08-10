// standalone/reg_tracker_typeid.cpp
// Storage + fill for the TrackerComponent type-id .rdata global.
//
//   0x1401c4b10  TrackerComponent  (writer TrackerComponent_RegisterType 0x14014b980,
//                verified against standalone/factory_registrations.csv:
//                `14014b980,TrackerComponent_RegisterType,&DWORD_1401c4b10,"TrackerComponent",...`)
//
// This slot was registered NOWHERE, so ResolveAddr(Exe, 0x1401c4b10) returned 0 and
// EngineFuncsInit set g_TypeId_TrackerComponent = nullptr. Every consumer of the
// shared FindComponent accessor therefore got a permanent "no TrackerComponent":
//   * engine_funcs.h fn_FindComponent_Tracker  (Entity_FindComponentByExeTypeId
//     early-returns null when typeIdPtr is null) -> ProductionUI's per-frame update
//     (code/Game/Dialog/productionui.cpp) would deref tracker+0x48 on NULL.
//   * code/Game/Action/action_base_rawmat.cpp:44  (s_pTid_c4b10)
//   * code/Game/Goal/statvaluedispatch.cpp:75     (s_tid_4b10)
// Both of the latter resolve the raw address themselves, so registering the slot
// revives them too.
//
// Two-phase, exactly like reg_citizen_lifeevent_handler.cpp / reg_component_spawn.cpp:
//   RegisterTrackerTypeIds()      — from AddressRegistryInit, BEFORE EngineFuncsInit,
//                                   so the engine_funcs global points at our storage.
//   FillTypeIdGlobals_tracker()   — from game_main.cpp AFTER Object::Factory::Startup,
//                                   which is the only place the real CRC ids exist
//                                   (filling earlier stores the 0xFFFFFFFF sentinel).
//
// NOT gated on an env opt-in: an unfilled type-id global is not a "safe default",
// it is a silent permanent miss (documented project trap "registered but filled
// with ZERO"), and the three consumers above all treat a miss as "component absent".
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>

static uint32_t g_tid_TrackerComponent = 0;   // 0x1401c4b10

extern "C" void FillTypeIdGlobals_tracker() {
    g_tid_TrackerComponent = ::Object::GetTypeIdByName("TrackerComponent");
    // VERIFY THE VALUE, not just the pointer: a registered slot holding 0 is
    // indistinguishable from an unregistered one at every call site, and this
    // project has been bitten by exactly that (a GetTypeIdByName on a type name
    // that does not exist). Report the outcome in BOTH arms.
    if (g_tid_TrackerComponent == 0) {
        std::fprintf(stderr, "[TrackerTypeId] ERROR: GetTypeIdByName(\"TrackerComponent\") "
                             "returned 0 — slot 0x1401c4b10 is live but EMPTY; "
                             "ProductionUI / RawMaterial / StatValue tracker lookups will all miss\n");
    } else {
        std::fprintf(stderr, "[TrackerTypeId] 0x1401c4b10 = %u (0x%08X)\n",
                     g_tid_TrackerComponent, g_tid_TrackerComponent);
    }
    std::fflush(stderr);
}

void RegisterTrackerTypeIds() {
    RegisterExeAddr(0x1401c4b10, reinterpret_cast<uintptr_t>(&g_tid_TrackerComponent));
}
