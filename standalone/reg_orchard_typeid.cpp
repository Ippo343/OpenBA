// standalone/reg_orchard_typeid.cpp
// Storage + fill for the OrchardComponent type-id .rdata global.
//
//   0x1401c46b0  OrchardComponent  (writer OrchardComponent_RegisterType 0x14014b400,
//                verified against standalone/factory_registrations.csv:42:
//                `14014b400,OrchardComponent_RegisterType,&DWORD_1401c46b0,
//                 "OrchardComponent","ControllerComponent",...`)
//
// This slot was registered NOWHERE, so ResolveAddr(Exe, 0x1401c46b0) returned 0 and
// EngineFuncsInit set g_TypeId_OrchardComponent = nullptr. Entity_FindComponentByExeTypeId
// early-returns null on a null typeIdPtr, so fn_FindComponent_Orchard reported
// "this entity has no OrchardComponent" for EVERY entity, forever.
//
// MEASURED 2026-08-09 on a demo orchard (BANISHED_BUILD_DEMO_ORCHARD=1,
// BANISHED_TOOL_HARNESS=orchardpanel), which is what identified this:
//   [OrchardPanel] ... comp=000002cf096642f8 type(+0x60)=0     <- harness found it by vtable
//   [OrchardUI] Slot11 self=... entity=000002cf0962b4d0 comp=0 newType=2cf085b2650
// i.e. Slot11 was handed a perfectly good resolved tree type and a valid entity, and the
// finder still returned 0 on the very entity the harness had just proven carries the
// component. Two user-visible orchard symptoms both trace to this one null:
//   * picking a tree type in the panel combo appears to work (the combo shows e.g.
//     "peach") but never reaches the component — OrchardUI::Slot11 bails, +0x60 stays 0.
//   * the yield percentage renders blank — OrchardUI::Slot7 bails on the same miss
//     before it can format the progress text.
//
// Other consumers that resolve 0x1401c46b0 themselves and are revived by this
// registration (note several carry MISNOMER comments — the CSV above is authoritative,
// the slot is OrchardComponent, not "OrchardDescription"/"OrchardAreaComponent"):
//   * code/Game/Work/plantorchard.cpp:184   (s_typeConst, "orchard type marker")
//   * code/Game/Work/tendarea.cpp:197       (s_typeId_OrchardAreaComponent)
//   * code/Game/Work/work_act_rawmat.cpp:122 (s_war_pMsgId2_c46b0 — correctly named)
//
// Two-phase, exactly like reg_tracker_typeid.cpp:
//   RegisterOrchardTypeIds()      — from AddressRegistryInit, BEFORE EngineFuncsInit,
//                                   so the engine_funcs global points at our storage.
//   FillTypeIdGlobals_orchard()   — from game_main.cpp AFTER Object::Factory::Startup,
//                                   the only point at which the real CRC ids exist
//                                   (filling earlier stores the 0xFFFFFFFF sentinel).
//
// NOT gated on an env opt-in, for the same reason as the tracker slot: an unfilled
// type-id global is not a "safe default", it is a silent permanent miss.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include "Component/OrchardComponent.h"
#include "Component/OrchardDescription.h"

static uint32_t g_tid_OrchardComponent = 0;   // 0x1401c46b0
// OrchardDescription (DAT_1401c46e8, writer OrchardDescription_Register 0x14014b440). Also
// registered nowhere; consumers worked around it with private by-name resolves
// (orchardcomponent_states.cpp's OrchardDescTypeId, harvestorchard.cpp:202). Giving the real
// slot storage means the exe-faithful `IsDerivedFrom(desc, *DAT_1401c46e8)` reads in
// OrchardComponent::OnMessage msg 2 and state 9 work as written.
static uint32_t g_tid_OrchardDescription = 0; // 0x1401c46e8

extern "C" void FillTypeIdGlobals_orchard() {
    g_tid_OrchardComponent = OrchardComponent::TypeId();
    g_tid_OrchardDescription = OrchardDescription::TypeId();
    if (g_tid_OrchardDescription == 0)
        std::fprintf(stderr, "[OrchardTypeId] ERROR: GetTypeIdByName(\"OrchardDescription\") "
                             "returned 0 — slot 0x1401c46e8 is live but EMPTY\n");
    else
        std::fprintf(stderr, "[OrchardTypeId] 0x1401c46e8 = %u (0x%08X)\n",
                     g_tid_OrchardDescription, g_tid_OrchardDescription);
    // Verify the VALUE, not the pointer: a registered slot holding 0 is
    // indistinguishable from an unregistered one at every call site. Report the
    // outcome in BOTH arms so an empty log can never read as success.
    if (g_tid_OrchardComponent == 0) {
        std::fprintf(stderr, "[OrchardTypeId] ERROR: GetTypeIdByName(\"OrchardComponent\") "
                             "returned 0 — slot 0x1401c46b0 is live but EMPTY; every "
                             "fn_FindComponent_Orchard lookup will still miss\n");
    } else {
        std::fprintf(stderr, "[OrchardTypeId] 0x1401c46b0 = %u (0x%08X)\n",
                     g_tid_OrchardComponent, g_tid_OrchardComponent);
    }
    std::fflush(stderr);
}

void RegisterOrchardTypeIds() {
    RegisterExeAddr(0x1401c46b0, reinterpret_cast<uintptr_t>(&g_tid_OrchardComponent));
    RegisterExeAddr(0x1401c46e8, reinterpret_cast<uintptr_t>(&g_tid_OrchardDescription));
}
