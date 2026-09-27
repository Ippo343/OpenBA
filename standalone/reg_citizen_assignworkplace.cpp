// standalone/reg_citizen_assignworkplace.cpp
// Phase-4 economy — register the CitizenComponent::AssignWorkplace port + its worker-
// roster helpers (code/Game/Component/citizen_assignworkplace.cpp) to their Ghidra exe
// addresses so standalone mode calls the clean C++ instead of returning NULL.
//
// The lead wires RegisterCitizenAssignWorkplace() into RegisterCodeFunctions() in
// standalone/address_registry.cpp AFTER Ghidra-verification + A/B. DORMANT until a
// caller exists (citizencomponent.cpp Update already ResolveAddr's 0x1401163d0, so
// registering it makes that path live).
//
// Closure (all already ported/registered — reused via ResolveAddr in the port):
//   0x140008090 Entity_FindComponent_Storage      0x140003050 find component
//   0x140003f60 find model/transform component     0x140002f90 find MeshInstance
//   0x140003110 grow-array append (ActHunt_DynArraySetCount_3110)
//   0x1400c2b70 model refresh   0x14012c3f0 status-icon toggle
//   0x14012ead0 / 0x14012eb50 material reserve      0x140042c60 audio emitter setup
//   0x140002280 audio emitter data   + Audio::Scene dispatch slots (a00/a08/a10/a18/a20)
// Any unregistered callee simply no-ops (guarded null in the port) — closure-safe.
#include "resolve.h"
#include <cstdint>

extern "C" void CitizenComponent_AssignWorkplace(void* self, void* workplaceIBP, int slotIndex); // 0x1401163d0
extern "C" uint64_t CitizenRoster_CountWorkers_129ff0(int64_t storageComp);                       // 0x140129ff0

void RegisterCitizenAssignWorkplace() {
    RegisterExeAddr(0x1401163d0, reinterpret_cast<uintptr_t>(&CitizenComponent_AssignWorkplace));
    RegisterExeAddr(0x140129ff0, reinterpret_cast<uintptr_t>(&CitizenRoster_CountWorkers_129ff0));
}
