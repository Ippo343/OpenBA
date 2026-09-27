// Animal spawn wave — AnimalComponent per-animal simulation (deer/livestock
// wander + aging + death). Owned by the animal-update lane.
//
// AnimalComponent is a real class (ENDGAME B.3.8): its Update / Startup / OnAdded
// slots (0x140106420 / 0x140106d50 / 0x140106c20) are reached through the compiler
// vtable and nothing reads those addresses back, so their rows are gone. What is
// left are the helper doors the Update body's callees resolve by address.
//
// Hook: address_registry.cpp calls RegisterAnimalUpdateLive().
// Shared animal type-id globals (0x1401c6f98 / 0x1401c8000 / herd + livestock
// ids / float 0x140172220) are published by reg_component_animal.cpp — not here.
#include "resolve.h"
#include "Object/factory_registration.h"      // Object::GetTypeIdByName
#include "Component/animalcomponent_funcs.h"
#include <cstdint>
#include "Component/CharacterComponent.h"
#include "Component/CharacterDescription.h"

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

void RegisterAnimalUpdateLive() {

    // --- AnimalComponent methods (animalcomponent.cpp) ---
    // 0x140105e20 SetBounds is intentionally NOT registered here: it already has
    // a faithful port (PastureComponent_AnimalSetBounds) registered by
    // reg_component_gap_D.cpp:71, and this lane calls it through ResolveAddr.
    RegisterExeAddr(0x140107320, reinterpret_cast<uintptr_t>(&AnimalComponent_UpdateBehavior));

    // --- type-id storage (values filled post-Startup, see above) ---
}
