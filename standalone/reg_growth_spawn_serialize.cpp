// GrowthComponent + SpawnComponent — slot-3 stream Serialize registrations
// (save/load wave, 2026-08-28).
//
// Registers the retail addresses of GrowthComponent::Serialize (0x1401207c0,
// code/Game/Component/growthcomponent_serialize.cpp) and
// SpawnComponent::Serialize (0x140055ee0, code/Game/Component/spawncomponent.cpp)
// so ResolveAddr(Module::Exe, ...) finds them. Unlike the animal-family wave
// (reg_animal_serialize.cpp), these TWO slots are wired live into their
// component vtables unconditionally (see growthcomponent.cpp / spawncomponent.cpp
// hand-expanded factories) — GrowthComponent is BANISHED_LIVE_GROWTH default-on
// (the default-on table), so its serializer is reachable the moment a save/load
// driver exists, and both fix the same class of bug: a loaded save's
// component kept the base-Component no-op serialize, so its real per-instance
// fields (GrowthComponent's resource/model sibling links + growthScale;
// SpawnComponent's per-entry spawn timers) came back zeroed.
//
// All dispatch slots and type ids these bodies need (Serializer ctor/dtor,
// Serialize(int&)/Serialize(float&), InternalBasePtr::Serialize,
// IsDerivedFrom, Time::Serialize, Component::Serialize base, the
// NaturalResourceComponent/ModelComponent/SpawnDescription type ids) are
// ALREADY registered elsewhere (address_registry.cpp, reg_component_economy.cpp,
// reg_animal_serialize.cpp, reg_component_nature.cpp, reg_pipeline_typeids.cpp,
// reg_component_spawn.cpp) — nothing new to add here beyond the two function
// bodies themselves.
//
// Hook: address_registry.cpp calls RegisterGrowthSpawnSerialize().
#include "resolve.h"
#include <cstdint>

// growthcomponent_serialize.cpp
extern "C" void GrowthComponent_Serialize2(void* self, void* stream);
// spawncomponent.cpp
extern "C" void SpawnComponent_Serialize(void* self, void* stream);

void RegisterGrowthSpawnSerialize() {
    RegisterExeAddr(0x1401207c0, reinterpret_cast<uintptr_t>(&GrowthComponent_Serialize2));
    RegisterExeAddr(0x140055ee0, reinterpret_cast<uintptr_t>(&SpawnComponent_Serialize));
}
