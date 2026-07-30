// Animal wave — slot-3 stream Serialize registrations (save/load parity).
//
// Registers the retail addresses of all six animal-family component
// serializers so ResolveAddr(Module::Exe, ...) finds them, plus the type-id
// storage and dispatch slots that ONLY the serialize path reads. Nothing here
// is reachable today: standalone has no save/load driver yet (GUL::SaveRestore
// 0x140095a90 is a no-op and nothing calls SaveLoad::SetOperation for a game
// save). Vtable slot-3 wiring is deliberately deferred to the repo-wide
// save/load wave — see the header of code/Game/Component/animalwave_serialize.cpp
// for why partial wiring would desync the stream.
//
// Unconditional (no BANISHED_LIVE_* gate): these are pure leaf registrations
// with no behavioural surface until a save runs, and two of them fix latent
// gaps outside the animal wave:
//   * 0x1401c4790 storage defuses the unguarded deref at moveto.cpp:404
//     (MoveTo::Serialize), which sits in the animal lane's blast radius the
//     moment any citizen/animal action serializes.
//   * dispatch slot 0x140152aa8 (System::Time::Serialize) was never
//     registered; destroy/immigration/nomad component serializers resolve it
//     and silently skipped their Time fields.
//
// Hook: address_registry.cpp calls RegisterAnimalSerialize().
#include "resolve.h"
#include "Object/factory_registration.h"  // Object::GetTypeIdByName
#include <cstdint>

struct PastureComponent;
struct WildAnimalComponent;

// animalwave_serialize.cpp
extern "C" void HerdComponent_Serialize(void* self, void* stream);
extern "C" void AnimalComponent_Serialize(void* self, void* stream);
extern "C" void LivestockComponent_Serialize(void* self, void* stream);
extern "C" void PopulationComponent_Serialize_Real(void* self, void* stream);
// pasturecomponent.cpp (Ghidra's shifted name: "Shutdown" = slot-3 Serialize)
void PastureComponent_Shutdown(PastureComponent* self, void* stream);
// wildanimalcomponent.cpp
void WildAnimalComponent_Serialize(WildAnimalComponent* self, void* stream);

// Serialize-only type-id storage. Values are CRC ids that exist only after
// Object::Factory startup, so they are filled by the Fill* pass below (same
// pattern as reg_action_child_ctors2.cpp).
static uint32_t g_tid_PopulationComponent = 0;  // 0x1401c4790
static uint32_t g_tid_ComponentBase       = 0;  // 0x1401c72a8

extern "C" void FillComponentTypeIdGlobals_animal_serialize() {
    if (g_tid_PopulationComponent == 0)
        g_tid_PopulationComponent = ::Object::GetTypeIdByName("PopulationComponent");
    if (g_tid_ComponentBase == 0)
        g_tid_ComponentBase = ::Object::GetTypeIdByName("Component");
}

void RegisterAnimalSerialize() {
    // --- slot-3 Serialize bodies (vtable +0x18) ---
    RegisterExeAddr(0x140121f90, reinterpret_cast<uintptr_t>(&HerdComponent_Serialize));
    RegisterExeAddr(0x140106e40, reinterpret_cast<uintptr_t>(&AnimalComponent_Serialize));
    RegisterExeAddr(0x140123e10, reinterpret_cast<uintptr_t>(&LivestockComponent_Serialize));
    RegisterExeAddr(0x1400544a0, reinterpret_cast<uintptr_t>(&PopulationComponent_Serialize_Real));
    RegisterExeAddr(0x140127f40, reinterpret_cast<uintptr_t>(&PastureComponent_Shutdown));
    RegisterExeAddr(0x14012e300, reinterpret_cast<uintptr_t>(&WildAnimalComponent_Serialize));

    // --- serialize-only type ids ---
    RegisterExeAddr(0x1401c4790, reinterpret_cast<uintptr_t>(&g_tid_PopulationComponent));
    RegisterExeAddr(0x1401c72a8, reinterpret_cast<uintptr_t>(&g_tid_ComponentBase));

    // --- System::Time::Serialize dispatch slot (verified against Runtime
    //     exports). Same storage shape as address_registry.cpp's
    //     RegisterDispatchSlot (that helper is file-static): callers read
    //     fn = *(fn_t*)ResolveAddr(Module::Exe, slot), so the registry must
    //     map the slot to a cell HOLDING the function pointer.
    static uintptr_t s_slot_TimeSerialize =
        (uintptr_t)ResolveExport("?Serialize@Time@System@@QEAAXAEAVSerializer@IO@@@Z");
    RegisterExeAddr(0x140152aa8, reinterpret_cast<uintptr_t>(&s_slot_TimeSerialize));
}
