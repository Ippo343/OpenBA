// standalone/reg_growth_typeid.cpp
// Storage + fill for the GrowthComponent type-id .rdata global.
//
//   0x1401c77b0  GrowthComponent  (writer GrowthComponent_RegisterType 0x14014f0d0,
//                per standalone/factory_registrations.csv:
//                `14014f0d0,GrowthComponent_RegisterType,&DAT_1401c77b0,
//                 "GrowthComponent","Component",...`)
//
// This slot was registered NOWHERE, so ResolveAddr(Exe, 0x1401c77b0) returned 0 and
// ActCNR_FindComponent_c460 (the reimpl of exe 0x14000c460) passed a null type-id pointer
// into FindComponentByTypeId, which early-outs to "no such component" for EVERY entity.
// Fourth instance of reference_registered_but_filled_with_zero /
// reference_null_typeid_global_fakes_ordering_bug in this codebase.
//
// MEASURED 2026-08-10, on the first orchard planting the reimplemented state machine ever
// drove to completion (BANISHED_LIVE_ORCHARD=1 BANISHED_ORCHARD_WORKERS=1): the seated
// orchardist executed the plant action, CreateNaturalResource::AbstractImpl spawned the tree
// entity fine, and then died on SetGrowthProgress(null):
//   [CreateNR] MISSING COMPONENT ... newEntity=... modelComp=0x...  growthComp=0
//   [CreateNR]   finder slot 0x1401c77b0 -> storage=0 value=0 |
//                GetTypeIdByName("GrowthComponent")=1845779407 |
//                components on entity=5, matching by NAME=1
// i.e. the spawned template DOES carry a GrowthComponent; only the finder could not see it.
// Retail's unguarded deref of the finder result is correct and stays unguarded.
//
// This is not orchard-specific: every natural resource the game spawns at runtime goes
// through the same finder, so the miss was game-wide and silent until something finally
// spawned one.
//
// Two-phase, exactly like reg_orchard_typeid.cpp:
//   RegisterGrowthTypeIds()      — from AddressRegistryInit, BEFORE EngineFuncsInit.
//   FillTypeIdGlobals_growth()   — from game_main.cpp AFTER Object::Factory::Startup, the
//                                  only point at which the real CRC ids exist.
//
// Not env-gated: an unfilled type-id global is not a safe default, it is a silent
// permanent miss.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include "Component/GrowthComponent.h"

static uint32_t g_tid_GrowthComponent = 0;   // 0x1401c77b0

extern "C" void FillTypeIdGlobals_growth() {
    g_tid_GrowthComponent = GrowthComponent::TypeId();
    // Check the VALUE, not the pointer: a registered slot holding 0 is indistinguishable
    // from an unregistered one at every call site. Print in both arms.
    if (g_tid_GrowthComponent == 0) {
        std::fprintf(stderr, "[GrowthTypeId] ERROR: GetTypeIdByName(\"GrowthComponent\") "
                             "returned 0 — slot 0x1401c77b0 is live but EMPTY; every "
                             "growth-component lookup will still miss\n");
    } else {
        std::fprintf(stderr, "[GrowthTypeId] 0x1401c77b0 = %u (0x%08X)\n",
                     g_tid_GrowthComponent, g_tid_GrowthComponent);
    }
    std::fflush(stderr);
}

void RegisterGrowthTypeIds() {
    RegisterExeAddr(0x1401c77b0, reinterpret_cast<uintptr_t>(&g_tid_GrowthComponent));
}
