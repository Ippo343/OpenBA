// Animal spawn wave — AnimalComponent per-animal simulation (deer/livestock
// wander + aging + death). Owned by the animal-update lane.
//
// animalcomponent.cpp no longer delegates to 0x140106420 through ResolveAddr —
// it carries the genuine 452-instruction body — so registering that address here
// is safe (the reg_component_nature.cpp:57-59 recursion warning no longer
// applies). Everything below is gated on BANISHED_LIVE_ANIMAL_UPDATE; a default
// run resolves none of it and stays byte-for-byte unchanged.
//
// Hook: address_registry.cpp calls RegisterAnimalUpdateLive().
// Shared animal type-id globals (0x1401c6f98 / 0x1401c8000 / herd + livestock
// ids / float 0x140172220) are published by reg_component_animal.cpp — not here.
#include "resolve.h"
#include "Object/factory_registration.h"      // Object::GetTypeIdByName
#include "Component/component_factory_base.h" // ComponentFactory::LiveEnabled
#include "Component/animalcomponent_funcs.h"
#include <cstdint>

// ---------------------------------------------------------------------------
// Type-id globals this lane needs that NOTHING else registers.
//
// Both are read by exe code the Update path calls into:
//   0x1401c7078 "CharacterComponent"  — the id Entity_FindComponent_Family
//       (0x1400012d0) scans for. Left unfilled, that finder returns null for
//       every entity, so AnimalComponent::Update can never reach the action
//       stack / model. citizenai_handlers.cpp has a hand-rolled fallback for
//       exactly this gap; publishing the id makes the real finder work.
//   0x1401c70b0 "CharacterDescription" — SetWanderTimer reads desc+0x2c (the
//       animation speed) off the character's description; without the id the
//       IsDerivedFrom guard yields null and the read would fault.
//
// Same storage+fill pattern as reg_component_animal.cpp: storage registered at
// AddressRegistryInit, values filled after Engine::Startup.
// ---------------------------------------------------------------------------
static uint32_t g_tid_CharacterComponent   = 0;  // 0x1401c7078
static uint32_t g_tid_CharacterDescription = 0;  // 0x1401c70b0

static bool AnimalUpdateEnabled() {
    return ::ComponentFactory::LiveEnabled("BANISHED_LIVE_ANIMAL_UPDATE");
}

// Idempotent, and safe to call before Object::Factory::Startup has assigned the
// real CRC ids — it simply leaves the globals at 0 and retries on the next call.
// animalcomponent.cpp's EnsureHelpers() calls this on the first live animal tick,
// which is always post-Startup, so no game_main.cpp hook is required. The symbol
// is still exported so the integrator CAN add it next to the other
// FillComponentTypeIdGlobals_* calls for deterministic ordering.
extern "C" void FillComponentTypeIdGlobals_animal_update() {
    if (!AnimalUpdateEnabled()) return;
    if (g_tid_CharacterComponent == 0)
        g_tid_CharacterComponent = ::Object::GetTypeIdByName("CharacterComponent");
    if (g_tid_CharacterDescription == 0)
        g_tid_CharacterDescription = ::Object::GetTypeIdByName("CharacterDescription");
}

void RegisterAnimalUpdateLive() {
    if (!AnimalUpdateEnabled()) return;

    // --- AnimalComponent methods (animalcomponent.cpp) ---
    // 0x140105e20 SetBounds is intentionally NOT registered here: it already has
    // a faithful port (PastureComponent_AnimalSetBounds) registered by
    // reg_component_gap_D.cpp:71, and this lane calls it through ResolveAddr.
    RegisterExeAddr(0x140106420, reinterpret_cast<uintptr_t>(&AnimalComponent_Update));         // vtable[9]
    RegisterExeAddr(0x140106d50, reinterpret_cast<uintptr_t>(&AnimalComponent_Startup));        // vtable[4]
    RegisterExeAddr(0x140106c20, reinterpret_cast<uintptr_t>(&AnimalComponent_OnAdd));          // vtable[6]
    RegisterExeAddr(0x140106020, reinterpret_cast<uintptr_t>(&AnimalComponent_FindHabitat));
    RegisterExeAddr(0x140107320, reinterpret_cast<uintptr_t>(&AnimalComponent_UpdateBehavior));
    RegisterExeAddr(0x140106250, reinterpret_cast<uintptr_t>(&AnimalComponent_SetWanderTimer));

    // --- type-id storage (values filled post-Startup, see above) ---
    RegisterExeAddr(0x1401c7078, reinterpret_cast<uintptr_t>(&g_tid_CharacterComponent));
    RegisterExeAddr(0x1401c70b0, reinterpret_cast<uintptr_t>(&g_tid_CharacterDescription));
}
