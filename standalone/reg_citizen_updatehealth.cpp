// standalone/reg_citizen_updatehealth.cpp
// Step-2.5 deferral close-out: CitizenComponent::UpdateHealth (0x1401127d0) + the
// complete disease subsystem (code/Game/Component/citizen_updatehealth.cpp).
//
// This closes the LAST citizen-lifecycle "deep closure" defer: the closure is
// bounded — everything below these 8 is already registered:
//   0x140111590 FindWorkplace (reg_component_gap_A)   0x1401159b0 ReleaseLink59b0 (ditto)
//   0x140018dd0/0x140018880 quadtree walker (reg_pf_geom)
//   0x140094e00 EventLog_AddCitizenEvent (reg_eventlog_citizen)
//   0x1401721b8 =12.0f (reg_component_fruit)   0x140172234 =300.0f (reg_citizen_needsupdate)
// Type ids resolve by name ("CitizenComponent"/"Disease" — both REGISTER_TYPE'd),
// independent of the env-gated 0x1401c7158/0x1401c5948 globals.
//
// Call sites already live: citizencomponent.cpp Update ResolveAddr's 0x1401127d0
// (s_fn_UpdateHealth) — registering makes the health/disease path live.
// The lead wires RegisterCitizenUpdateHealth() into RegisterCodeFunctions() in
// standalone/address_registry.cpp after Ghidra-verify + A/B.
#include "resolve.h"
#include <cstdint>

extern "C" char  CitizenComp_UpdateHealth(void* self, int64_t* dt);            // 0x1401127d0
extern "C" char  CitizenComp_RecoverOrDie(void* self);                         // 0x1401123b0
extern "C" void  CitizenComp_SpreadDisease(void* self);                        // 0x140112470
extern "C" void  CitizenComp_CatchDisease(void* self);                         // 0x140112620
extern "C" void  CitizenComp_Infect(void* self, void* disease);                // 0x140116b90
extern "C" char  CitizenComp_AnyCitizenSick(void* mgr);                        // 0x1400893c0
extern "C" void  CitizenComp_IntArrayResize(void* arr, int n, const uint32_t*);// 0x1400305e0
extern "C" void* Map_RadiusQuery_92990(float* map, void** out, float* pos,
                                       float* radius, int typeId, void* outArr);// 0x140092990

void RegisterCitizenUpdateHealth() {
    RegisterExeAddr(0x1401127d0, reinterpret_cast<uintptr_t>(&CitizenComp_UpdateHealth));
    RegisterExeAddr(0x140116b90, reinterpret_cast<uintptr_t>(&CitizenComp_Infect));
    // 0x1400893c0 doubles as engine_funcs' fn_BuildingList_CheckCondition (misnomer;
    // it is the "any citizen sick" scan) — registering it also fills that pointer.
    RegisterExeAddr(0x140092990, reinterpret_cast<uintptr_t>(&Map_RadiusQuery_92990));
}
