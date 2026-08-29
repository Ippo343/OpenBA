// Cemetery/Healer slot-3 stream Serialize registrations (save/load parity).
//
// Both bodies already live in code/Game/Component/{cemeterycomponent,healercomponent}.cpp
// as CemeteryComponent_Shutdown / HealerComponent_Shutdown (Ghidra's misleading auto-name
// for the real vtable slot [3] Serialize) and are already wired into each class's LIVE
// vtable slot 3 inside FinishLiveVtable (function-scope, not namespace-scope static init —
// see the CitizenAIComponent FillTypedVtable trap in reference_live_vtable_slot_must_be_wired_in_installlive.md).
// This file only adds the missing RegisterExeAddr(retail_addr, ...) leaves so
// ResolveAddr(Module::Exe, ...) finds them too, matching the pattern reg_animal_serialize.cpp
// set for the animal-wave serializers.
//
//   CemeteryComponent::Serialize  0x1400475c0  -- REAL fields: base, empty controller-compat
//                                                  stub block, then a versioned block that
//                                                  serializes plotCount(+0x50) with load-side
//                                                  resize of the plot index array(+0x48) and
//                                                  bulk-serializes the int array itself.
//   HealerComponent::Serialize    0x14004aeb0  -- EMPTY: base Component_Serialize plus two
//                                                  no-op Serializer ctor/dtor blocks, no
//                                                  per-instance fields at all.
//
// Unconditional (no BANISHED_LIVE_* gate) — pure leaf registrations with no behavioural
// surface until a save runs, same rationale as reg_animal_serialize.cpp.
#include "resolve.h"
#include "Component/cemeterycomponent_funcs.h"
#include "Component/healercomponent_funcs.h"
#include <cstdint>

void RegisterCemeteryHealerSerialize() {
    RegisterExeAddr(0x1400475c0, reinterpret_cast<uintptr_t>(&CemeteryComponent_Shutdown));
    RegisterExeAddr(0x14004aeb0, reinterpret_cast<uintptr_t>(&HealerComponent_Shutdown));
}
