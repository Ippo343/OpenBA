// Component registration wave — ANIMAL pathway type-id globals (herd/deer/livestock).
//
// The whole animal spawn chain keys off .rdata type-id globals that RegisterType
// fills in the original exe. None of them were registered in standalone, so every
// IsDerivedFrom/GetTypeId comparison in the chain silently no-ops (or null-derefs,
// e.g. HerdComponent_Update reads *s_pHerdTypeId unconditionally):
//   * DAT_1401c7858 = HerdDescription   (herd config lookup; null deref at first tick)
//   * DAT_1401c7820 = HerdComponent     (WildAnimalComponent's back-ref validation —
//                     NOTE: wildanimalcomponent.cpp historically mislabeled this
//                     "WildAnimal type"; factory_registrations.csv:268 says Herd)
//   * DAT_1401c6f98 = AnimalComponent   (member lookups in Herd/Pasture/HuntAnimal)
//   * DAT_1401c8000 = WildAnimalComponent
//   * DAT_1401c7970 = LivestockComponent  (pasture member validation on load)
//   * DAT_1401c79a8 = LivestockDescription (LivestockComponent_Update config gate)
// Plus one float constant the herd spawner reads:
//   * DAT_140172220 = 180.0f (AdjustSize spawn scatter scale, verified from retail
//     .rdata 0x43340000). 0x140172234 (300.0f) is already registered elsewhere and
//     is retail-correct — the 60.0 claim in old comments was wrong.
//
// Same storage+fill pattern as reg_component_spawn.cpp: storage registered at
// AddressRegistryInit, values filled after Engine::Startup via GetTypeIdByName.
//
// GATED: publishing these ids activates code that is currently null-guarded dead
// (e.g. WildAnimalComponent_Shutdown validation, hunt-action member lookups), so a
// default run stays byte-for-byte unchanged until one of the animal-wave flags is
// on. Any one flag suffices — the type ids are shared by all three lanes.
#include "resolve.h"
#include "Object/factory_registration.h"      // Object::GetTypeIdByName
#include <cstdint>
#include "Component/AnimalComponent.h"
#include "Component/HerdComponent.h"
#include "Component/HerdDescription.h"
#include "Component/LivestockComponent.h"
#include "Component/LivestockDescription.h"
#include "Component/WildAnimalComponent.h"

static float    g_f_HerdSpawnSpread        = 180.0f;  // 0x140172220

void RegisterComponentBatch_animal() {
    RegisterExeAddr(0x140172220, reinterpret_cast<uintptr_t>(&g_f_HerdSpawnSpread));
}
