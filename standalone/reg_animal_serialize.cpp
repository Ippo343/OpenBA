// Animal wave — the type-id storage and dispatch slot that ONLY the serialize
// path reads (save/load parity).
//
// The six animal-family slot-3 Serialize rows that used to live here
// (Herd/Animal/Livestock/Population/Pasture/WildAnimal) went in ENDGAME B.3.8:
// those classes are real classes whose compiler vtables reach Persist
// directly, and nothing read the addresses back.
//
// Unconditional (no BANISHED_LIVE_* gate): these are pure leaf registrations
// with no behavioural surface until a save runs, and they fix latent gaps
// outside the animal wave:
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
