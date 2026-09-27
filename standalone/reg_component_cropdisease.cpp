// standalone/reg_component_cropdisease.cpp
// Registration wave for the CROP INFESTATION / LIVESTOCK DISEASE lane
// (code/Game/Component/diseasecomponent.cpp). Wired via RegisterCropDiseaseBatch().
//
// ⚠ "DiseaseComponent" is the per-BUILDING crop-infestation component, NOT citizen
// sickness — see the header comment in code/Game/Component/diseasecomponent.cpp.
//
// Closure: every 0x140xxxxxx leaf these bodies call was already ported+registered by
// an earlier wave, so this file only adds the DiseaseComponent behaviour bodies plus
// four genuinely-new leaves (the five vtable-slot rows -- Update/Startup/OnAdded/
// OnRemoved/Serialize -- went in ENDGAME B.3.8: DiseaseComponent is a real class and
// nothing read those addresses back):
//     0x1400050a0  DiseaseComponent sibling finder   (no prior port)
//     0x1400537f0  ParticleComponent fade-out        (sibling of 0x140053660)
//     0x140019b70  EventArray_Grow                   (was static in eventlog.cpp)
//     0x1400951d0 / 0x140095230  Year/Season string   (ditto)
// Already registered elsewhere and merely called from here:
//   0x140004680 map finder (reg_component_nature)   0x140004fe0 pasture (reg_work_leaf)
//   0x1400052c0 natural-resource (reg_work_base)    0x140005200 particle (reg_component_nature)
//   0x140005380 / 0x140005440 crop / orchard (reg_work_accessor)
//   0x140005500 animal (reg_action_hunt)            0x140003ea0 state-machine (reg_player_path)
//   0x140005160 description (reg_player_drag)       0x14009fc00 tiles-in-area (reg_work_leaf)
//   0x1400c2ea0 mark-removal (reg_action_hunt)      0x140003110 / 0x140002cc0 DynArray
//   0x1401189b0 Component::Serialize (reg_component_economy)
//   0x140002340 ExternalPtr::Serialize (address_registry)
//   0x140126440 SetAnimIndex (reg_action_cnr)       0x140095030 TileIndex (reg_eventlog_citizen)
//
// rdata constants read by these bodies are all registered already: 0x1401721b8 (12.0),
// 0x140172234 (300.0), 0x14017225c (3600.0), 0x14017214c (1.0). 0x1401722c0 (the float
// sign mask) is inlined as a literal 0x80000000 bit XOR rather than read.
//
// Behaviour stays off unless BANISHED_LIVE_CROPDISEASE is set: registering an address
// is not the same as running it (reference_ported_registered_but_unwired), and the
// vtable slots that actually dispatch these bodies are installed only under that gate.

#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include "Component/DiseaseComponent.h"
#include "Component/DiseaseDescription.h"

class DiseaseComponent;

// --- ported bodies (code/Game/Component/diseasecomponent.cpp) ----------------
extern "C" bool  DiseaseComp_CheckCure(DiseaseComponent*);                   // 0x1400481b0
extern "C" void  DiseaseComp_Cure(DiseaseComponent*);                        // 0x140048320
extern "C" void  DiseaseComp_Infect(DiseaseComponent*);                      // 0x1400483a0
extern "C" void  DiseaseComp_InfectionTick(DiseaseComponent*, int64_t*);     // 0x140048610
extern "C" void  DiseaseComp_ActiveDisease(DiseaseComponent*, int64_t*);     // 0x140048840
extern "C" void  DiseaseComp_KillOnePlant(DiseaseComponent*);                // 0x140047b40
extern "C" void  DiseaseComp_KillOneAnimal(DiseaseComponent*);               // 0x140047d70
extern "C" bool  DiseaseComp_SameCropProbe(void*, int, int, int, int, int64_t); // 0x14009fb60
extern "C" void* DiseaseComp_GrownIdentity(uint64_t*, void*);                // 0x140047f80
extern "C" void* DiseaseComp_FindSibling(int64_t, void*);                    // 0x1400050a0

// --- shared event-log leaves (code/Game/Game/eventlog.cpp) -------------------
extern "C" void EventLog_ArrayGrow_Reg(void*, int, const void*);             // 0x140019b70
extern "C" void EventLog_YearString_Reg(void*, void*);                       // 0x1400951d0
extern "C" void EventLog_SeasonString_Reg(void*, void*, void*);              // 0x140095230

// Type-id globals this lane's bodies (and the exe's own copies, should any survive)
// read out of .rdata. Filled after Engine::Startup, when Object::Factory has assigned
// the real CRC ids — a null type-id global fakes an ordering bug
// (reference_null_typeid_global_fakes_ordering_bug), so these get storage here and a
// value later.
static uint32_t g_tid_DiseaseComponent   = 0;   // 0x1401c4170

extern "C" void FillTypeIdGlobals_cropdisease() {
    g_tid_DiseaseComponent   = DiseaseComponent::TypeId();
}

void RegisterCropDiseaseBatch() {
    // Behaviour bodies
    RegisterExeAddr(0x1400481b0, reinterpret_cast<uintptr_t>(&DiseaseComp_CheckCure));
    RegisterExeAddr(0x140048320, reinterpret_cast<uintptr_t>(&DiseaseComp_Cure));
    RegisterExeAddr(0x1400483a0, reinterpret_cast<uintptr_t>(&DiseaseComp_Infect));
    RegisterExeAddr(0x140048610, reinterpret_cast<uintptr_t>(&DiseaseComp_InfectionTick));
    RegisterExeAddr(0x140048840, reinterpret_cast<uintptr_t>(&DiseaseComp_ActiveDisease));
    RegisterExeAddr(0x140047b40, reinterpret_cast<uintptr_t>(&DiseaseComp_KillOnePlant));
    RegisterExeAddr(0x140047d70, reinterpret_cast<uintptr_t>(&DiseaseComp_KillOneAnimal));

    // New shared leaves
    RegisterExeAddr(0x14009fb60, reinterpret_cast<uintptr_t>(&DiseaseComp_SameCropProbe));
    RegisterExeAddr(0x140047f80, reinterpret_cast<uintptr_t>(&DiseaseComp_GrownIdentity));
    RegisterExeAddr(0x1400050a0, reinterpret_cast<uintptr_t>(&DiseaseComp_FindSibling));

    // Event-log leaves promoted out of eventlog.cpp's file scope
    RegisterExeAddr(0x140019b70, reinterpret_cast<uintptr_t>(&EventLog_ArrayGrow_Reg));
    RegisterExeAddr(0x1400951d0, reinterpret_cast<uintptr_t>(&EventLog_YearString_Reg));
    RegisterExeAddr(0x140095230, reinterpret_cast<uintptr_t>(&EventLog_SeasonString_Reg));

    // Type-id global storage (values filled by FillTypeIdGlobals_cropdisease)
    RegisterExeAddr(0x1401c4170, reinterpret_cast<uintptr_t>(&g_tid_DiseaseComponent));
}
