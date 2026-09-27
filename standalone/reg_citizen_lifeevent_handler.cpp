// standalone/reg_citizen_lifeevent_handler.cpp
// Registration for the citizen-death path (code/Game/Component/
// citizen_lifeevent_handler.cpp) — closes the last "Level-2 deep closure"
// defer of the CitizenComponent lifecycle backlog.
//
// Closure audit 2026-07-19: every other 0x140xxxxxx sub-callee of 0x140116dd0
// was already registered (FindComponent_2ed0, EntityResolvePos_2e20,
// IsDstReachable_f810, ValidateCell_a00f0, MarkEntityRemoval_c2ea0,
// DynArraySetCount_3110, FineCellDirty_a8c90, PfEnqueueCmd2_a6fe0,
// FindResData_4680, CtorFromDescription_c3720, RegisterEntity_c48a0,
// FindComponent_3f60, MapGetTerrainPos_9f3e0, PostLifeEvent_113af0, plus the
// CheckPtr/IBP-ctor/IsDerivedFrom/MemAlloc/Rand/Transform dispatch slots) —
// these seven are the only newly-registered code addresses.
//
// Also provides storage for two type-id .rdata globals, filled via
// GetTypeIdByName AFTER Engine::Startup (mirrors reg_component_tombstone.cpp):
//   0x1401c4a30 TombstoneComponent  (also read by statvaluedispatch.cpp +
//                                    cemeteryui.cpp — previously unresolvable)
//   0x1401c40c8 CemeteryDescription
//
// The integrator wires RegisterCitizenLifeEventHandler() into
// address_registry.cpp and FillTypeIdGlobals_citizen_lifeevent() into
// game_main.cpp (after Engine::Startup, alongside the other FillTypeIdGlobals_*).
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include "Component/CemeteryDescription.h"
#include "Component/TombstoneComponent.h"

// Forward declarations (code/Game/Component/citizen_lifeevent_handler.cpp).
extern "C" uint64_t CitizenComponent_HandleLifeEvent(void*, int, const char*);       // 0x140116dd0
extern "C" void     Citizen_FamilyRingOrFlag_3a70(void*, uint32_t);                  // 0x140113a70
extern "C" void**   Cemetery_FindNearestWithSlot_9740(void*, void**, uint32_t);      // 0x140089740
extern "C" uint32_t Cemetery_PickFreeSlot_6c60(void*);                               // 0x140046c60
extern "C" void     Cemetery_SpawnTombstone_6a90(void*, int, uint64_t, void*);       // 0x140046a90
extern "C" void     Entity_PlaceAtCell_7b20(void*, int, float);                      // 0x140057b20

// Type-id globals filled after Engine::Startup.
static uint32_t g_tid_TombstoneComponent  = 0;   // 0x1401c4a30
static uint32_t g_tid_CemeteryDescription = 0;   // 0x1401c40c8

extern "C" void FillTypeIdGlobals_citizen_lifeevent() {
    g_tid_TombstoneComponent  = TombstoneComponent::TypeId();
    g_tid_CemeteryDescription = CemeteryDescription::TypeId();
}

void RegisterCitizenLifeEventHandler() {
    RegisterExeAddr(0x140116dd0, reinterpret_cast<uintptr_t>(&CitizenComponent_HandleLifeEvent));
    RegisterExeAddr(0x140113a70, reinterpret_cast<uintptr_t>(&Citizen_FamilyRingOrFlag_3a70));
    RegisterExeAddr(0x140089740, reinterpret_cast<uintptr_t>(&Cemetery_FindNearestWithSlot_9740));
    RegisterExeAddr(0x140046c60, reinterpret_cast<uintptr_t>(&Cemetery_PickFreeSlot_6c60));
    RegisterExeAddr(0x140046a90, reinterpret_cast<uintptr_t>(&Cemetery_SpawnTombstone_6a90));
    RegisterExeAddr(0x140057b20, reinterpret_cast<uintptr_t>(&Entity_PlaceAtCell_7b20));
    // Type-id global storage (values filled later by FillTypeIdGlobals_citizen_lifeevent).
    RegisterExeAddr(0x1401c4a30, reinterpret_cast<uintptr_t>(&g_tid_TombstoneComponent));
    RegisterExeAddr(0x1401c40c8, reinterpret_cast<uintptr_t>(&g_tid_CemeteryDescription));
}
