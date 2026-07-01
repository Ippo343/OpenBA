// standalone/reg_town_workplace_registry_reader.cpp
// Phase-4 economy: register the TownManager profession-BST workplace READER
// (0x14008d100 FindAvailableBuilding), ported in
// code/Game/Game/town_workplace_registry.cpp. Its callee closure is already
// registered elsewhere and reached via ResolveAddr(Module::Exe, ...) at runtime:
//   0x140089470  optimize/sort   (OPTIONAL — null-guarded in the port)
//   0x14012e5c0  CalcMaxWorkers   (reg_component_nature / reg_tutorial_base)
//   0x140002e10  storage/reach    (reg_component_economy)
//   0x140003110  array-append + the 0x140017230 AVL insert family (writer side)
// The type-ids (WorkPlaceComponent / WorkComponent) are resolved self-contained
// via Object::GetTypeIdByName inside the port, so no .rdata DWORD dependency here.
// Dormant until a caller passes a real professionKey; A/B-safe no-op when the
// townMgr+0x100 BST is empty (default standalone).
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern "C" void* FindAvailableBuilding(int64_t townMgr, void* out, uint64_t professionKey); // 0x14008d100

void RegisterTownWorkplaceReader() {
    RegisterExeAddr(0x14008d100, reinterpret_cast<uintptr_t>(&FindAvailableBuilding));
}
#endif // STANDALONE_EXE
