// standalone/reg_economy_typeids.cpp
//
// Type-id .rdata globals needed to make BUILDING component-finds resolve in the
// standalone exe, so the citizen work/economy loop can validate + path to a workplace.
//
// In standalone the .rdata type-id globals are 0xFFFFFFFF (unfilled) unless we both
// (a) register the global's address -> a storage uint32_t and (b) fill that storage
// from GetTypeIdByName AFTER Engine::Startup (Object::Factory::Startup assigns the real
// CRC ids only then; InvokeAllRegistrations is too early). Exe reimpls that read these
// globals via ResolveAddr(Module::Exe, 0x1401cXXXX) then get the real CRC and their
// IsDerivedFrom / GetTypeId-keyed FindComponent scans match.
//
// These are SHARED globals — filling them un-dormants every reader (not just the citizen
// path). Add ids here only as the economy path needs them, and A/B the DEFAULT build
// (envs off) after each addition to catch world-build/render regressions.
//
//   0x1401c7938  InteractDescription  -- read by FUN_140123070 (Pf_RecursiveChildSum,
//                position-count of a building's InteractComponent) via Entity_impl_7fb0;
//                with it null, 7fb0 can't resolve a workplace's cell -> CheckWorkplaceValid
//                fails -> State_Work can't proceed. (InteractComponent 0x1401c7900 is
//                already registered+filled by RegisterWorkTailFireBatch.)
//
//   0x1401c79e0  MapComponent  -- read by FUN_140004680 (FloorComponent_FindResData,
//                the "find MapComponent on owner" scan). Entity_impl_7fb0 (workplace anchor
//                cell) tries PATH 1 (InteractComponent work-positions) first; a StorageBarn
//                genuinely has none (it's storage, not a production workplace), so PATH 1
//                returns 0 and 7fb0 falls to PATH 2, which finds the building's MapComponent
//                and reads its packed tile coords (+0x28 tileX / +0x2a tileY / +0x2c fpW /
//                +0x2e fpH -- written at spawn by starttown.cpp) to compute the building's
//                center cell. With c79e0 unfilled, FloorComponent_FindResData scans for
//                type-id 0, matches nothing, returns null -> PATH 2 fails too -> 7fb0 = -1
//                -> CheckWorkplaceValid invalid -> State_Work can't path to the workplace.

#ifdef STANDALONE_EXE

#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>

static uint32_t g_tid_InteractDescription = 0;   // 0x1401c7938
static uint32_t g_tid_MapComponent        = 0;   // 0x1401c79e0

// Register the .rdata global storage (value filled later by FillEconomyTypeIdGlobals).
void RegisterEconomyTypeIds() {
    RegisterExeAddr(0x1401c7938, reinterpret_cast<uintptr_t>(&g_tid_InteractDescription));
    RegisterExeAddr(0x1401c79e0, reinterpret_cast<uintptr_t>(&g_tid_MapComponent));
}

// Fill from the real-CRC type tree — call AFTER Engine::Startup.
extern "C" void FillEconomyTypeIdGlobals() {
    g_tid_InteractDescription = ::Object::GetTypeIdByName("InteractDescription");
    g_tid_MapComponent        = ::Object::GetTypeIdByName("MapComponent");
}

#endif // STANDALONE_EXE
