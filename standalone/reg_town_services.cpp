// standalone/reg_town_services.cpp
// Registration wave — the three town-service ControllerComponents whose retail state
// tables had never been ported: CemeteryComponent (graveyard), HealerComponent
// (hospital) and HappinessComponent (chapel/tavern/well-being sources).
//
// All three shared one root cause. Each was spawned by COMPONENT_INERT_FACTORY, so
// every instance carried the shared stub vtable, and each one's REGISTER into the
// TownManager is not a lifecycle slot at all -- it is the state-2 "built" enter of a
// controller state table that nothing in standalone ever populated:
//
//     CemeteryComponent  0x1400477a0 -> state 2 enter 0x140046fc0 -> townMgr + 0x40
//     HappinessComponent 0x14004ac10 -> state 2/13 enter 0x14004a9f0 -> townMgr + 0x50
//     HealerComponent    0x14004b020 -> state 2 enter 0x14004ae30 -> townMgr + 0x70
//
// The graveyard case is the one with a hard downstream proof: our already-ported
// Cemetery_FindNearestWithSlot_9740 reads *(GameWorld+0xf0)+0x40, and nothing else in
// the exe writes that array -- so every citizen death searched a permanently empty list
// and no burial could ever happen.
//
// Ghidra's OnCreate/OnRemove off-by-one bites all three: their slot-7 bodies call
// 0x140002cc0, which is the ERASE (it scans for a matching resolved pointer, shifts the
// tail down and decrements the count), not a push.
//
// Wired into address_registry.cpp as RegisterTownServices().
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include "Component/CemeteryComponent.h"
#include "Component/HappinessDescription.h"

// --- CemeteryComponent (code/Game/Component/cemeterycomponent.cpp) ---
extern "C" char CemeteryComponent_EnterBuilt(void* comp, void* sm);       // 0x140046fc0
extern "C" char CemeteryComponent_EnterDemolish(void* comp, void* sm);    // 0x140046f80

// --- HealerComponent (code/Game/Component/healercomponent.cpp) ---
extern "C" char HealerComponent_EnterBuilt(void* comp, void* sm);         // 0x14004ae30
extern "C" char HealerComponent_EnterDemolish(void* comp, void* sm);      // 0x14004adf0

// --- HappinessComponent (code/Game/Component/happinesscomponent.cpp) ---
extern "C" bool HappinessComponent_IsActive(void* self);                  // 0x14004a550
extern "C" void HappinessComponent_Register(void* self);                  // 0x14004a4f0
extern "C" void HappinessComponent_Unregister(void* self);                // 0x14004a4a0
extern "C" char HappinessComponent_EnterBuilt(void* comp, void* sm);      // 0x14004a9f0
extern "C" char HappinessComponent_Tick38(void* comp, void* sm, void* t, int i); // 0x14004a9a0
extern "C" char HappinessComponent_EnterDemolish(void* comp, void* sm);   // 0x14004a940

// ------------------------------------------------------------------
// HappinessDescription type-ID .rdata global (DAT_1401c42f8).
// ------------------------------------------------------------------
// HappinessComponent::IsActive (0x14004a550) opens with
//   IsDerivedFrom(self->_unk18, *0x1401c42f8)
// and then reads desc[+0x20] / [+0x24] / [+0x25] / [+0x28] through the result. In the
// original exe the global is .data written by HappinessDescription_Register; in
// standalone WE provide the storage (registered below) and FILL it after
// Engine::Startup, because Object::GetTypeIdByName is a registration-list lookup that
// returns 0 until the type tree is populated.
//
// This is not optional plumbing: with no storage, ResolveAddr returns null, the
// IsDerivedFrom guard fails, desc collapses to 0, and the very first happiness-carrying
// structure the start town places takes an ACCESS_VIOLATION reading 0x20 -- which is
// exactly what the first soak of this wave did, mid-StartTown_SpawnStructures.
// (0x1401c7cb8 RawMaterialDescription, the other id IsActive reads, is already
// registered and filled by reg_action_child_ctors2.cpp.)
static uint32_t g_tid_HappinessDescription = 0;   // 0x1401c42f8

// CemeteryComponent type-ID .rdata global (DAT_1401c4090), read by
// fn_FindComponent_Cemetery -- which CemeteryUI's Slot4 and Slot7 both use to reach the
// component. It had NO storage registered, so the global resolved null, the finder
// matched nothing, and the cemetery panel could not read a plot count even once its
// slots were wired. (The log named it: "NOT IMPLEMENTED: exe function at 0x1401c4090".)
static uint32_t g_tid_CemeteryComponent = 0;      // 0x1401c4090

extern "C" void FillTypeIdGlobals_town_services() {
    g_tid_HappinessDescription = HappinessDescription::TypeId();
    g_tid_CemeteryComponent    = CemeteryComponent::TypeId();
}

void RegisterTownServices() {
    RegisterExeAddr(0x1401c42f8, reinterpret_cast<uintptr_t>(&g_tid_HappinessDescription));
    RegisterExeAddr(0x1401c4090, reinterpret_cast<uintptr_t>(&g_tid_CemeteryComponent));

    // Graveyard. The lifecycle slots (Cemetery 6/7 = 0x140047200 / 0x1400471c0,
    // Healer 7 = 0x14004ae70) are real-class virtuals since ENDGAME B.3.8; nothing read
    // their addresses back, so their rows are gone. The state-table handlers stay.
    RegisterExeAddr(0x140046fc0, reinterpret_cast<uintptr_t>(&CemeteryComponent_EnterBuilt));
    RegisterExeAddr(0x140046f80, reinterpret_cast<uintptr_t>(&CemeteryComponent_EnterDemolish));

    // Hospital.
    RegisterExeAddr(0x14004ae30, reinterpret_cast<uintptr_t>(&HealerComponent_EnterBuilt));
    RegisterExeAddr(0x14004adf0, reinterpret_cast<uintptr_t>(&HealerComponent_EnterDemolish));

    // Happiness sources.
    RegisterExeAddr(0x14004a550, reinterpret_cast<uintptr_t>(&HappinessComponent_IsActive));
    RegisterExeAddr(0x14004a4f0, reinterpret_cast<uintptr_t>(&HappinessComponent_Register));
    RegisterExeAddr(0x14004a4a0, reinterpret_cast<uintptr_t>(&HappinessComponent_Unregister));
    RegisterExeAddr(0x14004a9f0, reinterpret_cast<uintptr_t>(&HappinessComponent_EnterBuilt));
    RegisterExeAddr(0x14004a9a0, reinterpret_cast<uintptr_t>(&HappinessComponent_Tick38));
    RegisterExeAddr(0x14004a940, reinterpret_cast<uintptr_t>(&HappinessComponent_EnterDemolish));
}
