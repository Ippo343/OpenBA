// FruitComponent + HappinessComponent — slot-3 stream Serialize registrations
// (save/load wave, 2026-08-28).
//
// Registers the retail addresses of FruitComponent::Serialize (0x14004a070,
// code/Game/Component/fruitcomponent_serialize.cpp) and
// HappinessComponent::Shutdown (0x14004aa80 — Ghidra's shifted name for the
// slot-3 Serialize, code/Game/Component/happinesscomponent.cpp) so
// ResolveAddr(Module::Exe, ...) finds them. Both are wired live into their
// component vtables unconditionally (see fruitcomponent.cpp's hand-expanded
// factory and happinesscomponent.cpp's HappinessComponent_FinishLiveVtable)
// — serialize round-trip is not an opt-in feature the way each component's
// live simulation gate (BANISHED_LIVE_FRUIT / BANISHED_LIVE_HAPPINESS) is.
// Without this, a loaded save's FruitComponent lost its ripeness/fruitFlag
// and HappinessComponent lost its happinessFlag (both reset to zero).
//
// All dispatch slots and type ids these bodies need (Serializer ctor/dtor,
// Serialize(bool&)/Serialize(float&), InternalBasePtr::Serialize,
// IsDerivedFrom, Component::Serialize base, the NaturalResourceComponent
// type id) are ALREADY registered elsewhere (address_registry.cpp,
// reg_animal_serialize.cpp, reg_component_nature.cpp) — nothing new to add
// here beyond the two function bodies themselves.
//
// Hook: address_registry.cpp calls RegisterFruitHappinessSerialize().
#include "resolve.h"
#include <cstdint>

// fruitcomponent_serialize.cpp
extern "C" void FruitComponent_Serialize(void* self, void* stream);
// happinesscomponent.cpp (Ghidra's shifted name: "Shutdown" = slot-3 Serialize)
struct HappinessComponent;
void HappinessComponent_Shutdown(HappinessComponent* self, void* stream);

void RegisterFruitHappinessSerialize() {
    RegisterExeAddr(0x14004a070, reinterpret_cast<uintptr_t>(&FruitComponent_Serialize));
    RegisterExeAddr(0x14004aa80, reinterpret_cast<uintptr_t>(&HappinessComponent_Shutdown));
}
