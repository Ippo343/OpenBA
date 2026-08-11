// standalone/reg_cleararea_typeids.cpp
// Storage + fill for the two ClearArea type-id .rdata globals.
//
//   0x1401c71c8  ClearAreaComponent    (writer ClearAreaComponent_RegisterType 0x14014e810,
//                factory_registrations.csv:239)
//   0x1401c7200  ClearAreaDescription  (writer ClearAreaDescription_Register 0x14014e850,
//                factory_registrations.csv:240)
//
// Both were registered NOWHERE, so ResolveAddr returned 0 for each. That is not orchard-
// specific:
//
//   * engine_funcs' `fn_FindComponent_SM` (the reimpl of exe FUN_14000a170) is defined as
//     Entity_FindComponentByExeTypeId(entity, out, g_TypeId_ClearAreaComponent_FC), and
//     EngineFuncsInit set that global from slot 0x1401c71c8 — i.e. to nullptr. The finder
//     early-outs on a null type-id pointer, so EVERY caller has been told "this entity has
//     no ClearAreaComponent" for every entity, silently. Same shape as the GrowthComponent
//     miss (reg_growth_typeid.cpp) and the OrchardComponent one (reg_orchard_typeid.cpp).
//   * clearareacomponent_states.cpp reads 0x1401c7200 through its DerefTid fallback, so it
//     survived; the orchard's state-20 enter reads the same slot to find the
//     ClearAreaDescription among the entity's sub-descriptions.
//
// Found while porting OrchardComponent state 20 (the panel's "cut" command), whose per-frame
// handler is the first ported caller of fn_FindComponent_SM on a path a player can trigger.
//
// Two-phase, exactly like reg_orchard_typeid.cpp / reg_growth_typeid.cpp:
//   RegisterClearAreaTypeIds()      — from AddressRegistryInit, BEFORE EngineFuncsInit, so
//                                     the engine_funcs global points at our storage.
//   FillTypeIdGlobals_cleararea()   — from game_main.cpp AFTER Object::Factory::Startup,
//                                     the only point at which the real CRC ids exist.
//
// Not env-gated: an unfilled type-id global is not a safe default, it is a silent
// permanent miss.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>

static uint32_t g_tid_ClearAreaComponent   = 0;   // 0x1401c71c8
static uint32_t g_tid_ClearAreaDescription = 0;   // 0x1401c7200

extern "C" void FillTypeIdGlobals_cleararea() {
    g_tid_ClearAreaComponent   = ::Object::GetTypeIdByName("ClearAreaComponent");
    g_tid_ClearAreaDescription = ::Object::GetTypeIdByName("ClearAreaDescription");
    // Check the VALUE, not the pointer: a registered slot holding 0 is indistinguishable
    // from an unregistered one at every call site. Print in both arms.
    if (g_tid_ClearAreaComponent == 0)
        std::fprintf(stderr, "[ClearAreaTypeId] ERROR: GetTypeIdByName(\"ClearAreaComponent\")"
                             " returned 0 — slot 0x1401c71c8 is live but EMPTY; every "
                             "fn_FindComponent_SM lookup will still miss\n");
    if (g_tid_ClearAreaDescription == 0)
        std::fprintf(stderr, "[ClearAreaTypeId] ERROR: "
                             "GetTypeIdByName(\"ClearAreaDescription\") returned 0 — slot "
                             "0x1401c7200 is live but EMPTY\n");
    std::fprintf(stderr, "[ClearAreaTypeId] 0x1401c71c8 = %u | 0x1401c7200 = %u\n",
                 g_tid_ClearAreaComponent, g_tid_ClearAreaDescription);
    std::fflush(stderr);
}

void RegisterClearAreaTypeIds() {
    RegisterExeAddr(0x1401c71c8, reinterpret_cast<uintptr_t>(&g_tid_ClearAreaComponent));
    RegisterExeAddr(0x1401c7200, reinterpret_cast<uintptr_t>(&g_tid_ClearAreaDescription));
}
